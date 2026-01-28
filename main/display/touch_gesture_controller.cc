#include "touch_gesture_controller.h"
#include "../application.h"
#include "../features/music/esp32_radio.h"
#include <esp_log.h>

static const char* TAG = "TouchGestureCtrl";

TouchGestureController* TouchGestureController::instance_ = nullptr;

TouchGestureController::TouchGestureController() {
    instance_ = this;
    ESP_LOGI(TAG, "Touch Gesture Controller initialized");
}

void TouchGestureController::SetScreenMode(ScreenMode mode) {
    if (mode != current_mode_) {
        previous_mode_ = current_mode_;
        current_mode_ = mode;
        
        const char* mode_names[] = {"IDLE", "CLOCK", "MENU", "AGENT_SELECT", "LUNAR", "LISTENING", "SPEAKING"};
        ESP_LOGI(TAG, "Screen mode changed: %s -> %s", 
                 mode_names[previous_mode_], mode_names[current_mode_]);
        
        // Hide all screens first
        if (menu_ui_) menu_ui_->Hide();
        if (agent_selector_) agent_selector_->Hide();
        if (clock_face_manager_) clock_face_manager_->Hide();
        if (lunar_calendar_) lunar_calendar_->Hide();
        if (analog_clock_) analog_clock_->Hide();
        
        // Show the appropriate screen
        switch (current_mode_) {
            case SCREEN_MODE_MENU:
                if (menu_ui_) menu_ui_->Show();
                break;
            case SCREEN_MODE_AGENT_SELECT:
                if (agent_selector_) agent_selector_->Show();
                break;
            case SCREEN_MODE_CLOCK:
                if (clock_face_manager_) {
                    clock_face_manager_->Show();
                } else if (analog_clock_) {
                    analog_clock_->Show();
                }
                break;
            case SCREEN_MODE_LUNAR:
                if (lunar_calendar_) lunar_calendar_->Show();
                break;
            default:
                break;
        }
    }
}

void TouchGestureController::GestureCallback(TouchGesture gesture, int16_t x, int16_t y) {
    if (instance_) {
        instance_->HandleGesture(gesture, x, y);
    }
}

void TouchGestureController::HandleGesture(TouchGesture gesture, int16_t x, int16_t y) {
    ESP_LOGI(TAG, "Gesture received: %d at (%d, %d), mode=%d", gesture, x, y, current_mode_);
    
    switch (gesture) {
        case TOUCH_GESTURE_LONG_PRESS:
            HandleLongPress(x, y);
            break;
        case TOUCH_GESTURE_SWIPE_UP:
            HandleSwipeUp(x, y);
            break;
        case TOUCH_GESTURE_SWIPE_DOWN:
            HandleSwipeDown(x, y);
            break;
        case TOUCH_GESTURE_SWIPE_LEFT:
            HandleSwipeLeft(x, y);
            break;
        case TOUCH_GESTURE_SWIPE_RIGHT:
            HandleSwipeRight(x, y);
            break;
        case TOUCH_GESTURE_TAP:
            HandleTap(x, y);
            break;
        case TOUCH_GESTURE_DOUBLE_TAP:
            HandleDoubleTap(x, y);
            break;
        default:
            break;
    }
}

void TouchGestureController::HandleLongPress(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "🖐️ Long Press - Toggle Menu");
    
    if (current_mode_ == SCREEN_MODE_MENU) {
        // Close menu, return to previous mode
        SetScreenMode(previous_mode_);
    } else {
        // Open menu from any screen
        SetScreenMode(SCREEN_MODE_MENU);
    }
}

void TouchGestureController::HandleSwipeUp(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👆 Swipe Up - Increase Volume");
    
    // Volume up (works in any mode except menu)
    if (current_mode_ != SCREEN_MODE_MENU) {
        current_volume_ = (current_volume_ < 100) ? current_volume_ + 10 : 100;
        ESP_LOGI(TAG, "🔊 Volume: %d%%", current_volume_);
        
        if (volume_callback_) {
            volume_callback_(+1);  // +1 = increase
        }
    }
}

void TouchGestureController::HandleSwipeDown(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👇 Swipe Down - Decrease Volume");
    
    // Volume down (works in any mode except menu)
    if (current_mode_ != SCREEN_MODE_MENU) {
        current_volume_ = (current_volume_ > 0) ? current_volume_ - 10 : 0;
        ESP_LOGI(TAG, "🔉 Volume: %d%%", current_volume_);
        
        if (volume_callback_) {
            volume_callback_(-1);  // -1 = decrease
        }
    }
}

