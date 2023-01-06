#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "aph.h"



/**
 * String structure.
 *   @buf: The internal buffer.
 *   @len, max: The current and maximum lengths.
 */
struct str_t {
	char *buf;
	uint32_t len, max;
};

/**
 * Context structure.
 *   @out, deps: The output and depdency file.
 *   @src, map: The sources and mapping structure.
 *   @lin, col: The line and column indices.
 *   @src_cur, src_cnt: The current and total sources.
 *   @lin_cur: The current line.
 */
struct ctx_t {
	FILE *out, *deps;
	struct str_t src, map;
	uint32_t lin, col;
	uint32_t src_cur, src_cnt;
	uint32_t lin_cur;
};

/*
 * local declarations
 */
static void app_proc(const char *path, struct ctx_t *ctx);
static void app_err(const char *fmt, ...);

static FILE *file_open(const char *path, const char *mode)
{
	FILE *file;

	file = fopen(path, mode);
	if(file == NULL)
		app_err("Failed to open '%s'. %s.", path, strerror(errno));

	return file;
}

static struct str_t str_new(void);
static void str_delete(struct str_t str);
static void str_ch(struct str_t *str, char ch);
static void str_add(struct str_t *str, const char *add);
static void str_vlq(struct str_t *str, int32_t val);
static void str_map(struct str_t *str, int32_t col, int32_t src, int32_t slin, int32_t scol, int32_t name);
static char *str_done(struct str_t *str);

static bool rdline(FILE *file, char **line, uint32_t *size);


struct opts_t {
	bool verb, css;
	const char *in, *out, *map, *deps;
};

/**
 * Main entry point.
 *   @argc: The argument count.
 *   @argv: The argument list.
 */
int main(int argc, char **argv)
{
	char *err;
	struct opts_t opts;

	struct aph_t aph[] = {
		APH_STR('\0', NULL, 0, &opts.in),
		APH_STR('o', "output", 0, &opts.out),
		APH_STR('m', "map", 0, &opts.map),
		APH_STR('d', "deps", 0, &opts.deps),
		APH_FLAG('v', "verbose", 0, &opts.verb),
		APH_FLAG('c', "css", 0, &opts.css),
		APH_END
	};

	if(aph_parse(aph, 0, argc, argv, &err) < 0)
		app_err(err);

	if(opts.in == NULL)
		app_err("Missing input file."); // TODO: this should be a list of inputs

	struct ctx_t  ctx;
	FILE *in, *out, *map;

	in = opts.in ? file_open(opts.in, "r") : stdin;
	out = opts.out ? file_open(opts.out, "w") : stdout;
	map = opts.map ? file_open(opts.map, "w") : NULL;

	ctx.out = out;
	ctx.src = str_new();
	ctx.map = str_new();
	ctx.lin = ctx.col = 0;
	ctx.src_cur = ctx.src_cnt = 0;
	ctx.lin_cur = 0;
	ctx.deps = opts.deps ? file_open(opts.deps, "w") : NULL;

	if(ctx.deps != NULL)
		fprintf(ctx.deps, "%s:", opts.out);

	app_proc(opts.in, &ctx);

	if(opts.map != NULL) {
		if(opts.css)
			fprintf(ctx.out, "/*# sourceMappingURL=%s */\n", opts.map);
		else
			fprintf(ctx.out, "//# sourceMappingURL=%s\n", opts.map);

		fprintf(map, "{\n");
		fprintf(map, "  \"version\": 3,\n");
		fprintf(map, "  \"sources\": [%s],\n", str_done(&ctx.src));
		fprintf(map, "  \"sourcesContent\": [],\n");
		fprintf(map, "  \"names\": [],\n");
		fprintf(map, "  \"mappings\": \"%s\"\n", str_done(&ctx.map));
		fprintf(map, "}\n");
	}

	str_delete(ctx.src);
	str_delete(ctx.map);

	if(opts.in != NULL)
		fclose(in);

	if(opts.out != NULL)
		fclose(out);

	if(opts.map != NULL)
		fclose(map);

	if(ctx.deps != NULL) {
		fprintf(ctx.deps, "\n");
		fclose(ctx.deps);
	}

	return 0;
}


/**
 * Process a file.
 *   @path: The path.
 *   @ctx: The context.
 */
