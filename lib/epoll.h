

struct EpollClient {

    bool isRead;

    int fd;

    // We use the port to get to the stream job or a control parameter
    // setter (job).
    struct QsPort *port;

    struct EpollClient *prev, *next;
};


struct Epoll {

    struct EpollClient *clients;

};


