#include "FakeCache.h"

bool CacheManager::is_in_cache(const std::string& url) {
    return cache.find(url) != cache.end();
}

std::shared_ptr<HttpResponse> CacheManager::get_cached_response(const std::string& url) {
    return is_in_cache(url) ? cache[url] : nullptr;
}

void CacheManager::store_response(const std::string& url, std::shared_ptr<HttpResponse> response) {
    cache[url] = response;
}