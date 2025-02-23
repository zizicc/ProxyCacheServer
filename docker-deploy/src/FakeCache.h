#ifndef FAKECACHE_H
#define FAKECACHE_H

#include <unordered_map>
#include <memory>
#include "HttpResponse.h"

class CacheManager {
private:
    std::unordered_map<std::string, std::shared_ptr<HttpResponse>> cache;

public:
    bool is_in_cache(const std::string& url) {
        return cache.find(url) != cache.end();
    }

    std::shared_ptr<HttpResponse> get_cached_response(const std::string& url) {
        return is_in_cache(url) ? cache[url] : nullptr;
    }

    void store_response(const std::string& url, std::shared_ptr<HttpResponse> response) {
        cache[url] = response;
    }
};

#endif