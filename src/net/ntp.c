#include "ntp.h"

#include <time.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/clock.h>

LOG_MODULE_REGISTER(app_ntp, CONFIG_APP_NTP_LOG_LEVEL);

/* ---------------------------------------- Time ------------------------------------------------ */

#define NTP_SERVER "pool.ntp.org"

int ntp_get_time(int64_t *out_seconds)
{
	/* Resolving the NTP pool occasionally times out (DNS_EAI_CANCELED) on a freshly associated
	 * link. A wake cycle has time to spare, so retry rather than lose the whole update.
	 */
	struct sntp_time sntp;
	int ret = 0;

	for (int attempt = 1; attempt <= NTP_ATTEMPTS; attempt++) {
		ret = sntp_simple(NTP_SERVER, NTP_TIMEOUT_MS, &sntp);
		if (ret == 0) {
			break;
		}
		LOG_WRN("SNTP attempt %d/%d failed: %d", attempt, NTP_ATTEMPTS, ret);
	}

	if (ret < 0) {
		LOG_ERR("SNTP failed: %d", ret);
		return ret;
	}

	/* mbedTLS checks certificate dates through time(), which reads this clock. It starts at 1970
	 * on every boot, so a connect without it set would find every certificate not yet valid.
	 */
	const struct timespec ts = {
		.tv_sec = (time_t)sntp.seconds,
		/* fraction is a binary fraction of a second. */
		.tv_nsec = (long)(((uint64_t)sntp.fraction * NSEC_PER_SEC) >> 32),
	};

	ret = sys_clock_settime(SYS_CLOCK_REALTIME, &ts);
	if (ret < 0) {
		LOG_ERR("Setting the wall clock failed: %d", ret);
		return ret;
	}

	*out_seconds = (int64_t)sntp.seconds;
	LOG_DBG("SNTP time: %lld", *out_seconds);

	return 0;
}
