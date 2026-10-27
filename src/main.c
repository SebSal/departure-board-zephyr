#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/smf.h>
#include <zephyr/sys/util.h>

#include "departure_types.h"
#include <hw/hw.h>
#include <net/ntp.h>
#include <net/wifi.h>
#include <config/config.h>
#include <transport/transport.h>
#include <ui/ui.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* ---------------------------------------- App globals ----------------------------------------- */

/* Longest the network part of a wake can take. */
#define FETCH_MAX_SEC                                                                              \
	DIV_ROUND_UP(WIFI_CONNECT_TIMEOUT_MS + NTP_GET_TIME_MAX_MS + TRANSPORT_UPDATE_MAX_MS,          \
				 MSEC_PER_SEC)

/* Departures of the current wake, filled by the app state. */
static struct monitor_data monitor_data;

/* ---------------------------------------- State machine context ------------------------------- */

/* Forward declaration of state table */
static const struct smf_state monitor_states[];

/* List of monitor states */
enum monitor_state {
	STATE_INIT,
	STATE_HOLD,
	STATE_CONFIG,
	STATE_APP,
	STATE_LOW_BATTERY,
	STATE_SLEEP
};

struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Events */
	struct k_event smf_event;
	int32_t events;

	/* Other state specific data add here */
	struct device_config config;
	int32_t battery_voltage;
	bool config_requested;
	bool hold_expired;
};

static struct s_object s_obj;

/* ---------------------------------------- Events ---------------------------------------------- */

/* Events that drive the state machine. */
#define EVENT_BTN_PRESS_SHORT  BIT(0)
#define EVENT_BTN_PRESS_LONG   BIT(1)
#define EVENT_BTN_RELEASE_LONG BIT(2)
#define EVENT_TIMER_EXPIRED    BIT(3)

#define EVENT_ALL                                                                                  \
	(EVENT_BTN_PRESS_SHORT | EVENT_BTN_PRESS_LONG | EVENT_BTN_RELEASE_LONG | EVENT_TIMER_EXPIRED)

/* Timeout of the current state. */
static void state_timer_expiry(struct k_timer *timer)
{
	k_event_post(&s_obj.smf_event, EVENT_TIMER_EXPIRED);
}

static K_TIMER_DEFINE(state_timer, state_timer_expiry, NULL);

static void button_event(enum hw_button_event event)
{
	switch (event) {
	case HW_BUTTON_SHORT:
		k_event_post(&s_obj.smf_event, EVENT_BTN_PRESS_SHORT);
		break;
	case HW_BUTTON_LONG:
		k_event_post(&s_obj.smf_event, EVENT_BTN_PRESS_LONG);
		break;
	case HW_BUTTON_LONG_RELEASE:
		k_event_post(&s_obj.smf_event, EVENT_BTN_RELEASE_LONG);
		break;
	}
}

/* ---------------------------------------- Helpers --------------------------------------------- */

/* Clear the monitor data. */
static void monitor_data_clear(void)
{
	for (int m = 0; m < MONITOR_COUNT; m++) {
		for (int l = 0; l < DEPARTURE_LINE_COUNT; l++) {
			monitor_data.departures[m].lines[l] = DEPARTURE_LINE_EMPTY;
		}
	}
}

/* ---------------------------------------- States ---------------------------------------------- */

/* State STATE_INIT - Config Check */
static void init_entry(void *o)
{
	LOG_INF("STATE INIT - Config Check");

	struct s_object *s = (struct s_object *)o;

	/* Checked before the config. Just got back to sleep as fast as possible.*/
	if (hw_has_battery() && s->battery_voltage < CONFIG_APP_BATTERY_LOW_CUTOFF_MV) {
		LOG_WRN("Battery at %d mV, below the %d mV cutoff. Skipping the wifi cycle.",
				s->battery_voltage, CONFIG_APP_BATTERY_LOW_CUTOFF_MV);
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_LOW_BATTERY]);
		return;
	}

	/* Check if config is valid. */
	int ret = config_validate();

	if (ret || s->config_requested) {
		/* Configuration error or a request from the shell, enter configuration mode. */
		if (ret) {
			LOG_ERR("Configuration faulty!");
		} else {
			LOG_INF("Configuration mode requested for this boot.");
		}
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_CONFIG]);
		return;
	}

	/* The config is known good from here on, so the configured brightness can be applied. */
	hw_display_set_brightness(s->config.device_brightness);

	/* User is holding down the button during startup, go to hold state. */
	if (hw_button_is_pressed()) {
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_HOLD]);
	} else {
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_APP]);
	}
}

