#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#ifdef CONFIG_TOUCH_PANEL_ENABLE
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_ft6x36.h>
#include <esp_lcd_touch_ft5x06.h>
#include "display/lcd_touch.h"
#endif
#include <lvgl.h>
#include <esp_lvgl_port.h>
#include <wifi_station.h>
#include "application.h"
#include "codecs/no_audio_codec.h"
#include "codecs/es8311_audio_codec.h"
#include "button.h"
#include "display/lcd_display.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "config.h"
#include "esp_lcd_ili9341.h"
#include "features/menu/menu_ui.h"
#include "features/lunar/lunar_calendar.h"
#include "features/theme/analog_clock.h"
#include "features/music/esp32_radio.h"
#ifdef CONFIG_SD_CARD_MMC_INTERFACE
#include "sdmmc.h"
#elif defined(CONFIG_SD_CARD_SPI_INTERFACE)
#include "sdspi.h"
#endif

#define TAG "XiaozhiAIIoTEs3n28p"

#define TOUCH_FT62XX_G_FT5201ID 0xA8     // FocalTech's panel ID
#define TOUCH_FT62XX_REG_NUMTOUCHES 0x02 // Number of touch points

#define TOUCH_FT62XX_NUM_X 0x33 // Touch X position
#define TOUCH_FT62XX_NUM_Y 0x34 // Touch Y position

#define TOUCH_FT62XX_REG_MODE 0x00        // Device mode, either WORKING or FACTORY
#define TOUCH_FT62XX_REG_READDATA 0x00    // Read data from register
#define TOUCH_FT62XX_REG_CALIBRATE 0x02   // Calibrate mode
#define TOUCH_FT62XX_REG_WORKMODE 0x00    // Work mode
#define TOUCH_FT62XX_REG_FACTORYMODE 0x40 // Factory mode
#define TOUCH_FT62XX_REG_THRESHHOLD 0x80  // Threshold for touch detection
#define TOUCH_FT62XX_REG_POINTRATE 0x88   // Point rate
#define TOUCH_FT62XX_REG_FIRMVERS 0xA6    // Firmware version
#define TOUCH_FT62XX_REG_CHIPID 0xA3      // Chip selecting
#define TOUCH_FT62XX_REG_INTMODE 0xA4     // Interrupt mode
#define TOUCH_FT62XX_REG_VENDID 0xA8      // FocalTech's panel ID

#define TOUCH_FT62XX_VENDID 0x11  // FocalTech's panel ID
#define TOUCH_FT6206_CHIPID 0x06  // Chip selecting
#define TOUCH_FT3236_CHIPID 0x33  // Chip selecting
#define TOUCH_FT6236_CHIPID 0x36  // Chip selecting
#define TOUCH_FT6236U_CHIPID 0x64 // Chip selecting
#define TOUCH_FT6336U_CHIPID 0x64 // Chip selecting

#define TOUCH_FT62XX_DEFAULT_THRESHOLD 64 // Default threshold for touch detection

// Global variables for touch callback

