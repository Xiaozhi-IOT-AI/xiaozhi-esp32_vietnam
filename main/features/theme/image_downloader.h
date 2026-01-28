#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>

/**
 * Image Downloader for Vietnam Xiaozhi devices
 * Downloads and caches images from server URLs
 * 
 * Features:
 * - Download images from HTTP/HTTPS URLs
 * - Cache images in PSRAM/internal RAM
 * - Decode JPEG/PNG to raw pixel data
 * - Thread-safe operations
 */

// Download result callback
typedef void (*ImageDownloadCallback)(const std::string& url, 
                                       const uint8_t* data, 
                                       size_t len, 
                                       bool success, 
                                       void* user_data);

// Image format
enum ImageFormat {
    IMAGE_FORMAT_UNKNOWN = 0,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_PNG,
    IMAGE_FORMAT_RGB565,
    IMAGE_FORMAT_RGB888
};

// Downloaded image info
struct DownloadedImage {
    std::string url;
    uint8_t* data = nullptr;
    size_t len = 0;
    int width = 0;
    int height = 0;
    ImageFormat format = IMAGE_FORMAT_UNKNOWN;
    bool valid = false;
};

class ImageDownloader {
public:
    static ImageDownloader& GetInstance() {
        static ImageDownloader instance;
        return instance;
    }
    
    // Download image from URL (async with callback)
    void DownloadAsync(const std::string& url, 
                       ImageDownloadCallback callback, 
                       void* user_data = nullptr);
    
    // Download image from URL (sync, blocking)
    bool DownloadSync(const std::string& url, DownloadedImage& image);
    
    // Get cached image (returns nullptr if not cached)
    const DownloadedImage* GetCached(const std::string& url);
    
    // Clear cache
    void ClearCache();
    void RemoveFromCache(const std::string& url);
    
    // Get cache stats
    size_t GetCacheSize() const;
    size_t GetCacheCount() const;
    
    // Set max cache size (in bytes)
    void SetMaxCacheSize(size_t max_size) { max_cache_size_ = max_size; }
    
private:
    ImageDownloader() = default;
    ~ImageDownloader();
    
    // Prevent copying
    ImageDownloader(const ImageDownloader&) = delete;
    ImageDownloader& operator=(const ImageDownloader&) = delete;
    
    bool DoDownload(const std::string& url, DownloadedImage& image);
    void AddToCache(const std::string& url, DownloadedImage& image);
    void EvictOldestFromCache();
    
    std::map<std::string, DownloadedImage> cache_;
    mutable std::mutex cache_mutex_;
    size_t max_cache_size_ = 512 * 1024;  // 512KB default
    size_t current_cache_size_ = 0;
};

#endif // IMAGE_DOWNLOADER_H
