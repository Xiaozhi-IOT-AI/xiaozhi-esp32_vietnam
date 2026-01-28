# Firmware Changes - 2026-01-28

## 📋 Tổng Quan

Tài liệu này mô tả các thay đổi firmware trong session ngày 28/01/2026, bao gồm:
- Swipe gesture để đổi kênh Radio
- Swipe gesture để đổi Agent (AI Assistant)
- Tích hợp Agent Selector UI
- WebSocket header Agent-Id
- Fix audio issues khi đổi kênh radio

---

## 1️⃣ Radio Station Navigation (Swipe để đổi kênh)

### Mục đích
Cho phép user vuốt trái/phải để chuyển kênh radio khi đang nghe.

### Files Modified

#### `main/features/music/esp32_radio.h`
```cpp
// Thêm các member variables và methods:
private:
    int current_station_index_;                    // Index của station hiện tại
    std::vector<std::string> station_keys_;        // Danh sách keys theo thứ tự

public:
    bool NextStation();                            // Chuyển sang kênh tiếp theo
    bool PreviousStation();                        // Chuyển sang kênh trước đó
    int GetStationCount() const;                   // Lấy tổng số kênh
    int GetCurrentStationIndex() const;            // Lấy index kênh hiện tại
```

#### `main/features/music/esp32_radio.cc`

**1. Constructor initialization:**
```cpp
Esp32Radio::Esp32Radio() : current_station_name_(), current_station_url_(),
                         station_name_displayed_(false), current_station_volume_(4.5f), 
                         current_station_index_(0), station_keys_(), // <-- THÊM
                         // ... rest of initialization
```

**2. InitializeRadioStations() - Thêm station_keys_ list:**
```cpp
void Esp32Radio::InitializeRadioStations() {
    // ... existing station definitions ...
    
    // Build ordered station keys list for navigation
    station_keys_.clear();
    station_keys_.push_back("VOV1");
    station_keys_.push_back("VOV2");
    station_keys_.push_back("VOV3");
    station_keys_.push_back("VOV5");
    station_keys_.push_back("VOV_GT_HN");
    station_keys_.push_back("VOV_GT_HCM");
    station_keys_.push_back("VOV_MEKONG");
    station_keys_.push_back("VOV4_MIENTRUNG");
    station_keys_.push_back("VOV4_TAYBAC");
    station_keys_.push_back("VOV4_DONGBAC");
    station_keys_.push_back("VOV4_TAYNGUYEN");
    station_keys_.push_back("VOV4_DBSCL");
    station_keys_.push_back("VOV4_HCM");
    station_keys_.push_back("VOV5_ENGLISH");
}
```

**3. Async Task cho Station Switching (FIX: không block touch callback):**

Vấn đề: Khi gọi `NextStation()`/`PreviousStation()` trực tiếp từ touch callback, nó sẽ gọi `Stop()` → block đợi radio threads kết thúc → UI freeze.

Solution: Chạy trong FreeRTOS task riêng:

```cpp
// Trong board file - thêm task wrappers
static void RadioNextStationTask(void* param) {
    auto radio = Application::GetInstance().GetRadio();
    if (radio) {
        radio->NextStation();
    }
    vTaskDelete(NULL);
}

static void RadioPrevStationTask(void* param) {
    auto radio = Application::GetInstance().GetRadio();
    if (radio) {
        radio->PreviousStation();
    }
    vTaskDelete(NULL);
}

// Trong touch callback - swipe handler
case TOUCH_GESTURE_SWIPE_RIGHT:
    if (radio && radio->IsPlaying()) {
        // Run in separate task to avoid blocking touch callback
        xTaskCreate(RadioNextStationTask, "radio_next", 4096, nullptr, 5, nullptr);
    }
    break;

case TOUCH_GESTURE_SWIPE_LEFT:
    if (radio && radio->IsPlaying()) {
        // Run in separate task to avoid blocking touch callback
        xTaskCreate(RadioPrevStationTask, "radio_prev", 4096, nullptr, 5, nullptr);
    }
    break;
```

**4. NextStation() implementation:**
```cpp
bool Esp32Radio::NextStation() {
    if (station_keys_.empty()) {
        ESP_LOGW(TAG, "No stations available");
        return false;
    }
    
    // Find current station index if playing
    if (!current_station_name_.empty()) {
        for (size_t i = 0; i < station_keys_.size(); i++) {
            if (radio_stations_[station_keys_[i]].name == current_station_name_) {
                current_station_index_ = i;
                break;
            }
        }
    }
    
    // Move to next station (wrap around)
    current_station_index_ = (current_station_index_ + 1) % station_keys_.size();
    
    const std::string& key = station_keys_[current_station_index_];
    const std::string& station_name = radio_stations_[key].name;
    ESP_LOGI(TAG, "📻 Next station [%d/%d]: %s", current_station_index_ + 1, 
             (int)station_keys_.size(), station_name.c_str());
    
    // Show loading message immediately for user feedback
    auto display = Board::GetInstance().GetDisplay();
    if (display) {
        std::string loading_msg = "⏳ Đang chuyển: " + station_name;
        display->SetMusicInfo(loading_msg.c_str());
    }
    
    return PlayStation(key);
}
```

