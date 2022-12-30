#include "SignalHandler.h"
#include <unistd.h>

int SignalHandler::sigint_pipefds[2];

int SignalHandler::sigalrm_pipefds[2];

void SignalHandler::sigintHandler(int signum)
{
    write(sigint_pipefds[1], "\n", 1);
}

void SignalHandler::sigalrmHandler(int signum)
{
    write(sigalrm_pipefds[1], "\n", 1);
}
