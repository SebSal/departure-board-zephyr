/* Screens for the 1 bit e-ink panel.
 *
 * The colour layout in ui.c packs six rows of line badge, destination and minutes into 320x240.
 * None of that survives at 1 bpp: the line colours collapse to a single black, and the small type
 * needed to fit a destination is hard to read on e-ink. This is a separate implementation of the
 * same ui.h rather than a set of conditionals in the colour one, so neither has to carry the
 * other's compromises.
 *
 * Everything here is black on white, the way a printed timetable is, and sized so it stays legible
 * from a distance. The panel is landscape, split into one section per station: a quiet header
 * naming the station, then its two departures with the wait set large on the right. Nobody reads a
 * departure board, they glance at it for a number, so that number is the one thing sized to be
 * found from across a platform.
 */

#include "ui.h"

#include <lvgl.h>
#include <net/http_client.h>

/* Supplied by the build as the output of "git describe". */
#ifndef APP_GIT_VERSION
#define APP_GIT_VERSION "unknown"
#endif

/* ---------------------------------------- Layout ---------------------------------------------- */

#define MARGIN         8
#define VERSION_OFFSET 2

/* The station header: a montserrat_16 name over a hairline. Its line height is 18, so the 22 px
 * text box leaves a couple of pixels of air, and the section rule is drawn into the top of it.
 */
#define HEADER_TEXT_H 22
#define HEADER_H      24

/* Letter spacing on the station name. Tracking it out is what makes a small label read as a
 * heading rather than as undersized body text.
 */
#define HEADER_TRACK 1

/* Space kept clear on the right of the header for the update time. "HH:MM" is a fixed shape, and
 * montserrat_14 sets it in 42.1 px.
 */
#define STAMP_W   46
#define STAMP_GAP 6

/* Columns for a departure row. Both are fixed so that all four rows line up with each other; the
 * destination takes whatever is left over and is ellipsised if it does not fit.
 *
 * LINE_COL_W holds three montserrat_32 digits, which is the widest a line number gets. TIME_COL_W
 * holds two montserrat_40 digits plus the montserrat_16 "min" and the gap between them. What is
 * left is 238 px of destination, about 13 characters at montserrat_32.
 */
#define LINE_COL_W 68
#define TIME_COL_W 94

/* Space reserved for the "min" that trails the number. It is a constant string, so the width is
 * fixed: montserrat_16 sets it in 32.2 px.
 */
#define UNIT_W   34
#define UNIT_GAP 4

/* Fonts, named by the job rather than the size, so the sizes can be retuned in one place. */
#define FONT_STATION (&lv_font_montserrat_16)
#define FONT_STAMP   (&lv_font_montserrat_14)
#define FONT_LINE    (&lv_font_montserrat_32)
#define FONT_DEST    (&lv_font_montserrat_32)
#define FONT_MINUTES (&lv_font_montserrat_40)
#define FONT_UNIT    (&lv_font_montserrat_16)

/* ---------------------------------------- Screen ---------------------------------------------- */

static void ui_draw(void)
{
	lv_refr_now(NULL);
}

static int32_t screen_width(void)
{
	return lv_display_get_horizontal_resolution(lv_display_get_default());
}

static int32_t screen_height(void)
{
	return lv_display_get_vertical_resolution(lv_display_get_default());
}

/* White page, black ink. There is no backlight, so the panel is read the way paper is. */
static lv_obj_t *screen_reset(void)
{
	lv_obj_t *scr = lv_scr_act();

	lv_obj_clean(lv_layer_top());
	lv_obj_clean(scr);
	lv_obj_remove_style_all(scr);
	lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	return scr;
}

/* ---------------------------------------- Widgets --------------------------------------------- */

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
						   lv_align_t align, int32_t x_ofs, int32_t y_ofs)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_align(label, align, x_ofs, y_ofs);

	return label;
}

/* Single line label of a fixed width, left aligned, that cuts overlong text with an ellipsis
 * rather than scrolling it, an e-ink panel cannot animate.
 */
static lv_obj_t *add_clipped_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
								   int32_t width, int32_t x_ofs)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
	lv_obj_set_width(label, width);
	lv_obj_align(label, LV_ALIGN_LEFT_MID, x_ofs, 0);

	return label;
}

/* Centred message that wraps within the margins. The width has to be set before the align, so
 * that the label is placed at its final size.
 */
static lv_obj_t *add_wrapped_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
								   int32_t y_ofs)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_width(label, screen_width() - 2 * MARGIN);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, y_ofs);

	return label;
}

/* Distance from the top of a label to the baseline of its text. LVGL sizes a label to the font's
 * line height and hangs the descender below the baseline, so two labels at different sizes only
 * share a baseline if one is offset by the difference of these.
 */
