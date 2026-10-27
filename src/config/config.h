#ifndef APP_CONFIG_CONFIG_H_
#define APP_CONFIG_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#define CFG_TREE_WIFI          "wifi"
#define CFG_NAME_WIFI_SSID     "ssid"
#define CFG_KEY_WIFI_SSID      CFG_TREE_WIFI "/" CFG_NAME_WIFI_SSID
#define CFG_NAME_WIFI_PASSWORD "password"
#define CFG_KEY_WIFI_PASSWORD  CFG_TREE_WIFI "/" CFG_NAME_WIFI_PASSWORD

#define CFG_TREE_MONITOR           "monitor"
#define CFG_NAME_MONITOR_A_LABEL   "a_label"
#define CFG_KEY_MONITOR_A_LABEL    CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_A_LABEL
#define CFG_NAME_MONITOR_A_ID      "a_id"
#define CFG_KEY_MONITOR_A_ID       CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_A_ID
#define CFG_NAME_MONITOR_B_LABEL   "b_label"
#define CFG_KEY_MONITOR_B_LABEL    CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_B_LABEL
#define CFG_NAME_MONITOR_B_ID      "b_id"
#define CFG_KEY_MONITOR_B_ID       CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_B_ID
#define CFG_NAME_MONITOR_MODULE    "module"
#define CFG_KEY_MONITOR_MODULE     CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_MODULE
#define CFG_NAME_MONITOR_BASE_LINK "base_link"
#define CFG_KEY_MONITOR_BASE_LINK  CFG_TREE_MONITOR "/" CFG_NAME_MONITOR_BASE_LINK

#define CFG_TREE_DEVICE              "device"
#define CFG_NAME_DEVICE_BRIGHTNESS   "brightness"
#define CFG_KEY_DEVICE_BRIGHTNESS    CFG_TREE_DEVICE "/" CFG_NAME_DEVICE_BRIGHTNESS
#define CFG_NAME_DEVICE_DISPLAY_TIME "display_time"
#define CFG_KEY_DEVICE_DISPLAY_TIME  CFG_TREE_DEVICE "/" CFG_NAME_DEVICE_DISPLAY_TIME

#define CFG_TREE_ADVANCED                "advanced"
#define CFG_NAME_ADVANCED_CONFIG_TIMEOUT "config_timeout"
#define CFG_KEY_ADVANCED_CONFIG_TIMEOUT  CFG_TREE_ADVANCED "/" CFG_NAME_ADVANCED_CONFIG_TIMEOUT
#define CFG_NAME_ADVANCED_ENTER_CONFIG   "enter_config"
#define CFG_KEY_ADVANCED_ENTER_CONFIG    CFG_TREE_ADVANCED "/" CFG_NAME_ADVANCED_ENTER_CONFIG
#define CFG_NAME_ADVANCED_TLS_VERIFY     "tls_verify"
#define CFG_KEY_ADVANCED_TLS_VERIFY      CFG_TREE_ADVANCED "/" CFG_NAME_ADVANCED_TLS_VERIFY

struct device_config {
	char wifi_ssid[33];
	char wifi_password[64];

	char monitor_a_label[30];
	char monitor_a_id[128];
	char monitor_b_label[30];
	char monitor_b_id[128];
	int32_t monitor_module;
	char monitor_base_link[128];

	int32_t device_brightness;
	int32_t device_display_time;

	int32_t advanced_config_timeout;
	int32_t advanced_enter_config;
	int32_t advanced_tls_verify;
};

/* Ranges the shell accepts for the integer settings. */
#define CFG_BRIGHTNESS_MIN     1
#define CFG_BRIGHTNESS_MAX     100
#define CFG_CONFIG_TIMEOUT_MIN 10
#define CFG_CONFIG_TIMEOUT_MAX 3600
#define CFG_DISPLAY_TIME_MIN   5
#define CFG_DISPLAY_TIME_MAX   300

/* Each setter persists one key and updates the copy returned by config_get(), so a dump shows what
 * was saved. The running state machine keeps the configuration it booted with; changes take effect
 * on the next boot. Strings that do not fit their field are refused. Returns 0, or negative errno.
 */
int config_set_wifi_ssid(const char *ssid);
int config_set_wifi_password(const char *password);

int config_set_monitor_a_label(const char *label);
int config_set_monitor_a_id(const char *id);
int config_set_monitor_b_label(const char *label);
int config_set_monitor_b_id(const char *id);
int config_set_monitor_module(int32_t module);
int config_set_monitor_base_link(const char *link);

int config_set_device_brightness(int32_t brightness);
int config_set_device_display_time(int32_t seconds);

int config_set_advanced_config_timeout(int32_t value);
int config_set_advanced_enter_config(int32_t value);
int config_set_advanced_tls_verify(int32_t value);

/* Checks if the advanced_enter_config flag was set at boot, and clears it. */
bool config_check_boot_request(void);

/* Checks that every required key was loaded and that advanced/tls_verify, if stored, is 0 or 1.
 * Logs each problem. Returns 0 if valid, or -EINVAL if not.
 */
int config_validate(void);

/* Configuration of the device. Change it through the setters. */
const struct device_config *config_get(void);

#endif /* APP_CONFIG_CONFIG_H_ */
