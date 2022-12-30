#ifndef CACHE_H
#define CACHE_H

#include "CacheEntry.h"
#include <memory>
#include <string>
#include <vector>

class CacheManager final {
public:
    static CacheManager& getInstance();

    CacheManager(CacheManager& other) = delete;

    void operator=(const CacheManager& other) = delete;

    CacheEntry* get(const std::string& url);

    CacheEntry* create(const std::string& url);

    void remove(const std::string& url);

    void clean();

private:
    static const int CACHE_ENTRIES_LIMIT = 100;

    static const unsigned int CACHE_CLEAN_ALARM_SECONDS = 60;

    CacheManager();

    ~CacheManager();

    std::vector<std::shared_ptr<CacheEntry>> cache_entries;
};

#endif // CACHE_H
