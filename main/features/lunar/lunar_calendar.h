#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <string>

struct LunarDate {
    int day;
    int month;
    int year;
    bool is_leap_month;
};

struct SolarDate {
    int day;
    int month;
    int year;
};

LunarDate solar_to_lunar(SolarDate solar);

// Sexagenary cycle (Can–Chi)
std::string can_chi_year(int lunar_year);
std::string can_chi_month(int lunar_year, int lunar_month);
std::string can_chi_day(SolarDate solar);
