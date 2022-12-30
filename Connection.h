#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstddef>

class Connection final {
public:
    static const size_t BUFFER_SIZE = 1048576;

    explicit Connection(const int socket_fd);

    ~Connection();

    void receiveData();

    void sendData();

    void forwardData(Connection& connection);

    void fillBuffer(const char* data, const size_t length);

    void resetBuffer();

    char* getBuffer();

    int getSocketFd() const;

    size_t getBufferPos() const;

    bool isBufferEmpty() const;

    bool isBufferFull() const;

    bool isClosed() const;

    void close();

    bool isErrorOccurred() const;

private:
    int socket_fd;
    char buffer[BUFFER_SIZE];
    size_t buffer_pos;
    bool closed;
    bool error_occurred;
};

#endif // CONNECTION_H
