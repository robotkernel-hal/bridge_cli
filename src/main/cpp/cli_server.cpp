//
// Created by crem_ja on 2/2/17.
//

#ifdef __VXWORKS__
#include <vxWorks.h>
#include <sockLib.h>
#endif

#include "cli_bridge.h"
#include "cli_server.h"
#include <unistd.h>
#include <arpa/inet.h>
#include "robotkernel/rt_helper.h"
#include "robotkernel/kernel.h"

#define lockMutex(mtx) \
    if(pthread_mutex_lock(&mtx) != 0) {\
        printf("Fatal: Unable to lock mutex in %s:%d  ->  %s", __FILE__, __LINE__, strerror(errno));\
        exit(-1);\
    }

#define unlockMutex(mtx) \
    if(pthread_mutex_unlock(&mtx) != 0) {\
        printf("Fatal: Unable to unlock mutex in %s:%d  ->  %s", __FILE__, __LINE__, strerror(errno));\
        exit(-1);\
    }

using namespace robotkernel;
using namespace string_util;

using namespace cli_bridge;

cli_server::cli_server(Client*client, int port) : 
    runnable(0, 0, client->name), 
    socketFD(-1), addr(), client(client)
{    
    if(port < 0 || port > 0xffff){
        throw str_exception("Invalid port number: %d", port);
    }
    socketFD = socket(PF_INET, SOCK_STREAM, 0);
    if (socketFD == -1) {
        throw str_exception("Unable zo create CLI interface socket (ERRNO: %d)", errno);
    }

    pthread_mutex_init(&allConnectionsLock, NULL);

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t) port);
    for (int i = 1; i <= 5 && bind(socketFD, (const sockaddr *) &addr, sizeof(addr)) == -1; ++i) {
        client->log(warning, "Unable to bind socket on port %d (ERRNO: %d)", ntohs(addr.sin_port), errno);
        addr.sin_port = htons((uint16_t) (port+i));
    }

    // set timeouts
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socketFD, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

cli_server::~cli_server() {
    if (socketFD != -1) {
        close(socketFD);
    }

    pthread_mutex_destroy(&allConnectionsLock);
}

void cli_server::run() {
    listen(socketFD, 3);

    client->log(info, "cli_server: waiting for connections on port %d ...\n", ntohs(addr.sin_port));
    while (running()) {
        try {
            new cli_connection(socketFD, this);
        } catch (str_exception &e) {
            if (running()) {
                //client->log(warning, "%s\n", e.what());
                sleep(1);
            }
        }
    }

    while (!all.empty()) {
        cli_connection *c = all.front();
        c->close();
        delete c;
    }
}

// ############# cli_connection

cli_connection::cli_connection(int socketFD, cli_server* server) : 
    connFD(-1), addr(), connectionThread(), cliServer(server)
{
    pthread_mutex_init(&lock, NULL);
    socklen_t size = sizeof(addr);
    bzero(&addr, size);
    connFD = accept(socketFD, (struct sockaddr *) &addr, &size);
    if (connFD == -1)
        throw str_exception("cli_connection: accept() error -> %s", strerror(errno));
    cliServer->client->log(info, "cli_connection: established (%s)\n", getRemoteName().c_str());
    pthread_create(&connectionThread, NULL, cli_connection::run, this);
}

cli_connection::~cli_connection() {
    close();
    pthread_mutex_destroy(&lock);
}

void* cli_connection::run(void *args) {
    cli_connection* self = (cli_connection *) args;
    int N = 256*1; //currently max message size maybe not enough for future
    char* buf = new char[N];
    bzero(buf, N);

    lockMutex(self->cliServer->allConnectionsLock);
    self->cliServer->all.push_front(self);
    unlockMutex(self->cliServer->allConnectionsLock);

    while(!self->stopRequested){
        ssize_t num = read(self->connFD, buf, N-1);
        if(num == N-1){
            self->cliServer->client->log(error, "Input buffer too small for message!");
            //TODO reallocate input buffer for messages greater than N
            //char* buf = realloc()
        }
        if (num <= 0){
            if(num == -1) {
                switch (errno) {
                    case ETIMEDOUT:
                        self->cliServer->client->log(error, "cli_connection: Read timed out (%s)\n", strerror(errno));
                        continue;
                    case EAGAIN:
                    case EINTR:
                        continue;
                    case ECONNRESET:
                        self->cliServer->client->log(warning, "cli_connection: Connection reset by %s (%s)\n", self->getRemoteName().c_str(), strerror(errno));
                        break;
                    default:
                        self->cliServer->client->log(error, "cli_connection: Error reading data: %s -> closing connection\n",
                                strerror(errno));
                        break;
                }
            } else if(num == 0){
                self->cliServer->client->log(info, "cli_connection: Connection closed by %s\n", self->getRemoteName().c_str());
            }
            self->close();
            delete self;
            goto FINALLY;
        }
        buf[num] = 0;
        self->cliServer->client->log(info, "cli_connection: READ: %s\n", buf);
        self->cliServer->client->onCliMessage(self, buf, num);
    }

FINALLY:    
    free(buf);
    return NULL;
}


void cli_connection::close() {
    lockMutex(lock);
    if(connFD == -1) {
        return;
    }

    cliServer->client->log(info, "cli_connection: closing connection to %s\n", getRemoteName().c_str());
    this->stopRequested = true;

    lockMutex(cliServer->allConnectionsLock);
    cliServer->all.remove_if([this](cli_connection *c) { return this == c; });
    unlockMutex(cliServer->allConnectionsLock);

    if(pthread_self() != connectionThread) {
        if (pthread_cancel(connectionThread) == 0) {
            if (pthread_join(connectionThread, NULL) != 0) {
                cliServer->client->log(warning, "cli_connection: close() Unable to join reader thread: %s", strerror(errno));
            }
        } else {
            cliServer->client->log(warning, "cli_connection: close() Unable to cancel reader thread: %s", strerror(errno));
        }
    }
    ::close(connFD);
    connFD = -1;
    unlockMutex(lock);
    cliServer->client->log(info, "cli_connection: connection closed (%s)\n", getRemoteName().c_str());
}

std::string cli_connection::getRemoteName(){
    return std::string(inet_ntoa(addr.sin_addr)) + ":" + 
        std::to_string(ntohs(addr.sin_port));
}

bool cli_connection::write(const char *msg, size_t len) {
    while (len > 0) {
        ssize_t num = ::write(connFD, msg, len);

        if (num == -1) {
            switch (errno) {
                case ETIMEDOUT:
                    cliServer->client->log(error, 
                            "cli_connection: Read timed out (%s)\n", 
                            strerror(errno));
                    continue;
                case EAGAIN:
                case EINTR:
                    continue;
                case ECONNRESET:
                    cliServer->client->log(warning, 
                            "cli_connection: Connection reset by %s (%s)\n", 
                            getRemoteName().c_str(), strerror(errno));
                    return false;
                default:
                    cliServer->client->log(error, 
                            "cli_connection: Error writing data: %s -> closing connection\n", 
                            strerror(errno));
                    return false;
            }
        }
        msg += num;
        len -= num;
    }

    return true;
}

