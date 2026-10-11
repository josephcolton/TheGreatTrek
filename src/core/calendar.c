#include "calendar.h"

#include <stdio.h>

/* Days from the civil epoch 1970-01-01 (Howard Hinnant's days_from_civil). */
static long days_from_civil(long y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(long z, long *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

static long epoch_1837(void)
{
    return days_from_civil(1837, 1, 1);
}

int cal_day(int year, int month, int mday)
{
    return (int)(days_from_civil(year, month, mday) - epoch_1837());
}

void cal_date(int day, int *year, int *month, int *mday)
{
    long y;
    int m, d;
    civil_from_days(epoch_1837() + day, &y, &m, &d);
    *year = (int)y;
    *month = m;
    *mday = d;
}

int cal_weekday(int day)
{
    int w = day % 7; /* day 0 was a Sunday */
    return w < 0 ? w + 7 : w;
}

int cal_month(int day)
{
    int y, m, d;
    cal_date(day, &y, &m, &d);
    return m;
}

Season cal_season(int day)
{
    int m = cal_month(day);
    if (m == 12 || m <= 2) {
        return SEASON_SUMMER;
    }
    if (m <= 5) {
        return SEASON_AUTUMN;
    }
    if (m <= 8) {
        return SEASON_WINTER;
    }
    return SEASON_SPRING;
}

bool cal_parse(const char *text, int *day)
{
    int y, m, d;
    if (!text || sscanf(text, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) {
        return false;
    }
    *day = cal_day(y, m, d);
    return true;
}

void cal_format_iso(int day, char *out, int cap)
{
    int y, m, d;
    cal_date(day, &y, &m, &d);
    snprintf(out, (size_t)cap, "%04d-%02d-%02d", y, m, d);
}
