#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

class SignalHandler final {
public:
    static int sigint_pipefds[2];

    static int sigalrm_pipefds[2];

    static void sigintHandler(int signum);

    static void sigalrmHandler(int signum);
};

#endif // SIGNALHANDLER_H