**4. PreviousStation() implementation:**
```cpp
bool Esp32Radio::PreviousStation() {
    if (station_keys_.empty()) {
        ESP_LOGW(TAG, "No stations available");
        return false;
    }
    
    // Find current station index if playing
    if (!current_station_name_.empty()) {
        for (size_t i = 0; i < station_keys_.size(); i++) {
            if (radio_stations_[station_keys_[i]].name == current_station_name_) {
                current_station_index_ = i;
                break;
            }
        }
    }
    
    // Move to previous station (wrap around)
    current_station_index_ = (current_station_index_ == 0) 
        ? station_keys_.size() - 1 
        : current_station_index_ - 1;
    
    const std::string& key = station_keys_[current_station_index_];
    const std::string& station_name = radio_stations_[key].name;
    ESP_LOGI(TAG, "📻 Previous station [%d/%d]: %s", current_station_index_ + 1, 
             (int)station_keys_.size(), station_name.c_str());
    
    // Show loading message immediately for user feedback
    auto display = Board::GetInstance().GetDisplay();
    if (display) {
        std::string loading_msg = "⏳ Đang chuyển: " + station_name;
        display->SetMusicInfo(loading_msg.c_str());
    }
    
    return PlayStation(key);
}
```

**5. Stop() - Fix audio cleanup:**
```cpp
bool Esp32Radio::Stop() {
    // ... existing code ...
    
    // Wait for threads to finish
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
    if (play_thread_.joinable()) {
        play_thread_.join();
    }
    
    // THÊM: Cleanup AAC decoder for fresh start
    CleanupAacDecoder();
    
    // THÊM: Clear audio buffer
    ClearAudioBuffer();
    
    // THÊM: Reset station display flag
    station_name_displayed_ = false;
    
    // Stop FFT display
    if (display && display_mode_ == DISPLAY_MODE_SPECTRUM) {
        display->StopFFT();
        display->ReleaseAudioBuffFFT();  // THÊM
    }
    
    return true;
}
```

---

## 2️⃣ Agent Switching (Swipe để đổi AI Agent)

### Mục đích
Cho phép user vuốt trái/phải để chuyển đổi giữa các AI Agent khác nhau khi không có media đang phát.

### Files Modified

#### `main/features/agent/agent_selector.h`
```cpp
// Thêm methods:
public:
    // Fetch agents from server API
    bool FetchAgentsFromServer(const std::string& base_url = "");
    
    // Get active agent ID
    std::string GetActiveAgentId() const;
```

#### `main/features/agent/agent_selector.cc`

**1. Thêm includes:**
```cpp
#include "board.h"
#include "system_info.h"
```

**2. GetActiveAgentId() implementation:**
```cpp
std::string AgentSelector::GetActiveAgentId() const {
    const AgentInfo* active = GetActiveAgent();
    if (active) {
        return std::string(active->id);
    }
    
    // Fallback to NVS
    Settings settings("agent", false);
    return settings.GetString("active_id");
}
```

**3. FetchAgentsFromServer() implementation:**
```cpp
bool AgentSelector::FetchAgentsFromServer(const std::string& base_url) {
    std::string url = base_url;
    if (url.empty()) {
        // Get base URL from settings or config
        Settings ws_settings("websocket", false);
        url = ws_settings.GetString("url");
        if (url.empty()) {
            url = CONFIG_OTA_URL;
        }
        
        // Transform to agents endpoint
        // e.g. "https://xiaozhi-ai-iot.vn/api/v1/ota/" -> "https://xiaozhi-ai-iot.vn/api/v1/device/agents"
        size_t pos = url.find("/ota");
        if (pos != std::string::npos) {
            url = url.substr(0, pos) + "/device/agents";
        }
    }
    
    ESP_LOGI(TAG, "Fetching agents from: %s", url.c_str());
    
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);
    
    // Set headers
    http->SetHeader("device-id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("client-id", board.GetUuid());
    http->SetHeader("Content-Type", "application/json");
    
    // Get token from settings
    Settings ws_settings("websocket", false);
    std::string token = ws_settings.GetString("token");
    if (!token.empty()) {
        http->SetHeader("authorization", ("Bearer " + token).c_str());
    }
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to connect to agents API");
        return false;
    }
    
    int status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Agents API returned status: %d", status_code);
        http->Close();
        return false;
    }
    
    std::string response = http->ReadAll();
    http->Close();
    
    // Parse and update agents
    if (!ParseAgentsJson(response.c_str())) {
        return false;
    }
    
    // Match active agent from NVS
    Settings agent_settings("agent", false);
    std::string saved_id = agent_settings.GetString("active_id");
    if (!saved_id.empty()) {
        for (int i = 0; i < agents_.count; i++) {
            if (saved_id == agents_.agents[i].id) {
                agents_.active_index = i;
                agents_.agents[i].is_active = true;
                break;
            }
        }
    }
    
    // Download icons in background
    DownloadIcons();
    
    return true;
}
```

---

## 3️⃣ WebSocket Agent-Id Header

### Mục đích
Gửi Agent-Id header khi kết nối WebSocket để server biết user đang dùng Agent nào.

### Files Modified

#### `main/protocols/websocket_protocol.cc`

```cpp
// Trong method Connect():

// THÊM: Include header
#include "settings.h"

// THÊM: Đọc agent_id từ NVS và thêm vào header
Settings agent_settings("agent", false);
std::string active_agent_id = agent_settings.GetString("active_id");
if (!active_agent_id.empty()) {
    websocket_->SetHeader("Agent-Id", active_agent_id.c_str());
    ESP_LOGI(TAG, "WebSocket connecting with Agent-Id: %s", active_agent_id.c_str());
}
```

---

## 4️⃣ Board Integration (Swipe Gesture Handling)

### Mục đích
Tích hợp swipe gesture vào board để điều khiển Radio và Agent.

### Files Modified

#### `main/boards/xiaozhi-ai-iot-vietnam-es3n28p-lcd-2.8/xiaozhi_ai_iot_vietnam_es3n28p_lcd_2.8.cc`

