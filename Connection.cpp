#include "Connection.h"
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(const int socket_fd)
    : socket_fd(socket_fd)
    , buffer_pos(0)
    , closed(false)
    , error_occurred(false)
{
}

Connection::~Connection()
{
    if (!closed) {
        close();
    }
    std::cout << "Connection destroyed!" << std::endl;
}

bool Connection::isClosed() const
{
    if (closed) {
        return true;
    }
    return false;
}

void Connection::close()
{
    closed = true;
    ::close(socket_fd);
    std::cout << "Connection closed!" << std::endl;
}

void Connection::receiveData()
{
    size_t bytes_read = read(socket_fd, buffer + buffer_pos, BUFFER_SIZE - buffer_pos);
    if (bytes_read == 0) {
        close();
    } else if (bytes_read == -1) {
        error_occurred = true;
        close();
    } else {
        buffer_pos += bytes_read;
    }
}

void Connection::sendData()
{
    size_t bytes_written = write(socket_fd, buffer, buffer_pos);
    if (bytes_written == -1) {
        error_occurred = true;
        close();
    } else {
        memmove(buffer, buffer + bytes_written, buffer_pos - bytes_written);
        buffer_pos -= bytes_written;
    }
}

char* Connection::getBuffer()
{
    return buffer;
}

int Connection::getSocketFd() const
{
    return socket_fd;
}

void Connection::forwardData(Connection& destination)
{
    memmove(destination.buffer, buffer, buffer_pos);
    destination.buffer_pos = buffer_pos;
    buffer_pos = 0;
}

size_t Connection::getBufferPos() const
{
    return buffer_pos;
}

bool Connection::isBufferEmpty() const
{
    return buffer_pos == 0;
}

bool Connection::isBufferFull() const
{
    return buffer_pos == BUFFER_SIZE;
}

bool Connection::isErrorOccurred() const
{
    return error_occurred;
}

void Connection::fillBuffer(const char* data, const size_t length)
{
    memmove(buffer, data, length);
    buffer_pos = length;
}

void Connection::resetBuffer()
{
    buffer_pos = 0;
}
