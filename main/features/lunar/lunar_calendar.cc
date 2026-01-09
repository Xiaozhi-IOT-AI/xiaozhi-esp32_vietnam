#include "lunar_calendar.h"

#include <stdio.h>
#include <time.h>
#include <stddef.h>

// ======================================================
// Vietnamese/Chinese lunar calendar data (1900+)
// bit 16   : leap month is 30 days (1) or 29 days (0)
// bit 15-4 : 12 months, 30 days (1) or 29 days (0)
// bit 3-0  : leap month number (0 = no leap)
// ======================================================
static const uint32_t LUNAR_DATA[] = {
    0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,
    0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,
    0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
    0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,
    0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,
    0x06ca0,0x0b550,0x15355,0x04da0,0x0a5b0,0x14573,0x052b0,0x0a9a8,0x0e950,0x06aa0,
    0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,
    0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b6a0,0x195a6,
    0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,
    0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x055c0,0x0ab60,0x096d5,0x092e0,
    0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,
    0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,
    0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,
    0x05aa0,0x076a3,0x096d0,0x04afb,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,
    0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,
    0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,
    0x0a2e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,
    0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,
    0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,
    0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a2d0,0x0d150,0x0f252,
    0x0d520
};

static constexpr int LUNAR_START_YEAR = 1900;
static constexpr int LUNAR_END_YEAR = LUNAR_START_YEAR + (int)(sizeof(LUNAR_DATA) / sizeof(LUNAR_DATA[0])) - 1;

static int is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int solar_to_days(int y, int m, int d) {
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int days = 0;
    for (int i = 1900; i < y; i++) days += is_leap_year(i) ? 366 : 365;
    for (int i = 1; i < m; i++) {
        days += mdays[i - 1];
        if (i == 2 && is_leap_year(y)) days++;
    }
    return days + d - 1;
}

static int jdn_from_gregorian(int y, int m, int d) {
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

LunarDate solar_to_lunar(SolarDate solar) {
    LunarDate res = {1, 1, LUNAR_START_YEAR, false};

    if (solar.year < LUNAR_START_YEAR || solar.year > LUNAR_END_YEAR) return res;

    int offset = solar_to_days(solar.year, solar.month, solar.day)
               - solar_to_days(1900, 1, 31);

    int lunar_year = LUNAR_START_YEAR;
    while (lunar_year <= LUNAR_END_YEAR) {
        int days_in_year = 0;
        uint32_t data = LUNAR_DATA[lunar_year - LUNAR_START_YEAR];

        for (int i = 0; i < 12; i++)
            days_in_year += (data & (0x8000 >> i)) ? 30 : 29;

        if (data & 0xF) days_in_year += (data & 0x10000) ? 30 : 29;

        if (offset < days_in_year) break;
        offset -= days_in_year;
        lunar_year++;
    }

    if (lunar_year > LUNAR_END_YEAR) return res;

    uint32_t data = LUNAR_DATA[lunar_year - LUNAR_START_YEAR];
    int leap = data & 0xF;
    int lunar_month = 1;
    bool is_leap = false;

    while (lunar_month <= 12) {
        int days_in_month = (data & (0x8000 >> (lunar_month - 1))) ? 30 : 29;

        if (offset < days_in_month) break;
        offset -= days_in_month;

        if (leap == lunar_month) {
            int leap_days = (data & 0x10000) ? 30 : 29;
            if (offset < leap_days) {
                is_leap = true;
                break;
            }
            offset -= leap_days;
        }
        lunar_month++;
    }

    res.year = lunar_year;
    res.month = lunar_month;
    res.day = offset + 1;
    res.is_leap_month = is_leap;
    return res;
}

static const char* const CAN[10] = {"Giáp","Ất","Bính","Đinh","Mậu","Kỷ","Canh","Tân","Nhâm","Quý"};
static const char* const CHI[12] = {"Tý","Sửu","Dần","Mão","Thìn","Tỵ","Ngọ","Mùi","Thân","Dậu","Tuất","Hợi"};

std::string can_chi_year(int lunar_year) {
    // 2024 = Giáp Thìn => can=(y+6)%10, chi=(y+8)%12
    int can = (lunar_year + 6) % 10;
    int chi = (lunar_year + 8) % 12;
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %s", CAN[can], CHI[chi]);
    return std::string(buf);
}

std::string can_chi_month(int lunar_year, int lunar_month) {
    int year_can = (lunar_year + 6) % 10;
    int month_can = (year_can * 2 + lunar_month + 1) % 10;
    int month_chi = (lunar_month + 1) % 12; // Month 1 => Dần

    char buf[32];
    snprintf(buf, sizeof(buf), "%s %s", CAN[month_can], CHI[month_chi]);
    return std::string(buf);
}

std::string can_chi_day(SolarDate solar) {
    int jdn = jdn_from_gregorian(solar.year, solar.month, solar.day);
    int day_can = (jdn + 9) % 10;
    int day_chi = (jdn + 1) % 12;

    char buf[32];
    snprintf(buf, sizeof(buf), "%s %s", CAN[day_can], CHI[day_chi]);
    return std::string(buf);
}
