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

#include <robotkernel/runnable.h>

namespace cli_bridge {
#ifdef EMACS
}
#endif

class Client;
class cli_server;

class cli_connection 
{
    private:
        int connFD;
        struct sockaddr_in addr;
        bool stopRequested;
        pthread_t connectionThread;
        pthread_mutex_t lock;
        cli_server* cliServer;

        static void* run(void* args);

    public:
        cli_connection(int socketFD, cli_server* cliServer);
        ~cli_connection();
        void close();
        std::string getRemoteName();
        bool write(const char* msg, size_t len);
};    

class cli_server :
    public robotkernel::runnable
{

    private:
        int socketFD;
        struct sockaddr_in addr;
        void run();

    public:
        Client* client;
        cli_server(Client* client, int port);
        ~cli_server();

        typedef std::list<cli_connection*> List;
        cli_server::List all;
        pthread_mutex_t allConnectionsLock;
};

#ifdef EMACS
{
#endif
}

#endif // PROJECT_CLISERVER_H

