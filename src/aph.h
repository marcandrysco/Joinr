#pragma once


#define APH_BADOPT  (-1)
#define APH_MISSING (-2)

/**
 * Options enumerator.
 *   @APH_DEF: Default options.
 *   @APH_ONCE: Only allow a single call to this flag.
 */
#define APH_DEF  (0)
#define APH_REQ  (0x01)
#define APH_ONCE (0x02)

enum aph_type_e {
	aph_end_v,
	aph_flag_v,
	aph_str_v,
};

/**
 * Data structure.
 *   @flag: The flag.
 *   @str: The string.
 */
union aph_data_u {
	bool *flag;
	const char **str;
};

struct aph_t {
	uint32_t cnt;
	char ch; 
	const char *str;
	uint32_t opts;
	enum aph_type_e type;
	union aph_data_u data;
};


/**
 * List node structure.
 *   @str: The string.
 *   @next: The next node.
 */
struct aph_node_t {
	const char *str;
	struct aph_node_t *next;
};

/**
 * List structure.
 *   @len: The length.
 *   @head, tail: The head and tail nodes.
 */
struct aph_list_t {
	uint32_t len;
	struct aph_node_t *head, **tail;
};

/*
 * function declarations
 */
int aph_parse(struct aph_t *aph, uint32_t opts, int argc, char **argv, char **err);

struct aph_list_t *aph_list_new(void);
void aph_list_delete(struct aph_list_t *list);
void aph_list_add(struct aph_list_t *list, const char *str);



#define APH_FLAG(ch, str, opts, ptr) { 0, ch,   str,  opts, aph_flag_v, { .flag = ptr } }

/**
 * Create a string row.
 *   @ch: The character.
 *   @st: The string.
 *   @opts: The options.
 *   @ptr: The output pointer.
 */
#define APH_STR(ch, st, opts, ptr) { 0, ch, st, opts, aph_str_v, { .str = ptr } }

/**
 * End the parsing list.
 */
#define APH_END { 0, '\0', NULL, 0, aph_end_v, { } }
