#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include "HttpResponse.h"
#include "Logger.h"
#include <unordered_map>
#include <list>
#include <mutex>
#include <string>
#include <memory>
#include <ctime>

struct CacheEntry {
    std::shared_ptr<HttpResponse> response;
    time_t expiry_time;
};

class CacheManager {
private:
    std::unordered_map<std::string, std::list<std::pair<std::string, CacheEntry>>::iterator> cache_map;
    std::list<std::pair<std::string, CacheEntry>> cache_list;
    size_t cache_capacity;
    std::mutex cache_mutex;
    Logger& logger;

    bool is_expired(const CacheEntry& entry) const;
    bool requires_validation(const CacheEntry& entry) const;
    time_t get_expiry_time(const HttpResponse& response) const;

public:
    explicit CacheManager(size_t capacity = 100);
    bool is_in_cache(const std::string& url);
    std::shared_ptr<HttpResponse> get_cached_response(const std::string& url);
    void store_response(int request_id, const std::string& url, std::shared_ptr<HttpResponse> response);
    void evict_if_needed();
};

#endif