class XiaozhiAIIoTEs3n28p : public WifiBoard {
 private:
  Button boot_button_;
  LcdDisplay *display_;
  i2c_master_bus_handle_t codec_i2c_bus_;
  MenuUI* menu_ui_ = nullptr;
  AnalogClock* analog_clock_ = nullptr;
  lv_obj_t* info_page_ = nullptr;
  bool menu_visible_ = false;
  bool clock_visible_ = false;
  bool info_visible_ = false;
#ifdef CONFIG_TOUCH_PANEL_ENABLE
  LcdTouch *touch_;
  // Touch interrupt semaphore
  SemaphoreHandle_t touch_isr_mux_ = nullptr;
#endif

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = DISPLAY_MIS0_PIN;
    buscfg.sclk_io_num = DISPLAY_SCK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    // Initialize LCD control IO
    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));
    
    // Initialize the LCD driver chip
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
    ESP_LOGI(TAG, "Install LCD driver ILI9341");
    esp_lcd_panel_reset(panel);

    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    display_ = new SpiLcdDisplay(panel_io, panel, 
        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
        DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
  }

  void InitializeI2c() {
      i2c_master_bus_config_t i2c_bus_cfg = {
          .i2c_port = AUDIO_CODEC_I2C_NUM,
          .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
          .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
          .clk_source = I2C_CLK_SRC_DEFAULT,
          .glitch_ignore_cnt = 7,
          .intr_priority = 0,
          .trans_queue_depth = 0,
          .flags = {
              .enable_internal_pullup = 1,
          },
      };
      ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
  }

  void CheckI2CDevice(uint8_t addr, const char* name) {
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = addr,
      .scl_speed_hz = 100000,
    };
    
    if (i2c_master_bus_add_device(codec_i2c_bus_, &dev_cfg, &dev_handle) == ESP_OK) {
      uint8_t data;
      if (i2c_master_receive(dev_handle, &data, 1, 100) == ESP_OK) {
        ESP_LOGI(TAG, "✓ Found %s at I2C address 0x%02X", name, addr);
      } else {
        ESP_LOGW(TAG, "✗ Device at 0x%02X (%s) not responding", addr, name);
      }
      i2c_master_bus_rm_device(dev_handle);
    } else {
      ESP_LOGE(TAG, "✗ Failed to add device at 0x%02X (%s)", addr, name);
    }
  }

