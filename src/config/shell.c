#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include "config.h"
#include <transport/transport.h>

LOG_MODULE_REGISTER(app_shell, CONFIG_APP_SHELL_LOG_LEVEL);

/* ---------------------------------------- Helpers --------------------------------------------- */

/* Size of a struct device_config field, for the length checks. */
#define CFG_FIELD_SIZE(field) SIZEOF_FIELD(struct device_config, field)

/* Prints a failed save. The setter has already logged the details. */
static int report_save(const struct shell *sh, const char *key, int rc)
{
	if (rc < 0) {
		shell_error(sh, "Failed to save %s: %d", key, rc);
	}
	return rc;
}

/* Saves a string, refusing one the config field cannot hold, rather than failing at the next
 * boot.
 */
static int set_string(const struct shell *sh, const char *key, const char *value, size_t size,
					  int (*setter)(const char *))
{
	size_t len = strlen(value);

	if (len >= size) {
		shell_error(sh, "%s is %u bytes, at most %u fit.", key, (unsigned int)len,
					(unsigned int)(size - 1));
		return -EINVAL;
	}
	return report_save(sh, key, setter(value));
}

/* Parses a whole number in [min, max]. Anything else - garbage, trailing characters, out of range -
 * is refused instead of silently becoming 0 the way atoi would have it.
 */
static int parse_int32(const struct shell *sh, const char *key, const char *arg, int32_t min,
					   int32_t max, int32_t *out)
{
	int err = 0;
	long value = shell_strtol(arg, 10, &err);

	if (err || value < min || value > max) {
		shell_error(sh, "%s must be a whole number from %d to %d.", key, min, max);
		return -EINVAL;
	}
	*out = (int32_t)value;
	return 0;
}

/* ---------------------------------------- Setters --------------------------------------------- */

static int set_label_0(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_MONITOR_A_LABEL, argv[1], CFG_FIELD_SIZE(monitor_a_label),
					  config_set_monitor_a_label);
}

static int set_label_1(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_MONITOR_B_LABEL, argv[1], CFG_FIELD_SIZE(monitor_b_label),
					  config_set_monitor_b_label);
}

static int set_id_0(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_MONITOR_A_ID, argv[1], CFG_FIELD_SIZE(monitor_a_id),
					  config_set_monitor_a_id);
}

static int set_id_1(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_MONITOR_B_ID, argv[1], CFG_FIELD_SIZE(monitor_b_id),
					  config_set_monitor_b_id);
}

static int apply_module(const struct shell *sh, const char *arg)
{
	int32_t module;
	int rc = parse_int32(sh, CFG_KEY_MONITOR_MODULE, arg, 0,
						 (int32_t)transport_provider_count() - 1, &module);

	if (rc < 0) {
		return rc;
	}

	LOG_DBG("module set to: %d (%s)", module, transport_provider_name(module));
	return report_save(sh, CFG_KEY_MONITOR_MODULE, config_set_monitor_module(module));
}

/* Reached when the argument did not match one of the enumerated modules below - so it is either
 * garbage or an out-of-range id, and apply_module rejects it.
 */
static int set_module(const struct shell *sh, size_t argc, char **argv)
{
	return apply_module(sh, argv[1]);
}

/* Handler shared by every enumerated module: argv[0] is the id we published. */
static int set_module_dyn(const struct shell *sh, size_t argc, char **argv)
{
	return apply_module(sh, argv[0]);
}

static int set_base_link(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_MONITOR_BASE_LINK, argv[1], CFG_FIELD_SIZE(monitor_base_link),
					  config_set_monitor_base_link);
}

static int set_ssid(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_WIFI_SSID, argv[1], CFG_FIELD_SIZE(wifi_ssid),
					  config_set_wifi_ssid);
}

static int set_password(const struct shell *sh, size_t argc, char **argv)
{
	return set_string(sh, CFG_KEY_WIFI_PASSWORD, argv[1], CFG_FIELD_SIZE(wifi_password),
					  config_set_wifi_password);
}

static int set_brightness(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_DEVICE_BRIGHTNESS, argv[1], CFG_BRIGHTNESS_MIN,
						 CFG_BRIGHTNESS_MAX, &value);

	if (rc < 0) {
		return rc;
	}
	return report_save(sh, CFG_KEY_DEVICE_BRIGHTNESS, config_set_device_brightness(value));
}

static int set_display_time(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_DEVICE_DISPLAY_TIME, argv[1], CFG_DISPLAY_TIME_MIN,
						 CFG_DISPLAY_TIME_MAX, &value);

	if (rc < 0) {
		return rc;
	}
	return report_save(sh, CFG_KEY_DEVICE_DISPLAY_TIME, config_set_device_display_time(value));
}

static int set_refresh_time(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_DEVICE_REFRESH_TIME, argv[1], CFG_REFRESH_TIME_MIN,
						 CFG_REFRESH_TIME_MAX, &value);

	if (rc < 0) {
		return rc;
	}
	return report_save(sh, CFG_KEY_DEVICE_REFRESH_TIME, config_set_device_refresh_time(value));
}

static int set_config_timeout(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_ADVANCED_CONFIG_TIMEOUT, argv[1], CFG_CONFIG_TIMEOUT_MIN,
						 CFG_CONFIG_TIMEOUT_MAX, &value);

	if (rc < 0) {
		return rc;
	}
	return report_save(sh, CFG_KEY_ADVANCED_CONFIG_TIMEOUT,
					   config_set_advanced_config_timeout(value));
}

