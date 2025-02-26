#ifndef FAKECACHE_H
#define FAKECACHE_H

#include <unordered_map>
#include <memory>
#include "HttpResponse.h"

class CacheManager {
private:

public:
    std::unordered_map<std::string, std::shared_ptr<HttpResponse>> cache;

    bool is_in_cache(const std::string& url);

    std::shared_ptr<HttpResponse> get_cached_response(const std::string& url);

    void store_response(const std::string& url, std::shared_ptr<HttpResponse> response);
    
};

#endif