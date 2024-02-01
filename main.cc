#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include "common.h"
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/event.h>
#endif
#if defined(__linux__)
#include <sys/epoll.h>
#endif
class TCPServer
{
public:
    TCPServer(int port)
    {
        port = port;
        listener_fd = -1;
        kq = -1;
        epoll_fd = -1;
    }
    void start()
    {
        createAndBindSocket();
        listen(listener_fd, 3);
        eventLoop();
    }

    ~TCPServer()
    {
        if (kq != -1)
        {
            close(kq);
        }
        if (epoll_fd != -1)
        {
            close(epoll_fd);
        }
        if (listener_fd != -1)
        {
            close(listener_fd);
        }
    }

private:
    int port;
    int listener_fd;
    int kq;
    int epoll_fd;

    void createAndBindSocket()
    {
        listener_fd = socket(AF_INET, SOCK_STREAM, 0);
        // Check errno
        if (listener_fd < 0)
        {
            handleError("Error creating socket: ");
        }

        struct sockaddr_in serv_addr;

        // Create socket structure and bind to ip address.
        serv_addr = {};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(PORT);

        // Check errno
        if (bind(listener_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            handleError("Error binding socket: ");
        }

        std::cout << "Listening on port " << PORT << std::endl;
    }

#if defined(__APPLE__) && defined(__MACH__)
    void setupKQueue()
    {
        kq = kqueue();

        struct kevent change_event[4],
            event[4];

        EV_SET(change_event, listener_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

        // Register kevent with the kqueue.
        if (kevent(kq, change_event, 1, NULL, 0, NULL) < 0)
        {
            handleError("Error registering kevent: ");
        }
    }

    void eventLoop()
    {
        setupKQueue();

        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        struct kevent change_event[4],
            event[4];

        int new_events = kevent(kq, NULL, 0, event, MAX_EVENTS, NULL);
        if (new_events < 0)
        {
            handleError("Error handling kevent: ");
        }

        // Start the event loop.
        while (true)
        {
            int new_events = kevent(kq, NULL, 0, event, MAX_EVENTS, NULL);
            if (new_events < 0)
            {
                handleError("Error handling kevent: ");
            }

            std::cout << "New events received. count:" << new_events << std::endl;
            for (int i = 0; i < new_events; i++)
            {

                int event_fd = event[i].ident;

                // Check for EOF. If so, close client fd
                if (event[i].flags & EV_EOF)
                {
                    std::cout << "Closing client fd: " << event[i].ident << std::endl;
                    close(event[i].ident);
                    continue;
                }
                else if (event_fd == listener_fd)
                {
                    std::cout << "New connection received." << std::endl;
                    int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
                    if (client_fd < 0)
                    {
                        handleError("Error accepting connection: ");
                    }

                    // Add the new client fd to the kqueue.
                    EV_SET(change_event, client_fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                    if (kevent(kq, change_event, 1, NULL, 0, NULL) < 0)
                    {
                        handleError("Error registering kevent: ");
                    }
                else if (event[i].filter & EVFILT_READ)
                {
                    char buf[BUFFER_SIZE];
                    int bytes_read = read(event_fd, buf, BUFFER_SIZE);
                    std::cout << "Bytes read: " << bytes_read << std::endl;
                    ssize_t bytes_written = write(polled_events[i].data.fd, buffer, bytes_read);
                    if (bytes_written < 0){
                        handleError("Error writing to client fd: ");
                    }
                }
            }
        }
    }
#endif

#if defined(__linux__)
    // Todo epoll event loop
    void eventLoop(){
        // We want to setup epoll here.
        struct epoll_event event;
        struct epoll_event polled_events[MAX_EVENTS];

        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        // Create epoll instance.
        int epfd = epoll_create1(EPOLL_CLOEXEC);
        if (epfd < 0){
            handleError("Error creating epoll instance: ");
        }

        event.events = EPOLLIN;
        event.data.fd = listener_fd;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, listener_fd, &event) < 0) {
            handleError("Error adding listener fd to epoll: ");
        }

        std::cout << "Epoll instance created. Starting event loop" << std::endl;

        while (true){
            int num_fds = epoll_wait(epfd, polled_events, MAX_EVENTS, -1);
            if (num_fds < 0) {
                handleError("Error polling events: ");
            }

            printf("New events received. count: %d\n", num_fds);

            for (int i = 0; i < num_fds; i++){
                // Handle new connection.
                if( polled_events[i].data.fd == listener_fd){
                    std::cout << "New connection received." << std::endl;

                    int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
                    if (client_fd < 0)
                    {
                        handleError("Error accepting connection: ");
                    }

                    struct epoll_event client_event;
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_fd;

                    // Add the new client fd to epoll.
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        handleError("Error adding client fd to epoll: ");
                    }
                    std::cout << "Added client fd to epoll: " << client_fd << std::endl;
                } else {
                    char buffer[BUFFER_SIZE];
                    ssize_t bytes_read = read(polled_events[i].data.fd, buffer, sizeof(buffer));
                    if (bytes_read < 0){
                        handleError("Error reading from client fd: ");
                    }
                   
                    if (bytes_read == 0){
                        std::cout << "Closing client fd: " << polled_events[i].data.fd << std::endl;
                        close(polled_events[i].data.fd);
                    } else {
                        std::cout << "Bytes read: " << bytes_read << std::endl;
                        // Echo back to client.
                        ssize_t bytes_written = write(polled_events[i].data.fd, buffer, bytes_read);
                        if (bytes_written < 0){
                            handleError("Error writing to client fd: ");
                        }
                    }
                }
            }
        }
    }
#endif

    void handleError(const char *msg)
    {
        std::cerr << msg << strerror(errno) << std::endl;
        exit(1);
    }
};

int main()
{
    TCPServer server(PORT);
    server.start();
    return 0;
}
