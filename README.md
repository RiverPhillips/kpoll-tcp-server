# KQueue TCP Server

This is a simple TCP server that uses the KQueue API to handle multiple connections.

## Usage

Build and start the server with the following commands:

```bash
    premake5 gmake2
    make
    ./bin/Debug/KQueueServer
```

To connect to the server, use netcat:

```bash
    nc localhost 8080
```
