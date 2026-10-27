#ifndef APP_UI_UI_H_
#define APP_UI_UI_H_

#include <stdbool.h>
#include <stdint.h>

#include <departure_types.h>

/* Passed for a step that has no result yet (running or not started). */
#define UI_STEP_PENDING INT32_MIN

/* Blanks the active screen. */
void ui_clear(void);

/* Draws the departure board, filling the screen with the two monitors' data and a battery icon. */
void ui_update(const struct monitor_data *data, int32_t battery_percent);

/* Draws the boot progress screen. */
void ui_progress(int32_t battery_voltage, int err_wifi, int err_update);

/* Draws the "in configuration mode" notice. */
void ui_configuration(void);

/* Draws the low-battery warning. */
void ui_battery_low(int32_t battery_voltage);

/* Draws the button-hold prompt. */
void ui_button_pressed(bool timer_expired);

#endif /* APP_UI_UI_H_ */
