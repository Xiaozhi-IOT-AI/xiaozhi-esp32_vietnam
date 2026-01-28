#include "agent_selector.h"
#include "../theme/theme_config.h"
#include "../theme/image_downloader.h"
#include "settings.h"
#include <cJSON.h>
#include <esp_log.h>
#include <cstring>

#define TAG "AgentSelector"

AgentSelector::AgentSelector(lv_obj_t* parent, int width, int height)
    : parent_(parent), width_(width), height_(height) {
    memset(&agents_, 0, sizeof(agents_));
    LoadActiveAgent();
    CreateUI();
}

AgentSelector::~AgentSelector() {
    // Free cached icons
    for (int i = 0; i < agents_.count; i++) {
        if (agents_.agents[i].icon_data) {
            free(agents_.agents[i].icon_data);
            agents_.agents[i].icon_data = nullptr;
        }
    }
    if (selector_root_) {
        lv_obj_delete(selector_root_);
    }
}

void AgentSelector::CreateUI() {
    // Create selector container (full screen)
    selector_root_ = lv_obj_create(parent_);
    if (!selector_root_) {
        ESP_LOGE(TAG, "Failed to create selector root object");
        return;
    }
    lv_obj_set_size(selector_root_, width_, height_);
    lv_obj_set_pos(selector_root_, 0, 0);
    lv_obj_set_style_bg_color(selector_root_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(selector_root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(selector_root_, 0, 0);
    lv_obj_set_style_radius(selector_root_, 0, 0);
    lv_obj_set_style_pad_all(selector_root_, 10, 0);
    lv_obj_add_flag(selector_root_, LV_OBJ_FLAG_HIDDEN);
    
    // Title
    title_label_ = lv_label_create(selector_root_);
    if (title_label_) {
        lv_label_set_text(title_label_, "Chọn Trợ Lý");
        lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    // Back button
    back_btn_ = lv_btn_create(selector_root_);
    if (back_btn_) {
        lv_obj_set_size(back_btn_, 50, 30);
        lv_obj_align(back_btn_, LV_ALIGN_TOP_LEFT, 0, 5);
        lv_obj_set_style_bg_color(back_btn_, lv_color_hex(0x3D5A80), 0);
        lv_obj_set_style_radius(back_btn_, 5, 0);
        lv_obj_add_event_cb(back_btn_, OnBackClick, LV_EVENT_CLICKED, this);
        
        lv_obj_t* back_label = lv_label_create(back_btn_);
        if (back_label) {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT);
            lv_obj_center(back_label);
        }
    }
    
    // Create swipe mode UI (portrait)
    if (use_swipe_mode_) {
        // Current agent container (center of screen)
        current_agent_container_ = lv_obj_create(selector_root_);
        lv_obj_set_size(current_agent_container_, width_ - 40, height_ - 150);
        lv_obj_align(current_agent_container_, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(current_agent_container_, lv_color_hex(0x2D2D44), 0);
        lv_obj_set_style_radius(current_agent_container_, 15, 0);
        lv_obj_set_style_border_width(current_agent_container_, 2, 0);
        lv_obj_set_style_border_color(current_agent_container_, lv_color_hex(0x4A90D9), 0);
        lv_obj_set_style_pad_all(current_agent_container_, 15, 0);
        lv_obj_clear_flag(current_agent_container_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(current_agent_container_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(current_agent_container_, OnAgentClick, LV_EVENT_CLICKED, this);
        
        // Agent icon (large, centered at top)
        agent_icon_ = lv_obj_create(current_agent_container_);
        lv_obj_set_size(agent_icon_, 80, 80);
        lv_obj_align(agent_icon_, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_radius(agent_icon_, 40, 0);
        lv_obj_set_style_bg_color(agent_icon_, lv_color_hex(0xFF69B4), 0);
        lv_obj_set_style_border_width(agent_icon_, 0, 0);
        lv_obj_clear_flag(agent_icon_, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t* icon_emoji = lv_label_create(agent_icon_);
        lv_label_set_text(icon_emoji, LV_SYMBOL_HOME);
        lv_obj_set_style_text_font(icon_emoji, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(icon_emoji, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon_emoji);
        
        // Agent name
        agent_name_label_ = lv_label_create(current_agent_container_);
        lv_label_set_text(agent_name_label_, "Chưa có Trợ lý");
        lv_obj_set_style_text_font(agent_name_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(agent_name_label_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(agent_name_label_, LV_ALIGN_TOP_MID, 0, 100);
        lv_obj_set_style_text_align(agent_name_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(agent_name_label_, width_ - 80);
        
        // Agent description
        agent_desc_label_ = lv_label_create(current_agent_container_);
        lv_label_set_text(agent_desc_label_, "Kéo trái/phải để chọn");
        lv_obj_set_style_text_font(agent_desc_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(agent_desc_label_, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(agent_desc_label_, LV_ALIGN_TOP_MID, 0, 130);
        lv_obj_set_style_text_align(agent_desc_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(agent_desc_label_, width_ - 80);
        lv_label_set_long_mode(agent_desc_label_, LV_LABEL_LONG_WRAP);
        
        // Navigation buttons
        prev_btn_ = lv_btn_create(selector_root_);
        lv_obj_set_size(prev_btn_, 50, 50);
        lv_obj_align(prev_btn_, LV_ALIGN_LEFT_MID, 0, 20);
        lv_obj_set_style_bg_color(prev_btn_, lv_color_hex(0x3D5A80), 0);
        lv_obj_set_style_radius(prev_btn_, 25, 0);
        lv_obj_add_event_cb(prev_btn_, OnPrevClick, LV_EVENT_CLICKED, this);
        lv_obj_t* prev_icon = lv_label_create(prev_btn_);
        lv_label_set_text(prev_icon, LV_SYMBOL_LEFT);
        lv_obj_center(prev_icon);
        
        next_btn_ = lv_btn_create(selector_root_);
        lv_obj_set_size(next_btn_, 50, 50);
        lv_obj_align(next_btn_, LV_ALIGN_RIGHT_MID, 0, 20);
        lv_obj_set_style_bg_color(next_btn_, lv_color_hex(0x3D5A80), 0);
        lv_obj_set_style_radius(next_btn_, 25, 0);
        lv_obj_add_event_cb(next_btn_, OnNextClick, LV_EVENT_CLICKED, this);
        lv_obj_t* next_icon = lv_label_create(next_btn_);
        lv_label_set_text(next_icon, LV_SYMBOL_RIGHT);
        lv_obj_center(next_icon);
        
        // Page indicator
        page_indicator_ = lv_label_create(selector_root_);
        lv_label_set_text(page_indicator_, "0/0");
        lv_obj_set_style_text_font(page_indicator_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(page_indicator_, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(page_indicator_, LV_ALIGN_BOTTOM_MID, 0, -15);
        
    } else {
        // List container for landscape mode (scrollable)
        list_container_ = lv_obj_create(selector_root_);
        if (list_container_) {
            lv_obj_set_size(list_container_, width_ - 20, height_ - 70);
            lv_obj_align(list_container_, LV_ALIGN_BOTTOM_MID, 0, -10);
            lv_obj_set_style_bg_opa(list_container_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(list_container_, 0, 0);
            lv_obj_set_style_pad_all(list_container_, 5, 0);
            lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(list_container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_scroll_dir(list_container_, LV_DIR_VER);
        }
    }
    
    ESP_LOGI(TAG, "Agent selector UI created (swipe mode: %s)", use_swipe_mode_ ? "yes" : "no");
}

void AgentSelector::UpdateAgentList() {
    // Clear existing items
    lv_obj_clean(list_container_);
    
    int item_width = width_ - 40;
    int item_height = 60;
    int gap = 8;
    
    auto& theme_mgr = ThemeManager::GetInstance();
    auto& theme = theme_mgr.GetTheme();
    
    for (int i = 0; i < agents_.count && i < MAX_AGENTS; i++) {
        AgentInfo* agent = &agents_.agents[i];
        
        // Create item container
        lv_obj_t* item = lv_obj_create(list_container_);
        lv_obj_set_size(item, item_width, item_height);
        lv_obj_set_style_pad_all(item, 10, 0);
        lv_obj_set_style_radius(item, 10, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        
        // Highlight active agent
        if (agent->is_active || i == agents_.active_index) {
            lv_obj_set_style_bg_color(item, lv_color_hex(0x4A90D9), 0);
            lv_obj_set_style_border_width(item, 2, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x00FF00), 0);
        } else {
            lv_obj_set_style_bg_color(item, lv_color_hex(0x2D2D44), 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x4A4A6A), 0);
        }
        
        // Store agent index
        lv_obj_set_user_data(item, (void*)(intptr_t)i);
        lv_obj_add_event_cb(item, OnAgentClick, LV_EVENT_CLICKED, this);
        
        // Icon placeholder (circle with emoji/initial)
        lv_obj_t* icon_bg = lv_obj_create(item);
        lv_obj_set_size(icon_bg, 40, 40);
        lv_obj_align(icon_bg, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_radius(icon_bg, 20, 0);
        lv_obj_set_style_bg_color(icon_bg, lv_color_hex(theme.primary_color), 0);
        lv_obj_set_style_border_width(icon_bg, 0, 0);
        
        lv_obj_t* icon_label = lv_label_create(icon_bg);
        char initial[4] = {0};
        if (agent->name[0]) {
            // Get first character (UTF-8 aware for Vietnamese)
            initial[0] = agent->name[0];
        } else {
            initial[0] = '?';
        }
        lv_label_set_text(icon_label, LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon_label);
        
        // Agent name
        lv_obj_t* name_label = lv_label_create(item);
        lv_label_set_text(name_label, agent->name);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 50, -8);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(name_label, item_width - 70);
        
        // Description
        if (agent->description[0]) {
            lv_obj_t* desc_label = lv_label_create(item);
            lv_label_set_text(desc_label, agent->description);
            lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(desc_label, lv_color_hex(0xAAAAAA), 0);
            lv_obj_align(desc_label, LV_ALIGN_LEFT_MID, 50, 10);
            lv_label_set_long_mode(desc_label, LV_LABEL_LONG_DOT);
            lv_obj_set_width(desc_label, item_width - 70);
        }
        
        // Active indicator
        if (agent->is_active || i == agents_.active_index) {
            lv_obj_t* check = lv_label_create(item);
            lv_label_set_text(check, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(check, lv_color_hex(0x00FF00), 0);
            lv_obj_align(check, LV_ALIGN_RIGHT_MID, -5, 0);
        }
        
        agent_items_[i] = item;
    }
    
    ESP_LOGI(TAG, "Updated agent list with %d agents", agents_.count);
}

void AgentSelector::Show() {
    if (selector_root_) {
        if (use_swipe_mode_) {
            UpdateCurrentAgentDisplay();
        } else {
            UpdateAgentList();
        }
        lv_obj_clear_flag(selector_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = true;
        ESP_LOGI(TAG, "Agent selector shown");
    }
}

void AgentSelector::Hide() {
    if (selector_root_) {
        lv_obj_add_flag(selector_root_, LV_OBJ_FLAG_HIDDEN);
        is_visible_ = false;
        ESP_LOGI(TAG, "Agent selector hidden");
    }
}

void AgentSelector::UpdateCurrentAgentDisplay() {
    if (!use_swipe_mode_ || agents_.count == 0) {
        return;
    }
    
    int index = agents_.active_index;
    if (index < 0 || index >= agents_.count) {
        index = 0;
        agents_.active_index = 0;
    }
    
    AgentInfo* agent = &agents_.agents[index];
    
    // Update name
    if (agent_name_label_) {
        lv_label_set_text(agent_name_label_, agent->name);
    }
    
    // Update description
    if (agent_desc_label_) {
        if (agent->description[0]) {
            lv_label_set_text(agent_desc_label_, agent->description);
        } else {
            lv_label_set_text(agent_desc_label_, "Nhấn để chọn trợ lý này");
        }
    }
    
    // Update icon background color based on agent index
    if (agent_icon_) {
        uint32_t colors[] = {0xFF69B4, 0x4A90D9, 0x98FB98, 0xFFCC00, 0xDDA0DD, 0xFF6B6B};
        lv_obj_set_style_bg_color(agent_icon_, lv_color_hex(colors[index % 6]), 0);
        
        // Update icon label with first character or emoji
        lv_obj_t* icon_label = lv_obj_get_child(agent_icon_, 0);
        if (icon_label) {
            if (agent->icon_loaded && agent->icon_data) {
                // TODO: Set actual image from downloaded data
                lv_label_set_text(icon_label, LV_SYMBOL_IMAGE);
            } else {
                // Use emoji based on agent type
                const char* icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_SETTINGS, LV_SYMBOL_AUDIO, 
                                       LV_SYMBOL_WIFI, LV_SYMBOL_GPS, LV_SYMBOL_BELL};
                lv_label_set_text(icon_label, icons[index % 6]);
            }
        }
    }
    
    // Update active indicator (border)
    if (current_agent_container_) {
        lv_obj_set_style_border_color(current_agent_container_, lv_color_hex(0x00FF00), 0);
    }
    
    // Update page indicator
    if (page_indicator_) {
        char page_str[16];
        snprintf(page_str, sizeof(page_str), "%d/%d", index + 1, agents_.count);
        lv_label_set_text(page_indicator_, page_str);
    }
    
    ESP_LOGI(TAG, "Displaying agent %d/%d: %s", index + 1, agents_.count, agent->name);
}

void AgentSelector::NextAgent() {
    if (agents_.count == 0) return;
    
    agents_.active_index = (agents_.active_index + 1) % agents_.count;
    
    // Update active flags
    for (int i = 0; i < agents_.count; i++) {
        agents_.agents[i].is_active = (i == agents_.active_index);
    }
    
    UpdateCurrentAgentDisplay();
    SaveActiveAgent();
    
    // Callback
    if (callback_) {
        callback_(&agents_.agents[agents_.active_index], callback_data_);
    }
}

void AgentSelector::PreviousAgent() {
    if (agents_.count == 0) return;
    
    agents_.active_index = (agents_.active_index > 0) ? agents_.active_index - 1 : agents_.count - 1;
    
    // Update active flags
    for (int i = 0; i < agents_.count; i++) {
        agents_.agents[i].is_active = (i == agents_.active_index);
    }
    
    UpdateCurrentAgentDisplay();
    SaveActiveAgent();
    
    // Callback
    if (callback_) {
        callback_(&agents_.agents[agents_.active_index], callback_data_);
    }
}

void AgentSelector::SelectAgent(int index) {
    if (index < 0 || index >= agents_.count) return;
    
    agents_.active_index = index;
    
    // Update active flags
    for (int i = 0; i < agents_.count; i++) {
        agents_.agents[i].is_active = (i == agents_.active_index);
    }
    
    UpdateCurrentAgentDisplay();
    SaveActiveAgent();
    
    // Callback
    if (callback_) {
        callback_(&agents_.agents[agents_.active_index], callback_data_);
    }
}

void AgentSelector::DownloadIcons() {
    auto& downloader = ImageDownloader::GetInstance();
    
    for (int i = 0; i < agents_.count; i++) {
        AgentInfo* agent = &agents_.agents[i];
        if (agent->icon_url[0] && !agent->icon_loaded) {
            int agent_index = i;
            downloader.DownloadAsync(agent->icon_url, 
                [](const std::string& url, const uint8_t* data, size_t len, bool success, void* user_data) {
                    if (success) {
                        ESP_LOGI(TAG, "Icon downloaded: %s (%zu bytes)", url.c_str(), len);
                        // Note: Would need to update the specific agent's icon data
                    }
                }, 
                reinterpret_cast<void*>(agent_index));
        }
    }
}

void AgentSelector::SetAgentIcon(int index, const uint8_t* data, size_t len) {
    if (index < 0 || index >= agents_.count) return;
    
    AgentInfo* agent = &agents_.agents[index];
    
    // Free old data
    if (agent->icon_data) {
        free(agent->icon_data);
    }
    
    // Copy new data
    agent->icon_data = (uint8_t*)malloc(len);
    if (agent->icon_data) {
        memcpy(agent->icon_data, data, len);
        agent->icon_len = len;
        agent->icon_loaded = true;
        ESP_LOGI(TAG, "Set icon for agent %d (%zu bytes)", index, len);
    }
}

void AgentSelector::SetAgents(const AgentList& list) {
    memcpy(&agents_, &list, sizeof(AgentList));
    if (is_visible_) {
        UpdateAgentList();
    }
}

bool AgentSelector::ParseAgentsJson(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse agents JSON");
        return false;
    }
    
    cJSON* agents_arr = cJSON_GetObjectItem(root, "agents");
    if (!agents_arr) {
        agents_arr = root;  // Try root as array
    }
    
    if (!cJSON_IsArray(agents_arr)) {
        cJSON_Delete(root);
        return false;
    }
    
    agents_.count = 0;
    agents_.active_index = -1;
    
    cJSON* agent_obj;
    cJSON_ArrayForEach(agent_obj, agents_arr) {
        if (agents_.count >= MAX_AGENTS) break;
        
        AgentInfo* agent = &agents_.agents[agents_.count];
        memset(agent, 0, sizeof(AgentInfo));
        
        cJSON* id = cJSON_GetObjectItem(agent_obj, "id");
        if (cJSON_IsString(id)) {
            strncpy(agent->id, id->valuestring, sizeof(agent->id) - 1);
            agent->id[sizeof(agent->id) - 1] = '\0';
        }
        
        cJSON* name = cJSON_GetObjectItem(agent_obj, "name");
        if (cJSON_IsString(name)) {
            strncpy(agent->name, name->valuestring, sizeof(agent->name) - 1);
            agent->name[sizeof(agent->name) - 1] = '\0';
        }
        
        cJSON* desc = cJSON_GetObjectItem(agent_obj, "description");
        if (cJSON_IsString(desc)) {
            strncpy(agent->description, desc->valuestring, sizeof(agent->description) - 1);
            agent->description[sizeof(agent->description) - 1] = '\0';
        }
        
        cJSON* icon = cJSON_GetObjectItem(agent_obj, "icon_url");
        if (cJSON_IsString(icon)) {
            strncpy(agent->icon_url, icon->valuestring, sizeof(agent->icon_url) - 1);
            agent->icon_url[sizeof(agent->icon_url) - 1] = '\0';
        }
        
        cJSON* active = cJSON_GetObjectItem(agent_obj, "is_active");
        agent->is_active = cJSON_IsTrue(active);
        
        if (agent->is_active) {
            agents_.active_index = agents_.count;
        }
        
        agents_.count++;
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Parsed %d agents, active index: %d", agents_.count, agents_.active_index);
    return true;
}

const AgentInfo* AgentSelector::GetActiveAgent() const {
    if (agents_.active_index >= 0 && agents_.active_index < agents_.count) {
        return &agents_.agents[agents_.active_index];
    }
    return nullptr;
}

void AgentSelector::SetCallback(AgentSelectedCallback callback, void* user_data) {
    callback_ = callback;
    callback_data_ = user_data;
}

bool AgentSelector::SaveActiveAgent() {
    Settings settings("agent", true);
    
    const AgentInfo* active = GetActiveAgent();
    if (active) {
        settings.SetString("active_id", active->id);
        settings.SetString("active_name", active->name);
        ESP_LOGI(TAG, "Saved active agent: %s", active->name);
        return true;
    }
    return false;
}

bool AgentSelector::LoadActiveAgent() {
    Settings settings("agent", false);
    
    std::string active_id = settings.GetString("active_id");
    if (!active_id.empty()) {
        // Will be matched when agents are set
        ESP_LOGI(TAG, "Loaded active agent ID: %s", active_id.c_str());
        return true;
    }
    return false;
}

void AgentSelector::OnAgentClick(lv_event_t* e) {
    AgentSelector* self = static_cast<AgentSelector*>(lv_event_get_user_data(e));
    lv_obj_t* item = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int index = (int)(intptr_t)lv_obj_get_user_data(item);
    
    if (index >= 0 && index < self->agents_.count) {
        // Update active agent
        for (int i = 0; i < self->agents_.count; i++) {
            self->agents_.agents[i].is_active = (i == index);
        }
        self->agents_.active_index = index;
        
        AgentInfo* selected = &self->agents_.agents[index];
        ESP_LOGI(TAG, "Agent selected: %s (%s)", selected->name, selected->id);
        
        // Save selection
        self->SaveActiveAgent();
        
        // Callback
        if (self->callback_) {
            self->callback_(selected, self->callback_data_);
        }
        
        // Update UI
        self->UpdateAgentList();
    }
}

void AgentSelector::OnBackClick(lv_event_t* e) {
    AgentSelector* self = static_cast<AgentSelector*>(lv_event_get_user_data(e));
    self->Hide();
}

void AgentSelector::OnPrevClick(lv_event_t* e) {
    AgentSelector* self = static_cast<AgentSelector*>(lv_event_get_user_data(e));
    self->PreviousAgent();
}

void AgentSelector::OnNextClick(lv_event_t* e) {
    AgentSelector* self = static_cast<AgentSelector*>(lv_event_get_user_data(e));
    self->NextAgent();
}
