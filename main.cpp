#include "FdUtils.h"
#include "Proxy.h"
#include "SignalHandler.h"
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Invalid number of arguments! Usage: <port>" << std::endl;
        exit(EXIT_FAILURE);
    }
    char* endptr = nullptr;
    errno = 0;
    int port = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || errno != 0 && port == 0) {
        std::cerr << "Invalid port!" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (pipe(SignalHandler::sigint_pipefds) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (FdUtils::set_fd_nonblock(SignalHandler::sigint_pipefds[1]) == -1) {
        close(SignalHandler::sigint_pipefds[1]);
        close(SignalHandler::sigint_pipefds[1]);
        exit(EXIT_FAILURE);
    }
    if (pipe(SignalHandler::sigalrm_pipefds) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (FdUtils::set_fd_nonblock(SignalHandler::sigalrm_pipefds[1]) == -1) {
        close(SignalHandler::sigint_pipefds[1]);
        close(SignalHandler::sigint_pipefds[1]);
        close(SignalHandler::sigalrm_pipefds[1]);
        close(SignalHandler::sigalrm_pipefds[1]);
        exit(EXIT_FAILURE);
    }

    struct sigaction saint;
    saint.sa_handler = SignalHandler::sigintHandler;
    saint.sa_flags = 0;
    sigemptyset(&saint.sa_mask);
    sigaction(SIGINT, &saint, NULL);
    struct sigaction saalrm;
    saalrm.sa_handler = SignalHandler::sigalrmHandler;
    saalrm.sa_flags = 0;
    sigemptyset(&saalrm.sa_mask);
    sigaction(SIGALRM, &saalrm, NULL);

    signal(SIGPIPE, SIG_IGN); // todo

    Proxy& proxy = Proxy::getInstance();
    proxy.start(port);

    close(SignalHandler::sigint_pipefds[0]);
    close(SignalHandler::sigint_pipefds[1]);
    close(SignalHandler::sigalrm_pipefds[1]);
    close(SignalHandler::sigalrm_pipefds[1]);

    exit(EXIT_SUCCESS);
}
