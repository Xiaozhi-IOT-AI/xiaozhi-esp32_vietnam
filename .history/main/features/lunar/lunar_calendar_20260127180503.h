#ifndef LUNAR_CALENDAR_H
#define LUNAR_CALENDAR_H

#include <lvgl.h>
#include <string>
#include <ctime>

/**
 * Vietnamese Lunar Calendar UI
 * 
 * Features:
 * - Display lunar date (Ngày âm lịch)
 * - Solar terms (Tiết khí)
 * - Heavenly stems and Earthly branches (Can Chi)
 * - Vietnamese format with proper diacritics
 * - Weather integration
 * - Portrait mode layout
 */

// Tiết khí (Solar terms) - 24 tiết trong năm
struct SolarTerm {
    int month;      // Tháng dương lịch
    int start_day;  // Ngày bắt đầu (xấp xỉ)
    const char* name_vi;     // Tên tiếng Việt
    const char* description; // Mô tả
};

// Lunar date info
struct LunarDateInfo {
    int lunar_day;
    int lunar_month;
    int lunar_year;
    bool is_leap_month;
    
    // Can Chi
    int day_stem;       // Thiên can của ngày (0-9)
    int day_branch;     // Địa chi của ngày (0-11)
    int month_stem;     // Thiên can của tháng
    int month_branch;   // Địa chi của tháng
    int year_stem;      // Thiên can của năm
    int year_branch;    // Địa chi của năm
    int hour_stem;      // Thiên can của giờ
    int hour_branch;    // Địa chi của giờ
    
    // Solar term
    int solar_term_index;   // -1 if not a solar term day
    
    // Display strings
    char lunar_date_str[64];      // "Ngày 9 - Tháng 12"
    char year_can_chi[32];        // "Năm Ất Tị"
    char day_can_chi[32];         // "Ngày Đinh Dậu"
    char hour_can_chi[32];        // "Giờ Đinh Dậu"
    char solar_term_str[32];      // "Tiết Tiểu Hàn"
    char truc_str[32];            // "Trực: Thành"
};

class LunarCalendar {
public:
    LunarCalendar(lv_obj_t* parent, int width, int height);
    ~LunarCalendar();
    
    // Show/Hide
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    
    // Update display with current date
    void Update();
    void SetDate(int year, int month, int day, int hour = 12);
    
    // Get lunar info for a date
    static LunarDateInfo GetLunarInfo(int solar_year, int solar_month, int solar_day, int hour = 12);
    
    // Get root object
    lv_obj_t* GetRoot() { return container_; }
    
    // Weather integration
    void SetWeather(const char* icon, float temperature, const char* city);
    
private:
    void CreateUI();
    void UpdateDisplay();
    
    // Lunar calendar calculations
    static int SolarToLunarDay(int year, int month, int day);
    static void GetCanChi(int jd, int& stem, int& branch);
    static int GetJulianDay(int year, int month, int day);
    static int GetSolarTermIndex(int month, int day);
    static const char* GetStemName(int stem);
    static const char* GetBranchName(int branch);
    static const char* GetTruc(int branch);
    static const char* GetZodiacEmoji(int branch);
    
    lv_obj_t* parent_;
    lv_obj_t* container_ = nullptr;
    
    // UI Elements
    lv_obj_t* weekday_label_ = nullptr;
    lv_obj_t* solar_date_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* weather_icon_ = nullptr;
    lv_obj_t* temp_label_ = nullptr;
    lv_obj_t* truc_label_ = nullptr;
    lv_obj_t* solar_term_label_ = nullptr;
    lv_obj_t* hour_can_chi_label_ = nullptr;
    lv_obj_t* lunar_date_label_ = nullptr;
    lv_obj_t* year_can_chi_label_ = nullptr;
    lv_obj_t* zodiac_icon_ = nullptr;
    
    int width_;
    int height_;
    bool is_visible_ = false;
    
    // Current date info
    LunarDateInfo current_info_;
    
    // Timer for auto update
    lv_timer_t* update_timer_ = nullptr;
    static void TimerCallback(lv_timer_t* timer);
};

// Thiên Can (Heavenly Stems)
static const char* THIEN_CAN[] = {
    "Giáp", "Ất", "Bính", "Đinh", "Mậu",
    "Kỷ", "Canh", "Tân", "Nhâm", "Quý"
};

// Địa Chi (Earthly Branches) 
static const char* DIA_CHI[] = {
    "Tý", "Sửu", "Dần", "Mão", "Thìn", "Tỵ",
    "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"
};

// Trực (Day quality)
static const char* TRUC[] = {
    "Kiến", "Trừ", "Mãn", "Bình", "Định", "Chấp",
    "Phá", "Nguy", "Thành", "Thu", "Khai", "Bế"
};

// Tiết khí (24 Solar Terms)
static const SolarTerm TIET_KHI[] = {
    {1, 5, "Tiểu Hàn", "Rét nhẹ"},
    {1, 20, "Đại Hàn", "Rét đậm"},
    {2, 4, "Lập Xuân", "Bắt đầu mùa xuân"},
    {2, 19, "Vũ Thủy", "Mưa xuân"},
    {3, 5, "Kinh Trập", "Sâu bọ nở"},
    {3, 20, "Xuân Phân", "Giữa xuân"},
    {4, 4, "Thanh Minh", "Trời trong sáng"},
    {4, 20, "Cốc Vũ", "Mưa rào"},
    {5, 5, "Lập Hạ", "Bắt đầu mùa hè"},
    {5, 21, "Tiểu Mãn", "Lúa nhỏ trổ"},
    {6, 5, "Mang Chủng", "Gieo mạ"},
    {6, 21, "Hạ Chí", "Giữa hè"},
    {7, 7, "Tiểu Thử", "Nóng nhẹ"},
    {7, 22, "Đại Thử", "Nóng gắt"},
    {8, 7, "Lập Thu", "Bắt đầu mùa thu"},
    {8, 23, "Xử Thử", "Hết nóng"},
    {9, 7, "Bạch Lộ", "Sương trắng"},
    {9, 23, "Thu Phân", "Giữa thu"},
    {10, 8, "Hàn Lộ", "Sương lạnh"},
    {10, 23, "Sương Giáng", "Sương rơi"},
    {11, 7, "Lập Đông", "Bắt đầu mùa đông"},
    {11, 22, "Tiểu Tuyết", "Tuyết nhỏ"},
    {12, 7, "Đại Tuyết", "Tuyết lớn"},
    {12, 21, "Đông Chí", "Giữa đông"}
};

// Thứ trong tuần
static const char* THU_VIET[] = {
    "Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", 
    "Thứ Năm", "Thứ Sáu", "Thứ Bảy"
};

#endif // LUNAR_CALENDAR_H
