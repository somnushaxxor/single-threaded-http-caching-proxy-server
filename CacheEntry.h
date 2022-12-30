#ifndef CACHE_ENTRY_H
#define CACHE_ENTRY_H

#include <string>

class CacheEntry final {
public:
    CacheEntry(const std::string& url);

    ~CacheEntry();

    const std::string& getUrl() const;

    const char* getData();

    void appendData(const char* buffer, size_t length);

    size_t getLength() const;

    void setFinished();

    bool isFinished() const;

    void startReading();

    void finishReading();

    bool isRead() const;

    bool isFailed() const;

    void setFailed();

    bool isUsed();

    void resetUsed();

private:
    const std::string url;
    std::string data;
    bool finished;
    bool failed;
    int readerCount;
    bool used;
};

#endif // CACHE_ENTRY_H