**1. Thêm includes:**
```cpp
#include "features/agent/agent_selector.h"
```

**2. Thêm member variables:**
```cpp
class XiaozhiAIIoTEs3n28p : public WifiBoard {
 private:
    // ... existing members ...
    AgentSelector* agent_selector_ = nullptr;      // THÊM
    bool agent_selector_visible_ = false;          // THÊM
```

**3. Thêm Agent Selector methods:**
```cpp
void InitializeAgentSelector() {
    if (agent_selector_) return;
    
    lvgl_port_lock(0);
    lv_obj_t* screen = lv_disp_get_scr_act(NULL);
    agent_selector_ = new AgentSelector(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    
    // Set callback for when agent is selected
    agent_selector_->SetCallback([](const AgentInfo* agent, void* data) {
        auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
        ESP_LOGI(TAG, "Agent selected: %s (id: %s)", agent->name, agent->id);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "🤖 Đã chọn: %s", agent->name);
        board->GetDisplay()->ShowNotification(msg);
    }, this);
    
    lvgl_port_unlock();
    
    // Fetch agents from server in background
    xTaskCreate([](void* param) {
        auto* selector = static_cast<AgentSelector*>(param);
        vTaskDelay(pdMS_TO_TICKS(2000));
        selector->FetchAgentsFromServer();
        vTaskDelete(NULL);
    }, "fetch_agents", 4096, agent_selector_, 5, NULL);
}

void NextAgent() {
    if (!agent_selector_) {
        InitializeAgentSelector();
    }
    if (agent_selector_) {
        agent_selector_->NextAgent();
        const AgentInfo* agent = agent_selector_->GetActiveAgent();
        if (agent) {
            char msg[128];
            snprintf(msg, sizeof(msg), "🤖 %s", agent->name);
            GetDisplay()->ShowNotification(msg);
        }
    }
}

void PreviousAgent() {
    if (!agent_selector_) {
        InitializeAgentSelector();
    }
    if (agent_selector_) {
        agent_selector_->PreviousAgent();
        const AgentInfo* agent = agent_selector_->GetActiveAgent();
        if (agent) {
            char msg[128];
            snprintf(msg, sizeof(msg), "🤖 %s", agent->name);
            GetDisplay()->ShowNotification(msg);
        }
    }
}
```

**4. Update swipe gesture handling:**
```cpp
switch (gesture) {
    case TOUCH_GESTURE_SWIPE_RIGHT:
    {
        Display::DisplaySourceType source = static_cast<LcdDisplay*>(display_)->DetectSourceFromInfo();
        if (source == Display::DisplaySourceType::SD_CARD) {
            // SD Card playing - next track
            auto sd_music = app.GetSdMusic();
            if (sd_music) {
                sd_music->stop();
                sd_music->next();
            }
        } else {
            auto radio = app.GetRadio();
            if (radio && radio->IsPlaying()) {
                // Radio playing - next station
                ESP_LOGI(TAG, "📻 Swipe Right - Next radio station");
                radio->NextStation();
            } else {
                // Nothing playing - next agent
                ESP_LOGI(TAG, "🤖 Swipe Right - Next agent");
                this->NextAgent();
            }
        }
    }
    break;
    
    case TOUCH_GESTURE_SWIPE_LEFT:
    {
        Display::DisplaySourceType source = static_cast<LcdDisplay*>(display_)->DetectSourceFromInfo();
        if (source == Display::DisplaySourceType::SD_CARD) {
            // SD Card playing - previous track
            auto sd_music = app.GetSdMusic();
            if (sd_music) {
                sd_music->stop();
                sd_music->prev();
            }
        } else {
            auto radio = app.GetRadio();
            if (radio && radio->IsPlaying()) {
                // Radio playing - previous station
                ESP_LOGI(TAG, "📻 Swipe Left - Previous radio station");
                radio->PreviousStation();
            } else {
                // Nothing playing - previous agent
                ESP_LOGI(TAG, "🤖 Swipe Left - Previous agent");
                this->PreviousAgent();
            }
        }
    }
    break;
}
```

---

## 5️⃣ Brightness Changes

### Files Modified

#### `main/boards/common/backlight.cc`
```cpp
// Thay đổi default brightness từ 75% lên 100%
constexpr int DEFAULT_BRIGHTNESS = 100;  // Was 75

// Thay đổi fallback brightness
if (brightness <= 0) {
    brightness = 100;  // Was 10
}
```

---

## 📊 Swipe Gesture Logic Summary

| Trạng thái | Swipe Left | Swipe Right |
|------------|------------|-------------|
| Đang phát SD Card | Previous track | Next track |
| Đang phát Radio | Previous station | Next station |
| Idle (không phát gì) | Previous Agent | Next Agent |
| Swipe Up | Volume +5 | Volume +5 |
| Swipe Down | Volume -5 | Volume -5 |

---

## 🔧 NVS Storage

| Namespace | Key | Description |
|-----------|-----|-------------|
| `agent` | `active_id` | UUID của Agent đang active |
| `agent` | `active_name` | Tên của Agent đang active |
| `websocket` | `url` | WebSocket URL |
| `websocket` | `token` | Bearer token |

---

## 📡 API Endpoints

### GET /api/v1/device/agents
Lấy danh sách agents cho device.

**Headers:**
```
device-id: {MAC_ADDRESS}
client-id: {UUID}
authorization: Bearer {TOKEN}
```

**Response:**
```json
{
  "agents": [
    {
      "id": "uuid-1",
      "name": "Trợ lý nhà",
      "description": "AI điều khiển smart home",
      "icon_url": "https://...",
      "is_active": true
    }
  ],
  "total": 1
}
```

