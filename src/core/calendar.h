/* Dates as day numbers: day 0 is 1 January 1837 (a Sunday). Gregorian calendar. */
#ifndef TREK_CALENDAR_H
#define TREK_CALENDAR_H

#include "data.h"

#include <stdbool.h>

int    cal_day(int year, int month, int mday);          /* month 1..12 */
void   cal_date(int day, int *year, int *month, int *mday);
int    cal_weekday(int day);                             /* 0 = Sunday */
int    cal_month(int day);                               /* 1..12 */
Season cal_season(int day);
/* Parses "YYYY-MM-DD"; returns false if malformed. */
bool   cal_parse(const char *text, int *day);
/* Writes "YYYY-MM-DD". */
void   cal_format_iso(int day, char *out, int cap);

#endif
