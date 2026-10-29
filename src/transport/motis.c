#include "motis.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <util/json_util.h>
#include <util/time_util.h>

LOG_MODULE_REGISTER(app_motis, CONFIG_APP_MOTIS_LOG_LEVEL);

/* ---------------------------------------- Constants ------------------------------------------- */

#define MOTIS_BASE_LINK_DEFAULT                                                                    \
	"https://api.transitous.org/api/v1/stoptimes?exactRadius=true&n=4&stopId="

/* Long enough for the longest headsign seen in the wild; anything past it is cut off
 * before the cleanup runs, which only ever shortens the string further.
 */
#define HEADSIGN_MAX 64

/* ---------------------------------------- Helpers --------------------------------------------- */

/* Reads the operator's line colour, six hex digits as GTFS writes them. A leading "#" is accepted
 * as well, in case the feed adds one. Anything else reads as black.
 */
static uint32_t parse_hex_color(const char *s, int len)
{
	if (len > 0 && s[0] == '#') {
		s++;
		len--;
	}
	if (len != 6) {
		return 0x000000;
	}

	uint32_t color = 0;

	for (int i = 0; i < 6; i++) {
		char c = s[i];
		uint32_t digit;

		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else {
			return 0x000000;
		}
		color = (color << 4) | digit;
	}

	return color;
}

/* Shortens a MOTIS headsign to something that survives a 16 character field.
 *
 * The feed spells destinations out in full - "S+U Frankfurter Allee (Berlin)" - because
 * it is written for journey planners with room to show them. Truncating that raw leaves
 * "S+U Frankfurter ", which names no station. Two rules recover most of the width:
 * drop the parenthesised city, and drop the leading station-type marker. Rewrites in
 * place and only ever shortens.
 */
static void shorten_headsign(char *s)
{
	/* "... (Berlin)" - only when the parenthesis actually closes the string, so a name
	 * that genuinely contains brackets is left alone.
	 */
	size_t len = strlen(s);

	if (len > 0 && s[len - 1] == ')') {
		char *open = strrchr(s, '(');

		if (open && open > s && open[-1] == ' ') {
			open[-1] = '\0';
		}
	}

	/* "S+U ", "U+S ", "S ", "U " - the mode markers VBB prefixes to station names. */
	static const char *const prefixes[] = {"S+U ", "U+S ", "S ", "U "};

	for (int i = 0; i < ARRAY_SIZE(prefixes); i++) {
		size_t plen = strlen(prefixes[i]);

		if (strncmp(s, prefixes[i], plen) == 0) {
			memmove(s, s + plen, strlen(s + plen) + 1);
			break;
		}
	}
}

/* ---------------------------------------- Parser ---------------------------------------------- */

/* Reads the departure time off a stop time's nested "place", which carries the realtime prediction
 * in "departure" and the timetable in "scheduledDeparture". The former is null when no realtime
 * data covers the trip, so fall back rather than drop it. Returns -1 when neither gives a usable
 * time.
 */
static int64_t parse_place(const char *json, const jsmntok_t *t, int r, int place, int e)
{
	int when = json_object_find_type(json, t, r, place, "departure", JSMN_STRING);

	if (when < 0) {
		when = json_object_find_type(json, t, r, place, "scheduledDeparture", JSMN_STRING);
	}
	if (when < 0) {
		return -1;
	}

	int64_t timestamp;
	int32_t offset;

	if (time_iso8601_to_unix(json_tok_str(json, &t[when]), json_tok_len(&t[when]), &timestamp,
							 &offset) != 0) {
		LOG_WRN("Unparsable timestamp for departure %d", e);
		return -1;
	}

	return timestamp;
}

/* Reads one stop time into line. Returns false for a trip that should not take a row: one that is
 * cancelled, one without a usable departure time, or one whose departure is not after
 * current_time.
 */