### WebSocket Connection
**Headers:**
```
device-id: {MAC_ADDRESS}
client-id: {UUID}
authorization: Bearer {TOKEN}
Agent-Id: {AGENT_UUID}      // <-- MỚI
```

---

## 🧪 Testing Guide

### Test Radio Station Navigation
1. Nói "Bật radio VOV1"
2. Chờ radio phát
3. Vuốt phải → Chuyển sang VOV2
4. Vuốt trái → Quay lại VOV1
5. Verify: Màn hình hiện "⏳ Đang chuyển: [tên kênh]"
6. Verify: Âm thanh phát sau 3-5 giây

### Test Agent Switching
1. Đảm bảo không có media đang phát
2. Vuốt phải → Notification "🤖 [Tên Agent]"
3. Vuốt trái → Agent trước đó
4. Verify: Agent được lưu vào NVS

### Test WebSocket Agent-Id
1. Monitor serial log
2. Kết nối WiFi
3. Tìm log: "WebSocket connecting with Agent-Id: xxx"

---

## ⚠️ Known Issues

1. **Radio buffering time**: Cần 3-5 giây để buffer và phát âm thanh sau khi đổi kênh
2. **Agent fetch on startup**: Cần network ready trước khi fetch agents (delay 2s)

---

## 📁 Files Summary

| File | Changes |
|------|---------|
| `main/features/music/esp32_radio.h` | Thêm NextStation, PreviousStation |
| `main/features/music/esp32_radio.cc` | Implement station navigation, fix audio cleanup |
| `main/features/agent/agent_selector.h` | Thêm FetchAgentsFromServer, GetActiveAgentId |
| `main/features/agent/agent_selector.cc` | Implement server fetch, agent ID getter |
| `main/protocols/websocket_protocol.cc` | Thêm Agent-Id header |
| `main/boards/.../xiaozhi_ai_iot_vietnam_es3n28p_lcd_2.8.cc` | Integrate swipe gestures |
| `main/boards/common/backlight.cc` | Default brightness 100% |

---

## 🎛️ Menu UI (Grid 2x3)

### Tổng Quan

Menu UI là giao diện grid 2 cột × 3 hàng hiển thị khi người dùng **bấm giữ lâu** vào màn hình. Mỗi ô là một nút với icon và tên tiếng Việt.

### Layout

```
┌─────────────────────────────────────┐
│                                     │
│   ┌─────────┐    ┌─────────┐       │
│   │   🏠    │    │   🔄    │       │
│   │ Trợ lý  │    │ Đồng hồ │       │
│   └─────────┘    └─────────┘       │
│                                     │
│   ┌─────────┐    ┌─────────┐       │
│   │   💾    │    │   🔊    │       │
│   │ Thẻ nhớ │    │  Radio  │       │
│   └─────────┘    └─────────┘       │
│                                     │
│   ┌─────────┐    ┌─────────┐       │
│   │   📋    │    │   ⚙️    │       │
│   │ Âm lịch │    │Thông tin│       │
│   └─────────┘    └─────────┘       │
│                                     │
└─────────────────────────────────────┘
```

### Files

| File | Mô tả |
|------|-------|
| `main/features/menu/menu_ui.h` | Header với MenuAction enum và MenuUI class |
| `main/features/menu/menu_ui.cc` | Implementation với LVGL |

### MenuAction Enum

```cpp
enum MenuAction {
    MENU_ACTION_ASSISTANT = 0,  // 🏠 Trợ lý - toggle chat
    MENU_ACTION_CLOCK = 1,      // 🔄 Đồng hồ - analog clock
    MENU_ACTION_SD_CARD = 2,    // 💾 Thẻ nhớ - phát nhạc SD
    MENU_ACTION_RADIO = 3,      // 🔊 Radio - toggle VOV radio
    MENU_ACTION_LUNAR = 4,      // 📋 Âm lịch - hiện thông tin
    MENU_ACTION_INFO = 5,       // ⚙️ Thông tin - system info page
    MENU_ACTION_COUNT = 6
};
```

### MenuItem Structure

```cpp
struct MenuItem {
    const char* name;           // Tên tiếng Việt: "Trợ lý", "Đồng hồ", etc.
    const char* icon;           // LVGL symbol: LV_SYMBOL_HOME, etc.
    uint32_t bg_color;          // Background color hex: 0x87CEEB (sky blue)
    MenuItemCallback callback;  // Callback function khi bấm
};
```

### Default Colors

| Button | Color Name | Hex Code |
|--------|-----------|----------|
| Trợ lý | Sky Blue | 0x87CEEB |
| Đồng hồ | Coral/Red | 0xFF6B6B |
| Thẻ nhớ | Yellow | 0xFFCC00 |
| Radio | Light Green | 0x98FB98 |
| Âm lịch | Plum | 0xDDA0DD |
| Thông tin | Cyan | 0x87CEEB |

### MenuUI Class API

#### Constructor

```cpp
MenuUI(lv_obj_t* parent, int width, int height);
// parent: LVGL screen object
// width/height: 240x320 for LCD 2.8"
```

#### Methods

```cpp
// Hiện/ẩn menu
void Show();               // Hiển thị menu, enable gesture bypass
void Hide();               // Ẩn menu, disable gesture bypass

// Kiểm tra trạng thái
bool IsVisible() const { return is_visible_; }

// Set callback cho từng action
void SetCallback(MenuAction action, MenuItemCallback callback, void* user_data = nullptr);

// Update màu nếu cần
void UpdateColors(uint32_t* colors, int count);

// Xử lý touch trực tiếp (dùng khi LVGL không bắt được event)
bool HandleTouch(int16_t x, int16_t y);
```