/* State STATE_HOLD - Button held at boot. */
static void hold_entry(void *o)
{
	LOG_INF("STATE_HOLD");

	struct s_object *s = (struct s_object *)o;

	s->hold_expired = false;

	/* Button is pressed, start counting. */
	ui_button_pressed(false);
	k_timer_start(&state_timer, K_SECONDS(CONFIG_APP_BUTTON_HOLD_SEC), K_NO_WAIT);
}

static enum smf_state_result hold_run(void *o)
{
	struct s_object *s = (struct s_object *)o;

	if (s->events & EVENT_TIMER_EXPIRED) {
		/* Held for the full duration, enter config mode as soon as button is released. */
		s->hold_expired = true;
		ui_button_pressed(true);
	}

	if (s->events & (EVENT_BTN_PRESS_SHORT | EVENT_BTN_RELEASE_LONG)) {
		if (s->hold_expired) {
			smf_set_state(SMF_CTX(s), &monitor_states[STATE_CONFIG]);
		} else {
			/* Released early, go back to normal operation. */
			k_timer_stop(&state_timer);
			smf_set_state(SMF_CTX(s), &monitor_states[STATE_APP]);
		}
	}
	return SMF_EVENT_HANDLED;
}

/* State STATE_CONFIG - Enable configuration.*/
static void config_entry(void *o)
{
	LOG_INF("STATE CONFIG");
	ui_configuration();

	struct s_object *s = (struct s_object *)o;

	k_timer_start(&state_timer, K_SECONDS(s->config.advanced_config_timeout), K_NO_WAIT);
}

static enum smf_state_result config_run(void *o)
{
	struct s_object *s = (struct s_object *)o;

	/* Change states on Button Press Event. */
	if (s->events & EVENT_BTN_PRESS_SHORT) {
		k_timer_stop(&state_timer);
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_SLEEP]);
	} else if (s->events & EVENT_TIMER_EXPIRED) {
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_SLEEP]);
	}
	return SMF_EVENT_HANDLED;
}

/* State STATE_APP - Normal application operation. */
static void app_entry(void *o)
{
	LOG_INF("STATE_APP");

	struct s_object *s = (struct s_object *)o;

	const char *const labels[MONITOR_COUNT] = {s->config.monitor_a_label,
											   s->config.monitor_b_label};
	const char *const ids[MONITOR_COUNT] = {s->config.monitor_a_id, s->config.monitor_b_id};

	/* Set the labels. */
	monitor_data_clear();
	for (int m = 0; m < MONITOR_COUNT; m++) {
		snprintf(monitor_data.departures[m].label, sizeof(monitor_data.departures[m].label), "%s",
				 labels[m]);
	}

	/* Run status display. */
	ui_progress(s->battery_voltage, UI_STEP_PENDING, UI_STEP_PENDING);

	LOG_INF("Connecting to WiFi.");

	int ret = wifi_connect(s->config.wifi_ssid, s->config.wifi_password);

	if (ret) {
		LOG_ERR("Wifi connection failed: %d", ret);
		/* wifi_connect() has already stopped the driver's retries. */
		ui_progress(s->battery_voltage, ret, UI_STEP_PENDING);
		goto error;
	}

	/* Update display. */
	ui_progress(s->battery_voltage, 0, UI_STEP_PENDING);

	/* Update data. The departure minutes are counted from the current time. */
	int64_t current_time;

	ret = ntp_get_time(&current_time);
	if (ret == 0) {
		ret = transport_update(&monitor_data, s->config.monitor_module, ids,
							   s->config.monitor_base_link, s->config.advanced_tls_verify != 0,
							   current_time);
	}

	/* The fetch is over either way, nothing below this needs the radio. */
	wifi_disconnect();

	if (ret) {
		LOG_ERR("Failed to update monitor data: %d", ret);
		ui_progress(s->battery_voltage, 0, ret);
		goto error;
	}

	/* Update display with the received data. */
	ui_update(&monitor_data, hw_battery_percent_from_mv(s->battery_voltage));

	/* Drop button and timer events posted while loading. */
	k_event_clear(&s->smf_event, EVENT_ALL);

	/* Display the updated data for the specified duration. */
	k_timer_start(&state_timer, K_SECONDS(s->config.device_display_time), K_NO_WAIT);
	return;

error:
	k_event_clear(&s->smf_event, EVENT_ALL);
	k_timer_start(&state_timer, K_SECONDS(CONFIG_APP_ERROR_DISPLAY_TIME_SEC), K_NO_WAIT);
}

static enum smf_state_result app_run(void *o)
{
	struct s_object *s = (struct s_object *)o;

	if (s->events & EVENT_BTN_PRESS_SHORT) {
		k_timer_stop(&state_timer);
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_SLEEP]);
	} else if (s->events & EVENT_TIMER_EXPIRED) {
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_SLEEP]);
	}
	return SMF_EVENT_HANDLED;
}

