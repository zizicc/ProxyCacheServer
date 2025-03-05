#include "CacheManager.h"
#include <iostream>

// Constructor
CacheManager::CacheManager(size_t capacity) : cache_capacity(capacity), logger(Logger::get_instance()) {}

// Check if an entry is expired
bool CacheManager::is_expired(const CacheEntry& entry) const {
    return std::time(nullptr) >= entry.expiry_time;
}

// Check if an entry requires validation (ETag / Last-Modified)
bool CacheManager::requires_validation(const CacheEntry& entry) const {
    return !entry.response->get_header("ETag").empty() || !entry.response->get_header("Last-Modified").empty();
}

// Calculate expiry time from response headers
time_t CacheManager::get_expiry_time(const HttpResponse& response) const {
    time_t expiry_time = std::time(nullptr);

    std::string cache_control = response.get_header("Cache-Control");
    if (!cache_control.empty()) {
        size_t pos = cache_control.find("max-age=");
        if (pos != std::string::npos) {
            int max_age = std::stoi(cache_control.substr(pos + 8));
            expiry_time += max_age;
        }
    } else {
        expiry_time += 86400;
    }

    std::string expires_header = response.get_header("Expires");
    if (!expires_header.empty()) {
        struct tm expiry_tm = {};
        if (strptime(expires_header.c_str(), "%a %b %d %H:%M:%S %Y", &expiry_tm)) {
            time_t expires_at = timegm(&expiry_tm);
            expiry_time = std::max(expiry_time, expires_at);
        } else {
            logger.log_warning(0, "Failed to parse Expires header: " + expires_header);
        }
    } else {
        expiry_time += 86400;
    }

    return expiry_time;
}

// Check if a URL is in the cache
bool CacheManager::is_in_cache(const std::string& url) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cache_map.find(url) != cache_map.end();
}

// Retrieve a cached response if it's still valid
std::shared_ptr<HttpResponse> CacheManager::get_cached_response(int request_id, const std::string& url) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = cache_map.find(url);

    if (it == cache_map.end()) {
        return nullptr;
    }

    CacheEntry& entry = it->second->second;
    if (is_expired(entry)) {
        logger.log_cache_status(request_id, "in cache, but expired at " + entry.response->get_header("Expires"));
        return nullptr;
    }

    if (requires_validation(entry)) {
        logger.log_cache_status(request_id, "in cache, requires validation");
    } else {
        logger.log_cache_status(request_id, "in cache, valid");
    }

    // Move accessed entry to the front (LRU policy)
    cache_list.splice(cache_list.begin(), cache_list, it->second);
    return entry.response;
}

// Store a response in cache
void CacheManager::store_response(int request_id, const std::string& url, std::shared_ptr<HttpResponse> response) {
    if (!response->is_cacheable()) {
        std::string reason = "not cacheable because status is " + response->get_status_line();
        if (!response->get_header("Cache-Control").empty() && response->get_header("Cache-Control").find("no-store") != std::string::npos) {
            reason = "not cacheable because Cache-Control: no-store";
        }
        logger.log_cache_status(request_id, reason);
        return;
    }

    std::lock_guard<std::mutex> lock(cache_mutex);

    // Handle "Vary" header by using a composite cache key
    std::string cache_key = url;
    if (!response->get_header("Vary").empty()) {
        cache_key += "|" + response->get_header("Vary"); // Use a delimiter for safety
    }

    evict_if_needed();  // Ensure cache capacity

    time_t expiry_time = get_expiry_time(*response);
    cache_list.emplace_front(cache_key, CacheEntry{response, expiry_time});
    cache_map[cache_key] = cache_list.begin();

    std::string expires = response->get_header("Expires");
    if (expires.empty()) {
        char buf[100];
        time_t expiry = get_expiry_time(*response); 
        strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", gmtime(&expiry)); 
        expires = std::string(buf);

    }
    logger.log_cache_status(request_id, "cached, expires at " + expires);
}

// Evict least recently used item if cache is full
void CacheManager::evict_if_needed() {
    if (!cache_list.empty() && cache_list.size() >= cache_capacity) {
        auto last = std::prev(cache_list.end());
        std::string evicted_url = last->first;
        logger.log_note(0, "Evicting " + evicted_url + " from cache");
        cache_map.erase(evicted_url);
        cache_list.pop_back();
    }
}

void CacheManager::print_cache_list(const std::list<std::pair<std::string, CacheEntry>>& cache_list) {
    std::cout << "Cache List Contents (LRU order):\n";
    for (const auto& entry : cache_list) {
        const std::string& url = entry.first;
        const CacheEntry& cache_entry = entry.second;
        std::cout << "URL: " << url << " | Expires at: " << cache_entry.expiry_time << "\n";
    }
}