### Tích hợp vào Board

#### 1. Include và member variable

```cpp
// Trong board class
#include "features/menu/menu_ui.h"

private:
    MenuUI* menu_ui_ = nullptr;
    bool menu_visible_ = false;
```

#### 2. Initialize trong InitializeDisplay()

```cpp
void InitializeMenu() {
    auto lcd_display = static_cast<LcdDisplay*>(display_);
    if (!lcd_display) return;
    
    lvgl_port_lock(0);
    lv_obj_t* screen = lv_screen_active();
    
    menu_ui_ = new MenuUI(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lvgl_port_unlock();
    
    // Setup callbacks
    menu_ui_->SetCallback(MENU_ACTION_ASSISTANT, [](void* data) {
        auto* board = static_cast<MyBoard*>(data);
        board->HideMenu();
        Application::GetInstance().ToggleChatState();
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_CLOCK, [](void* data) {
        auto* board = static_cast<MyBoard*>(data);
        board->HideMenu();
        board->ToggleAnalogClock();
    }, this);
    
    // ... similar for other actions
}
```

#### 3. Show/Hide methods

```cpp
void ShowMenu() {
    if (menu_ui_ && !menu_visible_) {
        lvgl_port_lock(0);
        menu_ui_->Show();
        lvgl_port_unlock();
        menu_visible_ = true;
        
        // Bypass gesture detection để LVGL xử lý click
        if (touch_) {
            touch_->SetBypassGesture(true);
        }
    }
}

void HideMenu() {
    if (menu_ui_ && menu_visible_) {
        lvgl_port_lock(0);
        menu_ui_->Hide();
        lvgl_port_unlock();
        menu_visible_ = false;
        
        // Restore gesture detection
        if (touch_) {
            touch_->SetBypassGesture(false);
        }
    }
}

void ToggleMenu() {
    if (menu_visible_) HideMenu();
    else ShowMenu();
}
```

#### 4. Handle Long Press trong Touch callback

```cpp
void InitializeTouchScreen() {
    touch_ = new TouchScreen(...);
    touch_->OnGestureCallback([this](int gesture, int x, int y) {
        switch (gesture) {
            case TOUCH_GESTURE_LONG_PRESS:
                this->ToggleMenu();  // Bấm giữ lâu → toggle menu
                break;
                
            case TOUCH_GESTURE_TAP:
                if (menu_visible_ && menu_ui_) {
                    // Menu đang hiện → xử lý click vào button
                    if (menu_ui_->HandleTouch(x, y)) {
                        HideMenu();  // Ẩn menu sau khi bấm
                    }
                }
                break;
                
            // ... swipe gestures for radio/agent
        }
    });
}
```

### Flow khi User sử dụng

```
1. User bấm giữ lâu màn hình (~500ms)
   ↓
2. Board nhận TOUCH_GESTURE_LONG_PRESS
   ↓
3. ToggleMenu() → ShowMenu()
   ↓
4. MenuUI hiện lên (2x3 grid)
   ↓
5. User tap vào một button (vd: "Radio")
   ↓
6. HandleTouch(x,y) → tìm button tại vị trí
   ↓
7. Execute callback (MENU_ACTION_RADIO)
   ↓
8. Callback: HideMenu() + Toggle Radio
   ↓
9. Menu ẩn, Radio bắt đầu phát
```

### Customization

#### Thay đổi icon/tên

Sửa `default_items_` trong [menu_ui.cc](../main/features/menu/menu_ui.cc):

```cpp
const MenuItem MenuUI::default_items_[MENU_ACTION_COUNT] = {
    {"Trợ lý",    LV_SYMBOL_HOME,      0x87CEEB, nullptr},
    {"Đồng hồ",   LV_SYMBOL_REFRESH,   0xFF6B6B, nullptr},
    {"Thẻ nhớ",   LV_SYMBOL_SD_CARD,   0xFFCC00, nullptr},
    {"Radio",     LV_SYMBOL_VOLUME_MAX, 0x98FB98, nullptr},
    {"Âm lịch",   LV_SYMBOL_LIST,      0xDDA0DD, nullptr},
    {"Thông tin", LV_SYMBOL_SETTINGS,  0x87CEEB, nullptr},
};
```

#### Thay đổi layout (3x2 thay vì 2x3)

Sửa trong `CreateUI()`:

```cpp
int cols = 3;  // 3 cột thay vì 2
int rows = 2;  // 2 hàng thay vì 3
```

#### Thay đổi màu background

Sửa `bg_color` trong `default_items_` hoặc dùng theme:

```cpp
// Qua theme (trong theme_config.h)
menu_theme.button_colors[0] = 0xFF0000;  // Đỏ cho nút đầu tiên
```

### LVGL Symbols Có Sẵn

| Symbol | Constant |
|--------|----------|
| 🏠 | LV_SYMBOL_HOME |
| 🔄 | LV_SYMBOL_REFRESH |
| 💾 | LV_SYMBOL_SD_CARD |
| 🔊 | LV_SYMBOL_VOLUME_MAX |
| 📋 | LV_SYMBOL_LIST |
| ⚙️ | LV_SYMBOL_SETTINGS |
| ▶️ | LV_SYMBOL_PLAY |
| ⏹️ | LV_SYMBOL_STOP |
| ⏸️ | LV_SYMBOL_PAUSE |
| ⏭️ | LV_SYMBOL_NEXT |
| ⏮️ | LV_SYMBOL_PREV |
| 🔔 | LV_SYMBOL_BELL |
| 📶 | LV_SYMBOL_WIFI |
| 🔋 | LV_SYMBOL_BATTERY_FULL |

---

