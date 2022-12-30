#include "ServerConnectionHandler.h"
#include "CacheManager.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <unistd.h>

static const std::string ERROR_MESSAGE_SERVER_HEADERS_TOO_LARGE = "HTTP/1.0 500 Internal Server Error\r\n\r\n<html><head><title>500 Internal Server Error</title></head><body><h2>500 Internal Server Error</h2><h3>Server response headers are too large!</h3></body></html>\r\n";

ServerConnectionHandler::ServerConnectionHandler(Proxy& proxy, const int socket_fd,
    ClientConnectionHandler& client_connection_handler)
    : proxy(proxy)
    , connection(*new Connection(socket_fd))
    , receiving_response_body(false)
    , client_connection_handler(client_connection_handler)
    , cache_entry(nullptr)
    , writing_to_cache(false)
    , client_connection_handler_closed(false)
{
}

ServerConnectionHandler::~ServerConnectionHandler()
{
    delete &connection;
    std::cout << "Server connection handler destroyed!" << std::endl;
}

void ServerConnectionHandler::handle()
{
    if (proxy.getPollEntry(connection.getSocketFd())->revents & POLLIN) {
        if (!connection.isClosed()) {
            connection.receiveData();
        }
        if (connection.isClosed()) {
            if (connection.isErrorOccurred()) {
                if (writing_to_cache) {
                    cache_entry->setFailed();
                } else if (!client_connection_handler_closed) {
                    client_connection_handler.getConnection().close();
                }
            } else {
                if (writing_to_cache) {
                    cache_entry->setFinished();
                } else if (!client_connection_handler_closed) {
                    client_connection_handler.setServerConnectionHandlerClosed();
                    proxy.getPollEntry(client_connection_handler.getConnection().getSocketFd())->events = POLLOUT;
                } else {
                    connection.close();
                }
            }
            return;
        }
        if (receiving_response_body) {
            if (writing_to_cache) {
                if (!cache_entry->isRead()) {
                    CacheManager::getInstance().remove(cache_entry->getUrl());
                    connection.close();
                    return;
                }
                cache_entry->appendData(connection.getBuffer(), connection.getBufferPos());
                connection.resetBuffer();
            } else if (!client_connection_handler_closed) {
                connection.forwardData(client_connection_handler.getConnection());
                proxy.getPollEntry(connection.getSocketFd())->events = 0;
                proxy.getPollEntry(client_connection_handler.getConnection().getSocketFd())->events = POLLOUT;
            } else {
                connection.close();
            }
        } else if (areHeadersReceived()) {
            std::string response_status_code;
            parseResponseStatusCode(response_status_code);
            if (response_status_code == "200" && !client_connection_handler_closed) {
                cache_entry = CacheManager::getInstance().create(
                    client_connection_handler.getResourceUrl());
                if (cache_entry != nullptr) {
                    writing_to_cache = true;
                    client_connection_handler.setCacheEntry(cache_entry);
                }
            }
            if (client_connection_handler_closed) {
                connection.close();
                return;
            }
            receiving_response_body = true;
            if (writing_to_cache) {
                cache_entry->appendData(connection.getBuffer(), connection.getBufferPos());
                connection.resetBuffer();
            } else {
                connection.forwardData(client_connection_handler.getConnection());
                proxy.getPollEntry(connection.getSocketFd())->events = 0;
                proxy.getPollEntry(client_connection_handler.getConnection().getSocketFd())->events = POLLOUT;
            }
        } else if (connection.isBufferFull() && !areHeadersReceived()) {
            connection.close();
            if (!client_connection_handler_closed) {
                client_connection_handler.setServerConnectionHandlerClosed();
                connection.fillBuffer(ERROR_MESSAGE_SERVER_HEADERS_TOO_LARGE.c_str(),
                    ERROR_MESSAGE_SERVER_HEADERS_TOO_LARGE.size());
                connection.forwardData(client_connection_handler.getConnection());
                proxy.getPollEntry(client_connection_handler.getConnection().getSocketFd())->events = POLLOUT;
            }
        }
    } else if (proxy.getPollEntry(connection.getSocketFd())->revents & POLLOUT) {
        connection.sendData();
        if (connection.isErrorOccurred()) {
            if (!client_connection_handler_closed) {
                client_connection_handler.getConnection().close();
            }
        } else if (connection.isBufferEmpty()) {
            proxy.getPollEntry(connection.getSocketFd())->events = POLLIN;
        }
    }
}

Connection& ServerConnectionHandler::getConnection()
{
    return connection;
}

bool ServerConnectionHandler::isAlive() const
{
    if (connection.isClosed()) {
        return false;
    }
    return true;
}

bool ServerConnectionHandler::areHeadersReceived()
{
    const char* response_headers_delimiter_pos = strstr(connection.getBuffer(), "\r\n\r\n");
    if (response_headers_delimiter_pos != nullptr && (response_headers_delimiter_pos - connection.getBuffer()) + 4 <= connection.getBufferPos()) {
        return true;
    }
    return false;
}

void ServerConnectionHandler::parseResponseStatusCode(std::string& code)
{
    const char* buffer = connection.getBuffer();
    std::stringstream ss;
    ss << buffer;
    ss >> code >> code;
}

void ServerConnectionHandler::setClientConnectionHandlerClosed()
{
    client_connection_handler_closed = true;
}
