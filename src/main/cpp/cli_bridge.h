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

#ifndef ROBOTKERNEL_CLI_BRIDGE_H
#define ROBOTKERNEL_CLI_BRIDGE_H

#include <memory>

#include "robotkernel/rk_type.h"
#include "robotkernel/bridge_base.h"
#include "robotkernel/kernel.h"
#include "robotkernel/service.h"
#include "cli_server.h"

namespace cli_bridge {
#ifdef EMACS
}
#endif

class cli : 
    public std::enable_shared_from_this<cli>,
    public robotkernel::bridge_base 
{
    private:
        robotkernel::service_t* parse_request(std::string &msg, robotkernel::service_arglist_t &req);
        robotkernel::rk_type parse_arg(std::string &args, std::string typeName, std::string paramName, size_t *sPos);

    public:
        //! construct cli_bridge client
        cli(const char*& bridgename, YAML::Node& node);

        //! destruct cli_bridge client
        ~cli();

        //! init method
        void init();

        //! deinit method
        void deinit();

        //! create and register service
        /*!
         * \param svc robotkernel service struct
         */
        void add_service(const robotkernel::service_t &svc);

        //! unregister and remove service
        /*!
         * \param svc robotkernel service struct
         */
        void remove_service(const robotkernel::service_t &svc);

        void handle_request(std::shared_ptr<cli_bridge::cli_connection> c, char* msg, ssize_t len);

    private:
        //! Server for cli connections
        std::shared_ptr<cli_bridge::cli_server> server;

        //! Port on server should listen
        int server_port;
        
        //! services map
        typedef std::map<std::pair<std::string, std::string>, robotkernel::service_t> service_map_t;
        service_map_t service_map;
        std::mutex service_map_mutex;
};

#ifdef EMACS
{
#endif
}

#endif

