# MQTT Subscribe Fix Guide - ✅ FIXED

## Vấn đề (ĐÃ ĐƯỢC SỬA)

Firmware hiện tại **CHỈ CÓ THỂ PUBLISH** message đến MQTT broker, nhưng **KHÔNG THỂ SUBSCRIBE** để nhận message từ server.

**Hậu quả:**
- Khi device gửi `hello`, server trả lời với UDP channel info
- Nhưng device **không nhận được** response vì không subscribe topic
- Timeout sau 10 giây: `Failed to receive server hello`

---

## Root Cause

### File: `main/protocols/mqtt_protocol.h`
```cpp
// HIỆN TẠI: Chỉ có publish_topic_
std::string publish_topic_;

// THIẾU: subscribe_topic_
// std::string subscribe_topic_;  // ← CẦN THÊM
```

### File: `main/protocols/mqtt_protocol.cc`

**Function `StartMqttClient()`:**
```cpp
// HIỆN TẠI (line 65): Chỉ đọc publish_topic
publish_topic_ = settings.GetString("publish_topic");

// THIẾU: Đọc subscribe_topic
// subscribe_topic_ = settings.GetString("subscribe_topic");
```

**Function `mqtt_->OnConnected()` callback:**
```cpp
// HIỆN TẠI (line 87-92): Không subscribe gì cả
mqtt_->OnConnected([this]() {
    if (on_connected_ != nullptr) {
        on_connected_();
    }
    esp_timer_stop(reconnect_timer_);
});

// THIẾU: Subscribe topic sau khi connect
```

---

## Giải pháp

### Step 1: Thêm member variable

**File: `main/protocols/mqtt_protocol.h`**

```diff
 private:
     EventGroupHandle_t event_group_handle_;
 
     std::string publish_topic_;
+    std::string subscribe_topic_;
 
     std::mutex channel_mutex_;
```

### Step 2: Đọc subscribe_topic từ settings

**File: `main/protocols/mqtt_protocol.cc`**

```diff
 bool MqttProtocol::StartMqttClient(bool report_error) {
     // ... existing code ...
     
     Settings settings("mqtt", false);
     auto endpoint = settings.GetString("endpoint");
     auto client_id = settings.GetString("client_id");
     auto username = settings.GetString("username");
     auto password = settings.GetString("password");
     int keepalive_interval = settings.GetInt("keepalive", 240);
     publish_topic_ = settings.GetString("publish_topic");
+    subscribe_topic_ = settings.GetString("subscribe_topic");
     
     // ... rest of function ...
```

### Step 3: Subscribe sau khi connect

**File: `main/protocols/mqtt_protocol.cc`**

```diff
     mqtt_->OnConnected([this]() {
         if (on_connected_ != nullptr) {
             on_connected_();
         }
         esp_timer_stop(reconnect_timer_);
+        
+        // Subscribe to receive messages from server
+        if (!subscribe_topic_.empty()) {
+            ESP_LOGI(TAG, "Subscribing to topic: %s", subscribe_topic_.c_str());
+            if (!mqtt_->Subscribe(subscribe_topic_)) {
+                ESP_LOGE(TAG, "Failed to subscribe to %s", subscribe_topic_.c_str());
+            }
+        }
     });
```

### Step 4: Đảm bảo Mqtt class có Subscribe method

**File: `main/boards/common/mqtt.h`** (hoặc tương tự)

```cpp
// Kiểm tra có method này chưa, nếu chưa thì thêm
class Mqtt {
public:
    // ...
    virtual bool Subscribe(const std::string& topic, int qos = 0) = 0;
    // ...
};
```

---

## Server Configuration

Server OTA đang gửi đúng config:

```json
{
    "mqtt": {
        "endpoint": "xiaozhi-ai-iot.vn:1883",
        "username": "xiaozhi_device",
        "password": "...",
        "publish_topic": "device-server",
        "subscribe_topic": "device/{MAC_ADDRESS}/#"
    }
}
```

Khi firmware subscribe `device/{MAC}/#`:
- Server publish response đến `device/{MAC}/server`
- Device nhận được và xử lý trong `mqtt_->OnMessage()` callback (đã có)

---

## MQTT Topics Flow

```
DEVICE                              SERVER
  │                                   │
  │──publish─→ "device-server"────────│ (hello message)
  │                                   │
  │                    ←──subscribe──│ "device-server"
  │                                   │
  │←─"device/{MAC}/server"──publish──│ (hello response với UDP info)
  │                                   │
  │                                   │
  │══════UDP Audio Channel══════════│
```

---

## Build & Flash

```bash
# Clean build
idf.py fullclean

# Build
idf.py build

# Flash (thay PORT bằng port thực, vd /dev/ttyUSB0)
idf.py -p PORT flash

# Monitor logs
idf.py -p PORT monitor
```

---

## Verification

Sau khi flash firmware mới, kiểm tra logs:

**Expected logs:**
```
I MQTT: Connecting to endpoint xiaozhi-ai-iot.vn:1883
I MQTT: Connected to endpoint
I MQTT: Subscribing to topic: device/98:a3:16:e8:df:48/#
I MQTT: Session ID: 8c77947b416d1f4f...
```

**Server logs:**
```
Received MQTT message type='hello' on topic='device-server'
Sent hello response to device/98:a3:16:e8:df:48/server
```

---

## Alternative (Không cần fix firmware)

Nếu không muốn update firmware, có thể dùng **WebSocket** thay vì MQTT:

1. Server đã gửi cả `websocket` và `mqtt` config trong OTA response
2. Firmware tự động fallback về WebSocket nếu MQTT fail
3. WebSocket **đang hoạt động tốt** hiện tại

---

## References

- `main/protocols/mqtt_protocol.cc` - MQTT protocol implementation
- `main/protocols/mqtt_protocol.h` - Header file
- `main/ota.cc` - OTA config parsing (đã lưu subscribe_topic)
- Backend: `backend/src/app/services/mqtt_device_handler.py` - Handler nhận hello
