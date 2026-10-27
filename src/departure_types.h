#ifndef APP_DEPARTURE_TYPES_H_
#define APP_DEPARTURE_TYPES_H_

#include <stdint.h>

struct departure_line {
	char line_number[5];
	uint32_t line_color;
	uint32_t line_text_color;
	char destination[17];
	int32_t departure_minutes; /* Minutes until departure, or DEPARTURE_NO_DATA. */
};

/* Departure_minutes of a row without a departure. */
#define DEPARTURE_NO_DATA (-1)

/* Initialiser for a row without a departure. */
#define DEPARTURE_LINE_EMPTY ((struct departure_line){.departure_minutes = DEPARTURE_NO_DATA})

/* Stations shown on the screen, A and B. */
#define MONITOR_COUNT 2

/* Departures listed per station. */
#define DEPARTURE_LINE_COUNT 2

struct departure_data {
	char label[30];
	char updated[6]; /* "HH:MM\0" */
	struct departure_line lines[DEPARTURE_LINE_COUNT];
};

struct monitor_data {
	struct departure_data departures[MONITOR_COUNT];
};

#endif /* APP_DEPARTURE_TYPES_H_ */
