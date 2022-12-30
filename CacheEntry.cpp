#include "CacheEntry.h"
#include <iostream>

CacheEntry::CacheEntry(const std::string& url)
    : url(url)
    , finished(false)
    , failed(false)
    , readerCount(0)
    , used(false)
{
}

CacheEntry::~CacheEntry()
{
    std::cout << "Cache entry destroyed! " << url << std::endl;
}

const char* CacheEntry::getData()
{
    used = true;
    return data.c_str();
}

void CacheEntry::appendData(const char* buffer, size_t length)
{
    used = true;
    data.append(buffer, length);
}

size_t CacheEntry::getLength() const
{
    return data.length();
}

void CacheEntry::setFinished()
{
    finished = true;
}

bool CacheEntry::isFinished() const
{
    return finished;
}

void CacheEntry::startReading()
{
    used = true;
    ++readerCount;
}

void CacheEntry::finishReading()
{
    --readerCount;
}

bool CacheEntry::isRead() const
{
    return readerCount > 0;
}

const std::string& CacheEntry::getUrl() const
{
    return url;
}

bool CacheEntry::isFailed() const
{
    return failed;
}

void CacheEntry::setFailed()
{
    failed = true;
}

bool CacheEntry::isUsed()
{
    return used;
}

void CacheEntry::resetUsed()
{
    used = false;
}