static int set_enter_config(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_ADVANCED_ENTER_CONFIG, argv[1], 0, 1, &value);

	if (rc < 0) {
		return rc;
	}

	rc = report_save(sh, CFG_KEY_ADVANCED_ENTER_CONFIG, config_set_advanced_enter_config(value));
	if (rc < 0) {
		return rc;
	}

	shell_print(sh,
				value ? "Config mode armed for the next boot." : "Config mode request cleared.");
	return 0;
}

static int set_tls_verify(const struct shell *sh, size_t argc, char **argv)
{
	int32_t value;
	int rc = parse_int32(sh, CFG_KEY_ADVANCED_TLS_VERIFY, argv[1], 0, 1, &value);

	if (rc < 0) {
		return rc;
	}
	return report_save(sh, CFG_KEY_ADVANCED_TLS_VERIFY, config_set_advanced_tls_verify(value));
}

/* ---------------------------------------- Getters --------------------------------------------- */

static int cmd_dump(const struct shell *sh, size_t argc, char **argv)
{
	const struct device_config *cfg = config_get();

	shell_print(sh, "wifi/ssid: %s", cfg->wifi_ssid);
	shell_print(sh, "wifi/password: %s", cfg->wifi_password[0] != '\0' ? "***" : "(not set)");
	shell_print(sh, "monitor/a_label: %s", cfg->monitor_a_label);
	shell_print(sh, "monitor/a_id: %s", cfg->monitor_a_id);
	shell_print(sh, "monitor/b_label: %s", cfg->monitor_b_label);
	shell_print(sh, "monitor/b_id: %s", cfg->monitor_b_id);
	shell_print(sh, "monitor/module: %d", cfg->monitor_module);
	shell_print(sh, "monitor/base_link: %s", cfg->monitor_base_link);
	shell_print(sh, "device/brightness: %d", cfg->device_brightness);
	shell_print(sh, "device/display_time: %d", cfg->device_display_time);
	shell_print(sh, "device/refresh_time: %d", cfg->device_refresh_time);
	shell_print(sh, "advanced/config_timeout: %d", cfg->advanced_config_timeout);
	shell_print(sh, "advanced/tls_verify: %d", cfg->advanced_tls_verify);

	return 0;
}
/* ---------------------------------------- Dynamic commands ------------------------------------ */

/* Generated from the provider table - adding a provider needs no change here. */
static void module_get(size_t idx, struct shell_static_entry *entry)
{
	static char syntax[TRANSPORT_PROVIDER_MAX][12];

	const char *name = (idx < ARRAY_SIZE(syntax)) ? transport_provider_name((int32_t)idx) : NULL;

	if (!name) {
		entry->syntax = NULL;
		return;
	}

	snprintf(syntax[idx], sizeof(syntax[idx]), "%u", (unsigned int)idx);

	entry->syntax = syntax[idx];
	entry->help = name;
	entry->subcmd = NULL;
	entry->handler = set_module_dyn;
	entry->args.mandatory = 1;
	entry->args.optional = 0;
}

SHELL_DYNAMIC_CMD_CREATE(module_providers, module_get);

/* ---------------------------------------- Command registration -------------------------------- */

SHELL_STATIC_SUBCMD_SET_CREATE(
	monitor_conf, SHELL_CMD_ARG(a_label, NULL, "Set the label of station A.", set_label_0, 2, 0),
	SHELL_CMD_ARG(a_id, NULL, "ID string of station A.", set_id_0, 2, 0),
	SHELL_CMD_ARG(b_label, NULL, "Set the label of station B.", set_label_1, 2, 0),
	SHELL_CMD_ARG(b_id, NULL, "ID string of station B.", set_id_1, 2, 0),
	SHELL_CMD_ARG(module, &module_providers, "Module ID for departure parsing.", set_module, 2, 0),
	SHELL_CMD_ARG(base_link, NULL, "Base Link for data service.", set_base_link, 2, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(wifi_conf,
							   SHELL_CMD_ARG(ssid, NULL, "Set WiFi SSID/Name.", set_ssid, 2, 0),
							   SHELL_CMD_ARG(password, NULL, "Set WiFi password.", set_password, 2,
											 0),
							   SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(device_conf,
							   SHELL_CMD_ARG(brightness, NULL,
											 "Set Screen Brightness in percent. E.g. 30.",
											 set_brightness, 2, 0),
							   SHELL_CMD_ARG(display_time, NULL,
											 "Seconds the departure board stays up. E.g. 30.",
											 set_display_time, 2, 0),
							   SHELL_CMD_ARG(refresh_time, NULL,
											 "Seconds between refreshes without deep sleep. "
											 "E.g. 60.",
											 set_refresh_time, 2, 0),
							   SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	adv_conf,
	SHELL_CMD_ARG(config_timeout, NULL, "Config mode timeout in seconds. E.g. 300.",
				  set_config_timeout, 2, 0),
	SHELL_CMD_ARG(enter_config, NULL,
				  "Enter config mode on the next boot (1) or clear the request (0).",
				  set_enter_config, 2, 0),
	SHELL_CMD_ARG(tls_verify, NULL, "Check the server certificate (1, default) or skip it (0).",
				  set_tls_verify, 2, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(monitor, &monitor_conf, "Monitor Settings for stations and labels.", NULL);
SHELL_CMD_REGISTER(wifi, &wifi_conf, "WiFi Configuration.", NULL);
SHELL_CMD_REGISTER(device, &device_conf, "Device Configuration.", NULL);
SHELL_CMD_REGISTER(advanced, &adv_conf, "Advanced settings. Don't touch.", NULL);
SHELL_CMD_REGISTER(dump, NULL, "Dump all current configuration values.", cmd_dump);
