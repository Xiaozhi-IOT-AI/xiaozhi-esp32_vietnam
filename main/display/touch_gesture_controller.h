#ifndef TOUCH_GESTURE_CONTROLLER_H
#define TOUCH_GESTURE_CONTROLLER_H

#include "lcd_touch.h"
#include "../features/menu/menu_ui.h"
#include "../features/agent/agent_selector.h"
#include "../features/theme/analog_clock.h"
#include <functional>

/**
 * Touch Gesture Controller for Vietnam Xiaozhi devices
 * 
 * Gesture Mappings:
 * - Long Press: Open/Close Settings Menu
 * - Swipe Up: Increase Volume
 * - Swipe Down: Decrease Volume  
 * - Swipe Left (Clock): Previous clock face
 * - Swipe Right (Clock): Next clock face
 * - Swipe Left (Agent): Previous agent
 * - Swipe Right (Agent): Next agent
 */

// Screen modes
enum ScreenMode {
    SCREEN_MODE_IDLE = 0,       // Default idle screen (weather/time)
    SCREEN_MODE_CLOCK,          // Analog clock display
    SCREEN_MODE_MENU,           // Settings menu
    SCREEN_MODE_AGENT_SELECT,   // Agent selection screen
    SCREEN_MODE_LISTENING,      // AI listening mode
    SCREEN_MODE_SPEAKING,       // AI speaking mode
    SCREEN_MODE_COUNT
};

// Volume change callback
typedef void (*VolumeChangeCallback)(int delta);  // delta: +1 or -1

// Clock face change callback  
typedef void (*ClockFaceChangeCallback)(int direction);  // direction: -1 left, +1 right

// Agent change callback
typedef void (*AgentChangeCallback)(int direction);  // direction: -1 prev, +1 next

class TouchGestureController {
public:
    TouchGestureController();
    ~TouchGestureController() = default;
    
    // Set UI components (must be called before using)
    void SetMenuUI(MenuUI* menu) { menu_ui_ = menu; }
    void SetAgentSelector(AgentSelector* selector) { agent_selector_ = selector; }
    void SetAnalogClock(AnalogClock* clock) { analog_clock_ = clock; }
    
    // Set callbacks
    void SetVolumeCallback(VolumeChangeCallback callback) { volume_callback_ = callback; }
    void SetClockFaceCallback(ClockFaceChangeCallback callback) { clock_face_callback_ = callback; }
    void SetAgentChangeCallback(AgentChangeCallback callback) { agent_change_callback_ = callback; }
    
    // Get/Set current screen mode
    ScreenMode GetScreenMode() const { return current_mode_; }
    void SetScreenMode(ScreenMode mode);
    
    // Main gesture handler (connect to LcdTouch::SetGestureCallback)
    void HandleGesture(TouchGesture gesture, int16_t x, int16_t y);
    
    // Static wrapper for callback
    static void GestureCallback(TouchGesture gesture, int16_t x, int16_t y);
    static void SetInstance(TouchGestureController* instance) { instance_ = instance; }
    
    // Volume control
    void SetCurrentVolume(int volume) { current_volume_ = volume; }
    int GetCurrentVolume() const { return current_volume_; }
    
    // Clock face control
    int GetCurrentClockFace() const { return current_clock_face_; }
    void SetClockFaceCount(int count) { clock_face_count_ = count; }
    
private:
    void HandleLongPress(int16_t x, int16_t y);
    void HandleSwipeUp(int16_t x, int16_t y);
    void HandleSwipeDown(int16_t x, int16_t y);
    void HandleSwipeLeft(int16_t x, int16_t y);
    void HandleSwipeRight(int16_t x, int16_t y);
    void HandleTap(int16_t x, int16_t y);
    void HandleDoubleTap(int16_t x, int16_t y);
    
    ScreenMode current_mode_ = SCREEN_MODE_IDLE;
    ScreenMode previous_mode_ = SCREEN_MODE_IDLE;
    
    // UI components (not owned)
    MenuUI* menu_ui_ = nullptr;
    AgentSelector* agent_selector_ = nullptr;
    AnalogClock* analog_clock_ = nullptr;
    
    // Callbacks
    VolumeChangeCallback volume_callback_ = nullptr;
    ClockFaceChangeCallback clock_face_callback_ = nullptr;
    AgentChangeCallback agent_change_callback_ = nullptr;
    
    // State
    int current_volume_ = 50;        // 0-100
    int current_clock_face_ = 0;     // Index of current clock face
    int clock_face_count_ = 2;       // Total clock faces available
    int current_agent_index_ = 0;    // Index of current agent
    
    static TouchGestureController* instance_;
};

#endif // TOUCH_GESTURE_CONTROLLER_H
