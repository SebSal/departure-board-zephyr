#include "vrs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <util/json_util.h>
#include <util/time_util.h>

LOG_MODULE_REGISTER(app_vrs, CONFIG_APP_VRS_LOG_LEVEL);

/* ---------------------------------------- Constants ------------------------------------------- */

#define VRS_BASE_LINK_DEFAULT "https://www.vrs.de/index.php?eID=tx_vrsinfo_departuremonitor&i="

#define TRAM_COLOR  0x000000
#define BUS_COLOR   0xCD0000
#define SBAHN_COLOR 0x008B00
#define RB_COLOR    0x000000

/* ---------------------------------------- Helpers --------------------------------------------- */

static void extract_line_number(char *dst, size_t dst_size, const char *src, int src_len)
{
	/* Removes "Taxibus" and "(RRX)" from the line name. */
	for (int i = 0; i < src_len - 1; i++) {
		if (src[i] == ' ' && src[i + 1] == '(') {
			src_len = i;
			break;
		}
	}
	const char *start = src;

	for (int i = 0; i < src_len; i++) {
		if (src[i] == ' ') {
			start = src + i + 1;
		}
	}
	int token_len = src_len - (int)(start - src);

	snprintf(dst, dst_size, "%.*s", token_len, start);
}

static uint32_t product_to_color(const char *s, int len)
{
	static const struct {
		const char *product;
		uint32_t color;
	} map[] = {
		{"LightRail", TRAM_COLOR},       {"Bus", BUS_COLOR},
		{"OnDemandServices", BUS_COLOR}, {"SuburbanTrains", SBAHN_COLOR},
		{"RegionalTrains", RB_COLOR},
	};
	for (int i = 0; i < ARRAY_SIZE(map); i++) {
		/* Compare the length too, otherwise a token that is a strict prefix of a
		 * product name ("Light") would match it.
		 */
		if (len == (int)strlen(map[i].product) && strncmp(s, map[i].product, len) == 0) {
			return map[i].color;
		}
	}
	return 0x000000;
}

/* Reads number, product and direction from an event's "line" object into ls. */
static void parse_line(const char *json, const jsmntok_t *t, int r, int line, int e,
					   struct departure_line *ls)
{
	int tok = json_object_find(json, t, r, line, "number");

	if (tok >= 0) {
		LOG_DBG("Line number for event %d: %.*s", e, json_tok_len(&t[tok]),
				json_tok_str(json, &t[tok]));
		extract_line_number(ls->line_number, sizeof(ls->line_number), json_tok_str(json, &t[tok]),
							json_tok_len(&t[tok]));
	}

	/* All product colours are dark enough for white text. */
	ls->line_text_color = 0xFFFFFF;

	tok = json_object_find(json, t, r, line, "product");
	if (tok >= 0) {
		LOG_DBG("Line product for event %d: %.*s", e, json_tok_len(&t[tok]),
				json_tok_str(json, &t[tok]));
		ls->line_color = product_to_color(json_tok_str(json, &t[tok]), json_tok_len(&t[tok]));
	}

	tok = json_object_find(json, t, r, line, "direction");
	if (tok >= 0) {
		LOG_DBG("Line direction for event %d: %.*s", e, json_tok_len(&t[tok]),
				json_tok_str(json, &t[tok]));
		json_copy_token(ls->destination, sizeof(ls->destination), json, &t[tok]);
	}
}

/* Turns the timestamp in an event's "departure" object into minutes from current_time. */
static void parse_departure(const char *json, const jsmntok_t *t, int r, int dep,
							int64_t current_time, struct departure_line *ls)
{
	int tok = json_object_find(json, t, r, dep, "timestamp");

	if (tok < 0) {
		return;
	}

	char tmp[12];

	json_copy_token(tmp, sizeof(tmp), json, &t[tok]);
	ls->departure_minutes = time_minutes_until(atoll(tmp), current_time);
	LOG_DBG("Event departure in %d min", ls->departure_minutes);
}

/* ---------------------------------------- Parser ---------------------------------------------- */

static int vrs_parse(char *json, struct departure_data *departure, int64_t current_time)
{
	jsmntok_t *t;

	json_strip_unicode_escapes(json);
	LOG_DBG("Parsing JSON data:\n%s", json);

	int r = json_tokenize_object(json, &t);

	if (r < 0) {
		LOG_ERR("Failed to parse JSON: %d", r);
		return r;
	}

	/* Extract "updated" from root object ("DD.MM. HH:MM:SS" → "HH:MM") */
	int updated = json_object_find_type(json, t, r, 0, "updated", JSMN_STRING);

	if (updated >= 0) {
		const char *val = json_tok_str(json, &t[updated]);
		const char *space = memchr(val, ' ', json_tok_len(&t[updated]));

		if (space) {
			LOG_DBG("Updated: %.*s", 5, space + 1);
			snprintf(departure->updated, sizeof(departure->updated), "%.5s", space + 1);
		}
	}

	int events = json_object_find_type(json, t, r, 0, "events", JSMN_ARRAY);

	if (events < 0) {
		LOG_ERR("\"events\" array not found");
		return -EBADMSG;
	}

	/* Event e fills row e, so an event that is not an object, or one without a timestamp, leaves
	 * its row empty.
	 */
	int idx = events + 1;

	for (int e = 0; e < t[events].size && e < DEPARTURE_LINE_COUNT && idx < r; e++) {
		int event = idx;

		idx += json_skip_tokens(t, r, idx);

		if (t[event].type != JSMN_OBJECT) {
			continue;
		}

		struct departure_line *ls = &departure->lines[e];
		int line = json_object_find_type(json, t, r, event, "line", JSMN_OBJECT);
		int dep = json_object_find_type(json, t, r, event, "departure", JSMN_OBJECT);

		if (line >= 0) {
			parse_line(json, t, r, line, e, ls);
		}
		if (dep >= 0) {
			parse_departure(json, t, r, dep, current_time, ls);
		}
	}

	return 0;
}

/* ---------------------------------------- Provider -------------------------------------------- */

const struct transport_provider vrs_provider = {
	.name = "VRS",
	.base_link_default = VRS_BASE_LINK_DEFAULT,
	.fetch = NULL,
	.parse = vrs_parse,
};
