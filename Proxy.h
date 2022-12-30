#ifndef PROXY_H
#define PROXY_H

#include "ClientConnectionHandler.h"
#include "ServerConnectionHandler.h"
#include <map>
#include <poll.h>
#include <vector>

class ClientConnectionHandler;

class ServerConnectionHandler;

class Proxy final {
public:
    static Proxy& getInstance();

    Proxy(Proxy& other) = delete;

    void operator=(const Proxy& other) = delete;

    void start(const int port);

    void addPollEntry(const int socket_fd, const short events);

    pollfd* getPollEntry(const int socket_fd);

    void addServerConnectionHandler(const int socket_fd, ServerConnectionHandler* server_connection_handler);

private:
    int socket_fd;
    std::vector<pollfd> pollfds;
    std::map<int, ClientConnectionHandler*> client_connection_handlers;
    std::map<int, ServerConnectionHandler*> server_connection_handlers;

    Proxy();

    ~Proxy();

    void initPoll();

    void acceptNewClient();
};

#endif // PROXY_H
