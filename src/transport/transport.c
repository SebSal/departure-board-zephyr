#include "transport.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "motis.h"
#include "vrs.h"
#include <net/http_client.h>

LOG_MODULE_REGISTER(app_transport, CONFIG_APP_TRANSPORT_LOG_LEVEL);

/* ---------------------------------------- Module state ---------------------------------------- */

#define TRANSPORT_HOST_SIZE 64
#define TRANSPORT_PATH_SIZE 256
#define TRANSPORT_BUF_SIZE  16384

/* Every known provider. New agencies add their descriptor here. A provider's index
 * is its id, stored as struct device_config.monitor_module.
 */
static const struct transport_provider *const providers[] = {
	&vrs_provider,
	&motis_provider,
};

/* Holds one body at a time: each monitor is parsed before the next one is fetched. */
static char body[TRANSPORT_BUF_SIZE];

BUILD_ASSERT(ARRAY_SIZE(providers) <= TRANSPORT_PROVIDER_MAX,
			 "Provider table outgrew TRANSPORT_PROVIDER_MAX");

/* ---------------------------------------- Providers ------------------------------------------- */

static const struct transport_provider *provider_get(int32_t module_id)
{
	if (module_id < 0 || module_id >= (int32_t)ARRAY_SIZE(providers)) {
		return NULL;
	}
	return providers[module_id];
}

size_t transport_provider_count(void)
{
	return ARRAY_SIZE(providers);
}

const char *transport_provider_name(int32_t module_id)
{
	const struct transport_provider *provider = provider_get(module_id);

	return provider ? provider->name : NULL;
}

/* ---------------------------------------- Fetching -------------------------------------------- */

/* Splits a base link ("https://host/path?query=") into host and path prefix.
 * path_prefix points into base, which must outlive the returned pointer. Returns 0, or
 * -ENAMETOOLONG if the host does not fit.
 */
static int split_base_link(const char *base, char *host, size_t host_size, const char **path_prefix)
{
	const char *host_start = strstr(base, "://");

	host_start = host_start ? host_start + 3 : base;

	const char *prefix = strchr(host_start, '/');

	if (!prefix) {
		prefix = host_start + strlen(host_start);
	}

	int len = snprintf(host, host_size, "%.*s", (int)(prefix - host_start), host_start);

	if (len >= (int)host_size) {
		LOG_ERR("Host in base link is longer than %u characters", (unsigned int)host_size - 1);
		return -ENAMETOOLONG;
	}

	*path_prefix = prefix;
	return 0;
}

/* Fetches one monitor into buf. The default for providers that leave fetch NULL. */
static int fetch_one(int sock, const char *host, const char *path_prefix, const char *call,
					 char *buf, size_t buf_size)
{
	char path[TRANSPORT_PATH_SIZE];

	int len = snprintf(path, sizeof(path), "%s%s", path_prefix, call);

	if (len >= (int)sizeof(path)) {
		LOG_ERR("Request path is longer than %u characters", (unsigned int)sizeof(path) - 1);
		return -ENAMETOOLONG;
	}

	LOG_DBG("GET https://%s%s", host, path);

	return http_get(sock, host, path, buf, buf_size);
}

/* Connects once, then fetches and parses whichever monitors are configured, one after the other
 * over the same socket. Stops at the first failure.
 */
static int fetch_departures(struct monitor_data *monitor, const struct transport_provider *provider,
							const char *const calls[], const char *base_link, bool tls_verify,
							int64_t current_time)
{
	const char *base =
		(base_link && base_link[0] != '\0') ? base_link : provider->base_link_default;
	const char *path_prefix;
	char host[TRANSPORT_HOST_SIZE];

	int ret = split_base_link(base, host, sizeof(host), &path_prefix);

	if (ret < 0) {
		return ret;
	}

	LOG_DBG("Fetching data from host: %s, path prefix: %s", host, path_prefix);

	int sock = http_connect(host, tls_verify);

	if (sock < 0) {
		LOG_ERR("Failed to connect: %d", sock);
		return sock;
	}

	/* A monitor with no id configured is skipped and keeps its placeholder data. */
	for (int i = 0; i < MONITOR_COUNT; i++) {
		if (!calls[i] || calls[i][0] == '\0') {
			continue;
		}

		if (provider->fetch) {
			ret = provider->fetch(sock, host, path_prefix, calls[i], body, sizeof(body));
		} else {
			ret = fetch_one(sock, host, path_prefix, calls[i], body, sizeof(body));
		}
		if (ret < 0) {
			LOG_ERR("Failed to fetch departure %c: %d", 'A' + i, ret);
			break;
		}

		/* Rows the response does not fill stay without a departure. The label is the caller's. */
		for (int l = 0; l < DEPARTURE_LINE_COUNT; l++) {
			monitor->departures[i].lines[l] = DEPARTURE_LINE_EMPTY;
		}

		ret = provider->parse(body, &monitor->departures[i], current_time);
		if (ret < 0) {
			LOG_ERR("Failed to parse departure %c: %d", 'A' + i, ret);
			break;
		}
	}

	http_close(sock);
	return ret;
}

/* ---------------------------------------- Update ---------------------------------------------- */

int transport_update(struct monitor_data *monitor, int32_t module_id, const char *const calls[],
					 const char *base_link, bool tls_verify, int64_t current_time)
{
	const struct transport_provider *provider = provider_get(module_id);

	if (!provider) {
		LOG_ERR("Unknown transportation module: %d", module_id);
		return -ENOTSUP;
	}

	LOG_INF("Updating monitor data using %s.", provider->name);

	return fetch_departures(monitor, provider, calls, base_link, tls_verify, current_time);
}
