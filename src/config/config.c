#include "config.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(app_config, CONFIG_APP_CONFIG_LOG_LEVEL);

/* ---------------------------------------- Settings state -------------------------------------- */

static struct device_config config = {
	.device_brightness = CONFIG_APP_DISPLAY_BRIGHTNESS,
	.device_display_time = CONFIG_APP_DISPLAY_TIME_SEC,
	.device_refresh_time = CONFIG_APP_REFRESH_TIME_SEC,
	.advanced_config_timeout = CONFIG_APP_CONFIG_TIMEOUT_DEFAULT_SEC,
	.advanced_tls_verify = 1,
};

/* Enforce bounds. */
BUILD_ASSERT(CONFIG_APP_DISPLAY_BRIGHTNESS >= CFG_BRIGHTNESS_MIN &&
				 CONFIG_APP_DISPLAY_BRIGHTNESS <= CFG_BRIGHTNESS_MAX,
			 "APP_DISPLAY_BRIGHTNESS is outside CFG_BRIGHTNESS_MIN..MAX");
BUILD_ASSERT(CONFIG_APP_DISPLAY_TIME_SEC >= CFG_DISPLAY_TIME_MIN &&
				 CONFIG_APP_DISPLAY_TIME_SEC <= CFG_DISPLAY_TIME_MAX,
			 "APP_DISPLAY_TIME_SEC is outside CFG_DISPLAY_TIME_MIN..MAX");
BUILD_ASSERT(CONFIG_APP_REFRESH_TIME_SEC >= CFG_REFRESH_TIME_MIN &&
				 CONFIG_APP_REFRESH_TIME_SEC <= CFG_REFRESH_TIME_MAX,
			 "APP_REFRESH_TIME_SEC is outside CFG_REFRESH_TIME_MIN..MAX");
BUILD_ASSERT(CONFIG_APP_CONFIG_TIMEOUT_DEFAULT_SEC >= CFG_CONFIG_TIMEOUT_MIN &&
				 CONFIG_APP_CONFIG_TIMEOUT_DEFAULT_SEC <= CFG_CONFIG_TIMEOUT_MAX,
			 "APP_CONFIG_TIMEOUT_DEFAULT_SEC is outside CFG_CONFIG_TIMEOUT_MIN..MAX");

static bool wifi_ssid_set;
static bool wifi_password_set;

static bool monitor_a_label_set;
static bool monitor_a_id_set;
static bool monitor_b_label_set;
static bool monitor_b_id_set;
static bool monitor_module_set;
static bool monitor_base_link_set;

static bool device_brightness_set;
static bool device_display_time_set;
static bool device_refresh_time_set;

static bool advanced_config_timeout_set;
static bool advanced_enter_config_set;
static bool advanced_tls_verify_invalid;

/* ---------------------------------------- Settings handlers ----------------------------------- */

/* Reads a string setting into buf. A stored value that does not fit, NUL included, is rejected. */
static int read_string(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg,
					   char *buf, size_t size, bool *set)
{
	if (len > size) {
		LOG_ERR("%s is %u bytes, at most %u fit", name, (unsigned int)len, (unsigned int)size);
		return -EINVAL;
	}

	int rc = read_cb(cb_arg, buf, size);

	if (rc < 0) {
		LOG_ERR("Failed to read %s: %d", name, rc);
		memset(buf, 0, size);
		return rc;
	}

	/* Check that string is NUL-terminated. */
	if (memchr(buf, '\0', rc) == NULL) {
		LOG_ERR("%s is not NUL-terminated", name);
		memset(buf, 0, size);
		return -EINVAL;
	}

	*set = true;
	return 0;
}

/* Reads an int32 setting. Anything not exactly four bytes wide is rejected, and *val keeps its
 * default.
 */
static int read_int32(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg,
					  int32_t *val, bool *set)
{
	if (len != sizeof(*val)) {
		LOG_ERR("%s is %u bytes, expected %u", name, (unsigned int)len, (unsigned int)sizeof(*val));
		return -EINVAL;
	}

	int32_t tmp;
	int rc = read_cb(cb_arg, &tmp, sizeof(tmp));

	if (rc != sizeof(tmp)) {
		LOG_ERR("Failed to read %s: %d", name, rc);
		return rc < 0 ? rc : -EINVAL;
	}

	*val = tmp;
	*set = true;
	return 0;
}

static int wifi_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int rc;

	if (strcmp(name, CFG_NAME_WIFI_SSID) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.wifi_ssid, sizeof(config.wifi_ssid),
						 &wifi_ssid_set);
		LOG_DBG("Read %s with value %s", name, config.wifi_ssid);
		return rc;
	}
	if (strcmp(name, CFG_NAME_WIFI_PASSWORD) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.wifi_password,
						 sizeof(config.wifi_password), &wifi_password_set);
		/* Length only */
		LOG_DBG("Read %s (%u characters)", name, (unsigned int)strlen(config.wifi_password));
		return rc;
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(wifi, CFG_TREE_WIFI, NULL, wifi_settings_set, NULL, NULL);

