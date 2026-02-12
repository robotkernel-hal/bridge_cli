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

#ifndef ROBOTKERNEL_CLI_BRIDGE_H
#define ROBOTKERNEL_CLI_BRIDGE_H

#include <memory>

#include "robotkernel/bridge_base.h"
#include "robotkernel/service.h"
#include "cli_server.h"

namespace cli_bridge {

class cli : 
    public std::enable_shared_from_this<cli>,
    public robotkernel::bridge_base 
{
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

}; // namespace cli_bridge

#endif // ROBOTKERNEL_CLI_BRIDGE_H

