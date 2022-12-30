#include "CacheManager.h"
#include <iostream>
#include <unistd.h>

CacheManager::CacheManager()
{
    alarm(CACHE_CLEAN_ALARM_SECONDS);
}

CacheManager::~CacheManager()
{
    cache_entries.clear();
    std::cout << "Cache manager destroyed!" << std::endl;
}

CacheManager& CacheManager::getInstance()
{
    static CacheManager cache_manager;
    return cache_manager;
}

CacheEntry* CacheManager::get(const std::string& url)
{
    for (auto cache_entry : cache_entries) {
        if (cache_entry->getUrl() == url) {
            return cache_entry.get();
        }
    }
    return nullptr;
}

CacheEntry* CacheManager::create(const std::string& url)
{
    if (cache_entries.size() < CACHE_ENTRIES_LIMIT) {
        auto cache_entry = std::make_shared<CacheEntry>(url);
        cache_entries.push_back(cache_entry);
        std::cout << "New cache entry created: " << url << std::endl;
        return cache_entry.get();
    }
    return nullptr;
}

void CacheManager::remove(const std::string& url)
{

    for (auto it = cache_entries.begin(); it != cache_entries.end(); ++it) {
        auto cache_entry = *it;
        if (cache_entry->getUrl() == url) {
            cache_entries.erase(it);
            std::cout << "Removed cache entry: " << url << std::endl;
            break;
        }
    }
}

void CacheManager::clean()
{
    std::cout << "Cleaning cache..." << std::endl;
    auto it = cache_entries.begin();
    while (it != cache_entries.end()) {
        auto cache_entry = *it;
        if (cache_entry->isUsed()) {
            cache_entry->resetUsed();
            ++it;
        } else {
            it = cache_entries.erase(it);
        }
    }
    std::cout << "Cache cleaned!" << std::endl;
    alarm(CACHE_CLEAN_ALARM_SECONDS);
}