#ifdef CONFIG_TOUCH_PANEL_ENABLE
  static void IRAM_ATTR touch_isr_callback(void* arg) {
    XiaozhiAIIoTEs3n28p *board = static_cast<XiaozhiAIIoTEs3n28p *>(arg);
    board->NotifyTouchEvent();
  }

  bool WaitForTouchEvent(TickType_t timeout = portMAX_DELAY) {
    if (touch_isr_mux_ != NULL) {
        BaseType_t result = xSemaphoreTake(touch_isr_mux_, timeout);
        return result == pdTRUE;
    }
    return false;
  }

  void NotifyTouchEvent() {
    if (touch_isr_mux_ != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(touch_isr_mux_, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
  void InitializeTouch() {
    ESP_LOGI(TAG, "Initialize touch controller FT6236G");
    ESP_LOGI(TAG, "Touch I2C: SDA=%d, SCL=%d, ADDR=0x%02X", TOUCH_I2C_SDA_PIN, TOUCH_I2C_SCL_PIN, TOUCH_I2C_ADDR);
    ESP_LOGI(TAG, "Touch pins: RST=%d, INT=%d", TOUCH_RST_PIN, TOUCH_INT_PIN);
    
    // Create touch interrupt semaphore
    touch_isr_mux_ = xSemaphoreCreateBinary();
    if (touch_isr_mux_ == NULL) {
        ESP_LOGE(TAG, "Failed to create touch semaphore");
    }
    
    // Manual reset of touch controller
    if (TOUCH_RST_PIN != GPIO_NUM_NC) {
      /* Prepare pin for touch controller reset */
      ESP_LOGI(TAG, "Resetting touch controller...");
      const gpio_config_t rst_gpio_config = {
        .pin_bit_mask = BIT64(TOUCH_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
      };
      gpio_config(&rst_gpio_config);
      
      gpio_set_level(TOUCH_RST_PIN, 0);  // Reset low
      vTaskDelay(pdMS_TO_TICKS(10));
      gpio_set_level(TOUCH_RST_PIN, 1);  // Reset high
      vTaskDelay(pdMS_TO_TICKS(200));    // 200ms is a minimum wait for touch controller to boot
        ESP_LOGI(TAG, "Touch controller reset complete");
    }

    if (TOUCH_INT_PIN != GPIO_NUM_NC) {
      const gpio_config_t int_gpio_config = {
        .pin_bit_mask = (1ULL << TOUCH_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
      };
      gpio_config(&int_gpio_config);
      gpio_install_isr_service(0);
      gpio_intr_enable(TOUCH_INT_PIN);
      gpio_isr_handler_add(TOUCH_INT_PIN, touch_isr_callback, this);
    }
    
    // Check I2C devices
    CheckI2CDevice(0x18, "ES8311 Audio Codec");
    CheckI2CDevice(0x38, "FT6236G Touch");
    
    // Touch configuration - enable interrupt, use polling mode
    const esp_lcd_touch_config_t tp_cfg = {
      .x_max = DISPLAY_WIDTH - 1,
      .y_max = DISPLAY_HEIGHT - 1,
      // TOUCH_RST_PIN already handled above, should not handle reset here by driver 
      // due to timing delay 10ms is very short and causes issues inside driver
      .rst_gpio_num = GPIO_NUM_NC, // TOUCH_RST_PIN
      .int_gpio_num = GPIO_NUM_NC, // TOUCH_INT_PIN
      .levels = {
        .reset = 0,
        .interrupt = 0,
      },
      .flags = {
        .swap_xy = DISPLAY_SWAP_XY ? 1U : 0U,
        .mirror_x = DISPLAY_MIRROR_X ? 0U : 1U,
        .mirror_y = DISPLAY_MIRROR_Y ? 1U : 0U,
      },
    };
    
    ESP_LOGI(TAG, "Touch config: x_max=%d, y_max=%d, swap_xy=%d, mirror_x=%d, mirror_y=%d",
             tp_cfg.x_max, tp_cfg.y_max, tp_cfg.flags.swap_xy, 
             tp_cfg.flags.mirror_x, tp_cfg.flags.mirror_y);

    // Use FT6x36 driver with custom debug panel IO (creates its own device handle)
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    tp_io_config.dev_addr = TOUCH_I2C_ADDR;
    tp_io_config.scl_speed_hz = 400 * 1000;  // 400kHz
    
    ESP_LOGI(TAG, "Creating touch I2C panel IO...");
    esp_err_t ret;
    ret = esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create touch I2C panel IO: %s", esp_err_to_name(ret));
      return;
    }
    
    ESP_LOGI(TAG, "Creating FT6x36 touch controller (compatible with FT6336)...");
    esp_lcd_touch_handle_t tp_ = NULL;
    ret = esp_lcd_touch_new_i2c_ft6x36(tp_io_handle, &tp_cfg, &tp_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create FT6x36 touch controller: %s", esp_err_to_name(ret));
      return;
    }
    ESP_LOGI(TAG, "✅ Touch panel FT6236G initialized successfully with custom driver!");

    // Set touch threshold
    uint8_t threshold_reg[1] = {TOUCH_FT62XX_DEFAULT_THRESHOLD};
    ret = esp_lcd_panel_io_tx_param(tp_io_handle, TOUCH_FT62XX_REG_THRESHHOLD, threshold_reg, 1);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to enable touch panel threshold: %s", esp_err_to_name(ret));
    }

    // Set point rate
    uint8_t rate_reg[1] = {0x0A}; // 100ms
    ret = esp_lcd_panel_io_tx_param(tp_io_handle, TOUCH_FT62XX_REG_POINTRATE, rate_reg, 1);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to enable touch panel point rate: %s", esp_err_to_name(ret));
    }

    // Set interrupt mode
    uint8_t int_mode_reg[1] = {0x01}; // 1 = interrupt output, 0 = polling
    ret = esp_lcd_panel_io_tx_param(tp_io_handle, TOUCH_FT62XX_REG_INTMODE, int_mode_reg, 1);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to enable touch panel interrupt output: %s", esp_err_to_name(ret));
    }

    touch_ = new I2cLcdTouch(tp_, tp_io_handle, 
                          DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                          DISPLAY_SWAP_XY, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    
    touch_->SetInterruptCallback([this]()->bool {
        return this->WaitForTouchEvent();
    });

    touch_->SetGestureCallback([this](TouchGesture gesture, int16_t x, int16_t y) {
      ESP_LOGI(TAG, "Touch gesture detected: %d at (%d, %d), menu_visible=%d, clock_visible=%d", 
               static_cast<int>(gesture), x, y, menu_visible_, clock_visible_);
      
      // When analog clock is visible, TAP to dismiss
      if (clock_visible_) {
        if (gesture == TOUCH_GESTURE_TAP || gesture == TOUCH_GESTURE_LONG_PRESS) {
          ESP_LOGI(TAG, "TAP/LongPress - hiding clock");
          this->ToggleAnalogClock();  // Hide clock
        }
        return;  // Don't process other gestures when clock visible
      }
      
      // When menu is visible, handle long press and TAP directly
      if (menu_visible_) {
        if (gesture == TOUCH_GESTURE_LONG_PRESS) {
          ESP_LOGI(TAG, "Long press - hiding menu");
          this->HideMenu();
        } else if (gesture == TOUCH_GESTURE_TAP && menu_ui_) {
          // Handle TAP directly on menu buttons
          ESP_LOGI(TAG, "TAP on menu at (%d, %d)", x, y);
          menu_ui_->HandleTouch(x, y);
        }
        return;
      }
      
      switch (gesture) {
        case TOUCH_GESTURE_SWIPE_RIGHT:
          {
            Display::DisplaySourceType source = static_cast<LcdDisplay*>(display_)->DetectSourceFromInfo();
            ESP_LOGI(TAG, "Current source detected: %d", static_cast<int>(source));
            if (source == Display::DisplaySourceType::SD_CARD) {
              ESP_LOGI(TAG, "Play Next track");
              auto& app = Application::GetInstance();
              auto sd_music = app.GetSdMusic();
              if (sd_music) {
                sd_music->stop();
                sd_music->next();
                vTaskDelay(pdMS_TO_TICKS(500));
              }
            } else {
              auto& board = Board::GetInstance();
              auto backlight = board.GetBacklight();
              int new_brightness = backlight->brightness();
              
              // Swipe right - increase brightness
              new_brightness += 5;
              if (new_brightness > 100) new_brightness = 100;
              ESP_LOGI(TAG, "Brightness: %d → %d", backlight->brightness(), new_brightness);
              
              backlight->SetBrightness(new_brightness);
              auto display = board.GetDisplay();
              display->ShowNotification("Brightness: " + std::to_string(new_brightness));
            }
          }
          break;
        case TOUCH_GESTURE_SWIPE_LEFT:
          {
            Display::DisplaySourceType source = static_cast<LcdDisplay*>(display_)->DetectSourceFromInfo();
            ESP_LOGI(TAG, "Current source detected: %d", static_cast<int>(source));
            if (source == Display::DisplaySourceType::SD_CARD) {
              ESP_LOGI(TAG, "Play Previous track");
              auto& app = Application::GetInstance();
              auto sd_music = app.GetSdMusic();
              if (sd_music) {
                sd_music->stop();
                sd_music->prev();
                vTaskDelay(pdMS_TO_TICKS(500));
              }
              break;
            }
            auto& board = Board::GetInstance();
            auto backlight = board.GetBacklight();
            int new_brightness = backlight->brightness();
            
            // Swipe left - decrease brightness
            new_brightness -= 5;
            if (new_brightness <= 0) new_brightness = 0;  // Min 5% to keep visible
            ESP_LOGI(TAG, "Brightness: %d → %d", backlight->brightness(), new_brightness);
            
            backlight->SetBrightness(new_brightness);
            auto display = board.GetDisplay();
            display->ShowNotification("Brightness: " + std::to_string(new_brightness));
          }
          break;
        case TOUCH_GESTURE_SWIPE_DOWN:
          {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 5;
            if (volume < 0) {
              volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
          }
          break;
        case TOUCH_GESTURE_SWIPE_UP:
          {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 5;
            if (volume > 100) {
              volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
          }
          break;
        case TOUCH_GESTURE_TAP:
          break;
        case TOUCH_GESTURE_DOUBLE_TAP:
          {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
              ResetWifiConfiguration();
            }
            app.ToggleChatState();
          }
          break;
        case TOUCH_GESTURE_LONG_PRESS:
          ESP_LOGW(TAG, "Long Press at (%d, %d)", x, y);
          {
            // Toggle menu on long press
            this->ToggleMenu();
          }
          break;
        default:
            break;
      } });
    ESP_LOGI(TAG, "Touch screen is ready - try touching now...");
  }
#endif

  void InitializeButtons() {
    boot_button_.OnMultipleClick([this]() {
        ResetWifiConfiguration();
    }, 5);

    boot_button_.OnClick([this]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      app.ToggleChatState();
    });
  }

  void InitializeTools() {
    static LampController lamp(BUILTIN_LED_GPIO);
  }

  void InitializeMenu() {
    auto lcd_display = static_cast<LcdDisplay*>(display_);
    if (!lcd_display) return;
    
    lvgl_port_lock(0);
    lv_obj_t* screen = lv_screen_active();
    if (!screen) {
      ESP_LOGE(TAG, "Cannot get LVGL screen for menu");
      lvgl_port_unlock();
      return;
    }
    
    menu_ui_ = new MenuUI(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lvgl_port_unlock();
    
    // Setup menu callbacks
    menu_ui_->SetCallback(MENU_ACTION_ASSISTANT, [](void* data) {
      ESP_LOGI(TAG, "Menu: Trợ lý selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      auto& app = Application::GetInstance();
      app.ToggleChatState();
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_CLOCK, [](void* data) {
      ESP_LOGI(TAG, "Menu: Đồng hồ selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      board->ToggleAnalogClock();
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_SD_CARD, [](void* data) {
      ESP_LOGI(TAG, "Menu: Thẻ nhớ selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      auto& app = Application::GetInstance();
      auto sd_music = app.GetSdMusic();
      if (sd_music) {
        sd_music->play();
        board->GetDisplay()->SetChatMessage("system", "🎵 Đang phát nhạc từ thẻ nhớ");
      } else {
        board->GetDisplay()->SetChatMessage("system", "❌ Không tìm thấy thẻ nhớ");
      }
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_RADIO, [](void* data) {
      ESP_LOGI(TAG, "Menu: Radio selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      
      // Get radio and play VOV station
      auto& app = Application::GetInstance();
      auto radio = app.GetRadio();
      if (radio) {
        if (radio->IsPlaying()) {
          radio->Stop();
          board->GetDisplay()->SetChatMessage("system", "📻 Radio đã dừng");
        } else {
          // Play VOV1 by default
          bool ok = radio->PlayStation("VOV1");
          if (ok) {
            board->GetDisplay()->SetChatMessage("system", "📻 Đang phát VOV1\nĐài tiếng nói Việt Nam");
          } else {
            board->GetDisplay()->SetChatMessage("system", "❌ Không thể kết nối radio");
          }
        }
      } else {
        board->GetDisplay()->SetChatMessage("system", "❌ Radio chưa được khởi tạo");
      }
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_LUNAR, [](void* data) {
      ESP_LOGI(TAG, "Menu: Âm lịch selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      
      // Get current lunar date
      time_t now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      
      LunarDateInfo lunar = LunarCalendar::GetLunarInfo(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1, 
        timeinfo.tm_mday,
        timeinfo.tm_hour
      );
      
      char msg[256];
      snprintf(msg, sizeof(msg), 
        "🌙 Âm lịch hôm nay\n\n"
        "%s\n"
        "%s\n"
        "%s\n"
        "%s",
        lunar.lunar_date_str,
        lunar.year_can_chi,
        lunar.day_can_chi,
        lunar.truc_str
      );
      board->GetDisplay()->SetChatMessage("system", msg);
    }, this);
    
    menu_ui_->SetCallback(MENU_ACTION_INFO, [](void* data) {
      ESP_LOGI(TAG, "Menu: Thông tin selected");
      auto* board = static_cast<XiaozhiAIIoTEs3n28p*>(data);
      board->HideMenu();
      board->ShowInfoPage();
    }, this);
    
    ESP_LOGI(TAG, "Menu UI initialized");
  }

  void InitAnalogClock() {
    if (analog_clock_) return;  // Already initialized
    
    lvgl_port_lock(0);
    lv_obj_t* screen = lv_disp_get_scr_act(NULL);
    
    // Create analog clock fullscreen (240x320)
    // Position at (0,0), size matches screen
    int diameter = std::min(DISPLAY_WIDTH, DISPLAY_HEIGHT);  // 240 for fullscreen clock face
    int x = 0;
    int y = (DISPLAY_HEIGHT - diameter) / 2;  // Center vertically
    
    analog_clock_ = new AnalogClock(screen, x, y, diameter);
    analog_clock_->Hide();  // Start hidden
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Analog clock initialized (fullscreen %dx%d)", diameter, diameter);
  }

  void ToggleAnalogClock() {
    if (!analog_clock_) {
      InitAnalogClock();
    }
    
    if (clock_visible_) {
      lvgl_port_lock(0);
      analog_clock_->StopAutoUpdate();
      analog_clock_->Hide();
      lvgl_port_unlock();
      clock_visible_ = false;
      ESP_LOGI(TAG, "Analog clock hidden");
    } else {
      lvgl_port_lock(0);
      analog_clock_->Show();
      analog_clock_->UpdateTime();
      analog_clock_->StartAutoUpdate();
      lvgl_port_unlock();
      clock_visible_ = true;
      ESP_LOGI(TAG, "Analog clock shown");
    }
  }

  void ShowInfoPage() {
    if (info_visible_) return;
    
    lvgl_port_lock(0);
    
    // Create fullscreen info container
    lv_obj_t* screen = lv_disp_get_scr_act(NULL);
    info_page_ = lv_obj_create(screen);
    lv_obj_set_size(info_page_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(info_page_, 0, 0);
    lv_obj_set_style_bg_color(info_page_, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(info_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_page_, 0, 0);
    lv_obj_set_style_pad_all(info_page_, 10, 0);
    lv_obj_set_scrollbar_mode(info_page_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(info_page_, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t* title = lv_label_create(info_page_);
    lv_label_set_text(title, "🤖 Xiaozhi AI IOT Việt Nam");
    lv_obj_set_style_text_font(title, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, DISPLAY_WIDTH - 20);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Info content - scrollable
    lv_obj_t* content = lv_label_create(info_page_);
    lv_label_set_text(content, 
      "\n\n📌 Phiên bản: 1.0.0\n\n"
      "👥 Team phát triển:\n"
      "   Xiaozhi AI IoT Việt Nam\n\n"
      "🌐 Website:\n"
      "   www.xiaozhi-ai-iot.vn\n\n"
      "━━━━━━━━━━━━━━━━━━\n\n"
      "✨ Tính năng:\n\n"
      "• Trợ lý AI giọng nói\n"
      "  Hỗ trợ tiếng Việt\n\n"
      "• Đồng hồ Analog\n"
      "  Hiển thị đẹp mắt\n\n"
      "• Âm lịch Việt Nam\n"
      "  Xem ngày tháng âm\n\n"
      "• Phát nhạc SD Card\n"
      "  Hỗ trợ MP3/WAV\n\n"
      "• Radio VOV\n"
      "  Nghe đài tiếng nói VN\n\n"
      "• Điều khiển IoT\n"
      "  Nhà thông minh\n\n"
      "━━━━━━━━━━━━━━━━━━\n\n"
      "📱 Liên hệ hỗ trợ:\n"
      "   www.xiaozhi-ai-iot.vn\n\n"
      "💡 Vuốt lên/xuống để xem thêm\n"
      "👆 Chạm để đóng"
    );
    lv_obj_set_style_text_font(content, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(content, lv_color_white(), 0);
    lv_obj_set_width(content, DISPLAY_WIDTH - 30);
    lv_obj_align(content, LV_ALIGN_TOP_LEFT, 5, 30);
    lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP);
    
    lvgl_port_unlock();
    
    info_visible_ = true;
    // Enable bypass for scroll handling
    if (touch_) {
      touch_->SetBypassGesture(true);
    }
    ESP_LOGI(TAG, "Info page shown");
  }

  void HideInfoPage() {
    if (!info_visible_) return;
    
    lvgl_port_lock(0);
    if (info_page_) {
      lv_obj_delete(info_page_);
      info_page_ = nullptr;
    }
    lvgl_port_unlock();
    
    info_visible_ = false;
    if (touch_) {
      touch_->SetBypassGesture(false);
    }
    ESP_LOGI(TAG, "Info page hidden");
  }

  void ShowMenu() {
    if (menu_ui_ && !menu_visible_) {
      lvgl_port_lock(0);
      menu_ui_->Show();
      lvgl_port_unlock();
      menu_visible_ = true;
      // Bypass gesture detection so LVGL can handle button clicks
      if (touch_) {
        touch_->SetBypassGesture(true);
      }
      ESP_LOGI(TAG, "Menu shown, gesture bypass enabled");
    }
  }

  void HideMenu() {
    if (menu_ui_ && menu_visible_) {
      lvgl_port_lock(0);
      menu_ui_->Hide();
      lvgl_port_unlock();
      menu_visible_ = false;
      // Restore gesture detection for normal operation
      if (touch_) {
        touch_->SetBypassGesture(false);
      }
      ESP_LOGI(TAG, "Menu hidden, gesture bypass disabled");
    }
  }

  void ToggleMenu() {
    if (menu_visible_) {
      HideMenu();
    } else {
      ShowMenu();
    }
  }

 public:
  XiaozhiAIIoTEs3n28p(): boot_button_(BOOT_BUTTON_GPIO)
  {
    InitializeI2c();
    InitializeSpi();
    InitializeLcdDisplay();
#ifdef CONFIG_TOUCH_PANEL_ENABLE
    InitializeTouch();
#endif
    InitializeButtons();
    InitializeTools();
    InitializeMenu();
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      GetBacklight()->RestoreBrightness();
    }
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual AudioCodec* GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(codec_i2c_bus_, AUDIO_CODEC_I2C_NUM,
      AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
      AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
      AUDIO_CODEC_ES8311_ADDR, true, true);
    return &audio_codec;
  }

  virtual Display *GetDisplay() override { return display_; }

#ifdef CONFIG_TOUCH_PANEL_ENABLE
  virtual LcdTouch *GetTouch() override { return touch_; }
#endif

  virtual Backlight *GetBacklight() override {
    static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                  DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    return &backlight;
  }

#ifdef CONFIG_SD_CARD_MMC_INTERFACE
    virtual SdCard* GetSdCard() override {
#ifdef CARD_SDMMC_BUS_WIDTH_4BIT
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO,
                           CARD_SDMMC_D1_GPIO,
                           CARD_SDMMC_D2_GPIO,
                           CARD_SDMMC_D3_GPIO);
#else
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO);
#endif
        return &sdmmc;
    }
#endif
#ifdef CONFIG_SD_CARD_SPI_INTERFACE
    virtual SdCard* GetSdCard() override {
        static SdSPI sdspi(CARD_SPI_MISO_GPIO,
                           CARD_SPI_MOSI_GPIO,
                           CARD_SPI_SCLK_GPIO,
                           CARD_SPI_CS_GPIO);
        return &sdspi;
    }
#endif

};

DECLARE_BOARD(XiaozhiAIIoTEs3n28p);