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

#include "robotkernel/rk_type.h"
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

static void skipWhitespace(string &args, size_t *sPos) {
    while (*sPos != string::npos && *sPos < args.length() && args[*sPos] == ' ') {
        (*sPos)++;
    }
}

const string TYPENAME_STRING = string("string");
const string TYPENAME_INT8 = string("int8_t");
const string TYPENAME_INT16 = string("int16_t");
const string TYPENAME_INT32 = string("int32_t");
const string TYPENAME_INT64 = string("int64_t");
const string TYPENAME_UINT8 = string("uint8_t");
const string TYPENAME_UINT16 = string("uint16_t");
const string TYPENAME_UINT32 = string("uint32_t");
const string TYPENAME_UINT64 = string("uint64_t");
const string TYPENAME_FLOAT = string("float");
const string TYPENAME_DOUBLE = string("double");
const string TYPENAME_VECTOR = string("vector");

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

static void parse_args(const service_t &svc, std::string &args, service_arglist_t &req) {
    YAML::Node message_definition = YAML::Load(svc.service_definition);
    if (!message_definition["request"]) {
        return;
    }

    const YAML::Node &request = message_definition["request"];
    size_t sPos = 0;
    for (YAML::const_iterator it = request.begin(); it != request.end(); ++it) {
        for (const auto& kv : *it) {
            string key   = kv.first.as<string>();
            string value = kv.second.as<string>();

            if (TYPENAME_VECTOR.compare(0, TYPENAME_VECTOR.size(), key) == 0) {
                char c = args[sPos];
                if (c != '{') {
                    throw runtime_error(string_printf("Parse error for argument: %s -> Vectors must use {} braces", key.c_str()));
                }

#define add_type(name, type, func, ...)                                                         \
                if (name == key) {                                                              \
                    vector<type> result;                                                        \
                    sPos++;                                                                     \
                    for (int i = 0; true; ++i) {                                                \
                        skipWhitespace(args, &sPos);                                            \
                        if (sPos >= args.length() || sPos == string::npos) {                    \
                            throw runtime_error(string_printf("Parse error for argument: %s ->" \
                                        " Vectors must use {} braces", key.c_str()));           \
                        }                                                                       \
                        c = args[sPos];                                                         \
                        if (c == ',') {                                                         \
                            sPos++;                                                             \
                            continue;                                                           \
                        } else if (c == '}') {                                                  \
                            break;                                                              \
                        } else {                                                                \
                            result.push_back((type)func(args.c_str(), __VA_ARGS__));            \
                        }                                                                       \
                    }                                                                           \
                    req.push_back(result);                                                      \
                }

                add_type(TYPENAME_INT8, int8_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT16, int16_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT32, int32_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT64, int64_t, strtoll, NULL, 0)
                else add_type(TYPENAME_UINT8, uint8_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT16, uint16_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT32, uint32_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT64, uint64_t, strtoull, NULL, 0)
                else add_type(TYPENAME_FLOAT, float, strtof, NULL)
                else add_type(TYPENAME_DOUBLE, double, strtod, NULL)
                else { throw runtime_error(string_printf("Unsupported type <%s> (Not implemented yet)", key.c_str())); }

#undef add_type
            } else if (key == TYPENAME_STRING) {
                if (args[sPos] == '"') {
                    unsigned long strStart = sPos++;
                    bool escape = false;
                    bool finish = false;

                    while (sPos < args.length()) {
                        char c = args[sPos];
                        if (c == '"' && !escape) {
                            req.push_back(args.substr(strStart + 1, sPos - 1));
                            finish = true;
                            break;
                        }

                        escape = c == '\\' && !escape;
                        sPos++;
                    }

                    if (!finish) {
                        throw runtime_error(string_printf("Parse error for argument: %s -> Strings must use quotation marks", value.c_str()));
                    }
                } else {
                    // string in not quoted so just take until next space
                    unsigned long delim = args.find(" ");
                    req.push_back(args.substr(sPos, delim));
                    sPos = delim;
                }
            } else {
#define add_type(name, type, func, ...) \
            if (name == key) { req.push_back((type)(func(value.c_str(), __VA_ARGS__))); }

                add_type(TYPENAME_INT8, int8_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT16, int16_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT32, int32_t, strtol, NULL, 0)
                else add_type(TYPENAME_INT64, int64_t, strtoll, NULL, 0)
                else add_type(TYPENAME_UINT8, uint8_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT16, uint16_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT32, uint32_t, strtoul, NULL, 0)
                else add_type(TYPENAME_UINT64, uint64_t, strtoull, NULL, 0)
                else add_type(TYPENAME_FLOAT, float, strtof, NULL)
                else add_type(TYPENAME_DOUBLE, double, strtod, NULL)
                else { throw runtime_error(string_printf("Unsupported type <%s> (Not implemented yet)", key.c_str())); }
            }
#undef add_type
        }
    }
}

