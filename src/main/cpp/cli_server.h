//! command line interface robotkernel bridge
/*!
 * author: Jan Cremer <jan.cremer@dlr.de>, Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of bridge_cli.
 *
 * bridge_cli is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bridge_cli is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.	If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PROJECT_CLISERVER_H
#define PROJECT_CLISERVER_H

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <list>
#include <deque>
#include <mutex>
#include <string>
#include <functional>

namespace cli_bridge {
#ifdef EMACS
}
#endif

class Client;
class CliServer;

class CliConnection {
    private:
        int connFD;
        struct sockaddr_in addr;
        bool stopRequested;
        pthread_t connectionThread;
        pthread_mutex_t lock;
        CliServer* cliServer;

        static void* run(void* args);


    public:
        std::function<void(CliConnection*, char*, ssize_t)> messageHandler;
        typedef std::list<CliConnection*> List;
        static CliConnection::List all;
        static pthread_mutex_t allConnectionsLock;

        CliConnection(int socketFD, CliServer* cliServer);
        ~CliConnection();
        void close();
        std::string getRemoteName();
        bool write(const char* msg, size_t len);
};    

class CliServer {

    private:
        int socketFD;
        struct sockaddr_in addr;
        bool stopRequested;
        pthread_t serverThread;
        static void* run(void* args);

    public:
        Client* client;
        std::function<void(CliConnection*)> onConnectHandler;
        std::function<void(CliConnection*)> onDisconnectHandler;

        CliServer(Client* client, int port);
        ~CliServer();
        void start();
        void stop();
        void onConnect(CliConnection* client);
        void onDisconnect(CliConnection *client);

};

#ifdef EMACS
{
#endif
}


#endif //PROJECT_CLISERVER_H
