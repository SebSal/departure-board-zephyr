#include "time_util.h"

#include <errno.h>
#include <stdint.h>

/* ---------------------------------------- Civil date arithmetic ------------------------------- */

/* Days between 1970-01-01 and the given civil date. Howard Hinnant's days_from_civil,
 * which is exact over the whole proleptic Gregorian range and needs no lookup table.
 */
static int64_t days_from_civil(int y, int m, int d)
{
	y -= m <= 2;
	const int64_t era = (y >= 0 ? y : y - 399) / 400;
	const int64_t yoe = y - era * 400;                                  /* [0, 399] */
	const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; /* [0, 365] */
	const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          /* [0, 146096] */

	return era * 146097 + doe - 719468;
}

/* The inverse, so that a Unix instant can be turned back into a year. */
static int year_from_days(int64_t z)
{
	z += 719468;
	const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	const int64_t doe = z - era * 146097; /* [0, 146096] */
	const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); /* [0, 365] */
	const int64_t mp = (5 * doy + 2) / 153;                      /* [0, 11] */
	const int64_t mo = mp + (mp < 10 ? 3 : -9);                  /* [1, 12] */

	return (int)(yoe + era * 400 + (mo <= 2));
}

/* ---------------------------------------- ISO 8601 -------------------------------------------- */

/* Reads n decimal digits. Returns the value, or -1 if any character is not a digit. */
static int read_digits(const char *s, int n)
{
	int v = 0;

	for (int i = 0; i < n; i++) {
		if (s[i] < '0' || s[i] > '9') {
			return -1;
		}
		v = v * 10 + (s[i] - '0');
	}
	return v;
}

int time_iso8601_to_unix(const char *s, int len, int64_t *out_unix, int32_t *out_offset)
{
	if (len < 19 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':') {
		return -EINVAL;
	}

	int year = read_digits(s, 4);
	int month = read_digits(s + 5, 2);
	int day = read_digits(s + 8, 2);
	int hour = read_digits(s + 11, 2);
	int minute = read_digits(s + 14, 2);
	int second = read_digits(s + 17, 2);

	if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || minute < 0 ||
		second < 0) {
		return -EINVAL;
	}

	int pos = 19;

	/* Fractional seconds mean nothing to a minute display, but the zone comes after them. */
	if (pos < len && s[pos] == '.') {
		int digits_start = ++pos;

		while (pos < len && s[pos] >= '0' && s[pos] <= '9') {
			pos++;
		}
		if (pos == digits_start) {
			return -EINVAL;
		}
	}

	/* "Z", or no zone at all, means the wall clock is already UTC. Anything left over that is
	 * not a zone is refused rather than read as UTC.
	 */
	int32_t offset = 0;

	if (pos < len && !(s[pos] == 'Z' && pos + 1 == len)) {
		const char *zone = s + pos + 1;
		int zone_len = len - pos - 1;
		int off_hour = -1;
		int off_min = -1;

		if (s[pos] != '+' && s[pos] != '-') {
			return -EINVAL;
		}

		/* "+HH:MM", "+HHMM" or "+HH". */
		if (zone_len == 5 && zone[2] == ':') {
			off_hour = read_digits(zone, 2);
			off_min = read_digits(zone + 3, 2);
		} else if (zone_len == 4) {
			off_hour = read_digits(zone, 2);
			off_min = read_digits(zone + 2, 2);
		} else if (zone_len == 2) {
			off_hour = read_digits(zone, 2);
			off_min = 0;
		}

		if (off_hour < 0 || off_hour > 23 || off_min < 0 || off_min > 59) {
			return -EINVAL;
		}
		offset = (int32_t)(off_hour * 3600 + off_min * 60);
		if (s[pos] == '-') {
			offset = -offset;
		}
	}

	*out_unix =
		days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second - offset;
	*out_offset = offset;
	return 0;
}

/* ---------------------------------------- EU summer time -------------------------------------- */

/* Unix instant of the last Sunday of month in year, at 01:00 UTC - the moment the
 * EU switches. Both March and October have 31 days, which is all this is used for.
 */
static int64_t last_sunday_0100z(int year, int month)
{
	int64_t last = days_from_civil(year, month, 31);
	/* 1970-01-01 was a Thursday, so (days + 4) % 7 counts from Sunday. */
	int weekday = (int)((last + 4) % 7);

	return (last - weekday) * 86400 + 3600;
}

int32_t time_eu_utc_offset(int64_t utc_seconds)
{
	if (utc_seconds < 0) {
		return 3600;
	}

	int year = year_from_days(utc_seconds / 86400);
	int64_t summer_start = last_sunday_0100z(year, 3);
	int64_t summer_end = last_sunday_0100z(year, 10);

	return (utc_seconds >= summer_start && utc_seconds < summer_end) ? 7200 : 3600;
}

/* ---------------------------------------- Departures ------------------------------------------ */

int32_t time_minutes_until(int64_t departure, int64_t now)
{
	int64_t diff = departure - now;

	return diff > 0 ? (int32_t)(diff / 60) : 0;
}
