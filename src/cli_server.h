//! command line interface robotkernel bridge
/*!
 * author: Jan Cremer <jan.cremer@dlr.de>, Robert Burger <robert.burger@dlr.de>
 */

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

#ifndef PROJECT_CLISERVER_H
#define PROJECT_CLISERVER_H

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <list>
#include <deque>
#include <mutex>
#include <memory>
#include <string>
#include <functional>

#include <robotkernel/runnable.h>

namespace cli_bridge {
#ifdef EMACS
}
#endif

class cli;
class cli_server;

class cli_connection : 
    public std::enable_shared_from_this<cli_connection>,
    public robotkernel::runnable
{
    private:
        int conn_fd;                        //!< \brief Connection socket file descriptor.
        struct sockaddr_in addr;            //!< \brief Socket address information.
        std::shared_ptr<cli_server> server; //!< \brief Parent server instance.

        //! \brief Connection handle thread.
        void run();

    public:
        cli_connection(int srv_fd, std::shared_ptr<cli_server> cliServer);

        ~cli_connection();
        
        std::string getRemoteName();

        bool write(const char* msg, size_t len);
};    

class cli_server :
    public std::enable_shared_from_this<cli_server>,
    public robotkernel::runnable
{
    private:
        int srv_fd;
        struct sockaddr_in addr;

        //! \brief Server thread which creates and destroyes connection threads if needed.
        void run();

    public:
        std::shared_ptr<cli> parent;

        typedef std::list<std::shared_ptr<cli_connection> > connection_list_t;
        cli_server::connection_list_t all;      //!< \brief List with all active connections.
        std::mutex connection_list_mutex;       //!< \brief Mutex to lock list on access.

    public:
        cli_server(std::shared_ptr<cli> parent, int port);
        ~cli_server();
};

#ifdef EMACS
{
#endif
}

#endif // PROJECT_CLISERVER_H