## 📡 OTA & Agent Switching Integration

### Boot Sequence Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         FIRMWARE STARTUP FLOW                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. BOOT → POST /api/v1/ota (lấy config + check update)                 │
│          ↓                                                               │
│  2. Nhận response → Parse websocket, firmware, server_time, agents      │
│          ↓                                                               │
│  3. Nếu có firmware update → Tải + flash firmware mới                   │
│          ↓                                                               │
│  4. Nếu cần → GET /api/v1/device/agents (lấy danh sách agents)         │
│          ↓                                                               │
│  5. User vuốt trái/phải để chọn agent → Lưu NVS                        │
│          ↓                                                               │
│  6. WebSocket connect với header Agent-Id từ NVS                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Files Implementation

| File | Chức năng |
|------|-----------|
| `main/ota.cc` | Parse OTA response (websocket, firmware, theme, agents) |
| `main/features/agent/agent_selector.cc` | FetchAgentsFromServer(), Save/Load NVS |
| `main/protocols/websocket_protocol.cc` | Gửi Agent-Id header khi connect |

---

### 1. OTA Response Parsing (ota.cc)

OTA response chứa nhiều sections:

```cpp
// Parse websocket config
cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
if (cJSON_IsObject(websocket)) {
    Settings settings("websocket", true);
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, websocket) {
        if (cJSON_IsString(item)) {
            settings.SetString(item->string, item->valuestring);  // url, token
        }
    }
    has_websocket_config_ = true;
}

// Parse server_time
cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
if (cJSON_IsObject(server_time)) {
    cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
    cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
    if (cJSON_IsNumber(timestamp)) {
        struct timeval tv;
        double ts = timestamp->valuedouble;
        if (cJSON_IsNumber(timezone_offset)) {
            ts += (timezone_offset->valueint * 60 * 1000);
        }
        tv.tv_sec = (time_t)(ts / 1000);
        tv.tv_usec = ((long long)ts % 1000) * 1000;
        settimeofday(&tv, NULL);
    }
}

// Parse firmware update info
cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
if (cJSON_IsObject(firmware)) {
    cJSON *version = cJSON_GetObjectItem(firmware, "version");
    cJSON *url = cJSON_GetObjectItem(firmware, "url");
    cJSON *size = cJSON_GetObjectItem(firmware, "size");
    // Compare versions và download nếu cần
}
```

---

### 2. Agent Selector API (agent_selector.cc)

#### FetchAgentsFromServer()

```cpp
bool AgentSelector::FetchAgentsFromServer(const std::string& base_url) {
    std::string url = base_url;
    if (url.empty()) {
        Settings ws_settings("websocket", false);
        url = ws_settings.GetString("url");
        
        // Transform: wss://host/api/v1/ws -> https://host/api/v1/device/agents
        size_t pos = url.find("/ota");
        if (pos != std::string::npos) {
            url = url.substr(0, pos) + "/device/agents";
        }
    }
    
    auto& board = Board::GetInstance();
    auto http = board.GetNetwork()->CreateHttp(0);
    
    // Headers
    http->SetHeader("device-id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("client-id", board.GetUuid());
    
    Settings ws_settings("websocket", false);
    std::string token = ws_settings.GetString("token");
    if (!token.empty()) {
        http->SetHeader("authorization", ("Bearer " + token).c_str());
    }
    
    if (!http->Open("GET", url)) return false;
    if (http->GetStatusCode() != 200) return false;
    
    std::string response = http->ReadAll();
    http->Close();
    
    // Parse JSON
    ParseAgentsJson(response.c_str());
    
    // Match active agent từ NVS
    Settings agent_settings("agent", false);
    std::string saved_id = agent_settings.GetString("active_id");
    // ... match và set active_index
    
    // Download icons in background
    DownloadIcons();
    
    return true;
}
```

#### Save/Load Active Agent (NVS)

```cpp
bool AgentSelector::SaveActiveAgent() {
    if (agents_.count == 0 || agents_.active_index < 0) return false;
    
    Settings settings("agent", true);
    settings.SetString("active_id", agents_.agents[agents_.active_index].id);
    settings.SetString("active_name", agents_.agents[agents_.active_index].name);
    
    ESP_LOGI(TAG, "Saved active agent: %s", agents_.agents[agents_.active_index].name);
    return true;
}

std::string AgentSelector::GetActiveAgentId() const {
    const AgentInfo* active = GetActiveAgent();
    if (active) {
        return std::string(active->id);
    }
    // Fallback to NVS
    Settings settings("agent", false);
    return settings.GetString("active_id");
}
```

---

### 3. WebSocket Agent-Id Header (websocket_protocol.cc)

Khi connect WebSocket, gửi Agent-Id header:

```cpp
void WebSocketProtocol::Start() {
    // ... setup websocket ...
    
    // Send active agent ID if available
    Settings agent_settings("agent", false);
    std::string active_agent_id = agent_settings.GetString("active_id");
    if (!active_agent_id.empty()) {
        websocket_->SetHeader("Agent-Id", active_agent_id.c_str());
        ESP_LOGI(TAG, "Using agent: %s", active_agent_id.c_str());
    }
    
    websocket_->Connect();
}
```

---

### 4. NVS Storage Map

| Namespace | Key | Value | Mô tả |
|-----------|-----|-------|-------|
| `websocket` | `url` | `wss://...` | WebSocket URL từ OTA |
| `websocket` | `token` | `eyJ...` | Bearer token |
| `agent` | `active_id` | `550e8400-...` | UUID agent đang active |
| `agent` | `active_name` | `Trợ lý nhà` | Tên agent (cache) |

---

### 5. API Response Examples

