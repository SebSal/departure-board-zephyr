#ifndef APP_UTIL_TIME_UTIL_H_
#define APP_UTIL_TIME_UTIL_H_

#include <stdint.h>

/* Parses "YYYY-MM-DDTHH:MM:SS" into Unix seconds. */
int time_iso8601_to_unix(const char *s, int len, int64_t *out_unix, int32_t *out_offset);

/* Seconds to add to a UTC instant to get Central European wall clock time: 3600 in
 * winter, 7200 in summer. For feeds that timestamp purely in UTC and so give the
 * parser nothing else to go on.
 */
int32_t time_eu_utc_offset(int64_t utc_seconds);

/* Whole minutes from now until departure, both Unix seconds. */
int32_t time_minutes_until(int64_t departure, int64_t now);

#endif /* APP_UTIL_TIME_UTIL_H_ */