void TouchGestureController::HandleSwipeLeft(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👈 Swipe Left");
    
    // Check if Radio is playing - change to previous station
    auto radio = Application::GetInstance().GetRadio();
    if (radio && radio->IsPlaying()) {
        ESP_LOGI(TAG, "📻 Radio playing - Previous station");
        radio->PreviousStation();
        return;
    }
    
    switch (current_mode_) {
        case SCREEN_MODE_CLOCK:
            // Previous clock face
            if (clock_face_manager_) {
                clock_face_manager_->PreviousFace();
                current_clock_face_ = clock_face_manager_->GetCurrentFaceIndex();
            } else {
                current_clock_face_ = (current_clock_face_ > 0) 
                    ? current_clock_face_ - 1 
                    : clock_face_count_ - 1;
            }
            ESP_LOGI(TAG, "⏰ Clock face: %d/%d", current_clock_face_ + 1, clock_face_count_);
            
            if (clock_face_callback_) {
                clock_face_callback_(-1);  // -1 = previous
            }
            break;
            
        case SCREEN_MODE_AGENT_SELECT:
            // Previous agent
            if (agent_selector_) {
                agent_selector_->PreviousAgent();
                current_agent_index_ = agent_selector_->GetActiveIndex();
                ESP_LOGI(TAG, "🤖 Agent: %d", current_agent_index_ + 1);
                
                if (agent_change_callback_) {
                    agent_change_callback_(-1);  // -1 = previous
                }
            }
            break;
            
        case SCREEN_MODE_IDLE:
            // Switch to clock mode
            SetScreenMode(SCREEN_MODE_CLOCK);
            break;
            
        case SCREEN_MODE_LUNAR:
            // Switch to idle from lunar
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        default:
            break;
    }
}

void TouchGestureController::HandleSwipeRight(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👉 Swipe Right");
    
    // Check if Radio is playing - change to next station
    auto radio = Application::GetInstance().GetRadio();
    if (radio && radio->IsPlaying()) {
        ESP_LOGI(TAG, "📻 Radio playing - Next station");
        radio->NextStation();
        return;
    }
    
    switch (current_mode_) {
        case SCREEN_MODE_CLOCK:
            // Next clock face
            if (clock_face_manager_) {
                clock_face_manager_->NextFace();
                current_clock_face_ = clock_face_manager_->GetCurrentFaceIndex();
            } else {
                current_clock_face_ = (current_clock_face_ < clock_face_count_ - 1) 
                    ? current_clock_face_ + 1 
                    : 0;
            }
            ESP_LOGI(TAG, "⏰ Clock face: %d/%d", current_clock_face_ + 1, clock_face_count_);
            
            if (clock_face_callback_) {
                clock_face_callback_(+1);  // +1 = next
            }
            break;
            
        case SCREEN_MODE_AGENT_SELECT:
            // Next agent
            if (agent_selector_) {
                agent_selector_->NextAgent();
                current_agent_index_ = agent_selector_->GetActiveIndex();
                ESP_LOGI(TAG, "🤖 Agent: %d", current_agent_index_ + 1);
                
                if (agent_change_callback_) {
                    agent_change_callback_(+1);  // +1 = next
                }
            }
            break;
            
        case SCREEN_MODE_IDLE:
            // Switch to lunar calendar from idle
            SetScreenMode(SCREEN_MODE_LUNAR);
            break;
            
        case SCREEN_MODE_LUNAR:
            // Switch to clock from lunar
            SetScreenMode(SCREEN_MODE_CLOCK);
            break;
            
        default:
            break;
    }
}

void TouchGestureController::HandleTap(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👆 Tap at (%d, %d)", x, y);
    
    switch (current_mode_) {
        case SCREEN_MODE_MENU:
            // Tap in menu - handled by MenuUI button click
            // LVGL will handle this
            break;
            
        case SCREEN_MODE_AGENT_SELECT:
            // Tap on agent - select current one and go back
            if (agent_selector_) {
                agent_selector_->Hide();
            }
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        case SCREEN_MODE_CLOCK:
            // Tap on clock - return to idle
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        case SCREEN_MODE_LUNAR:
            // Tap on lunar - return to idle
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        case SCREEN_MODE_IDLE:
            // Tap on idle - maybe show brief status
            break;
            
        default:
            break;
    }
}

void TouchGestureController::HandleDoubleTap(int16_t x, int16_t y) {
    ESP_LOGI(TAG, "👆👆 Double Tap at (%d, %d)", x, y);
    
    // Double tap could be used for:
    // - Wake up from sleep
    // - Quick action (e.g., start listening)
    // - Toggle between screens
    
    switch (current_mode_) {
        case SCREEN_MODE_IDLE:
            // Switch to clock
            SetScreenMode(SCREEN_MODE_CLOCK);
            break;
            
        case SCREEN_MODE_CLOCK:
            // Switch back to idle
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        case SCREEN_MODE_LUNAR:
            // Switch back to idle
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        case SCREEN_MODE_AGENT_SELECT:
            // Confirm selection and go back
            SetScreenMode(SCREEN_MODE_IDLE);
            break;
            
        default:
            break;
    }
}
