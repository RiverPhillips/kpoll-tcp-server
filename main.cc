// C++ Tcp Kqueue Server
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/event.h>

const int PORT = 8080;
const int MAX_EVENTS = 4;
const int BUFFER_SIZE = 1024;

class TCPServer
{
public:
    TCPServer(int port)
    {
        port = port;
        listener_fd = -1;
        kq = -1;
    }
    void start()
    {
        createAndBindSocket();
        listen(listener_fd, 3);
        setupKQueue();
        std::cout << "Waiting for connections..." << std::endl;
        eventLoop();
    }

    ~TCPServer()
    {
        if (listener_fd != -1)
        {
            close(listener_fd);
        }
        if (kq != -1)
        {
            close(kq);
        }
    }

private:
    int port;
    int listener_fd;
    int kq;

    void createAndBindSocket()
    {
        listener_fd = socket(AF_INET, SOCK_STREAM, 0);
        // Check errno
        if (listener_fd < 0)
        {
            handleError("Error creating socket: ");
        }

        struct sockaddr_in serv_addr, client_addr;

        // Create socket structure and bind to ip address.
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(PORT);

        // Check errno
        if (bind(listener_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            handleError("Error binding socket: ");
        }

        std::cout << "Listening on port " << PORT << std::endl;
    }

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
                }
                else if (event[i].filter & EVFILT_READ)
                {
                    char buf[BUFFER_SIZE];
                    int bytes_read = read(event_fd, buf, BUFFER_SIZE);
                    std::cout << "Bytes read: " << bytes_read << std::endl;
                }
            }
        }
    }

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
