#include "Proxy.h"
#include "CacheManager.h"
#include "ClientConnectionHandler.h"
#include "SignalHandler.h"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const int LISTEN_BACKLOG = 100;

Proxy::Proxy()
    : socket_fd(-1)
{
}

Proxy::~Proxy()
{
    std::cout << "Destroying proxy..." << std::endl;
    if (socket_fd != -1) {
        close(socket_fd);
    }
    pollfds.clear();
    for (auto client_connection_handler : client_connection_handlers) {
        delete client_connection_handler.second;
    }
    client_connection_handlers.clear();
    for (auto server_connection_handler : server_connection_handlers) {
        delete server_connection_handler.second;
    }
    server_connection_handlers.clear();
    std::cout << "Proxy destroyed!" << std::endl;
}

Proxy& Proxy::getInstance()
{
    static Proxy proxy;
    return proxy;
}

void Proxy::start(const int port)
{
    sockaddr_in proxy_addr;
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy_addr.sin_port = htons(port);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return;
    }
    if (bind(socket_fd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) == -1) {
        perror("bind");
        return;
    }
    if (listen(socket_fd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        return;
    }
    std::cout << "Proxy started on port: " << port << std::endl;
    initPoll();
    while (true) {
        int poll_result;
        while ((poll_result = poll(pollfds.data(), pollfds.size(), -1)) == -1 && errno == EINTR) {
            continue;
        }
        if (poll_result == -1) {
            perror("poll");
            return;
        }
        if (pollfds[1].revents & POLLIN) {
            std::cout << "Proxy stopped by signal!" << std::endl;
            return;
        }
        if (pollfds[2].revents & POLLIN) {
            int value;
            if (read(SignalHandler::sigalrm_pipefds[0], &value, 1) != 1) {
                std::cerr << "Failed to clean cache!" << std::endl;
                return;
            }
            CacheManager::getInstance().clean();
        }
        if (pollfds[0].revents & POLLIN) {
            acceptNewClient();
        }
        for (int i = 3; i < pollfds.size(); i++) {
            if (pollfds[i].revents) {
                auto client_it = client_connection_handlers.find(pollfds[i].fd);
                if (client_it != client_connection_handlers.end()) {
                    client_it->second->handle();
                    if (!client_connection_handlers[pollfds[i].fd]->isAlive()) {
                        delete client_it->second;
                        client_connection_handlers.erase(client_it);
                        (pollfds.data() + i)->fd = -1;
                    }
                    continue;
                }
                auto server_it = server_connection_handlers.find(pollfds[i].fd);
                if (server_it != server_connection_handlers.end()) {
                    server_it->second->handle();
                    if (!server_connection_handlers[pollfds[i].fd]->isAlive()) {
                        delete server_it->second;
                        server_connection_handlers.erase(server_it);
                        (pollfds.data() + i)->fd = -1;
                    }
                }
            }
        }
        auto it = pollfds.begin() + 3;
        while (it != pollfds.end()) {
            if ((*it).fd == -1) {
                it = pollfds.erase(it);
            } else {
                ++it;
            }
        }
        for (auto client_connection_handler_pair : client_connection_handlers) {
            auto client_connection_handler = client_connection_handler_pair.second;
            if (client_connection_handler->isReadingFromCache() && (client_connection_handler->getCacheEntry()->isFailed() || client_connection_handler->getCacheEntry()->isFinished() || (client_connection_handler->getCacheBytesRead() != client_connection_handler->getCacheEntry()->getLength()))) {
                getPollEntry(client_connection_handler->getConnection().getSocketFd())->events = POLLOUT;
            }
        }
    }
}

void Proxy::initPoll()
{
    pollfd poll_entry;
    poll_entry.fd = socket_fd;
    poll_entry.events = POLLIN;
    pollfds.push_back(poll_entry);
    poll_entry.fd = SignalHandler::sigint_pipefds[0];
    poll_entry.events = POLLIN;
    pollfds.push_back(poll_entry);
    poll_entry.fd = SignalHandler::sigalrm_pipefds[0];
    poll_entry.events = POLLIN;
    pollfds.push_back(poll_entry);
}

void Proxy::acceptNewClient()
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    int client_socket_fd;
    if ((client_socket_fd = accept(socket_fd, (struct sockaddr*)&addr, &addr_size)) == -1) {
        std::cerr << "Error occurred while accepting new client" << std::endl;
        perror("accept");
        return;
    }
    addPollEntry(client_socket_fd, POLLIN);
    auto client_connection_handler = new ClientConnectionHandler(*this, client_socket_fd);
    client_connection_handlers[client_socket_fd] = client_connection_handler;
    std::cout << "New client accepted! " << client_socket_fd << std::endl;
}

void Proxy::addPollEntry(const int socket_fd, const short events)
{
    pollfd poll_entry;
    poll_entry.fd = socket_fd;
    poll_entry.events = events;
    pollfds.push_back(poll_entry);
}

void Proxy::addServerConnectionHandler(const int socket_fd, ServerConnectionHandler* server_connection_handler)
{
    server_connection_handlers[socket_fd] = server_connection_handler;
}

pollfd* Proxy::getPollEntry(const int socket_fd)
{
    for (size_t i = 0; i < pollfds.size(); i++) {
        if (pollfds[i].fd == socket_fd) {
            return pollfds.data() + i;
        }
    }
    return nullptr;
}
