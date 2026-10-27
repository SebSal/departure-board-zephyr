#include "ui.h"

#include <string.h>

#include <lvgl.h>

#include <net/http_client.h>

/* Supplied by the build as the output of "git describe". */
#ifndef APP_GIT_VERSION
#define APP_GIT_VERSION "unknown"
#endif

/* ---------------------------------------- Layout ---------------------------------------------- */

#define LINES_PER_SCREEN 6

#define LINE_OFFSET   0
#define DEST_OFFSET   40
#define TIME_OFFSET   -30
#define MIN_OFFSET    0
#define LINE_BTN_SIZE 30

#define BATTERY_BODY_W 26
#define BATTERY_BODY_H 13
#define BATTERY_BARS   5
#define BATTERY_OFFSET 0

#define VERSION_OFFSET 2

/* ---------------------------------------- Styles ---------------------------------------------- */

static lv_style_t style_white;
static lv_style_t style_gray;
static lv_style_t style_darkblue;

static void style_init_flat(lv_style_t *style, lv_color_t color)
{
	lv_style_init(style);
	lv_style_set_bg_color(style, color);
	lv_style_set_bg_opa(style, LV_OPA_COVER);
	lv_style_set_border_width(style, 0);
	lv_style_set_radius(style, 0);
}

static void ui_styles_init(void)
{
	static bool init;

	if (init) {
		return;
	}

	style_init_flat(&style_white, lv_color_white());
	style_init_flat(&style_gray, lv_color_make(200, 200, 200));
	style_init_flat(&style_darkblue, lv_color_make(40, 70, 110));

	init = true;
}

/* ---------------------------------------- Screen ---------------------------------------------- */

static int32_t screen_width(void)
{
	return lv_display_get_horizontal_resolution(lv_display_get_default());
}

static int32_t screen_height(void)
{
	return lv_display_get_vertical_resolution(lv_display_get_default());
}

static void ui_draw(void)
{
	lv_refr_now(NULL);
}

static lv_obj_t *screen_reset(void)
{
	lv_obj_t *scr = lv_scr_act();

	lv_obj_clean(lv_layer_top());
	lv_obj_clean(scr);
	lv_obj_remove_style_all(scr);
	lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

	return scr;
}

/* ---------------------------------------- Widgets --------------------------------------------- */

static lv_obj_t *add_section(lv_obj_t *parent, int32_t scr_width, int32_t section_h,
							 int32_t start_line, lv_style_t *style)
{
	/* Background section */
	lv_obj_t *section = lv_obj_create(parent);

	lv_obj_set_size(section, scr_width, section_h);
	lv_obj_set_pos(section, 0, start_line);
	lv_obj_add_style(section, style, 0);
	lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

	return section;
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, lv_color_t color,
						   const lv_font_t *font, lv_align_t align, int32_t x_ofs, int32_t y_ofs)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, color, 0);
	lv_obj_set_style_text_font(label, font, 0);

	lv_obj_align(label, align, x_ofs, y_ofs);

	return label;
}

/* Centred message that wraps at three quarters of the screen. The width has to be set
 * before the align, so that the label is placed at its final size.
 */
static lv_obj_t *add_wrapped_label(lv_obj_t *parent, const char *text, lv_color_t color,
								   const lv_font_t *font, int32_t y_ofs)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, color, 0);
	lv_obj_set_style_text_font(label, font, 0);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(label, screen_width() * 3 / 4);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, y_ofs);

	return label;
}

static void add_btn_to_section(lv_obj_t *section, int32_t btn_size, const char *text,
							   lv_color_t btn_color, lv_color_t txt_color, lv_align_t align,
							   int32_t x_ofs)
{
	lv_obj_t *btn = lv_btn_create(section);

	lv_obj_set_width(btn, btn_size);
	lv_obj_set_height(btn, btn_size);
	lv_obj_set_style_bg_color(btn, btn_color, 0);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(btn, 0, 0);
	lv_obj_set_style_radius(btn, 5, 0);

	/* Button decides its own font size based on the text length. */
	if (strlen(text) <= 2) {
		lv_obj_set_style_text_font(btn, &lv_font_montserrat_20, 0);
	} else if (strlen(text) == 3) {
		lv_obj_set_style_text_font(btn, &lv_font_montserrat_14, 0); /* code */
	} else {
		lv_obj_set_style_text_font(btn, &lv_font_montserrat_10, 0);
	}
	lv_obj_t *label = lv_label_create(btn);

	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, txt_color, 0);
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	lv_obj_align(btn, align, x_ofs, 0);
}

