#include "image_downloader.h"
#include "board.h"
#include "http_client.h"
#include <esp_log.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "ImageDownloader"

ImageDownloader::~ImageDownloader() {
    ClearCache();
}

bool ImageDownloader::DoDownload(const std::string& url, DownloadedImage& image) {
    if (url.empty() || url.length() < 10) {
        ESP_LOGE(TAG, "Invalid URL");
        return false;
    }
    
    ESP_LOGI(TAG, "Downloading image from: %s", url.c_str());
    
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (!network) {
        ESP_LOGE(TAG, "Network not available");
        return false;
    }
    
    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return false;
    }
    
    http->SetHeader("User-Agent", "XiaozhiESP32/1.0");
    http->SetHeader("Accept", "image/*");
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open connection to %s", url.c_str());
        return false;
    }
    
    int status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP error: %d", status_code);
        http->Close();
        return false;
    }
    
    // Read response
    std::string data = http->ReadAll();
    http->Close();
    
    if (data.empty()) {
        ESP_LOGE(TAG, "Empty response");
        return false;
    }
    
    // Allocate memory for image data
    image.data = (uint8_t*)malloc(data.size());
    if (!image.data) {
        ESP_LOGE(TAG, "Failed to allocate memory for image (%zu bytes)", data.size());
        return false;
    }
    
    memcpy(image.data, data.c_str(), data.size());
    image.len = data.size();
    image.url = url;
    image.valid = true;
    
    // Detect format from magic bytes
    if (image.len >= 3) {
        if (image.data[0] == 0xFF && image.data[1] == 0xD8 && image.data[2] == 0xFF) {
            image.format = IMAGE_FORMAT_JPEG;
            ESP_LOGI(TAG, "Detected JPEG format");
        } else if (image.data[0] == 0x89 && image.data[1] == 0x50 && 
                   image.data[2] == 0x4E && image.len >= 4 && image.data[3] == 0x47) {
            image.format = IMAGE_FORMAT_PNG;
            ESP_LOGI(TAG, "Detected PNG format");
        }
    }
    
    ESP_LOGI(TAG, "Downloaded %zu bytes from %s", image.len, url.c_str());
    return true;
}

void ImageDownloader::DownloadAsync(const std::string& url, 
                                     ImageDownloadCallback callback, 
                                     void* user_data) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(url);
        if (it != cache_.end() && it->second.valid) {
            ESP_LOGI(TAG, "Image found in cache: %s", url.c_str());
            if (callback) {
                callback(url, it->second.data, it->second.len, true, user_data);
            }
            return;
        }
    }
    
    // Create task for async download
    struct DownloadTaskParams {
        std::string url;
        ImageDownloadCallback callback;
        void* user_data;
    };
    
    auto* params = new DownloadTaskParams{url, callback, user_data};
    
    xTaskCreate([](void* arg) {
        auto* params = static_cast<DownloadTaskParams*>(arg);
        
        DownloadedImage image;
        bool success = ImageDownloader::GetInstance().DownloadSync(params->url, image);
        
        if (params->callback) {
            params->callback(params->url, 
                            success ? image.data : nullptr, 
                            success ? image.len : 0, 
                            success, 
                            params->user_data);
        }
        
        delete params;
        vTaskDelete(nullptr);
    }, "img_dl", 8192, params, 5, nullptr);
}

bool ImageDownloader::DownloadSync(const std::string& url, DownloadedImage& image) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(url);
        if (it != cache_.end() && it->second.valid) {
            ESP_LOGI(TAG, "Image found in cache: %s", url.c_str());
            image = it->second;
            // Don't transfer ownership of data pointer
            image.data = (uint8_t*)malloc(it->second.len);
            if (image.data) {
                memcpy(image.data, it->second.data, it->second.len);
            }
            return image.data != nullptr;
        }
    }
    
    // Download
    if (!DoDownload(url, image)) {
        return false;
    }
    
    // Add to cache
    AddToCache(url, image);
    
    return true;
}

const DownloadedImage* ImageDownloader::GetCached(const std::string& url) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(url);
    if (it != cache_.end() && it->second.valid) {
        return &it->second;
    }
    return nullptr;
}

void ImageDownloader::AddToCache(const std::string& url, DownloadedImage& image) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if we need to evict
    while (current_cache_size_ + image.len > max_cache_size_ && !cache_.empty()) {
        EvictOldestFromCache();
    }
    
    // Make a copy for cache
    DownloadedImage cached_image;
    cached_image.url = image.url;
    cached_image.len = image.len;
    cached_image.width = image.width;
    cached_image.height = image.height;
    cached_image.format = image.format;
    cached_image.valid = image.valid;
    
    cached_image.data = (uint8_t*)malloc(image.len);
    if (cached_image.data) {
        memcpy(cached_image.data, image.data, image.len);
        cache_[url] = cached_image;
        current_cache_size_ += image.len;
        ESP_LOGI(TAG, "Cached image: %s (%zu bytes, total cache: %zu)", 
                 url.c_str(), image.len, current_cache_size_);
    }
}

void ImageDownloader::EvictOldestFromCache() {
    // Simple FIFO eviction (just remove first element)
    if (!cache_.empty()) {
        auto it = cache_.begin();
        ESP_LOGI(TAG, "Evicting from cache: %s", it->first.c_str());
        if (it->second.data) {
            current_cache_size_ -= it->second.len;
            free(it->second.data);
        }
        cache_.erase(it);
    }
}

void ImageDownloader::ClearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    for (auto& pair : cache_) {
        if (pair.second.data) {
            free(pair.second.data);
        }
    }
    cache_.clear();
    current_cache_size_ = 0;
    
    ESP_LOGI(TAG, "Cache cleared");
}

void ImageDownloader::RemoveFromCache(const std::string& url) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(url);
    if (it != cache_.end()) {
        if (it->second.data) {
            current_cache_size_ -= it->second.len;
            free(it->second.data);
        }
        cache_.erase(it);
        ESP_LOGI(TAG, "Removed from cache: %s", url.c_str());
    }
}

size_t ImageDownloader::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return current_cache_size_;
}

size_t ImageDownloader::GetCacheCount() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.size();
}
