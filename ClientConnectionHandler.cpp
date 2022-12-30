#include "ClientConnectionHandler.h"
#include "CacheManager.h"
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const std::string ERROR_MESSAGE_PAYLOAD_TO_LARGE = "HTTP/1.0 413 Payload Too Large\r\n\r\n<html><head><title>413 Payload Too Large</title></head><body><h2>413 Payload Too Large</h2><h3>HTTP request is too large for the server to process.</h3></body></html>\r\n";
static const std::string ERROR_MESSAGE_METHOD_NOT_ALLOWED = "HTTP/1.0 405 Method Not Allowed\r\n\r\n<html><head><title>405 Method Not Allowed</title></head><body><h2>405 Method Not Allowed</h2><h3>Proxy server does not support the HTTP method used in the request.</h3></body></html>\r\n";
static const std::string ERROR_MESSAGE_NOT_FOUND = "HTTP/1.0 404 Not Found\r\n\r\n<html><head><title>404 Not Found</title></head><body><h2>Server not found.</h2></body></html>\r\n";
static const std::string ERROR_MESSAGE_VERSION_NOT_SUPPORTED = "HTTP/1.0 505 HTTP Version Not Supported\r\n\r\n<html><head><title>505 HTTP Version Not Supported</title></head><body><h2>505 HTTP Version Not Supported</h2><h3>Proxy server does not support the HTTP version used in the request.</h3></body></html>\r\n";
static const std::string ERROR_MESSAGE_BAD_GATEWAY = "HTTP/1.0 502 Bad Gateway\r\n\r\n<html><head><title>502 Bad Gateway</title></head><body><h2>502 Bad Gateway</h2><h3>Unable to connection to server.</h3></body></html>\r\n";
static const std::string ERROR_MESSAGE_INTERNAL_ERROR = "HTTP/1.0 500 Internal Server Error\r\n\r\n<html><head><title>500 Internal Server Error</title></head><body><h2>500 Internal Server Error</h2></body></html>\r\n";

static const int DEFAULT_HTTP_PORT = 80;

ClientConnectionHandler::ClientConnectionHandler(Proxy& proxy, const int socket_fd)
    : proxy(proxy)
    , connection(*new Connection(socket_fd))
    , server_connection_handler(nullptr)
    , server_connection_handler_closed(false)
    , reading_from_cache(false)
    , errorState(NO_ERROR)
    , cache_entry(nullptr)
    , cache_bytes_read(0)
{
}

ClientConnectionHandler::~ClientConnectionHandler()
{
    delete &connection;
    std::cout << "Client connection handler destroyed!" << std::endl;
}

void ClientConnectionHandler::handle()
{ // test server closed connection scenario todo
    if (proxy.getPollEntry(connection.getSocketFd())->revents & POLLOUT) {
        if (errorState != NO_ERROR) {
            connection.sendData();
            if (connection.isErrorOccurred() || connection.isBufferEmpty()) {
                connection.close();
            }
        } else if (reading_from_cache) {
            if (cache_entry->isFailed()) {
                connection.close();
                cache_entry->finishReading();
                return;
            }
            if (connection.isBufferEmpty()) {
                size_t cache_part_size;
                if (Connection::BUFFER_SIZE >= (cache_entry->getLength() - cache_bytes_read)) {
                    cache_part_size = cache_entry->getLength() - cache_bytes_read;
                } else {
                    cache_part_size = Connection::BUFFER_SIZE;
                }
                connection.fillBuffer(cache_entry->getData() + cache_bytes_read, cache_part_size);
                cache_bytes_read += cache_part_size;
            }
            connection.sendData();
            if (connection.isErrorOccurred()) {
                cache_entry->finishReading();
                return;
            }
            if (connection.isBufferEmpty()) {
                if (cache_entry->isFinished()) {
                    if (cache_bytes_read == cache_entry->getLength()) {
                        cache_entry->finishReading();
                        connection.close();
                    }
                } else {
                    if (cache_bytes_read == cache_entry->getLength()) {
                        proxy.getPollEntry(connection.getSocketFd())->events = 0;
                    }
                }
            }
        } else {
            connection.sendData();
            if (connection.isErrorOccurred()) {
                server_connection_handler->getConnection().close();
                server_connection_handler->setClientConnectionHandlerClosed();
                return;
            }
            if (connection.isBufferEmpty()) {
                if (server_connection_handler_closed) {
                    connection.close();
                } else {
                    proxy.getPollEntry(server_connection_handler->getConnection().getSocketFd())->events = POLLIN;
                    proxy.getPollEntry(connection.getSocketFd())->events = 0;
                }
            }
        }
    } else if (proxy.getPollEntry(connection.getSocketFd())->revents & POLLIN) {
        connection.receiveData();
        if (connection.isErrorOccurred()) {
            return;
        }
        if (isRequestLineReceived()) {
            std::string method;
            std::string protocol;
            std::string host;
            int port = DEFAULT_HTTP_PORT;
            std::string version;
            parseRequestLine(method, resource_url, protocol, host, port, version);
            std::cout << "Request: " << method << " " << resource_url << " " << protocol << " " << host << " " << port
                      << " "
                      << version
                      << std::endl;
            if (!isRequestSupported(method, protocol, version)) {
                prepareForSendingErrorMessage();
                return;
            }
            if (CacheManager::getInstance().get(resource_url) != nullptr) {
                cache_entry = CacheManager::getInstance().get(resource_url);
                reading_from_cache = true;
                cache_entry->startReading();
                connection.resetBuffer();
                std::cout << "Reading from cache: " << resource_url << std::endl;
                proxy.getPollEntry(connection.getSocketFd())->events = POLLOUT;
            } else {
                connectToServer(host, port);
                if (errorState != NO_ERROR) {
                    prepareForSendingErrorMessage();
                    return;
                }
                connection.forwardData(server_connection_handler->getConnection());
                proxy.getPollEntry(connection.getSocketFd())->events = 0;
            }
        } else if (connection.isBufferFull() && !isRequestLineReceived()) {
            errorState = PAYLOAD_TOO_LARGE;
            prepareForSendingErrorMessage();
        }
    }
}

