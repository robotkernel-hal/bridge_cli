//
// Created by crem_ja on 2/2/17.
//

/*
 * This file is part of module_ethercat.
 *
 * module_ethercat is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_ethercat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_ethercat; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef __VXWORKS__
#include <vxWorks.h>
#include <sockLib.h>
#endif

#include "cli_bridge.h"
#include "cli_server.h"
#include <unistd.h>
#include <arpa/inet.h>
#include "robotkernel/helpers.h"

using namespace robotkernel;
using namespace cli_bridge;
using namespace std;

cli_server::cli_server(std::shared_ptr<cli_bridge::cli> parent, int port) : 
    runnable(0, 0, parent->name), srv_fd(-1), addr(), parent(parent)
{    
    if (port < 0 || port > 0xffff){
        throw runtime_error(string_printf("Invalid port number: %d", port));
    }

    srv_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (srv_fd == -1) {
        throw runtime_error(string_printf("Unable zo create CLI interface socket (ERRNO: %d)", errno));
    }

    int optval = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

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
    stop();

    if (srv_fd != -1) {
        close(srv_fd);
    }
}

//! \brief Server thread which creates and destroyes connection threads if needed.
void cli_server::run() {
    listen(srv_fd, 3);

    parent->log(info, "cli_server: waiting for connections on port %d ...\n", ntohs(addr.sin_port));

    while (running()) {
        // check all connection if they are still alive
        {
            std::unique_lock<std::mutex> lock(connection_list_mutex);
            all.remove_if([&](const std::shared_ptr<cli_connection>& c) { return !c->running(); });
        }

        try {
            auto conn = make_shared<cli_connection>(srv_fd, shared_from_this());

            std::unique_lock<std::mutex> lock(connection_list_mutex);
            all.push_front(conn);
        } catch (std::exception &e) {
            if (running()) {
                sleep(1);
            }
        }
    }

    parent->log(info, "cli_server: finishing, cleaning up...\n");

    std::unique_lock<std::mutex> lock(connection_list_mutex);
    while (!all.empty()) {
        auto c = all.front();
    }
}

// ############# cli_connection

cli_connection::cli_connection(int srv_fd, std::shared_ptr<cli_server> server) : 
    conn_fd(-1), addr(), server(server)
{
    socklen_t size = sizeof(addr);
    bzero(&addr, size);
    conn_fd = accept(srv_fd, (struct sockaddr *) &addr, &size);
    if (conn_fd == -1) {
        throw runtime_error(string_printf("cli_connection: accept() error -> %s", strerror(errno)));
    }

    server->parent->log(info, "cli_connection: established (%s)\n", getRemoteName().c_str());
    start();
}

cli_connection::~cli_connection() {
    std::unique_lock<std::mutex> lock(connection_mutex);
    server->parent->log(info, "cli_connection: closing connection to %s\n", getRemoteName().c_str());

    // stop does not necessarily join, because runnable may alread be exited
    join();
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
            
            run_flag = false;
            goto FINALLY;
        }
        buf[num] = 0;
        server->parent->log(verbose, "cli_connection: READ: %s\n", buf);
        server->parent->handle_request(shared_from_this(), buf, num);
        write(prompt.c_str(), prompt.size());
    }

FINALLY:    
    free(buf);
    server->parent->log(info, "cli_connection: thread exited\n");
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

