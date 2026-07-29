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

#include "robotkernel/helpers.h"
#include "robotkernel/service.h"

#include "cli_bridge.h"
#include "cli_server.h"

#include <functional>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <vector>
#include <regex>
#include <inttypes.h>

using namespace std;
using namespace robotkernel;
using namespace robotkernel::helpers;

BRIDGE_DEF(cli_bridge, cli_bridge::cli);

using namespace cli_bridge;

static std::string trim(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isspace(*it))
        it++;

    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && isspace(*rit))
        rit++;

    return std::string(it, rit.base());
}

static std::string escape(std::string in) {
    std::regex_replace(in, std::regex("\n"), "\\n");
    std::regex_replace(in, std::regex("\r"), "\\r");
    std::regex_replace(in, std::regex("\t"), "\\t");
    return in;
}

cli::cli(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_cli", node)
{
    server_port = get_as<int>(node, "port", 5094);
}

cli::~cli() {
}

void cli::init() {
    server = make_shared<cli_bridge::cli_server>(shared_from_this(), server_port);
    server->start();
}

//! deinit method
void cli::deinit() {
    server->stop();
    server = nullptr;
}

void cli::handle_request(std::shared_ptr<cli_bridge::cli_connection> c, char *buf, ssize_t len) {
    string msg(buf, (unsigned long) (buf[len - 1] == '\n' ? len - 1 : len));
    msg = trim(msg);
    if (msg.empty()) {
        return;
    }

    try {
        std::stringstream ss(msg);
        std::string svc_owner, svc_name, svc_req, result = "";
        ss >> svc_owner >> svc_name;
        std::getline(ss >> std::ws, svc_req);

        auto svc_it = service_map.find(std::make_pair(svc_owner, svc_name));
        if (svc_it == service_map.end()) {
            if (msg == string("!list")) {
                result += "\n";
                for (const auto& kv : service_map) {
                    const service_t &s = kv.second;
                    result += std::string("[") + s.owner + " " + s.name + std::string("]\n") + s.service_definition + "\n";
                }
            } else if (svc_owner == string("!info")) {
                auto def = robotkernel::get_service_definition(svc_name);
                result += def;
            } else if (msg == string("!quit")) {
                log(info, "event=cli_client_quit\n");
                c->stop();
            } else if (msg == string("!help")) {
                result += "\n";
                result += "'!help': Print this help.\n";
                result += "'!list': Get a list of available services.\n";
                result += "'!info <service definition name>: Get signature for service definition.\n";
                result += "'!quit': Quit CLI.\n";
            } else {
                result = std::string("ERR: Command or service not found: '") + escape(msg) +
                    string("'\n Use !help to get CLI instructions.\n");
            }
        } else {
            auto& svc = svc_it->second;
            log(verbose, "event=execute_service svc_name=%s.%s %s\n", svc.owner.c_str(), svc.name.c_str(), svc.service_definition.c_str());

            YAML::Node req = YAML::Load(svc_req), resp;
            if (svc.callback(req, resp) != 0) {
                throw runtime_error(
                        "CliBridge: Internal error in service call. See previous messages in error log for details.");
            }

            YAML::Emitter emit;
            emit << resp;

            result = std::string(emit.c_str()) + "\n";
        }

        if (result.length() > 0) {
            c->write(result.c_str(), result.length());
        }
    } catch (std::exception& e) {
        const string &err = string_printf("Exception in service call: %s\n%s\n", msg.c_str(), e.what());
        log(warning, "event=execute_service exception=\"%s\"", err.c_str());
        c->write(err.c_str(), err.length());
    }
}



void cli::add_service(const robotkernel::service_t &svc) {
    std::unique_lock<std::mutex> lock(service_map_mutex);
    service_map[std::make_pair(svc.owner, svc.name)] = svc;
}


void cli::remove_service(const robotkernel::service_t &svc) {
    std::unique_lock<std::mutex> lock(service_map_mutex);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        if ((it->first.first == svc.owner) && (it->first.second == svc.name)) {
            it = service_map.erase(it);
            break;
        }
    }
}