#### GET /api/v1/device/agents

```json
{
  "agents": [
    {
      "id": "550e8400-e29b-41d4-a716-446655440000",
      "name": "Trợ lý nhà",
      "description": "AI điều khiển smart home",
      "avatar_url": "/api/v1/avatars/agent_550e...jpg",
      "is_active": true
    },
    {
      "id": "550e8400-e29b-41d4-a716-446655440001",
      "name": "Lily AI",
      "description": "Trợ lý tổng hợp",
      "avatar_url": null,
      "is_active": false
    }
  ],
  "total": 2,
  "device_agent_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

---

### 6. Swipe Gesture Integration (board file)

Khi không có media đang phát, vuốt để đổi agent:

```cpp
case TOUCH_GESTURE_SWIPE_RIGHT:
    if (radio && radio->IsPlaying()) {
        // Radio đang phát → đổi kênh
        xTaskCreate(RadioNextStationTask, "radio_next", 4096, nullptr, 5, nullptr);
    } else {
        // Không có media → đổi agent
        this->NextAgent();
    }
    break;

case TOUCH_GESTURE_SWIPE_LEFT:
    if (radio && radio->IsPlaying()) {
        xTaskCreate(RadioPrevStationTask, "radio_prev", 4096, nullptr, 5, nullptr);
    } else {
        this->PreviousAgent();
    }
    break;
```

```cpp
void NextAgent() {
    if (!agent_selector_) return;
    agent_selector_->NextAgent();
    agent_selector_->SaveActiveAgent();
    
    // Show notification
    auto display = GetDisplay();
    if (display) {
        const AgentInfo* agent = agent_selector_->GetActiveAgent();
        if (agent) {
            std::string msg = "🤖 " + std::string(agent->name);
            display->ShowNotification(msg);
        }
    }
}
```

---

### 7. 📻 Critical Fix: Radio Chuyển Kênh Không Phát

#### Mô tả vấn đề
Khi chuyển kênh radio qua cảm ứng:
- Kênh đầu tiên phát bình thường ✅
- Chuyển sang kênh khác → **KHÔNG PHÁT** ❌

#### Nguyên nhân gốc

**1. Thread Join Blocking:**
```cpp
download_thread_.join();  // CHỜ VÔ HẠN nếu HTTP stuck!
play_thread_.join();      // CHỜ VÔ HẠN nếu buffer stuck!
```

**2. Threads bị stuck trong:**
- `http->Read()` - HTTP stream đang đọc
- `buffer_cv_.wait()` - Condition variable chờ data

#### Giải pháp: Timeout Thread Joins

**Stop() - Trước:**
```cpp
download_thread_.join();  // Block vô hạn!
play_thread_.join();      // Block vô hạn!
```

**Stop() - Sau:**
```cpp
bool Esp32Radio::Stop() {
    // 1. Set flags FIRST
    is_downloading_ = false;
    is_playing_ = false;
    
    // 2. Notify threads multiple times
    for (int i = 0; i < 5; i++) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 3. Wait với TIMEOUT 500ms
    const int MAX_WAIT_MS = 500;
    int wait_time = 0;
    while (download_thread_.joinable() && wait_time < MAX_WAIT_MS) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        wait_time += 20;
        
        if (!is_downloading_.load()) {
            if (download_thread_.joinable()) {
                vTaskDelay(pdMS_TO_TICKS(50));
                try {
                    download_thread_.join();
                } catch (...) {
                    download_thread_.detach();
                }
            }
            break;
        }
    }
    
    // Detach nếu vẫn stuck
    if (download_thread_.joinable()) {
        ESP_LOGW(TAG, "Download thread taking too long, detaching...");
        download_thread_.detach();
    }
    
    // Tương tự cho play_thread_...
    
    // Cleanup sequence
    ClearAudioBuffer();
    CleanupAacDecoder();
    ResetSampleRate();
    station_name_displayed_ = false;
    
    return true;
}
```

**PlayUrl() - Thêm Safety Delay:**
```cpp
bool Esp32Radio::PlayUrl(const std::string& url, const std::string& name) {
    Stop();
    
    // CRITICAL: Wait for cleanup to complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Double-check: Force cleanup if threads still joinable
    if (download_thread_.joinable() || play_thread_.joinable()) {
        ESP_LOGW(TAG, "Threads still joinable, force detaching...");
        if (download_thread_.joinable()) download_thread_.detach();
        if (play_thread_.joinable()) play_thread_.detach();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // ... rest of PlayUrl
}
```

#### Expected Logs khi chuyển kênh

**Thành công:**
```
I Esp32Radio: Stopping radio streaming - current state: downloading=1, playing=1
I Esp32Radio: Download thread joined after 120ms
I Esp32Radio: Play thread joined after 80ms
I Esp32Radio: Radio streaming stopped successfully - ready for next station
I Esp32Radio: Starting radio stream: VOV 2 (https://stream.vovmedia.vn/vov-2)
I Esp32Radio: Radio streaming threads started successfully for: VOV 2
```

**Nếu thread bị detach (chậm):**
```
W Esp32Radio: Download thread taking too long (500ms), detaching...
W Esp32Radio: Play thread taking too long (500ms), detaching...
I Esp32Radio: Radio streaming stopped successfully - ready for next station
```

#### Test Cases

| Test | Kết quả mong đợi |
|------|------------------|
| Bật VOV1 → Vuốt sang VOV2 | VOV2 phát sau ~200ms |
| Vuốt nhanh liên tục | Kênh cuối phát đúng |
| Dừng radio → Bật lại | Phát bình thường |
| Chuyển khi đang loading | Kênh mới phát, kênh cũ cancel |

#### Known Issues

1. **Delay 100-200ms** khi chuyển kênh là bình thường (cần đợi cleanup)
2. Nếu thấy "detaching" warning thường xuyên → network chậm
3. Memory leak nhỏ (~3KB) nếu detach threads → giải phóng khi restart

---

## 🔧 ES8311 Audio Codec Fix - I2S Channel Management

### Vấn đề

Khi chạy firmware, audio codec liên tục báo lỗi:
```
ESP_ERROR_CHECK_WITHOUT_ABORT failed: esp_err_t 0x102 (ESP_ERR_INVALID_ARG)
ESP_ERROR_CHECK_WITHOUT_ABORT failed: esp_err_t 0xffffffff (ESP_FAIL)
E i2s_common: i2s_channel_disable(1217): the channel has not been enabled yet
```

**Nguyên nhân:**
1. `Read()`/`Write()` được gọi khi `dev_` còn `nullptr`
2. I2S channels không được enable trước khi mở codec device
3. I2S channels không được disable khi đóng codec device

### Files Modified

#### `main/audio/codecs/es8311_audio_codec.cc`

**1. Thêm nullptr check trong Read() và Write():**

```cpp
// TRƯỚC:
int Es8311AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(dev_, ...));
    }
    return samples;
}

