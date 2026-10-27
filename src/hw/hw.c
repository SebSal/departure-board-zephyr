#include "hw.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

LOG_MODULE_REGISTER(app_hw, CONFIG_APP_HW_LOG_LEVEL);

/* ---------------------------------------- Devices --------------------------------------------- */

/* Optional hardware, present when the board devicetree describes it. */
#define HAS_POWER_RAILS DT_NODE_HAS_PROP(DT_PATH(zephyr_user), disp_en_gpios)
#define HAS_BACKLIGHT   DT_NODE_EXISTS(DT_ALIAS(ledbacklight))
#define HAS_BATTERY     DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static const struct device *const longpress_dev = DEVICE_DT_GET(DT_PATH(longpress));

#if HAS_POWER_RAILS
static const struct gpio_dt_spec disp_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), disp_en_gpios);
#endif

#if HAS_BACKLIGHT
static const struct pwm_dt_spec pwm_backlight = PWM_DT_SPEC_GET(DT_ALIAS(ledbacklight));
#endif

#if HAS_BATTERY
static const struct gpio_dt_spec bat_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), bat_en_gpios);
static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

#define VBATT_HIGH_SIDE_OHMS DT_PROP(DT_PATH(zephyr_user), vbatt_high_side_ohms)
#define VBATT_LOW_SIDE_OHMS  DT_PROP(DT_PATH(zephyr_user), vbatt_low_side_ohms)

static const struct voltage_divider_dt_spec vbatt = {
	.full_ohms = VBATT_HIGH_SIDE_OHMS + VBATT_LOW_SIDE_OHMS,
	.output_ohms = VBATT_LOW_SIDE_OHMS,
};
#endif

/* ---------------------------------------- Button ---------------------------------------------- */

static hw_button_cb_t button_cb;

void hw_button_set_callback(hw_button_cb_t cb)
{
	button_cb = cb;
}

static void button_update(enum hw_button_event event)
{
	if (button_cb) {
		button_cb(event);
	}
}

static void button_input_cb(struct input_event *evt, void *user_data)
{
	LOG_DBG("Button event: code=%d, value=%d", evt->code, evt->value);
	if (evt->value == 0) {
		/* The longpress driver reports only the long code going back to 0 on release. */
		if (evt->code == INPUT_KEY_L) {
			LOG_INF("Long press released.");
			button_update(HW_BUTTON_LONG_RELEASE);
		}
	} else {
		switch (evt->code) {
		case INPUT_KEY_S:
			LOG_INF("Short press detected.");
			button_update(HW_BUTTON_SHORT);
			break;
		case INPUT_KEY_L:
			LOG_INF("Long press detected.");
			button_update(HW_BUTTON_LONG);
			break;
		default:
			break;
		}
	}
}

INPUT_CALLBACK_DEFINE(longpress_dev, button_input_cb, NULL);

bool hw_button_is_pressed(void)
{
	return gpio_pin_get_dt(&btn) > 0;
}

/* ---------------------------------------- Power ----------------------------------------------- */

#if HAS_POWER_RAILS || HAS_BATTERY
/* Enables an output rail, releasing the pad hold from the previous deep sleep first. */
static int rail_enable(const struct gpio_dt_spec *rail)
{
	if (!gpio_is_ready_dt(rail)) {
		return -ENODEV;
	}

	gpio_hold_dis(rail->pin);

	return gpio_pin_configure_dt(rail, GPIO_OUTPUT_ACTIVE);
}

/* Switches an output rail off and holds it there through deep sleep. */
static void rail_disable(const struct gpio_dt_spec *rail)
{
	gpio_pin_set_dt(rail, 0);
	gpio_hold_en(rail->pin);
}
#endif

int hw_power_up(void)
{
	if (!gpio_is_ready_dt(&btn)) {
		return -ENODEV;
	}

	int ret = 0;

	/* Enable display, button and battery voltage measurement. */
#if HAS_POWER_RAILS
	ret = rail_enable(&disp_en);
#endif
#if HAS_BATTERY
	if (ret == 0) {
		ret = rail_enable(&bat_en);
	}
#endif
	if (ret == 0) {
		ret = gpio_pin_configure_dt(&btn, GPIO_INPUT);
	}
	if (ret < 0) {
		LOG_ERR("Failed to configure power GPIOs: %d", ret);
		return ret;
	}

	return 0;
}

/* Switch off the display and battery measurement divider to save power. */
static void power_down_peripherals(void)
{
#if HAS_POWER_RAILS
	rail_disable(&disp_en);
#endif
#if HAS_BATTERY
	rail_disable(&bat_en);
#endif
}

void hw_enter_deep_sleep(void)
{
	if (gpio_is_ready_dt(&btn)) {
		gpio_pin_configure_dt(&btn, GPIO_INPUT);
		esp_sleep_enable_ext1_wakeup(BIT(btn.pin), ESP_EXT1_WAKEUP_ANY_LOW);
	} else {
		LOG_ERR("GPIO device not ready, need hw reset to wake up.");
	}

	LOG_INF("Entering deep sleep.");
	hw_watchdog_stop();
	power_down_peripherals();
	sys_poweroff();
}