static int32_t font_ascent(const lv_font_t *font)
{
	return font->line_height - font->base_line;
}

/* Vertical shift that puts a centred label's baseline onto another centred label's, when both sit
 * in the same box. Centring aligns their boxes, and boxes of different line heights do not put
 * their baselines in the same place.
 */
static int32_t baseline_offset(int32_t box_h, const lv_font_t *ref, const lv_font_t *font)
{
	return ((box_h - ref->line_height) / 2 + font_ascent(ref)) -
		   ((box_h - font->line_height) / 2 + font_ascent(font));
}

/* Invisible box used purely to carry a coordinate system, so the things inside it can be aligned
 * against its edges instead of against the screen.
 */
static lv_obj_t *add_container(lv_obj_t *parent, int32_t width, int32_t height, int32_t x,
							   int32_t y)
{
	lv_obj_t *box = lv_obj_create(parent);

	lv_obj_remove_style_all(box);
	lv_obj_set_size(box, width, height);
	lv_obj_set_pos(box, x, y);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	return box;
}

/* Hairline. A filled one pixel object rather than a line widget, so it renders the same whether
 * or not anti aliasing is available at 1 bpp.
 */
static void add_rule(lv_obj_t *parent, int32_t x, int32_t y, int32_t width)
{
	lv_obj_t *rule = lv_obj_create(parent);

	lv_obj_remove_style_all(rule);
	lv_obj_set_size(rule, width, 1);
	lv_obj_set_pos(rule, x, y);
	lv_obj_set_style_bg_color(rule, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
}

/* Build version, tucked into the bottom right corner of the status screens. */
static void add_version(lv_obj_t *parent)
{
	add_label(parent, APP_GIT_VERSION, &lv_font_montserrat_10, LV_ALIGN_BOTTOM_RIGHT,
			  -VERSION_OFFSET, -VERSION_OFFSET);
}

/* ---------------------------------------- Departure screen ------------------------------------ */

/* The station name, small and tracked out over a hairline. It is deliberately the quietest thing
 * in the section: it changes twice a screen, while the departures under it are what is read.
 *
 * The label prints as configured rather than being upper cased. LVGL's stock Montserrat carries
 * ASCII only, so a name with an umlaut already has no glyph for it, and forcing case would not
 * help that while making a mess of the rest.
 */
static void add_station_header(lv_obj_t *parent, int32_t width,
							   const struct departure_data *station)
{
	/* A label draws its text from the top of its box, so the name is centred in a box of its own
	 * rather than given a height and left to sit against the rule above it.
	 */
	lv_obj_t *box = add_container(parent, width - 2 * MARGIN, HEADER_TEXT_H, MARGIN, 0);
	lv_obj_t *name = add_clipped_label(box, station->label, FONT_STATION,
									   width - 2 * MARGIN - STAMP_W - STAMP_GAP, 0);

	lv_obj_set_style_text_letter_space(name, HEADER_TRACK, 0);

	/* When the feed last refreshed, smaller and on the same baseline as the name. Empty until the
	 * first successful update, and an empty label draws nothing.
	 */
	add_label(box, station->updated, FONT_STAMP, LV_ALIGN_RIGHT_MID, 0,
			  baseline_offset(HEADER_TEXT_H, FONT_STATION, FONT_STAMP));

	add_rule(parent, MARGIN, HEADER_TEXT_H, width - 2 * MARGIN);
}

/* One departure: line number and destination on the left, the wait on the right. */
static void add_departure_row(lv_obj_t *parent, int32_t width, int32_t height, int32_t y,
							  const struct departure_line *line)
{
	lv_obj_t *row = add_container(parent, width, height, 0, y);

	if (line->departure_minutes == DEPARTURE_NO_DATA) {
		add_label(row, "NO DATA", FONT_DEST, LV_ALIGN_LEFT_MID, LINE_COL_W, 0);
		return;
	}

	char departure_time[6];

	lv_snprintf(departure_time, sizeof(departure_time), "%d", line->departure_minutes);

	add_label(row, line->line_number, FONT_LINE, LV_ALIGN_LEFT_MID, 0, 0);

	/* The number is centred in the row and "min" is hung off it, keeping the pair flush right as
	 * the number grows from one digit to two.
	 *
	 * The unit is placed from the top edge and pushed down by the difference in ascent, so the
	 * two sit on a shared baseline. Aligning their boxes instead would drop "min" by the
	 * difference in descender, and would push the taller label clean out of the row.
	 */
	lv_obj_t *minutes =
		add_label(row, departure_time, FONT_MINUTES, LV_ALIGN_RIGHT_MID, -(UNIT_W + UNIT_GAP), 0);
	lv_obj_t *unit = add_label(row, "min", FONT_UNIT, LV_ALIGN_RIGHT_MID, 0, 0);

	lv_obj_align_to(unit, minutes, LV_ALIGN_OUT_RIGHT_TOP, UNIT_GAP,
					font_ascent(FONT_MINUTES) - font_ascent(FONT_UNIT));

	/* Whatever is left between the line number and the time. */
	add_clipped_label(row, line->destination, FONT_DEST, width - LINE_COL_W - TIME_COL_W,
					  LINE_COL_W);
}

/* One station: the name header, then its two departures inset by the margin. */
static void add_section(lv_obj_t *scr, int32_t y, int32_t height,
						const struct departure_data *station)
{
	int32_t width = screen_width();
	int32_t rows_w = width - 2 * MARGIN;
	int32_t rows_h = height - HEADER_H;
	int32_t row_h = rows_h / DEPARTURE_LINE_COUNT;
	lv_obj_t *section = add_container(scr, width, height, 0, y);
	lv_obj_t *rows = add_container(section, rows_w, rows_h, MARGIN, HEADER_H);

	add_station_header(section, width, station);

	for (int l = 0; l < DEPARTURE_LINE_COUNT; l++) {
		add_departure_row(rows, rows_w, row_h, l * row_h, &station->lines[l]);
	}
}

/* ---------------------------------------- Entry points ---------------------------------------- */

void ui_clear(void)
{
	screen_reset();
	ui_draw();
}

void ui_update(const struct monitor_data *data, int32_t battery_percent)
{
	lv_obj_t *scr = screen_reset();
	int32_t section_h = screen_height() / MONITOR_COUNT;

	/* This board has no battery divider, so the charge is not shown. */
	LV_UNUSED(battery_percent);

	for (int m = 0; m < MONITOR_COUNT; m++) {
		/* Inset to the same width as the header rules, so every rule on the panel shares one pair
		 * of margins. It goes on the last row of the section above rather than the first row of
		 * the one below, which leaves every station name the same clearance above it.
		 */
		if (m > 0) {
			add_rule(scr, MARGIN, m * section_h - 1, screen_width() - 2 * MARGIN);
		}

		add_section(scr, m * section_h, section_h, &data->departures[m]);
	}

	ui_draw();
}

/* True once a step has reported back with an error. */
static bool step_failed(int err)
{
	return err != UI_STEP_PENDING && err != 0;
}

/* Nothing is drawn for the steps while they run. Each one would cost a full panel refresh, which
 * on e-ink takes long enough that the step has moved on before it can be read, and with the wifi
 * kept up they mostly finish at once. The panel holds the last board until the next one is ready.
 * Only a failed step is drawn, replacing the board so that no stale departures stay up.
 */
void ui_progress(int32_t battery_voltage, int err_wifi, int err_update)
{
	const char *step;
	int err;
	char buf[48];

	LV_UNUSED(battery_voltage);

	if (step_failed(err_wifi)) {
		step = "WIFI";
		err = err_wifi;
	} else if (step_failed(err_update)) {
		step = "UPDATE";
		err = err_update;
	} else {
		return;
	}

	if (err == HTTP_CERT_INVALID) {
		lv_snprintf(buf, sizeof(buf), "%s ERROR: certificate", step);
	} else {
		lv_snprintf(buf, sizeof(buf), "%s ERROR: %d", step, err);
	}

	lv_obj_t *scr = screen_reset();

	add_wrapped_label(scr, buf, &lv_font_montserrat_20, 0);

	add_version(scr);
	ui_draw();
}

void ui_configuration(void)
{
	lv_obj_t *scr = screen_reset();

	add_wrapped_label(scr, "CONFIGURATION - press Button to leave!", &lv_font_montserrat_20, 0);

	add_version(scr);
	ui_draw();
}

void ui_battery_low(int32_t battery_voltage)
{
	lv_obj_t *scr = screen_reset();
	char buf[32];

	add_wrapped_label(scr, "BATTERY LOW - please charge!", &lv_font_montserrat_20, -30);

	lv_snprintf(buf, sizeof(buf), "%d mV", (int)battery_voltage);
	add_label(scr, buf, &lv_font_montserrat_14, LV_ALIGN_CENTER, 0, 20);
	ui_draw();
}

void ui_button_pressed(bool timer_expired)
{
	lv_obj_t *scr = screen_reset();
	char text[64];

	if (timer_expired) {
		lv_snprintf(text, sizeof(text), "Stop Holding button.");
	} else {
		lv_snprintf(text, sizeof(text), "Hold button for %d sec to enter configuration.",
					CONFIG_APP_BUTTON_HOLD_SEC);
	}

	add_wrapped_label(scr, text, &lv_font_montserrat_20, 0);

	add_version(scr);
	ui_draw();
}