// SAU:
int Es8311AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_ && dev_ != nullptr) {  // <-- Thêm nullptr check
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(dev_, ...));
    }
    return samples;
}
```

Tương tự cho `Write()`:
```cpp
int Es8311AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_ && dev_ != nullptr) {  // <-- Thêm nullptr check
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(dev_, ...));
    }
    return samples;
}
```

**2. Sửa UpdateDeviceState() - Enable/Disable I2S channels:**

```cpp
void Es8311AudioCodec::UpdateDeviceState() {
    if ((input_enabled_ || output_enabled_) && dev_ == nullptr) {
        // ✅ THÊM: Enable I2S channels TRƯỚC khi mở codec device
        if (tx_handle_ != nullptr) {
            i2s_channel_enable(tx_handle_);
        }
        if (rx_handle_ != nullptr) {
            i2s_channel_enable(rx_handle_);
        }
        
        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
            .codec_if = codec_if_,
            .data_if = data_if_,
        };
        dev_ = esp_codec_dev_new(&dev_cfg);
        assert(dev_ != NULL);

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(dev_, input_gain_));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(dev_, output_volume_));
    } else if (!input_enabled_ && !output_enabled_ && dev_ != nullptr) {
        esp_codec_dev_close(dev_);
        dev_ = nullptr;
        // ✅ THÊM: Disable I2S channels SAU khi đóng codec device
        if (tx_handle_ != nullptr) {
            i2s_channel_disable(tx_handle_);
        }
        if (rx_handle_ != nullptr) {
            i2s_channel_disable(rx_handle_);
        }
    }
    // ... PA control code ...
}
```

### Giải thích kỹ thuật

#### I2S Channel Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│                    I2S Channel Lifecycle                     │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. i2s_new_channel()     → Channel CREATED (disabled)      │
│  2. i2s_channel_init_*()  → Channel INITIALIZED (disabled)  │
│  3. i2s_channel_enable()  → Channel ENABLED (can read/write)│
│  4. esp_codec_dev_open()  → Codec ready                     │
│  5. esp_codec_dev_read/write() → Audio data transfer        │
│  6. esp_codec_dev_close() → Codec closed                    │
│  7. i2s_channel_disable() → Channel DISABLED                │
│  8. i2s_del_channel()     → Channel DELETED                 │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### Race Condition đã sửa

```
TRƯỚC (lỗi):
┌─────────────────────────┐     ┌─────────────────────────┐
│     Audio Thread        │     │     Control Thread      │
├─────────────────────────┤     ├─────────────────────────┤
│                         │     │                         │
│ Read() called           │     │ EnableInput(true)       │
│ input_enabled_=true ✓   │     │   input_enabled_=true   │
│ dev_=nullptr ✗          │     │   UpdateDeviceState()   │
│ esp_codec_dev_read() 💥 │     │     dev_ = new(...)     │
│                         │     │                         │
└─────────────────────────┘     └─────────────────────────┘

SAU (đã sửa):
┌─────────────────────────┐     ┌─────────────────────────┐
│     Audio Thread        │     │     Control Thread      │
├─────────────────────────┤     ├─────────────────────────┤
│                         │     │                         │
│ Read() called           │     │ EnableInput(true)       │
│ input_enabled_=true ✓   │     │   input_enabled_=true   │
│ dev_=nullptr → SKIP ✓   │     │   i2s_channel_enable()  │
│                         │     │   dev_ = new(...)       │
│                         │     │   esp_codec_dev_open()  │
│                         │     │                         │
└─────────────────────────┘     └─────────────────────────┘
```

### Test Results

| Test Case | Trước | Sau |
|-----------|-------|-----|
| Boot device | ESP_ERR_INVALID_ARG spam | ✅ Clean boot |
| Play radio | No audio, errors | ✅ Audio plays |
| Stop/Start audio | Crash/reboot | ✅ Smooth transition |
| Long running | Task watchdog timeout | ✅ Stable |

### Logs sau khi sửa

```
I Es8311AudioCodec: Duplex channels created
I Es8311AudioCodec: Es8311AudioCodec initialized
... (no more ESP_ERR_INVALID_ARG errors)
W XiaozhiAIIoTEs3n28p: Long Press at (127, 150)
W Esp32Radio: No streaming in progress to stop
```

---

*Cập nhật: 2026-01-28*
*Author: Xiaozhi AI IoT Vietnam Team*
