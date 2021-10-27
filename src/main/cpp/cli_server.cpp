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

using namespace robotkernel;
using namespace string_util;
using namespace cli_bridge;
using namespace std;

cli_server::cli_server(std::shared_ptr<cli_bridge::cli> parent, int port) : 
    runnable(0, 0, parent->name), 
    srv_fd(-1), addr(), parent(parent)
{    
    if(port < 0 || port > 0xffff){
        throw str_exception("Invalid port number: %d", port);
    }

    srv_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (srv_fd == -1) {
        throw str_exception("Unable zo create CLI interface socket (ERRNO: %d)", errno);
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t) port);
    for (int i = 1; i <= 5 && bind(srv_fd, (const sockaddr *) &addr, sizeof(addr)) == -1; ++i) {
        parent->log(warning, "Unable to bind socket on port %d (ERRNO: %d)", ntohs(addr.sin_port), errno);
        addr.sin_port = htons((uint16_t) (port+i));
    }

    // set timeouts
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    setsockopt(srv_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(srv_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

cli_server::~cli_server() {
    if (srv_fd != -1) {
        close(srv_fd);
    }
}

void cli_server::run() {
    listen(srv_fd, 3);

    parent->log(info, "cli_server: waiting for connections on port %d ...\n", ntohs(addr.sin_port));
    while (running()) {
        try {
            add_connection();
        } catch (str_exception &e) {
            if (running()) {
                //parent->log(warning, "%s\n", e.what());
                sleep(1);
            }
        }
    }

    std::unique_lock<std::mutex> lock(connection_list_mutex);
    while (!all.empty()) {
        auto c = all.front();
    }
}

//! \brief Try to create a new CLI connection
void cli_server::add_connection() {
    auto conn = make_shared<cli_connection>(srv_fd, shared_from_this());

    std::unique_lock<std::mutex> lock(connection_list_mutex);
    all.push_front(conn);
}

//! \brief Release a CLI connection
/*!
 * \param[in] conn      Conection to release.
 */
void cli_server::release_connection(std::shared_ptr<cli_connection> conn) {
    std::unique_lock<std::mutex> lock(connection_list_mutex);
    all.remove_if([&](std::shared_ptr<cli_connection> c) { return conn == c; });
}

// ############# cli_connection

cli_connection::cli_connection(int srv_fd, std::shared_ptr<cli_server> server) : 
    conn_fd(-1), addr(), server(server)
{
    socklen_t size = sizeof(addr);
    bzero(&addr, size);
    conn_fd = accept(srv_fd, (struct sockaddr *) &addr, &size);
    if (conn_fd == -1) {
        throw str_exception("cli_connection: accept() error -> %s", strerror(errno));
    }

    server->parent->log(info, "cli_connection: established (%s)\n", getRemoteName().c_str());
    start();
}

cli_connection::~cli_connection() {
    std::unique_lock<std::mutex> lock(connection_mutex);
    server->parent->log(info, "cli_connection: closing connection to %s\n", getRemoteName().c_str());

    stop();
    
    if (conn_fd) {
        close(conn_fd);
        conn_fd = -1;
    }

    server->parent->log(info, "cli_connection: connection closed (%s)\n", getRemoteName().c_str());
}

void cli_connection::run() {
    int N = 256*1; //currently max message size maybe not enough for future
    char* buf = new char[N];
    bzero(buf, N);

    std::string prompt = "robotkernel$ ";
    write(prompt.c_str(), prompt.size());

    while(running()){
        ssize_t num = read(conn_fd, buf, N-1);
        if(num == N-1){
            server->parent->log(error, "Input buffer too small for message!");
            //TODO reallocate input buffer for messages greater than N
            //char* buf = realloc()
        }
        if (num <= 0){
            if(num == -1) {
                switch (errno) {
                    case ETIMEDOUT:
                        server->parent->log(error, "cli_connection: Read timed out (%s)\n", strerror(errno));
                        continue;
                    case EAGAIN:
                    case EINTR:
                        continue;
                    case ECONNRESET:
                        server->parent->log(warning, "cli_connection: Connection reset by %s (%s)\n", getRemoteName().c_str(), strerror(errno));
                        break;
                    default:
                        server->parent->log(error, "cli_connection: Error reading data: %s -> closing connection\n",
                                strerror(errno));
                        break;
                }
            } else if(num == 0){
                server->parent->log(info, "cli_connection: Connection closed by %s\n", getRemoteName().c_str());
            }
            
            server->release_connection(shared_from_this());
            goto FINALLY;
        }
        buf[num] = 0;
        server->parent->log(info, "cli_connection: READ: %s\n", buf);
        server->parent->onCliMessage(this, buf, num);
        write(prompt.c_str(), prompt.size());
    }

FINALLY:    
    free(buf);
}

std::string cli_connection::getRemoteName(){
    return std::string(inet_ntoa(addr.sin_addr)) + ":" + 
        std::to_string(ntohs(addr.sin_port));
}

bool cli_connection::write(const char *msg, size_t len) {
    while (len > 0) {
        ssize_t num = ::write(conn_fd, msg, len);

        if (num == -1) {
            switch (errno) {
                case ETIMEDOUT:
                    server->parent->log(error, 
                            "cli_connection: Read timed out (%s)\n", 
                            strerror(errno));
                    continue;
                case EAGAIN:
                case EINTR:
                    continue;
                case ECONNRESET:
                    server->parent->log(warning, 
                            "cli_connection: Connection reset by %s (%s)\n", 
                            getRemoteName().c_str(), strerror(errno));
                    return false;
                default:
                    server->parent->log(error, 
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