service_t *cli::parse_request(string &msg, service_arglist_t &req) {
    unsigned long delim = msg.find(" ");
    std::string param0 = msg.substr(0, delim);
    std::string rest = msg.substr(delim + 1);
    delim = rest.find(" ");
    std::string param1 = rest.substr(0, delim);
    auto svcIt = service_map.find(std::make_pair(param0, param1));
    if (svcIt == service_map.end()) {
        return NULL;
    }
    service_t &svc = svcIt->second;

    string args = (delim == string::npos ? std::string("") : rest.substr(delim + 1));
    parse_args(svc, args, req);

    return &svc;
}


string parse_response(service_t *svc, service_arglist_t &resp) {
    stringstream response;

    YAML::Node message_definition = YAML::Load(svc->service_definition);
    const YAML::Node &mdResp = message_definition["response"];

    YAML::Emitter out;

    if (mdResp) {
        auto mdIt = mdResp.begin();
        out << YAML::BeginSeq;

        for (auto it = resp.begin(); it != resp.end() && mdIt != mdResp.end(); ++it, ++mdIt) {
            out << YAML::BeginMap;
            for (const auto& kv : *mdIt) {
                //response << kv.second.as<string>() << ": " << it->to_string() << endl;
#define add_type(name, type, fmt)                                               \
                if ((name) == typeid(std::vector<type>)) {                      \
                    std::vector<type> v = *it;                                  \
                    out << YAML::Key << kv.second.as<string>() << YAML::Value;  \
                    out << YAML::BeginSeq;                                      \
                    for (unsigned int i = 0; i < v.size(); ++i){                \
                        out << string_printf(fmt, v[i]);                        \
                    }                                                           \
                    out << YAML::EndSeq;                                        \
                }
                
                add_type(it->type(), int8_t, "%" PRId8)
                else add_type(it->type(), int16_t, "%" PRId16)
                else add_type(it->type(), int32_t, "%" PRId32)
                else add_type(it->type(), int64_t, "%" PRId64)
                else add_type(it->type(), uint8_t, "%" PRIu8)
                else add_type(it->type(), uint16_t, "%" PRIu16)
                else add_type(it->type(), uint32_t, "%" PRIu32)
                else add_type(it->type(), uint64_t, "%" PRIu64)
                else add_type(it->type(), float, "%f")
                else add_type(it->type(), double, "%lf")
                else {
                    out << YAML::Key << kv.second.as<string>() << YAML::Value << it->to_string();
                }
#undef add_type
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }

    response << out.c_str() << endl;
    return response.str();
}


void cli::handle_request(std::shared_ptr<cli_bridge::cli_connection> c, char *buf, ssize_t len) {
    string msg(buf, (unsigned long) (buf[len - 1] == '\n' ? len - 1 : len));
    msg = trim(msg);
    if (msg.empty()) {
        return;
    }
    try {
        service_arglist_t req;
        service_t *svc = parse_request(msg, req);
        string result;
        if (!svc) {
            if (msg == string("!list")) {
                result += "\n";
                for (const auto& kv : service_map) {
                    const service_t &s = kv.second;
                    result += std::string("[") + s.owner + " " + s.name + std::string("]\n") + s.service_definition + "\n";
                }
            } else if (msg == string("!quit")) {
                c->stop();
            } else if (msg == string("!help")) {
                result += "\n";
                result += "'!help': Print this help.\n";
                result += "'!list': Get a list of available services.\n";
                result += "'!quit': Quit CLI.\n";
            } else {
                result = std::string("ERR: Command or service not found: '") + escape(msg) +
                    string("'\n Use !help to get CLI instructions.\n");
            }
        } else {
            log(verbose, "calling: %s.%s %s", svc->owner.c_str(), svc->name.c_str(), svc->service_definition.c_str());

            service_arglist_t resp;
            if (svc->callback(req, resp) != 0) {
                throw runtime_error(
                        "CliBridge: Internal error in service call. See previous messages in error log for details.");
            }

            result = parse_response(svc, resp);
        }

        c->write(result.c_str(), result.length());
    } catch (std::exception& e) {
        const string &err = string_printf("Exception in service call: %s\n%s\n", msg.c_str(), e.what());
        log(warning, "CliBridge: %s", err.c_str());
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