static int monitor_settings_set(const char *name, size_t len, settings_read_cb read_cb,
								void *cb_arg)
{
	int rc;

	if (strcmp(name, CFG_NAME_MONITOR_A_LABEL) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.monitor_a_label,
						 sizeof(config.monitor_a_label), &monitor_a_label_set);
		LOG_DBG("Read %s with value %s", name, config.monitor_a_label);
		return rc;
	}
	if (strcmp(name, CFG_NAME_MONITOR_A_ID) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.monitor_a_id,
						 sizeof(config.monitor_a_id), &monitor_a_id_set);
		LOG_DBG("Read %s with value %s", name, config.monitor_a_id);
		return rc;
	}
	if (strcmp(name, CFG_NAME_MONITOR_B_LABEL) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.monitor_b_label,
						 sizeof(config.monitor_b_label), &monitor_b_label_set);
		LOG_DBG("Read %s with value %s", name, config.monitor_b_label);
		return rc;
	}
	if (strcmp(name, CFG_NAME_MONITOR_B_ID) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.monitor_b_id,
						 sizeof(config.monitor_b_id), &monitor_b_id_set);
		LOG_DBG("Read %s with value %s", name, config.monitor_b_id);
		return rc;
	}
	if (strcmp(name, CFG_NAME_MONITOR_MODULE) == 0) {
		rc = read_int32(name, len, read_cb, cb_arg, &config.monitor_module, &monitor_module_set);
		LOG_DBG("Read %s with value %d", name, config.monitor_module);
		return rc;
	}
	if (strcmp(name, CFG_NAME_MONITOR_BASE_LINK) == 0) {
		rc = read_string(name, len, read_cb, cb_arg, config.monitor_base_link,
						 sizeof(config.monitor_base_link), &monitor_base_link_set);
		LOG_DBG("Read %s with value %s", name, config.monitor_base_link);
		return rc;
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(monitor, CFG_TREE_MONITOR, NULL, monitor_settings_set, NULL, NULL);

static int device_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int rc;

	if (strcmp(name, CFG_NAME_DEVICE_BRIGHTNESS) == 0) {
		rc = read_int32(name, len, read_cb, cb_arg, &config.device_brightness,
						&device_brightness_set);
		LOG_DBG("Read %s with value %d", name, config.device_brightness);
		return rc;
	}
	if (strcmp(name, CFG_NAME_DEVICE_DISPLAY_TIME) == 0) {
		rc = read_int32(name, len, read_cb, cb_arg, &config.device_display_time,
						&device_display_time_set);
		LOG_DBG("Read %s with value %d", name, config.device_display_time);
		return rc;
	}
	if (strcmp(name, CFG_NAME_DEVICE_REFRESH_TIME) == 0) {
		rc = read_int32(name, len, read_cb, cb_arg, &config.device_refresh_time,
						&device_refresh_time_set);
		LOG_DBG("Read %s with value %d", name, config.device_refresh_time);
		return rc;
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(device, CFG_TREE_DEVICE, NULL, device_settings_set, NULL, NULL);

static int advanced_settings_set(const char *name, size_t len, settings_read_cb read_cb,
								 void *cb_arg)
{
	int rc;

	if (strcmp(name, CFG_NAME_ADVANCED_CONFIG_TIMEOUT) == 0) {
		rc = read_int32(name, len, read_cb, cb_arg, &config.advanced_config_timeout,
						&advanced_config_timeout_set);
		LOG_DBG("Read %s with value %d", name, config.advanced_config_timeout);
		return rc;
	}
	if (strcmp(name, CFG_NAME_ADVANCED_ENTER_CONFIG) == 0) {
		bool valid = false;

		rc = read_int32(name, len, read_cb, cb_arg, &config.advanced_enter_config, &valid);
		/* Flags that the key exists, so config_check_boot_request deletes it - a malformed
		 * one included, which would otherwise be reported again on every boot.
		 */
		advanced_enter_config_set = true;
		LOG_DBG("Read %s with value %d", name, config.advanced_enter_config);
		return rc;
	}
	if (strcmp(name, CFG_NAME_ADVANCED_TLS_VERIFY) == 0) {
		bool valid = false;

		rc = read_int32(name, len, read_cb, cb_arg, &config.advanced_tls_verify, &valid);
		/* A broken value must not quietly fall back to the default, config_validate refuses it. */
		advanced_tls_verify_invalid =
			!valid || (config.advanced_tls_verify != 0 && config.advanced_tls_verify != 1);
		LOG_DBG("Read %s with value %d", name, config.advanced_tls_verify);
		return rc;
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(advanced, CFG_TREE_ADVANCED, NULL, advanced_settings_set, NULL,
							   NULL);

/* ---------------------------------------- Setters --------------------------------------------- */

static int save_string(const char *key, const char *value, char *field, size_t size)
{
	size_t len = strlen(value);

	if (len >= size) {
		LOG_ERR("%s is %u bytes, at most %u fit", key, (unsigned int)len, (unsigned int)(size - 1));
		return -EINVAL;
	}

	int rc = settings_save_one(key, value, len + 1);

	if (rc < 0) {
		LOG_ERR("Failed to save %s: %d", key, rc);
		return rc;
	}

	memcpy(field, value, len + 1);
	return 0;
}

static int save_int32(const char *key, int32_t value, int32_t *field)
{
	int rc = settings_save_one(key, &value, sizeof(value));

	if (rc < 0) {
		LOG_ERR("Failed to save %s: %d", key, rc);
		return rc;
	}

	*field = value;
	return 0;
}

int config_set_wifi_ssid(const char *ssid)
{
	return save_string(CFG_KEY_WIFI_SSID, ssid, config.wifi_ssid, sizeof(config.wifi_ssid));
}

int config_set_wifi_password(const char *password)
{
	return save_string(CFG_KEY_WIFI_PASSWORD, password, config.wifi_password,
					   sizeof(config.wifi_password));
}

int config_set_monitor_a_label(const char *label)
{
	return save_string(CFG_KEY_MONITOR_A_LABEL, label, config.monitor_a_label,
					   sizeof(config.monitor_a_label));
}

int config_set_monitor_a_id(const char *id)
{
	return save_string(CFG_KEY_MONITOR_A_ID, id, config.monitor_a_id, sizeof(config.monitor_a_id));
}

int config_set_monitor_b_label(const char *label)
{
	return save_string(CFG_KEY_MONITOR_B_LABEL, label, config.monitor_b_label,
					   sizeof(config.monitor_b_label));
}

int config_set_monitor_b_id(const char *id)
{
	return save_string(CFG_KEY_MONITOR_B_ID, id, config.monitor_b_id, sizeof(config.monitor_b_id));
}

int config_set_monitor_module(int32_t module)
{
	return save_int32(CFG_KEY_MONITOR_MODULE, module, &config.monitor_module);
}

int config_set_monitor_base_link(const char *link)
{
	return save_string(CFG_KEY_MONITOR_BASE_LINK, link, config.monitor_base_link,
					   sizeof(config.monitor_base_link));
}

int config_set_device_brightness(int32_t brightness)
{
	return save_int32(CFG_KEY_DEVICE_BRIGHTNESS, brightness, &config.device_brightness);
}

int config_set_device_display_time(int32_t seconds)
{
	return save_int32(CFG_KEY_DEVICE_DISPLAY_TIME, seconds, &config.device_display_time);
}

int config_set_device_refresh_time(int32_t seconds)
{
	return save_int32(CFG_KEY_DEVICE_REFRESH_TIME, seconds, &config.device_refresh_time);
}

int config_set_advanced_config_timeout(int32_t value)
{
	return save_int32(CFG_KEY_ADVANCED_CONFIG_TIMEOUT, value, &config.advanced_config_timeout);
}

int config_set_advanced_enter_config(int32_t value)
{
	return save_int32(CFG_KEY_ADVANCED_ENTER_CONFIG, value, &config.advanced_enter_config);
}

int config_set_advanced_tls_verify(int32_t value)
{
	int rc = save_int32(CFG_KEY_ADVANCED_TLS_VERIFY, value, &config.advanced_tls_verify);

	if (rc == 0) {
		advanced_tls_verify_invalid = false;
	}
	return rc;
}

/* ---------------------------------------- Getters --------------------------------------------- */

bool config_check_boot_request(void)
{
	bool requested = config.advanced_enter_config != 0;

	config.advanced_enter_config = 0;

	/* Cleared immediately after read. */
	if (advanced_enter_config_set) {
		int rc = settings_delete(CFG_KEY_ADVANCED_ENTER_CONFIG);

		if (rc) {
			LOG_ERR("Failed to clear %s: %d", CFG_KEY_ADVANCED_ENTER_CONFIG, rc);
		}
		advanced_enter_config_set = false;
	}

	return requested;
}

const struct device_config *config_get(void)
{
	return &config;
}

/* ---------------------------------------- Validation ------------------------------------------ */

int config_validate(void)
{
	int ret = 0;

	if (!wifi_ssid_set) {
		LOG_ERR("wifi/ssid not set");
		ret = -EINVAL;
	}
	if (!wifi_password_set) {
		LOG_ERR("wifi/password not set");
		ret = -EINVAL;
	}
	if (!monitor_a_label_set) {
		LOG_ERR("monitor/a_label not set");
		ret = -EINVAL;
	}
	if (!monitor_a_id_set) {
		LOG_ERR("monitor/a_id not set");
		ret = -EINVAL;
	}
	if (!monitor_b_label_set) {
		LOG_ERR("monitor/b_label not set");
		ret = -EINVAL;
	}
	if (!monitor_b_id_set) {
		LOG_ERR("monitor/b_id not set");
		ret = -EINVAL;
	}
	if (!monitor_module_set) {
		LOG_ERR("monitor/module not set");
		ret = -EINVAL;
	}
	if (advanced_tls_verify_invalid) {
		LOG_ERR("advanced/tls_verify is not 0 or 1");
		ret = -EINVAL;
	}

	return ret;
}