static void app_proc(const char *path, struct ctx_t *ctx)
{
	FILE *file;
	uint32_t src = ctx->src_cnt++, lin = 0, size = 256;
	char *line = malloc(256);
	regex_t reg;
	regmatch_t match[2];

	if(ctx->src.len > 0)
		str_add(&ctx->src, ", ");

	str_add(&ctx->src, "\"");
	str_add(&ctx->src, path);
	str_add(&ctx->src, "\"");

	file = fopen(path, "r");
	if(file == NULL)
		app_err("Failed to open '%s'. %s.", path, strerror(errno));

	assert(regcomp(&reg, "^[ \t]*//@include\\[\\([a-zA-Z0-9._-]*\\)\\][ \t]*\n$", 0) == 0);

	while(rdline(file, &line, &size)) {
		if(regexec(&reg, line, 2, match, 0) == 0) {
			char *iter;

			assert(asprintf(&iter, "%.*s", match[1].rm_eo - match[1].rm_so, line + match[1].rm_so) >= 0);

			if(ctx->deps != NULL)
				fprintf(ctx->deps, " %s", iter);

			app_proc(iter, ctx);
			free(iter);
		}
		else {
			fprintf(ctx->out, "%s", line);
			str_map(&ctx->map, 0, src - ctx->src_cur, lin - ctx->lin_cur, 0, 0);
			str_ch(&ctx->map, ';');
			//printf(":: col %d src %d slin %d scol %d\n", 0, src - ctx->src_cur, lin - ctx->lin_cur, 0);
			ctx->src_cur = src;
			ctx->lin_cur = lin;
			ctx->lin++;
		}

		lin++;
	}

	free(line);
	regfree(&reg);
	fclose(file);
}

/**
 * Report an error.
 *   @fmt: The format string.
 *   @...: The argument.
 */
static void app_err(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);

	exit(1);
}


/**
 * Create a new, empty string.
 *   &returns: The string.
 */
static struct str_t str_new(void)
{
	return (struct str_t){ malloc(256), 0, 256 };
}

/**
 * Delete a string.
 *   @str: The string.
 */
static void str_delete(struct str_t str)
{
	free(str.buf);
}

/**
 * Write a character to a string.
 *   @str: The string.
 *   @ch: The character.
 */
static void str_ch(struct str_t *str, char ch) {
	if(str->len == str->max)
		str->buf = realloc(str->buf, str->max *= 2);

	str->buf[str->len++] = ch;
}

/**
 * Add to a string.
 *   @str: The string.
 *   @add: The additional characters.
 */
static void str_add(struct str_t *str, const char *add)
{
	while(*add != '\0')
		str_ch(str, *add++);
}

/**
 * Write a variable length quantity.
 *   @str: The string.
 *   @val: The value.
 */
static void str_vlq(struct str_t *str, int32_t val)
{
	static const char b64[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint32_t u32 = (val < 0) ? (1 | ((-val) << 1)) : (val << 1);

	do {
		str_ch(str, b64[((u32 >= 0x20) ? 0x20 : 0) | (u32 & 0x1f)]);
		u32 >>= 5;
	} while(u32 > 0);
}

static void str_map(struct str_t *str, int32_t col, int32_t src, int32_t slin, int32_t scol, int32_t name)
{
	str_vlq(str, col);
	str_vlq(str, src);
	str_vlq(str, slin);
	str_vlq(str, scol);
	//str_vlq(str, name);
}


/**
 * End a string with a null and return it.
 *   @str: The string.
 *   &returns: The raw string.
 */
static char *str_done(struct str_t *str)
{
	if(str->len == str->max)
		str->buf = realloc(str->buf, str->max *= 2);

	str->buf[str->len++] = '\0';

	return str->buf;
}


/**
 * Read a line.
 *   @file: The file.
 *   @line: Ref. The line.
 *   @size: Ref. The size.
 *   &returns: True if read, false if at end-of-file.
 */
static bool rdline(FILE *file, char **line, uint32_t *size)
{
	char ch;
	uint32_t i = 0;

	while(true) {
		ch = fgetc(file);
		if(ch == EOF)
			break;

		(*line)[i++] = ch;
		if(i == *size)
			*line = realloc(*line, *size *= 2);

		if(ch == '\n')
			break;
	}

	(*line)[i] = '\0';

	return i > 0;
}
