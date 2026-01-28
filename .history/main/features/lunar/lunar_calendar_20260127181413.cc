#include "lunar_calendar.h"
#include <esp_log.h>
#include <cmath>
#include <cstring>
#include <sys/time.h>
#include "misc/lv_timer_private.h"

#define TAG "LunarCalendar"

// Lunar calendar data table (simplified - 1900-2100)
// Each entry encodes lunar month info for one year
// In practice, you'd use a full table or API
static const int LUNAR_INFO[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,
    0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,
    0x0d520
};

LunarCalendar::LunarCalendar(lv_obj_t* parent, int width, int height)
    : parent_(parent), width_(width), height_(height) {
    CreateUI();
}

LunarCalendar::~LunarCalendar() {
    if (update_timer_) {
        lv_timer_delete(update_timer_);
    }
    if (container_) {
        lv_obj_delete(container_);
    }
}

void LunarCalendar::CreateUI() {
    // Main container
    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x87CEEB), 0);  // Sky blue
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 10, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    int y_pos = 10;
    int line_height = 28;
    
    // Row 1: Weekday
    weekday_label_ = lv_label_create(container_);
    lv_label_set_text(weekday_label_, "Thứ Ba");
    lv_obj_set_style_text_font(weekday_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weekday_label_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_align(weekday_label_, LV_ALIGN_TOP_MID, 0, y_pos);
    y_pos += line_height;
    
    // Row 2: Solar date and time
    lv_obj_t* date_time_cont = lv_obj_create(container_);
    lv_obj_set_size(date_time_cont, width_ - 20, 30);
    lv_obj_set_pos(date_time_cont, 0, y_pos);
    lv_obj_set_style_bg_opa(date_time_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_time_cont, 0, 0);
    lv_obj_set_style_pad_all(date_time_cont, 0, 0);
    lv_obj_clear_flag(date_time_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    solar_date_label_ = lv_label_create(date_time_cont);
    lv_label_set_text(solar_date_label_, "27/01/2026");
    lv_obj_set_style_text_font(solar_date_label_, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(solar_date_label_, lv_color_hex(0x2D2D44), 0);
    lv_obj_align(solar_date_label_, LV_ALIGN_LEFT_MID, 10, 0);
    
    time_label_ = lv_label_create(date_time_cont);
    lv_label_set_text(time_label_, "17:19:18");
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(0x2D2D44), 0);
    lv_obj_align(time_label_, LV_ALIGN_RIGHT_MID, -10, 0);
    y_pos += 40;
    
    // Row 3: Weather
    lv_obj_t* weather_cont = lv_obj_create(container_);
    lv_obj_set_size(weather_cont, width_ - 20, 40);
    lv_obj_set_pos(weather_cont, 0, y_pos);
    lv_obj_set_style_bg_opa(weather_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_cont, 0, 0);
    lv_obj_clear_flag(weather_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    weather_icon_ = lv_label_create(weather_cont);
    lv_label_set_text(weather_icon_, LV_SYMBOL_IMAGE);  // Will be replaced with actual weather icon
    lv_obj_set_style_text_font(weather_icon_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(weather_icon_, lv_color_hex(0xFFCC00), 0);
    lv_obj_align(weather_icon_, LV_ALIGN_LEFT_MID, 30, 0);
    
    temp_label_ = lv_label_create(weather_cont);
    lv_label_set_text(temp_label_, "29.8 °C");
    lv_obj_set_style_text_font(temp_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(temp_label_, lv_color_hex(0x00CED1), 0);
    lv_obj_align(temp_label_, LV_ALIGN_RIGHT_MID, -30, 0);
    y_pos += 50;
    
    // Row 4: Trực and Tiết khí
    lv_obj_t* truc_tiet_cont = lv_obj_create(container_);
    lv_obj_set_size(truc_tiet_cont, width_ - 20, 30);
    lv_obj_set_pos(truc_tiet_cont, 0, y_pos);
    lv_obj_set_style_bg_opa(truc_tiet_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(truc_tiet_cont, 0, 0);
    lv_obj_clear_flag(truc_tiet_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    truc_label_ = lv_label_create(truc_tiet_cont);
    lv_label_set_text(truc_label_, "Trực: Thành");
    lv_obj_set_style_text_font(truc_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(truc_label_, lv_color_hex(0x2D2D44), 0);
    lv_obj_align(truc_label_, LV_ALIGN_LEFT_MID, 10, 0);
    
    solar_term_label_ = lv_label_create(truc_tiet_cont);
    lv_label_set_text(solar_term_label_, "Tiết Tiểu Hàn");
    lv_obj_set_style_text_font(solar_term_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(solar_term_label_, lv_color_hex(0x2D2D44), 0);
    lv_obj_align(solar_term_label_, LV_ALIGN_RIGHT_MID, -10, 0);
    y_pos += 35;
    
    // Row 5: Giờ Can Chi (in yellow/gold)
    hour_can_chi_label_ = lv_label_create(container_);
    lv_label_set_text(hour_can_chi_label_, "Giờ Đinh Dậu");
    lv_obj_set_style_text_font(hour_can_chi_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hour_can_chi_label_, lv_color_hex(0xDAA520), 0);  // Golden
    lv_obj_align(hour_can_chi_label_, LV_ALIGN_TOP_MID, 0, y_pos);
    y_pos += 30;
    
    // Row 6: Lunar date (large, centered)
    lunar_date_label_ = lv_label_create(container_);
    lv_label_set_text(lunar_date_label_, "Ngày 9 - Tháng 12");
    lv_obj_set_style_text_font(lunar_date_label_, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lunar_date_label_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_align(lunar_date_label_, LV_ALIGN_TOP_MID, 0, y_pos);
    y_pos += 30;
    
    // Row 7: Year Can Chi (in yellow/gold)
    year_can_chi_label_ = lv_label_create(container_);
    lv_label_set_text(year_can_chi_label_, "Năm Ất Tỵ");
    lv_obj_set_style_text_font(year_can_chi_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(year_can_chi_label_, lv_color_hex(0xDAA520), 0);  // Golden
    lv_obj_align(year_can_chi_label_, LV_ALIGN_TOP_MID, 0, y_pos);
    
    ESP_LOGI(TAG, "Lunar calendar UI created");
}

void LunarCalendar::Show() {
    if (container_) {
        Update();
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = true;
        
        // Start auto-update timer
        if (!update_timer_) {
            update_timer_ = lv_timer_create(TimerCallback, 1000, this);
        }
        
        ESP_LOGI(TAG, "Lunar calendar shown");
    }
}

void LunarCalendar::Hide() {
    if (container_) {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = false;
        
        // Stop timer
        if (update_timer_) {
            lv_timer_delete(update_timer_);
            update_timer_ = nullptr;
        }
        
        ESP_LOGI(TAG, "Lunar calendar hidden");
    }
}

void LunarCalendar::Update() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    
    SetDate(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, 
            timeinfo->tm_mday, timeinfo->tm_hour);
}

void LunarCalendar::SetDate(int year, int month, int day, int hour) {
    current_info_ = GetLunarInfo(year, month, day, hour);
    UpdateDisplay();
}

void LunarCalendar::UpdateDisplay() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm* timeinfo = localtime(&tv.tv_sec);
    
    // Weekday
    if (weekday_label_) {
        lv_label_set_text(weekday_label_, THU_VIET[timeinfo->tm_wday]);
    }
    
    // Solar date
    if (solar_date_label_) {
        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d",
                 timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
        lv_label_set_text(solar_date_label_, date_str);
    }
    
    // Time
    if (time_label_) {
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                 timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        lv_label_set_text(time_label_, time_str);
    }
    
    // Trực
    if (truc_label_) {
        lv_label_set_text(truc_label_, current_info_.truc_str);
    }
    
    // Solar term
    if (solar_term_label_) {
        lv_label_set_text(solar_term_label_, current_info_.solar_term_str);
    }
    
    // Hour Can Chi
    if (hour_can_chi_label_) {
        lv_label_set_text(hour_can_chi_label_, current_info_.hour_can_chi);
    }
    
    // Lunar date
    if (lunar_date_label_) {
        lv_label_set_text(lunar_date_label_, current_info_.lunar_date_str);
    }
    
    // Year Can Chi
    if (year_can_chi_label_) {
        lv_label_set_text(year_can_chi_label_, current_info_.year_can_chi);
    }
}

void LunarCalendar::SetWeather(const char* icon, float temperature, const char* city) {
    if (weather_icon_) {
        lv_label_set_text(weather_icon_, icon);
    }
    if (temp_label_) {
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.1f °C", temperature);
        lv_label_set_text(temp_label_, temp_str);
    }
}

void LunarCalendar::TimerCallback(lv_timer_t* timer) {
    LunarCalendar* self = static_cast<LunarCalendar*>(timer->user_data);
    if (self && self->is_visible_) {
        self->Update();
    }
}

// Get Julian day number
int LunarCalendar::GetJulianDay(int year, int month, int day) {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

// Get Can Chi for a Julian day
void LunarCalendar::GetCanChi(int jd, int& stem, int& branch) {
    stem = (jd + 9) % 10;
    branch = (jd + 1) % 12;
}

// Get solar term index for a date
int LunarCalendar::GetSolarTermIndex(int month, int day) {
    for (int i = 0; i < 24; i++) {
        if (TIET_KHI[i].month == month) {
            // Check if within a few days of the solar term
            if (abs(day - TIET_KHI[i].start_day) <= 2) {
                return i;
            }
        }
    }
    
    // Find the current solar term period
    for (int i = 23; i >= 0; i--) {
        if (month > TIET_KHI[i].month || 
            (month == TIET_KHI[i].month && day >= TIET_KHI[i].start_day)) {
            return i;
        }
    }
    return 23;  // Đông Chí (last term of previous year)
}

const char* LunarCalendar::GetStemName(int stem) {
    if (stem >= 0 && stem < 10) {
        return THIEN_CAN[stem];
    }
    return "";
}

const char* LunarCalendar::GetBranchName(int branch) {
    if (branch >= 0 && branch < 12) {
        return DIA_CHI[branch];
    }
    return "";
}

const char* LunarCalendar::GetTruc(int branch) {
    if (branch >= 0 && branch < 12) {
        return TRUC[branch];
    }
    return "";
}

const char* LunarCalendar::GetZodiacEmoji(int branch) {
    const char* zodiac_emojis[] = {
        "🐀", "🐂", "🐅", "🐇", "🐉", "🐍",
        "🐴", "🐐", "🐒", "🐓", "🐕", "🐖"
    };
    if (branch >= 0 && branch < 12) {
        return zodiac_emojis[branch];
    }
    return "";
}

// Simplified lunar date calculation
LunarDateInfo LunarCalendar::GetLunarInfo(int solar_year, int solar_month, int solar_day, int hour) {
    LunarDateInfo info = {};
    
    // For this implementation, we'll use an approximation
    // In production, use a proper lunar calendar library or lookup table
    
    // Julian day for the solar date
    int jd = GetJulianDay(solar_year, solar_month, solar_day);
    
    // Get Can Chi for day
    GetCanChi(jd, info.day_stem, info.day_branch);
    
    // Get Can Chi for hour (Giờ)
    // Hours in Chinese time: 23-1: Tý, 1-3: Sửu, etc.
    int hour_branch = ((hour + 1) / 2) % 12;
    int hour_stem = (info.day_stem * 2 + hour_branch) % 10;
    info.hour_stem = hour_stem;
    info.hour_branch = hour_branch;
    
    // Approximate lunar date (simplified calculation)
    // This is a rough approximation - real implementation needs lunar tables
    int lunar_new_year_jd = GetJulianDay(solar_year, 1, 22);  // Approximate
    int days_since_new_year = jd - lunar_new_year_jd;
    
    if (days_since_new_year < 0) {
        // Previous lunar year
        info.lunar_year = solar_year - 1;
        days_since_new_year += 354;  // Approximate lunar year length
    } else {
        info.lunar_year = solar_year;
    }
    
    // Approximate lunar month and day
    info.lunar_month = (days_since_new_year / 30) + 1;
    if (info.lunar_month > 12) info.lunar_month = 12;
    info.lunar_day = (days_since_new_year % 30) + 1;
    if (info.lunar_day > 30) info.lunar_day = 30;
    
    // Year Can Chi
    int year_offset = (info.lunar_year - 4) % 60;  // 1984 is Giáp Tý
    info.year_stem = year_offset % 10;
    info.year_branch = year_offset % 12;
    
    // Month Can Chi (simplified)
    info.month_stem = (info.year_stem * 2 + info.lunar_month) % 10;
    info.month_branch = (info.lunar_month + 1) % 12;
    
    // Solar term
    info.solar_term_index = GetSolarTermIndex(solar_month, solar_day);
    
    // Build display strings
    snprintf(info.lunar_date_str, sizeof(info.lunar_date_str), 
             "Ngày %d - Tháng %d", info.lunar_day, info.lunar_month);
    
    snprintf(info.year_can_chi, sizeof(info.year_can_chi), 
             "Năm %s %s", GetStemName(info.year_stem), GetBranchName(info.year_branch));
    
    snprintf(info.day_can_chi, sizeof(info.day_can_chi), 
             "Ngày %s %s", GetStemName(info.day_stem), GetBranchName(info.day_branch));
    
    snprintf(info.hour_can_chi, sizeof(info.hour_can_chi), 
             "Giờ %s %s", GetStemName(info.hour_stem), GetBranchName(info.hour_branch));
    
    snprintf(info.truc_str, sizeof(info.truc_str), 
             "Trực: %s", GetTruc(info.day_branch));
    
    if (info.solar_term_index >= 0 && info.solar_term_index < 24) {
        snprintf(info.solar_term_str, sizeof(info.solar_term_str), 
                 "Tiết %s", TIET_KHI[info.solar_term_index].name_vi);
    } else {
        info.solar_term_str[0] = '\0';
    }
    
    return info;
}