/* State STATE_LOW_BATTERY - Warn and go back to sleep without ever starting the radio. */
static void low_battery_entry(void *o)
{
	LOG_INF("STATE_LOW_BATTERY");

	struct s_object *s = (struct s_object *)o;

	ui_battery_low(s->battery_voltage);

	k_timer_start(&state_timer, K_SECONDS(CONFIG_APP_ERROR_DISPLAY_TIME_SEC), K_NO_WAIT);
}

static enum smf_state_result low_battery_run(void *o)
{
	struct s_object *s = (struct s_object *)o;

	/* Any button also ends the wake. */
	if (s->events & (EVENT_TIMER_EXPIRED | EVENT_BTN_PRESS_SHORT | EVENT_BTN_PRESS_LONG)) {
		k_timer_stop(&state_timer);
		smf_set_state(SMF_CTX(s), &monitor_states[STATE_SLEEP]);
	}
	return SMF_EVENT_HANDLED;
}

/* State STATE_SLEEP - Deep sleep. */
static void sleep_entry(void *o)
{
	LOG_INF("STATE_SLEEP");

	hw_enter_deep_sleep();
}

static enum smf_state_result sleep_run(void *o)
{
	/* Never reached, sleep_entry powers off the device. */
	return SMF_EVENT_HANDLED;
}

/* State table */
static const struct smf_state monitor_states[] = {
	[STATE_INIT] = SMF_CREATE_STATE(init_entry, NULL, NULL, NULL, NULL),
	[STATE_HOLD] = SMF_CREATE_STATE(hold_entry, hold_run, NULL, NULL, NULL),
	[STATE_CONFIG] = SMF_CREATE_STATE(config_entry, config_run, NULL, NULL, NULL),
	[STATE_APP] = SMF_CREATE_STATE(app_entry, app_run, NULL, NULL, NULL),
	[STATE_LOW_BATTERY] = SMF_CREATE_STATE(low_battery_entry, low_battery_run, NULL, NULL, NULL),
	[STATE_SLEEP] = SMF_CREATE_STATE(sleep_entry, sleep_run, NULL, NULL, NULL),
};

/* ---------------------------------------- Main loop ------------------------------------------- */

int main(void)
{
	/* Initialize the event and register for button gestures before the rails come up, so that
	 * a button press during startup has somewhere to go.
	 */
	k_event_init(&s_obj.smf_event);
	hw_button_set_callback(button_event);

	if (hw_power_up()) {
		hw_device_failure("GPIO port");
	}

	/* Initialize settings subsystem and load configuration. */
	int ret = settings_subsys_init();

	if (ret == 0) {
		ret = settings_load();
	}
	if (ret < 0) {
		LOG_ERR("Failed to load settings: %d", ret);
	}

	s_obj.config_requested = config_check_boot_request();

	s_obj.config = *config_get();

	/* Arm the watchdog once the timeouts are known. Watchdog covers the whole cycle and is not fed.
	 */
	int32_t update_sec =
		FETCH_MAX_SEC + MAX(s_obj.config.device_display_time, CONFIG_APP_ERROR_DISPLAY_TIME_SEC);
	int32_t config_sec = CONFIG_APP_BUTTON_HOLD_SEC + s_obj.config.advanced_config_timeout;

	if (hw_watchdog_start(MAX(update_sec, config_sec))) {
		hw_device_failure("Watchdog");
	}

	/* Read battery voltage.*/
	if (hw_has_battery()) {
		ret = hw_battery_read_mv(&s_obj.battery_voltage);
		if (ret < 0) {
			hw_device_failure("Battery ADC");
		}
		LOG_INF("Battery voltage: %d mV", s_obj.battery_voltage);
	}

	/* Check if devices have been initialized. */
	if (!hw_display_is_ready()) {
		hw_device_failure("Display");
	}

	/* Set backlight brightness. */
	hw_display_set_brightness(CONFIG_APP_DISPLAY_BRIGHTNESS);

	ui_clear();
	hw_display_blanking_off();

	/* Set initial state */
	smf_set_initial(SMF_CTX(&s_obj), &monitor_states[STATE_INIT]);

	/* Run the state machine */
	while (1) {
		/* Block until an event is detected */
		s_obj.events = k_event_wait_safe(&s_obj.smf_event, EVENT_ALL, false, K_FOREVER);

		/* State machine terminates if a non-zero value is returned */
		ret = smf_run_state(SMF_CTX(&s_obj));
		if (ret) {
			/* handle return code and terminate state machine */
			break;
		}
	}

	/* Should never be reached. Still caught by watchdog. */
	return 0;
}