bool ClientConnectionHandler::isRequestLineReceived() const
{
    const char* request_delimiter_pos = strstr(connection.getBuffer(), "\r\n\r\n");
    if (request_delimiter_pos != nullptr && (request_delimiter_pos - connection.getBuffer()) + 4 <= connection.getBufferPos()) {
        return true;
    }
    return false;
}

void ClientConnectionHandler::connectToServer(const std::string& host, const int port)
{
    std::cout << "Connecting to " << host << " " << port << "... " << std::endl;
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    hostent* server_hostent = gethostbyname(host.c_str());
    if (server_hostent == nullptr) {
        herror("gethostbyname");
        errorState = NOT_FOUND;
        return;
    }
    memcpy(&server_addr.sin_addr.s_addr, server_hostent->h_addr, server_hostent->h_length);
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
        perror("socket");
        errorState = INTERNAL_ERROR;
        return;
    }
    if (connect(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        errorState = BAD_GATEWAY;
        close(server_socket_fd);
        return;
    }
    proxy.addPollEntry(server_socket_fd, POLLOUT);
    server_connection_handler = new ServerConnectionHandler(proxy, server_socket_fd, *this);
    proxy.addServerConnectionHandler(server_socket_fd, server_connection_handler);
    std::cout << "Connected to " << host << " " << port << "! " << server_socket_fd << std::endl;
}

void ClientConnectionHandler::parseRequestLine(std::string& method, std::string& url, std::string& protocol,
    std::string& host, int& port,
    std::string& version)
{ // refactor todo
    const char* buffer = connection.getBuffer();
    std::stringstream ss;
    ss << buffer;
    ss >> method >> url >> version;
    size_t res = url.find("://");
    if (res != std::string::npos) {
        protocol = url.substr(0, res);
    }
    res += 3;
    size_t pslash = url.find("/", res);
    size_t pport = url.find(':', res);
    if (pport != std::string::npos && pport < pslash) {
        host = url.substr(res, pport - res);
        pport += 1;
        std::string tmp = url.substr(pport, pslash - pport);
        port = atoi(tmp.c_str());
    } else if (res > 2) {
        host = url.substr(res, pslash - res);
    }
}

Connection& ClientConnectionHandler::getConnection()
{
    return connection;
}

bool ClientConnectionHandler::isRequestSupported(const std::string& method, const std::string& protocol,
    const std::string& version)
{
    if (method != "GET") {
        errorState = METHOD_NOT_ALLOWED;
        return false;
    }
    if (version != "HTTP/1.0") {
        errorState = VERSION_NOT_SUPPORTED;
        return false;
    }
    return true;
}

void ClientConnectionHandler::prepareForSendingErrorMessage()
{
    std::string error_message;
    if (errorState == PAYLOAD_TOO_LARGE) {
        error_message = ERROR_MESSAGE_PAYLOAD_TO_LARGE;
    } else if (errorState == NOT_FOUND) {
        error_message = ERROR_MESSAGE_NOT_FOUND;
    } else if (errorState == BAD_GATEWAY) {
        error_message = ERROR_MESSAGE_BAD_GATEWAY;
    } else if (errorState == INTERNAL_ERROR) {
        error_message = ERROR_MESSAGE_INTERNAL_ERROR;
    } else if (errorState == VERSION_NOT_SUPPORTED) {
        error_message = ERROR_MESSAGE_VERSION_NOT_SUPPORTED;
    } else if (errorState == METHOD_NOT_ALLOWED) {
        error_message = ERROR_MESSAGE_METHOD_NOT_ALLOWED;
    }
    connection.fillBuffer(error_message.c_str(), error_message.size());
    proxy.getPollEntry(connection.getSocketFd())->events = POLLOUT;
}

bool ClientConnectionHandler::isAlive() const
{
    if (connection.isClosed()) {
        return false;
    }
    return true;
}

void ClientConnectionHandler::setServerConnectionHandlerClosed()
{
    server_connection_handler_closed = true;
}

const std::string& ClientConnectionHandler::getResourceUrl() const
{
    return resource_url;
}

bool ClientConnectionHandler::isReadingFromCache() const
{
    return reading_from_cache;
}

CacheEntry* ClientConnectionHandler::getCacheEntry()
{
    return cache_entry;
}

size_t ClientConnectionHandler::getCacheBytesRead() const
{
    return cache_bytes_read;
}

void ClientConnectionHandler::setCacheEntry(CacheEntry* const cache_entry)
{
    this->cache_entry = cache_entry;
    reading_from_cache = true;
    cache_entry->startReading();
    connection.resetBuffer();
}
