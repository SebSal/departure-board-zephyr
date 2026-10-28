#include "wifi.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(app_wifi, CONFIG_APP_WIFI_LOG_LEVEL);

/* ---------------------------------------- Module state ---------------------------------------- */

/* The driver reconnects on its own, so tolerate a few failed attempts before giving up. */
#define WIFI_MAX_ASSOC_FAILURES 3

static K_SEM_DEFINE(wifi_ready, 0, 1);

/* Written by the event handler, read by wifi_connect() once the semaphore is taken. */
static int wifi_connect_result;
static int wifi_assoc_failures;

/* ---------------------------------------- Events ---------------------------------------------- */

static void wifi_event_handler(uint64_t event, struct net_if *iface, void *info, size_t info_length,
							   void *user_data)
{
	switch (event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		const struct wifi_status *status = info;

		if (status->status == 0) {
			break;
		}

		wifi_assoc_failures++;
		LOG_WRN("Association attempt %d failed (status %d)", wifi_assoc_failures, status->status);

		/* Fail the cycle instead of blocking until WIFI_CONNECT_TIMEOUT_MS. */
		if (wifi_assoc_failures >= WIFI_MAX_ASSOC_FAILURES) {
			wifi_connect_result = -ECONNREFUSED;
			k_sem_give(&wifi_ready);
		}
		break;
	}

	case NET_EVENT_IPV4_ADDR_ADD:
		wifi_connect_result = 0;
		k_sem_give(&wifi_ready);
		break;

	default:
		break;
	}
}

/* Net_mgmt matches layer and layer code exactly and only ORs the command bits, so events from
 * different layers cannot share one mask. L2 (Wi-Fi) and L3 (IPv4) need a handler each.
 */
NET_MGMT_REGISTER_EVENT_HANDLER(wifi_l2_handler, NET_EVENT_WIFI_CONNECT_RESULT, wifi_event_handler,
								NULL);
NET_MGMT_REGISTER_EVENT_HANDLER(wifi_l3_handler, NET_EVENT_IPV4_ADDR_ADD, wifi_event_handler, NULL);

/* ---------------------------------------- Connection ------------------------------------------ */

bool wifi_is_connected(void)
{
	struct net_if *iface = net_if_get_default();

	return net_if_is_up(iface) && net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) != NULL;
}

int wifi_connect(const char *ssid, const char *psk)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params params = {
		.ssid = (const uint8_t *)ssid,
		.ssid_length = strlen(ssid),
		.psk = (const uint8_t *)psk,
		.psk_length = strlen(psk),
		.channel = WIFI_CHANNEL_ANY,
		.security = WIFI_SECURITY_TYPE_PSK,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
	};

	k_sem_reset(&wifi_ready);
	wifi_assoc_failures = 0;

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));

	if (ret == -EALREADY) {
		/* The driver is reconnecting after a drop, wait for it like for our own request. The
		 * lease may have arrived just before the semaphore was reset.
		 */
		LOG_INF("WiFi reconnect already in progress");
		if (wifi_is_connected()) {
			return 0;
		}
	} else if (ret) {
		LOG_ERR("WiFi connect request failed: %d", ret);
		return ret;
	}

	if (k_sem_take(&wifi_ready, K_MSEC(WIFI_CONNECT_TIMEOUT_MS)) != 0) {
		LOG_ERR("WiFi connect timed out");
		wifi_disconnect();
		return -ETIMEDOUT;
	}

	if (wifi_connect_result) {
		LOG_ERR("WiFi connect failed: %d", wifi_connect_result);
		wifi_disconnect();
		return wifi_connect_result;
	}

	struct in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);

	if (addr) {
		LOG_INF("WiFi connected, IP: %d.%d.%d.%d", addr->s4_addr[0], addr->s4_addr[1],
				addr->s4_addr[2], addr->s4_addr[3]);
	} else {
		LOG_INF("WiFi connected");
	}

	return 0;
}

void wifi_disconnect(void)
{
	struct net_if *iface = net_if_get_default();
	int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

	if (ret) {
		LOG_ERR("WiFi disconnect failed: %d", ret);
		return;
	}
	LOG_INF("WiFi disconnected");
}
