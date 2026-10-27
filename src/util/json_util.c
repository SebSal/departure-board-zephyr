/* Included before json_util.h (which sets JSMN_HEADER) so that this translation
 * unit provides the one definition of the jsmn functions for the whole app.
 */
#include "jsmn.h"
#include "json_util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/sys/util.h>

/* ---------------------------------------- Token storage --------------------------------------- */

/* Sized for the largest departure feed seen. */
#define JSON_TOKEN_COUNT 1280

static jsmntok_t json_tokens[JSON_TOKEN_COUNT];

int json_tokenize_object(const char *json, jsmntok_t **tokens)
{
	jsmn_parser p;

	*tokens = json_tokens;

	jsmn_init(&p);
	int r = jsmn_parse(&p, json, strlen(json), json_tokens, ARRAY_SIZE(json_tokens));

	if (r < 0) {
		return r;
	}
	if (r < 1 || json_tokens[0].type != JSMN_OBJECT) {
		return -EBADMSG;
	}
	return r;
}

/* ---------------------------------------- Token helpers --------------------------------------- */

int json_streq(const char *json, const jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

int json_skip_tokens(const jsmntok_t *t, int count, int i)
{
	/* Counts the values still to be skipped instead of recursing, so a deeply nested feed
	 * cannot run the stack out. Tokens are in document order, so each one read is the next
	 * pending value. An object adds a key and a value per entry, an array one value per element.
	 * A size that overstates the tokens behind it ends the skip at count, not in stale tokens.
	 */
	int pending = 1;
	int k = i;

	while (pending > 0 && k < count) {
		pending--;
		if (t[k].type == JSMN_OBJECT) {
			pending += 2 * t[k].size;
		} else if (t[k].type == JSMN_ARRAY) {
			pending += t[k].size;
		}
		k++;
	}
	return k - i;
}

int json_object_find(const char *json, const jsmntok_t *t, int count, int obj, const char *key)
{
	if (obj < 0 || obj >= count || t[obj].type != JSMN_OBJECT) {
		return -1;
	}

	int idx = obj + 1;

	for (int i = 0; i < t[obj].size && idx + 1 < count; i++) {
		int value = idx + 1;

		if (json_streq(json, &t[idx], key) == 0) {
			return value;
		}
		idx = value + json_skip_tokens(t, count, value);
	}
	return -1;
}

int json_object_find_type(const char *json, const jsmntok_t *t, int count, int obj, const char *key,
						  jsmntype_t type)
{
	int value = json_object_find(json, t, count, obj, key);

	return (value >= 0 && t[value].type == type) ? value : -1;
}

void json_copy_token(char *dst, size_t dst_size, const char *json, const jsmntok_t *tok)
{
	snprintf(dst, dst_size, "%.*s", tok->end - tok->start, json + tok->start);
}

bool json_token_is_true(const char *json, const jsmntok_t *tok)
{
	return tok->type == JSMN_PRIMITIVE && tok->end - tok->start == 4 &&
		   strncmp(json + tok->start, "true", 4) == 0;
}

/* ---------------------------------------- Text cleanup ---------------------------------------- */

void json_strip_unicode_escapes(char *s)
{
	static const struct {
		const char *seq;
		char ch;
	} map[] = {
		{"\\u00e4", 'a'}, {"\\u00c4", 'A'}, {"\\u00f6", 'o'}, {"\\u00d6", 'O'},
		{"\\u00fc", 'u'}, {"\\u00dc", 'U'}, {"\\u00df", 's'}, {"\\/", '/'},
	};
	for (int i = 0; i < ARRAY_SIZE(map); i++) {
		char *p;
		int seq_len = strlen(map[i].seq);

		while ((p = strstr(s, map[i].seq)) != NULL) {
			*p = map[i].ch;
			memmove(p + 1, p + seq_len, strlen(p + seq_len) + 1);
		}
	}
}

void json_fold_utf8_german(char *s)
{
	static const struct {
		const char *seq;
		char ch;
	} map[] = {
		{"\xc3\xa4", 'a'}, {"\xc3\x84", 'A'}, {"\xc3\xb6", 'o'}, {"\xc3\x96", 'O'},
		{"\xc3\xbc", 'u'}, {"\xc3\x9c", 'U'}, {"\xc3\x9f", 's'},
	};
	for (int i = 0; i < ARRAY_SIZE(map); i++) {
		char *p;

		while ((p = strstr(s, map[i].seq)) != NULL) {
			*p = map[i].ch;
			memmove(p + 1, p + 2, strlen(p + 2) + 1);
		}
	}
}
