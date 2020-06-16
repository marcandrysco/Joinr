#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aph.h"


/*
 * local declarations
 */
static struct aph_t *get_ch(struct aph_t *aph, char ch);
static struct aph_t *get_long(struct aph_t *aph, const char *name, size_t len);
static struct aph_t *get_plain(struct aph_t *aph);

static int put_val(struct aph_t *row, const char *str);

static int mk_err(int ret, char **err, const char *fmt, ...);


/**
 * Parse the command-line arguments.
 *   @aph: The parsing description.
 *   @opts: The parsing option.
 *   @argc: The argument count.
 *   @argv: The argument array.
 *   @err: Out. Optional. The error string.
 *   &returns: The number of parsed arguments on success, negative on error.
 */
int aph_parse(struct aph_t *aph, uint32_t opts, int argc, char **argv, char **err)
{
	int i = 1;
	struct aph_t *find;

	for(find = aph; find->type != aph_end_v; find++) {
		switch(find->type) {
		case aph_end_v: break;
		case aph_flag_v: *find->data.flag = false; break;
		case aph_str_v: *find->data.str = NULL; break;
		}
	}

	while(argv[i] != NULL) {
		if(argv[i][0] == '-') {
			if(argv[i][1] == '-') {
				char *eq;
				size_t len;

				eq = strchr(argv[i] + 2, '=');
				if(eq != NULL)
					*eq = '\0';

				len = eq ? (eq - (argv[i] + 2)) : strlen(argv[i] + 2);
				find = get_long(aph, argv[i] + 2, len);
				if(find == NULL)
					return mk_err(APH_BADOPT, err, "%s: Unknown option '--%.*s'.", argv[0], len, argv[i] + 2);

				if(find->type != aph_flag_v) {
					if(eq == NULL) {
						i++;
						if(argv[i] == NULL)
							mk_err(APH_MISSING, err, "%s: Missing argument for '--%.*s'.", argv[0], len, argv[i] + 2);

						put_val(find, argv[i]);
					}
					else
						put_val(find, eq + 1);
				}
				else {
					if(eq != NULL)
						return mk_err(APH_BADOPT, err, "%s: Option '--%.*s' does not take a value.", argv[0], len, argv[i] + 2);

					*find->data.flag = true;
				}
			}
			else {
				unsigned int k;

				for(k = 1; argv[i][k] != '\0'; k++) {
					char ch = argv[i][k];

					find = get_ch(aph, ch);
					if(find == NULL)
						return mk_err(APH_BADOPT, err, "%s: Unknown option '-%c'.", argv[0], ch);

					if(find->type != aph_flag_v) {
						if(argv[i][k+1] == '\0') {
							i++;
							if(argv[i] == NULL)
								mk_err(APH_MISSING, err, "%s: Missing argument for '-%c'.", argv[0], ch);

							put_val(find, argv[i]);
						}
						else
							put_val(find, argv[i] + k + 1);

						break;
					}
					else
						*find->data.flag = true;
				}
			}

			i++;
		}
		else {
			find = get_plain(aph);
			if(find == NULL)
				return mk_err(APH_BADOPT, err, "%s: Unknown option '%s'.", argv[i]);

			put_val(find, argv[i]);
			i++;
		}
	}

	return 0;
}



/**
 * Create a new, empty list.
 *   &returns: The list.
 */
struct aph_list_t *aph_list_new(void)
{
	struct aph_list_t *list;

	list = malloc(sizeof(struct aph_list_t));
	*list = (struct aph_list_t){ 0, NULL, &list->head };

	return list;
}

/**
 * Delete a list.
 *   @list: The list.
 */
void aph_list_delete(struct aph_list_t *list)
{
	struct aph_node_t *node;

	while(list->head != NULL) {
		list->head = (node = list->head)->next;
		free(node);
	}

	free(list);
}

/**
 * Add to a list.
 *   @list: The list.
 *   @str: The string.
 */
void aph_list_add(struct aph_list_t *list, const char *str)
{
	*list->tail = malloc(sizeof(struct aph_node_t));
	**list->tail = (struct aph_node_t){ str, NULL };
	list->tail = &(*list->tail)->next;
}


/**
 * Get a row based off a short name.
 *   @aph: The list of rows.
 *   @ch: The short name.
 *   &returns: The row if found, null otherwise.
 */
static struct aph_t *get_ch(struct aph_t *aph, char ch)
{
	while(aph->type != aph_end_v) {
		if(aph->ch == ch)
			return aph;

		aph++;
	}

	return NULL;
}

/**
 * Get a row based off a long name.
 *   @aph: The list of rows.
 *   @name: The long name.
 *   @len: The name length.
 *   &returns: The row if found, null otherwise.
 */
static struct aph_t *get_long(struct aph_t *aph, const char *name, size_t len)
{
	while(aph->type != aph_end_v) {
		if((aph->str != NULL) && (memcmp(aph->str, name, len) == 0) && (aph->str[len] == '\0'))
			return aph;

		aph++;
	}

	return NULL;
}

/**
 * Retrieve a plain argument.
 *   @aph: The list of rows.
 *   &returns: The row if found, null otherwise.
 */
static struct aph_t *get_plain(struct aph_t *aph)
{
	while(aph->type != aph_end_v) {
		if((aph->ch == '\0') && (aph->str == NULL))
			return aph;

		aph++;
	}

	return NULL;
}


/**
 * Write a value given a row.
 *   @row: The row.
 *   @str: The string value.
 *   &returns: Zero on success, non-zero on error.
 */
static int put_val(struct aph_t *row, const char *str)
{
	switch(row->type) {
	case aph_end_v: break;
	case aph_flag_v: break;
	case aph_str_v: *row->data.str = str; break;
	}

	return 0;
}


/**
 * Create an return an error.
 *   @ret: The returned error code.
 *   @err: Out. Optional. The error string.
 *   @fmt: The printf-style format string.
 *   @...: The printf-style arguments.
 */
static int mk_err(int ret, char **err, const char *fmt, ...)
{
	if(err != NULL) {
		va_list args;

		va_start(args, fmt);
		assert(vasprintf(err, fmt, args) >= 0);
		va_end(args);
	}

	return ret;
}