static bool parse_stop_time(const char *json, const jsmntok_t *t, int r, int st, int e,
							int64_t current_time, struct departure_line *line)
{
	/* A cancelled trip still occupies a slot in the response. Showing it would promise a
	 * departure that is not coming, which is worse than showing nothing.
	 */
	int cancelled = json_object_find(json, t, r, st, "cancelled");

	if (cancelled >= 0 && json_token_is_true(json, &t[cancelled])) {
		LOG_DBG("Skipping cancelled departure %d", e);
		return false;
	}

	int name = json_object_find_type(json, t, r, st, "routeShortName", JSMN_STRING);

	if (name < 0 || json_tok_len(&t[name]) == 0) {
		name = json_object_find_type(json, t, r, st, "displayName", JSMN_STRING);
	}
	if (name >= 0) {
		LOG_DBG("Line number for departure %d: %.*s", e, json_tok_len(&t[name]),
				json_tok_str(json, &t[name]));
		json_copy_token(line->line_number, sizeof(line->line_number), json, &t[name]);
	}

	/* The operator's own colours for the line, straight from the feed. Without them the line
	 * shows white on black.
	 */
	line->line_text_color = 0xFFFFFF;

	int color = json_object_find_type(json, t, r, st, "routeColor", JSMN_STRING);

	if (color >= 0) {
		LOG_DBG("Colour for departure %d: %.*s", e, json_tok_len(&t[color]),
				json_tok_str(json, &t[color]));
		line->line_color = parse_hex_color(json_tok_str(json, &t[color]), json_tok_len(&t[color]));
	}

	color = json_object_find_type(json, t, r, st, "routeTextColor", JSMN_STRING);
	if (color >= 0) {
		LOG_DBG("Text colour for departure %d: %.*s", e, json_tok_len(&t[color]),
				json_tok_str(json, &t[color]));
		line->line_text_color =
			parse_hex_color(json_tok_str(json, &t[color]), json_tok_len(&t[color]));
	}

	int headsign = json_object_find_type(json, t, r, st, "headsign", JSMN_STRING);

	if (headsign >= 0) {
		char buf[HEADSIGN_MAX];

		json_copy_token(buf, sizeof(buf), json, &t[headsign]);
		shorten_headsign(buf);
		LOG_DBG("Line direction for departure %d: %s", e, buf);
		/* Explicit precision rather than a bare "%s": the truncation is intended,
		 * and spelling it out keeps -Wformat-truncation quiet.
		 */
		snprintf(line->destination, sizeof(line->destination), "%.*s",
				 (int)sizeof(line->destination) - 1, buf);
	}

	int place = json_object_find(json, t, r, st, "place");
	int64_t timestamp = place >= 0 ? parse_place(json, t, r, place, e) : -1;

	/* A trip without a usable time is skipped like a cancelled one. */
	if (timestamp < 0) {
		LOG_WRN("No departure time for departure %d, skipping it", e);
		return false;
	}

	/* The query starts at the server's now, but a delayed fetch, a clock apart from ours or a
	 * realtime prediction ahead of the timetable can still hand over a trip that has left.
	 */
	if (timestamp <= current_time) {
		LOG_DBG("Skipping departure %d, departure %lld is not after %lld", e, timestamp,
				current_time);
		return false;
	}

	line->departure_minutes = time_minutes_until(timestamp, current_time);
	LOG_DBG("Departure in %d min", line->departure_minutes);

	return true;
}

/* Writes the wall clock time of current_time to departure->updated. MOTIS timestamps everything
 * in UTC and names the zone only as "Europe/Berlin", so unlike the feeds that carry a numeric
 * offset there is nothing to read the wall clock off. The EU switching rule supplies it.
 */
static void set_updated(struct departure_data *departure, int64_t current_time)
{
	int64_t local = current_time + time_eu_utc_offset(current_time);

	if (local < 0) {
		local = 0;
	}
	int hours = (int)((local / 3600) % 24);
	int minutes = (int)((local / 60) % 60);

	snprintf(departure->updated, sizeof(departure->updated), "%02d:%02d", hours, minutes);
	LOG_DBG("Updated: %s", departure->updated);
}

static int motis_parse(char *json, struct departure_data *departure, int64_t current_time)
{
	jsmntok_t *t;

	json_fold_utf8_german(json);
	LOG_DBG("Parsing JSON data:\n%s", json);

	int r = json_tokenize_object(json, &t);

	if (r < 0) {
		LOG_ERR("Failed to parse JSON: %d", r);
		return r;
	}

	int stop_times = json_object_find_type(json, t, r, 0, "stopTimes", JSMN_ARRAY);

	if (stop_times < 0) {
		LOG_ERR("\"stopTimes\" array not found");
		return -EBADMSG;
	}

	/* Counts filled rows rather than array entries, so that skipping a cancelled trip
	 * pulls the next one up instead of leaving a gap on the display.
	 */
	int filled = 0;

	int idx = stop_times + 1;

	for (int e = 0; e < t[stop_times].size && idx < r && filled < DEPARTURE_LINE_COUNT; e++) {
		int st = idx;

		idx += json_skip_tokens(t, r, idx); /* advance regardless of what is read below */

		if (t[st].type != JSMN_OBJECT) {
			continue;
		}

		/* Built aside and only kept once it has a departure time. */
		struct departure_line line = DEPARTURE_LINE_EMPTY;

		if (parse_stop_time(json, t, r, st, e, current_time, &line)) {
			departure->lines[filled++] = line;
		}
	}

	set_updated(departure, current_time);

	return 0;
}

/* ---------------------------------------- Provider -------------------------------------------- */

const struct transport_provider motis_provider = {
	.name = "MOTIS",
	.base_link_default = MOTIS_BASE_LINK_DEFAULT,
	.fetch = NULL,
	.parse = motis_parse,
};
