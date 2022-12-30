#ifndef SERVERCONNECTIONHANDLER_H
#define SERVERCONNECTIONHANDLER_H

#include "ClientConnectionHandler.h"
#include "Connection.h"
#include "IConnectionHandler.h"
#include "Proxy.h"

class Proxy;

class ClientConnectionHandler;

class ServerConnectionHandler final : public IConnectionHandler {
public:
    ServerConnectionHandler(Proxy& proxy, const int socket_fd, ClientConnectionHandler& client_connection_handler);

    ~ServerConnectionHandler() override;

    void handle() override;

    Connection& getConnection() override;

    bool isAlive() const override;

    void setClientConnectionHandlerClosed();

private:
    Proxy& proxy;
    Connection& connection;
    ClientConnectionHandler& client_connection_handler;
    bool client_connection_handler_closed;
    CacheEntry* cache_entry;
    bool writing_to_cache;
    bool receiving_response_body;

    bool areHeadersReceived();

    void parseResponseStatusCode(std::string& code);
};

#endif // SERVERCONNECTIONHANDLER_H