void hw_device_failure(const char *device)
{
	LOG_ERR("%s not ready, going to sleep.", device);
	hw_enter_deep_sleep();
}

/* ---------------------------------------- Watchdog -------------------------------------------- */

static void watchdog_expired(const struct device *dev, int channel_id)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel_id);

	sys_reboot(SYS_REBOOT_COLD);
}

int hw_watchdog_start(int32_t awake_time)
{
	int ret = 0;

	if (!device_is_ready(wdt)) {
		LOG_ERR("Watchdog device not ready");
		return -ENODEV;
	}

	int32_t timeout_sec = MAX(awake_time + CONFIG_APP_WATCHDOG_TIMEOUT_MARGIN_SEC,
							  CONFIG_APP_WATCHDOG_TIMEOUT_MIN_SEC);

	const struct wdt_timeout_cfg wdt_config = {
		.window.min = 0U,
		.window.max = timeout_sec * MSEC_PER_SEC,
		.callback = watchdog_expired,
		.flags = WDT_FLAG_RESET_SOC,
	};

	ret = wdt_install_timeout(wdt, &wdt_config);
	if (ret < 0) {
		LOG_ERR("Failed to install watchdog timeout: %d", ret);
		return ret;
	}

	/* wdt_setup() enables the watchdog and feeds it once, no further feeding happens. */
	ret = wdt_setup(wdt, 0);
	if (ret < 0) {
		LOG_ERR("Failed to setup watchdog: %d", ret);
		return ret;
	}

	LOG_INF("Watchdog armed with %d s.", timeout_sec);
	return 0;
}

void hw_watchdog_stop(void)
{
	if (device_is_ready(wdt)) {
		wdt_disable(wdt);
	}
}

/* ---------------------------------------- Battery --------------------------------------------- */

int hw_battery_read_mv(int32_t *val_ret)
{
#if !HAS_BATTERY
	ARG_UNUSED(val_ret);

	return -ENOTSUP;
#else
	int ret = 0;

	if (!adc_is_ready_dt(&adc_channel)) {
		LOG_ERR("ADC controller device %s not ready", adc_channel.dev->name);
		return -ENODEV;
	}

	ret = adc_channel_setup_dt(&adc_channel);
	if (ret < 0) {
		LOG_ERR("Could not setup channel #%d (%d)", 0, ret);
		return ret;
	}

	int16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
	};

	ret = adc_sequence_init_dt(&adc_channel, &sequence);
	if (ret < 0) {
		LOG_ERR("Could not initialize sequence");
		return ret;
	}

	ret = adc_read(adc_channel.dev, &sequence);
	if (ret < 0) {
		LOG_ERR("Could not read (%d)", ret);
		return ret;
	}

	int32_t val_mv = buf;

	ret = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
	if (ret < 0) {
		LOG_WRN("Could not convert to mV (%d)", ret);
		return ret;
	}

	LOG_DBG("Measured voltage before scaling %d mV", val_mv);

	/* Scale back up through the voltage divider to the pack voltage. */
	ret = voltage_divider_scale_dt(&vbatt, &val_mv);
	if (ret < 0) {
		LOG_WRN("Could not scale through the voltage divider (%d)", ret);
		return ret;
	}

	*val_ret = val_mv;

	return 0;
#endif
}

/* Curve from google, not sure if correct. Has to be tested. */
int32_t hw_battery_percent_from_mv(int32_t mv)
{
	static const struct {
		int32_t mv;
		int32_t percent;
	} curve[] = {
		{4200, 100}, {4110, 90}, {4020, 80}, {3950, 70}, {3870, 60}, {3840, 50},
		{3800, 40},  {3770, 30}, {3730, 20}, {3690, 10}, {3610, 5},  {3270, 0},
	};

	if (mv >= curve[0].mv) {
		return 100;
	}

	for (size_t i = 1; i < ARRAY_SIZE(curve); i++) {
		if (mv >= curve[i].mv) {
			int32_t span_mv = curve[i - 1].mv - curve[i].mv;
			int32_t span_percent = curve[i - 1].percent - curve[i].percent;

			return curve[i].percent + (mv - curve[i].mv) * span_percent / span_mv;
		}
	}

	return 0;
}

/* ---------------------------------------- Display --------------------------------------------- */

bool hw_display_is_ready(void)
{
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return false;
	}

#if HAS_BACKLIGHT
	if (!pwm_is_ready_dt(&pwm_backlight)) {
		LOG_ERR("Display backlight PWM not ready");
		return false;
	}
#endif

	return true;
}

int hw_display_set_brightness(int32_t brightness)
{
#if !HAS_BACKLIGHT
	ARG_UNUSED(brightness);

	return -ENOTSUP;
#else
	int ret = 0;

	/* Display is very bright, divide by 200 instead of 100. */
	ret = pwm_set_pulse_dt(&pwm_backlight, pwm_backlight.period * brightness / 200);
	if (ret < 0) {
		LOG_ERR("Failed to set display brightness: %d", ret);
		return ret;
	}
	return 0;
#endif
}

void hw_display_blanking_off(void)
{
	display_blanking_off(display_dev);
}
