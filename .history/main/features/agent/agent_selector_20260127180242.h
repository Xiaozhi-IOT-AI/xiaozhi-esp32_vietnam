#ifndef AGENT_SELECTOR_H
#define AGENT_SELECTOR_H

#include <lvgl.h>
#include <string>
#include <vector>
#include <functional>

/**
 * Agent Selector UI for multi-agent support
 * Allows users to switch between different AI agents
 * Each agent has:
 * - ID (from server)
 * - Name (displayed)
 * - Description
 * - Icon URL (optional, downloaded and cached)
 * - Active status
 * 
 * Features:
 * - Swipe left/right to navigate agents
 * - Download and display agent icons from server
 * - Portrait mode layout
 */

// Maximum agents per device
#define MAX_AGENTS 10

// Agent info structure
struct AgentInfo {
    char id[64];
    char name[64];
    char description[128];
    char icon_url[256];
    bool is_active;
    // Cached icon data
    uint8_t* icon_data = nullptr;
    size_t icon_len = 0;
    bool icon_loaded = false;
};

// Agent list structure
struct AgentList {
    AgentInfo agents[MAX_AGENTS];
    int count;
    int active_index;
};

// Callback when agent is selected
typedef void (*AgentSelectedCallback)(const AgentInfo* agent, void* user_data);

class AgentSelector {
public:
    AgentSelector(lv_obj_t* parent, int width, int height);
    ~AgentSelector();
    
    // Show/hide agent selector
    void Show();
    void Hide();
    bool IsVisible() const { return is_visible_; }
    
    // Set agents list
    void SetAgents(const AgentList& list);
    bool ParseAgentsJson(const char* json_str);
    
    // Get current agents
    const AgentList& GetAgents() const { return agents_; }
    const AgentInfo* GetActiveAgent() const;
    int GetActiveIndex() const { return agents_.active_index; }
    
    // Navigation (for swipe gestures)
    void NextAgent();
    void PreviousAgent();
    void SelectAgent(int index);
    
    // Download agent icons from URLs
    void DownloadIcons();
    void SetAgentIcon(int index, const uint8_t* data, size_t len);
    
    // Set selection callback
    void SetCallback(AgentSelectedCallback callback, void* user_data = nullptr);
    
    // Save/Load from NVS
    bool SaveActiveAgent();
    bool LoadActiveAgent();
    
    // Get root object
    lv_obj_t* GetRoot() { return selector_root_; }
    
private:
    void CreateUI();
    void UpdateAgentList();
    void UpdateCurrentAgentDisplay();
    static void OnAgentClick(lv_event_t* e);
    static void OnBackClick(lv_event_t* e);
    static void OnPrevClick(lv_event_t* e);
    static void OnNextClick(lv_event_t* e);
    
    lv_obj_t* parent_;
    lv_obj_t* selector_root_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* list_container_ = nullptr;
    lv_obj_t* back_btn_ = nullptr;
    lv_obj_t* agent_items_[MAX_AGENTS] = {nullptr};
    
    // Swipe navigation UI
    lv_obj_t* current_agent_container_ = nullptr;
    lv_obj_t* agent_icon_ = nullptr;
    lv_obj_t* agent_name_label_ = nullptr;
    lv_obj_t* agent_desc_label_ = nullptr;
    lv_obj_t* prev_btn_ = nullptr;
    lv_obj_t* next_btn_ = nullptr;
    lv_obj_t* page_indicator_ = nullptr;
    
    int width_;
    int height_;
    bool is_visible_ = false;
    bool use_swipe_mode_ = true;  // Portrait mode with swipe
    
    AgentList agents_;
    AgentSelectedCallback callback_ = nullptr;
    void* callback_data_ = nullptr;
};

#endif // AGENT_SELECTOR_H