/* Battery icon: an outlined body with a terminal nub, filled in five 20% steps. */
static void add_battery(lv_obj_t *parent, int32_t percent, lv_align_t align, int32_t x_ofs,
						int32_t y_ofs)
{
	const int32_t body_w = BATTERY_BODY_W;
	const int32_t body_h = BATTERY_BODY_H;

	if (percent < 0) {
		percent = 0;
	} else if (percent > 100) {
		percent = 100;
	}

	int32_t bars = percent / 20 + 1;

	if (bars > BATTERY_BARS) {
		bars = BATTERY_BARS;
	}

	lv_obj_t *body = lv_obj_create(parent);

	lv_obj_set_size(body, body_w, body_h);
	lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_color(body, lv_color_white(), 0);
	lv_obj_set_style_border_width(body, 1, 0);
	lv_obj_set_style_radius(body, 2, 0);
	lv_obj_set_style_pad_all(body, 1, 0);
	lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(body, align, x_ofs, y_ofs);

	/* Terminal nub, hung off the right hand edge of the body. */
	lv_obj_t *nub = lv_obj_create(parent);

	lv_obj_set_size(nub, 2, body_h / 2);
	lv_obj_set_style_bg_color(nub, lv_color_white(), 0);
	lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(nub, 0, 0);
	lv_obj_set_style_radius(nub, 0, 0);
	lv_obj_set_style_pad_all(nub, 0, 0);
	lv_obj_clear_flag(nub, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align_to(nub, body, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

	lv_obj_t *fill = lv_obj_create(body);
	int32_t inner_w = body_w - 2 * (1 + 1);

	lv_obj_set_size(fill, inner_w * bars / BATTERY_BARS, body_h - 2 * (1 + 1));
	lv_obj_set_style_bg_color(fill, percent < 20 ? lv_color_hex(0xE03030) : lv_color_hex(0x30C030),
							  0);
	lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(fill, 0, 0);
	lv_obj_set_style_radius(fill, 0, 0);
	lv_obj_set_style_pad_all(fill, 0, 0);
	lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
}

/* Separator between two sections. A plain 1 px high object rather than an lv_line, which would
 * keep a pointer to a point array that has to outlive it.
 */
static void add_line(int32_t width, int32_t pos_y)
{
	lv_obj_t *line = lv_obj_create(lv_layer_top());

	lv_obj_set_size(line, width, 1);
	lv_obj_set_style_bg_color(line, lv_color_hex(0x505050), 0); /* Light gray line color. */
	lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(line, 0, 0);
	lv_obj_set_style_radius(line, 0, 0);
	lv_obj_set_style_pad_all(line, 0, 0);
	lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_pos(line, 0, pos_y);
}

/* Build version, tucked into the bottom right corner of the status screens. */
static void add_version(lv_obj_t *parent)
{
	add_label(parent, APP_GIT_VERSION, lv_color_white(), &lv_font_montserrat_10,
			  LV_ALIGN_BOTTOM_RIGHT, -VERSION_OFFSET, -VERSION_OFFSET);
}

/* ---------------------------------------- Departure screen ------------------------------------ */

/* Station name on the left. */
static lv_obj_t *add_headline(lv_obj_t *scr, int32_t width, int32_t height, int32_t y,
							  const char *label)
{
	lv_obj_t *section = add_section(scr, width, height, y, &style_darkblue);

	add_label(section, label, lv_color_white(), &lv_font_montserrat_12, LV_ALIGN_LEFT_MID, 0, 0);

	return section;
}

/* One departure: line badge, destination, and the minutes until it leaves. */
static void add_departure_row(lv_obj_t *scr, int32_t width, int32_t height, int32_t y,
							  const struct departure_line *line, lv_style_t *style)
{
	lv_obj_t *section = add_section(scr, width, height, y, style);

	if (line->departure_minutes == DEPARTURE_NO_DATA) {
		add_label(section, "NO DATA", lv_color_black(), &lv_font_montserrat_20, LV_ALIGN_LEFT_MID,
				  DEST_OFFSET, 0);
		return;
	}

	add_btn_to_section(section, LINE_BTN_SIZE, line->line_number, lv_color_hex(line->line_color),
					   lv_color_hex(line->line_text_color), LV_ALIGN_LEFT_MID, LINE_OFFSET);
	add_label(section, line->destination, lv_color_black(), &lv_font_montserrat_20,
			  LV_ALIGN_LEFT_MID, DEST_OFFSET, 0);

	char minutes[6];

	lv_snprintf(minutes, sizeof(minutes), "%d", line->departure_minutes);
	add_label(section, minutes, lv_color_black(), &lv_font_montserrat_20, LV_ALIGN_RIGHT_MID,
			  TIME_OFFSET, 0);
	add_label(section, "Min", lv_color_black(), &lv_font_montserrat_12, LV_ALIGN_RIGHT_MID,
			  MIN_OFFSET, 0);
}

/* Draws a line under the current section. */
static int32_t next_line(int32_t width, int32_t y, int32_t height)
{
	y += height;
	add_line(width, y);

	return y;
}

static void departure_screen_fill(const struct monitor_data *data, int32_t battery_percent)
{
	lv_obj_t *scr = screen_reset();
	int32_t width = screen_width();
	int32_t section_h = screen_height() / LINES_PER_SCREEN;
	int32_t half_h = section_h / 2;
	int32_t y = 0;

	lv_style_t *const row_style[DEPARTURE_LINE_COUNT] = {&style_white, &style_gray};

	for (int m = 0; m < MONITOR_COUNT; m++) {
		const struct departure_data *dep = &data->departures[m];
		lv_obj_t *head = add_headline(scr, width, half_h, y, dep->label);

		/* Headline A carries the battery icon, headline B the time of the last update. */
		if (m == 0) {
			add_battery(head, battery_percent, LV_ALIGN_RIGHT_MID, BATTERY_OFFSET, 0);
		} else {
			add_label(head, dep->updated, lv_color_white(), &lv_font_montserrat_12,
					  LV_ALIGN_RIGHT_MID, 0, 0);
		}
		y = next_line(width, y, half_h);

		for (int l = 0; l < DEPARTURE_LINE_COUNT; l++) {
			add_departure_row(scr, width, section_h, y, &dep->lines[l], row_style[l]);
			y = next_line(width, y, section_h);
		}

		add_section(scr, width, half_h, y, &style_white); /* padding */
		y = next_line(width, y, half_h);
	}
}

/* ---------------------------------------- Status screens -------------------------------------- */

/* "<step> ...", plus the outcome once the step has reported back. */
static void format_step(char *buf, size_t size, const char *step, int err)
{
	if (err == UI_STEP_PENDING) {
		lv_snprintf(buf, size, "%s ...", step);
	} else if (err == 0) {
		lv_snprintf(buf, size, "%s ... OK", step);
	} else if (err == HTTP_CERT_INVALID) {
		lv_snprintf(buf, size, "%s ... ERROR: certificate", step);
	} else {
		lv_snprintf(buf, size, "%s ... ERROR: %d", step, err);
	}
}

void ui_progress(int32_t battery_voltage, int err_wifi, int err_update)
{
	lv_obj_t *scr = screen_reset();
	int32_t x_ofs = screen_width() / 5;
	char buf[48];

	lv_snprintf(buf, sizeof(buf), "Battery Voltage: %d mV", (int)battery_voltage);
	add_label(scr, buf, lv_color_white(), &lv_font_montserrat_14, LV_ALIGN_LEFT_MID, x_ofs, -21);

	format_step(buf, sizeof(buf), "Connecting to Wifi", err_wifi);
	add_label(scr, buf, lv_color_white(), &lv_font_montserrat_14, LV_ALIGN_LEFT_MID, x_ofs, 0);

	if (err_wifi == 0) { /* Only display if the Wi-Fi step is done. */
		format_step(buf, sizeof(buf), "Getting Data", err_update);
	} else {
		buf[0] = '\0';
	}
	add_label(scr, buf, lv_color_white(), &lv_font_montserrat_14, LV_ALIGN_LEFT_MID, x_ofs, 21);

	add_version(scr);
	ui_draw();
}

void ui_configuration(void)
{
	lv_obj_t *scr = screen_reset();

	add_wrapped_label(scr, "CONFIGURATION - press Button to leave!", lv_color_make(255, 0, 0),
					  &lv_font_montserrat_14, 0);

	add_version(scr);
	ui_draw();
}

void ui_battery_low(int32_t battery_voltage)
{
	lv_obj_t *scr = screen_reset();
	char buf[32];

	add_wrapped_label(scr, "BATTERY LOW - please charge!", lv_color_make(255, 0, 0),
					  &lv_font_montserrat_20, -20);

	lv_snprintf(buf, sizeof(buf), "%d mV", (int)battery_voltage);
	add_label(scr, buf, lv_color_white(), &lv_font_montserrat_14, LV_ALIGN_CENTER, 0, 20);

	add_battery(scr, 0, LV_ALIGN_CENTER, -14, 60);
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

	add_wrapped_label(scr, text, lv_color_make(255, 0, 0), &lv_font_montserrat_12, 0);

	add_version(scr);
	ui_draw();
}

/* ---------------------------------------- Entry points ---------------------------------------- */

void ui_clear(void)
{
	screen_reset();
	ui_draw();
}

void ui_update(const struct monitor_data *data, int32_t battery_percent)
{
	ui_styles_init();
	departure_screen_fill(data, battery_percent);
	ui_draw();
}
