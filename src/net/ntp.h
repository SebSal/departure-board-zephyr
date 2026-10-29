#ifndef APP_NET_NTP_H_
#define APP_NET_NTP_H_

#include <stdint.h>

#define NTP_TIMEOUT_MS 5000
#define NTP_ATTEMPTS   2

/* Longest ntp_get_time() can take: each attempt resolves the pool and then queries it. */
#define NTP_GET_TIME_MAX_MS (NTP_ATTEMPTS * (CONFIG_NET_SOCKETS_DNS_TIMEOUT + NTP_TIMEOUT_MS))

/* Fetches the current Unix time via SNTP and sets the system wall clock to it, which the TLS
 * certificate date check reads. Returns 0 and writes *out_seconds on success, negative errno
 * otherwise.
 */
int ntp_get_time(int64_t *out_seconds);

#endif /* APP_NET_NTP_H_ */
