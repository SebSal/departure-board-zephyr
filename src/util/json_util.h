#ifndef APP_UTIL_JSON_UTIL_H_
#define APP_UTIL_JSON_UTIL_H_

#include <stdbool.h>
#include <stddef.h>

/* jsmn is a single-header library that emits its implementation into every
 * translation unit unless JSMN_HEADER is defined.
 */
#define JSMN_HEADER
#include "jsmn.h"

/* Parses json into the one token pool, which every parser shares. Returns the token count, a
 * negative jsmn error, or -EBADMSG if the root is not an object.
 */
int json_tokenize_object(const char *json, jsmntok_t **tokens);

/* Length of a token's text. */
static inline int json_tok_len(const jsmntok_t *tok)
{
	return tok->end - tok->start;
}

/* Start of a token's text inside json. Not NUL-terminated, json_tok_len() gives its length. */
static inline const char *json_tok_str(const char *json, const jsmntok_t *tok)
{
	return json + tok->start;
}

/* Returns 0 if tok is a string token equal to s, -1 otherwise. */
int json_streq(const char *json, const jsmntok_t *tok, const char *s);

/* Returns the number of tokens making up the value at index i, so that
 * i + json_skip_tokens(t, count, i) is the next sibling.
 */
int json_skip_tokens(const jsmntok_t *t, int count, int i);

/* Token index of the value belonging to direct child key key of the object at obj,
 * or -1. Nested values are skipped over, so only the object's own keys can match.
 */
int json_object_find(const char *json, const jsmntok_t *t, int count, int obj, const char *key);

/* As json_object_find(), but -1 as well when the value is not of the given type. */
int json_object_find_type(const char *json, const jsmntok_t *t, int count, int obj, const char *key,
						  jsmntype_t type);

/* Copies a string token into dst, truncating to fit. */
void json_copy_token(char *dst, size_t dst_size, const char *json, const jsmntok_t *tok);

/* True if tok is the primitive true. */
bool json_token_is_true(const char *json, const jsmntok_t *tok);

/* Rewrites the escape sequences the departure feeds use (German umlauts and "\/"). */
void json_strip_unicode_escapes(char *s);

/* Folds the German letters a feed sends as raw UTF-8 down to ASCII, in place. */
void json_fold_utf8_german(char *s);

#endif /* APP_UTIL_JSON_UTIL_H_ */
