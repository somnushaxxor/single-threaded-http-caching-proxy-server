#ifndef CLIENTCONNECTIONHANDLER_H
#define CLIENTCONNECTIONHANDLER_H

#include "CacheEntry.h"
#include "Connection.h"
#include "IConnectionHandler.h"
#include "Proxy.h"
#include "ServerConnectionHandler.h"
#include <string>

enum ErrorState {
    NO_ERROR,
    METHOD_NOT_ALLOWED,
    NOT_FOUND,
    BAD_GATEWAY,
    VERSION_NOT_SUPPORTED,
    PAYLOAD_TOO_LARGE,
    INTERNAL_ERROR
};

class Proxy;

class ServerConnectionHandler;

class ClientConnectionHandler final : public IConnectionHandler {
public:
    ClientConnectionHandler(Proxy& proxy, const int socket_fd);

    ~ClientConnectionHandler() override;

    void handle() override;

    Connection& getConnection() override;

    bool isAlive() const override;

    const std::string& getResourceUrl() const;

    CacheEntry* getCacheEntry();

    void setCacheEntry(CacheEntry* const cache_entry);

    size_t getCacheBytesRead() const;

    void setServerConnectionHandlerClosed();

    bool isReadingFromCache() const;

private:
    Proxy& proxy;
    Connection& connection;
    ServerConnectionHandler* server_connection_handler;
    bool server_connection_handler_closed;
    std::string resource_url;
    CacheEntry* cache_entry;
    bool reading_from_cache;
    size_t cache_bytes_read;
    ErrorState errorState;

    void prepareForSendingErrorMessage();

    bool isRequestLineReceived() const;

    void parseRequestLine(std::string& method, std::string& url, std::string& protocol, std::string& host, int& port,
        std::string& version);

    bool isRequestSupported(const std::string& method, const std::string& protocol, const std::string& version);

    void connectToServer(const std::string& host, const int port);
};

#endif // CLIENTCONNECTIONHANDLER_H
