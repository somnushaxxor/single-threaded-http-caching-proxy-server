#ifndef ICONNECTIONHANDLER_H
#define ICONNECTIONHANDLER_H

#include "Connection.h"

class IConnectionHandler {
public:
    virtual ~IConnectionHandler() = default;

    virtual void handle() = 0;

    virtual Connection& getConnection() = 0;

    virtual bool isAlive() const = 0;
};

#endif // ICONNECTIONHANDLER_H
