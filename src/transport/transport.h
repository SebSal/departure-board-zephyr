#ifndef APP_TRANSPORT_TRANSPORT_H_
#define APP_TRANSPORT_TRANSPORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <departure_types.h>
#include <net/http_client.h>

/* Longest transport_update() can take: one connect, then one request per monitor. */
#define TRANSPORT_UPDATE_MAX_MS (HTTP_CONNECT_MAX_MS + MONITOR_COUNT * HTTP_TIMEOUT_MS)

/* Max amount of transportation providers. Needed for static allocation of the shell helper. */
#define TRANSPORT_PROVIDER_MAX 8

/* A transportation provider contributes parsing, and optionally its own per-monitor
 * fetching.
 */
struct transport_provider {
	const char *name; /* "VRS" - for logging */

	/* Base link used when the device has no base_link setting stored. */
	const char *base_link_default;

	/* Optional alternative fetch function. If NULL the default fetch_one() is used. It has to
	 * leave buf NUL-terminated, as http_get() does.
	 */
	int (*fetch)(int sock, const char *host, const char *path_prefix, const char *call, char *buf,
				 size_t buf_size);

	/* JSON parser for the provider's departure format. The rows of out arrive reset to
	 * DEPARTURE_LINE_EMPTY. Returns 0, or negative on failure.
	 */
	int (*parse)(char *json, struct departure_data *out, int64_t current_time);
};

/* Runs a full update - fetch, parse - for the selected provider. current_time is Unix seconds, the
 * reference the departure minutes are counted from.
 */
int transport_update(struct monitor_data *monitor, int32_t module_id, const char *const calls[],
					 const char *base_link, bool tls_verify, int64_t current_time);

/* Number of known providers.. */
size_t transport_provider_count(void);

/* Display name of a provider. Needed for shell. */
const char *transport_provider_name(int32_t module_id);

#endif /* APP_TRANSPORT_TRANSPORT_H_ */
