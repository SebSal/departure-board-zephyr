#ifndef APP_HW_HW_H_
#define APP_HW_HW_H_

#include <stdbool.h>
#include <stdint.h>

/* Button gestures, as reported by the longpress input driver. */
enum hw_button_event {
	HW_BUTTON_SHORT,        /* Released before the longpress delay. */
	HW_BUTTON_LONG,         /* Still held when the longpress delay ran out. */
	HW_BUTTON_LONG_RELEASE, /* Released after HW_BUTTON_LONG. */
};

/* Called from the input thread for every button gesture. */
typedef void (*hw_button_cb_t)(enum hw_button_event event);

/* Registers the button callback. */
void hw_button_set_callback(hw_button_cb_t cb);

/* Initials the hardware. */
int hw_power_up(void);

/* True while the user is holding the button down. */
bool hw_button_is_pressed(void);

/* Arms the watchdog once for the whole wake, or for one refresh cycle without deep sleep. */
int hw_watchdog_start(int32_t awake_budget_sec);

/* Feeds the watchdog. Only used without deep sleep, once per refresh cycle. */
void hw_watchdog_feed(void);

/* Stops the watchdog. */
void hw_watchdog_stop(void);

/* True if the board has a battery that can be measured. */
bool hw_has_battery(void);

/* Reads the battery through the divider and scales the result back up to the pack voltage. */
int hw_battery_read_mv(int32_t *val_ret);

/* Estimates the battery percentage from the voltage. */
int32_t hw_battery_percent_from_mv(int32_t mv);

/* True if the display and its backlight PWM are both ready. */
bool hw_display_is_ready(void);

/* Sets the backlight PWM to brightness percent. */
int hw_display_set_brightness(int32_t brightness);

/* Takes the panel out of blanking. */
void hw_display_blanking_off(void);

/* Brings the device into deep sleep. */
void hw_enter_deep_sleep(void);

/* Prints the error and enters deep sleep in error case. */
void hw_device_failure(const char *what);

#endif /* APP_HW_HW_H_ */
