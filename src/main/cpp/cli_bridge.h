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


#include "robotkernel/kernel.h"
#include "robotkernel/service.h"
#include "cli_server.h"

#ifndef ROBOTKERNEL_CLI_BRIDGE_H
#define ROBOTKERNEL_CLI_BRIDGE_H


#include "robotkernel/rk_type.h"
#include "robotkernel/bridge_base.h"

namespace cli_bridge {
#ifdef EMACS
}
#endif

class Client : public robotkernel::bridge_base {
    private:
        robotkernel::service_t* parseRequest(std::string &msg, robotkernel::service_arglist_t &req);
        void parseArgs(const robotkernel::service_t &svc, std::string &args, robotkernel::service_arglist_t &req);
        robotkernel::rk_type parseArg(std::string &args, std::string typeName, std::string paramName, size_t *sPos);
        robotkernel::rk_type parseVectorArg(std::string &args, std::string typeName, std::string paramName, size_t *sPos);
        robotkernel::rk_type parseStringArg(std::string &args, std::string &value, size_t *sPos);
    public:
        //! construct cli_bridge client
        Client(const char*& bridgename, YAML::Node& node);

        //! destruct cli_bridge client
        ~Client();

        void add_service(const robotkernel::service_t &svc);
        void remove_service(const robotkernel::service_t &svc);

        void onCliMessage(cli_bridge::cli_connection* c, char* msg, ssize_t len);

    private:
        //! Server for cli connections
        cli_bridge::cli_server cliServer;
        
        //! services map
        typedef std::map<std::pair<std::string, std::string>, robotkernel::service_t> service_map_t;
        service_map_t service_map;
        pthread_mutex_t service_map_lock;
};

#ifdef EMACS
{
#endif
}

#endif

