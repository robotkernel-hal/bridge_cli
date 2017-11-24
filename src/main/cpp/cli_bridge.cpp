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

#include "robotkernel/rk_type.h"
#include "robotkernel/helpers.h"
#include "robotkernel/service.h"

#include "cli_bridge.h"
#include "cli_server.h"

#include <functional>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include "string_util/string_util.h"
#include <locale>
#include <vector>

using namespace std;
using namespace std::placeholders;
using namespace robotkernel;
using namespace string_util;

BRIDGE_DEF(cli_bridge, cli_bridge::Client);

using namespace cli_bridge;

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

Client::Client(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_cli", node), 
    cliServer(this, get_as<int>(node, "port", 5094)) 
{
    pthread_mutex_init(&service_map_lock, NULL);
    cliServer.start();
}


Client::~Client() {
    cliServer.stop();
    pthread_mutex_destroy(&service_map_lock);
}


static std::string escape(std::string in) {
    const int N = 3;
    const string escapeStrings[N] = {"\n", "\r", "\t"};
    const string replaceStrings[N] = {"\\n", "\\r", "\\t"};

    for (int i = 0; i < N; ++i) {
        in = string_replace(in, escapeStrings[i], replaceStrings[i]);
    }
    return in;
}

static void skipWhitespace(string &args, size_t *sPos) {
    while (*sPos != string::npos && *sPos < args.length() && args[*sPos] == ' ') {
        (*sPos)++;
    }
}


rk_type Client::parseVectorArg(string &args, string typeName, string paramName, size_t *sPos) {
    char c = args[*sPos];
    if (c != '{') {
        throw str_exception("Parse error for argument: %s -> Vectors must use {} braces", paramName.c_str());
    }
    vector<rk_type> result;
    (*sPos)++;
    for (int i = 0; true; ++i) {
        skipWhitespace(args, sPos);
        if (*sPos >= args.length() || *sPos == string::npos) {
            throw str_exception("Parse error for argument: %s -> Vectors must use {} braces", paramName.c_str());
        }
        c = args[*sPos];
        if (c == ',') {
            (*sPos)++;
            continue;
        } else if (c == '}') {
            break;
        } else {
            result.push_back(parseArg(args, typeName, paramName + format_string("[%d]", i), sPos));
        }
    }
    return rk_type(result);
}


rk_type Client::parseStringArg(string &args, string &value, size_t *sPos) {
    if (args[*sPos] != '"') {
        throw str_exception("Parse error for argument: %s -> Strings must use quotation marks", value.c_str());
    }
    unsigned long strStart = (*sPos)++;
    bool escape = false;
    while (*sPos < args.length()) {
        char c = args[*sPos];
        if (c == '"' && !escape) {
            string arg = args.substr(strStart + 1, (*sPos) - 1);
            rk_type rkType(arg);
            return rkType;
        }
        escape = c == '\\' && !escape;
        (*sPos)++;
    }
    throw str_exception("Parse error for argument: %s -> Strings must use quotation marks", value.c_str());
}


rk_type Client::parseArg(string &args, string typeName, string paramName, size_t *sPos) {
    skipWhitespace(args, sPos);

    if (*sPos >= args.length() || *sPos == string::npos) {
        throw str_exception("Too few arguments: Missing %s", paramName.c_str());
    }

    if (TYPENAME_VECTOR.compare(0, TYPENAME_VECTOR.size(), typeName) == 0) {
        return parseVectorArg(args, typeName.substr(TYPENAME_VECTOR.size()), paramName, sPos);
    } else if (typeName == TYPENAME_STRING) {
        return parseStringArg(args, paramName, sPos);
    } else {
        unsigned long delim = args.find(" ");
        std::string arg = args.substr(*sPos, delim);
        *sPos = delim;
        if (typeName == TYPENAME_INT8) {
            return rk_type((int8_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_INT8) {
            return rk_type((int8_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_INT16) {
            return rk_type((int16_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_INT32) {
            return rk_type((int32_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_INT64) {
            return rk_type((int64_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_UINT8) {
            return rk_type((uint8_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_UINT16) {
            return rk_type((uint16_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_UINT32) {
            return rk_type((uint32_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_UINT64) {
            return rk_type((uint64_t) atol(arg.c_str()));
        } else if (typeName == TYPENAME_FLOAT) {
            return rk_type((float) atof(arg.c_str()));
        } else if (typeName == TYPENAME_DOUBLE) {
            return rk_type(atof(arg.c_str()));
        } else {
            throw str_exception("Unsupported type <%s> (Not implemented yet)", typeName.c_str());
        }
    }
}


void Client::parseArgs(const service_t &svc, std::string &args, service_arglist_t &req) {
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

            rk_type x = parseArg(args, key, value, &sPos);
            req.push_back(x);
        }
    }
}


service_t *Client::parseRequest(string &msg, service_arglist_t &req) {
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
    parseArgs(svc, args, req);

    return &svc;
}


string parseResponse(service_t *svc, service_arglist_t &resp) {
    stringstream response;
    response << "OK" << endl;

    YAML::Node message_definition = YAML::Load(svc->service_definition);
    const YAML::Node &mdResp = message_definition["response"];

    if (mdResp) {
        auto mdIt = mdResp.begin();
        for (auto it = resp.begin(); it != resp.end() && mdIt != mdResp.end(); ++it, ++mdIt) {
            for (const auto& kv : *mdIt) {
                response << kv.second.as<string>() << ": " << it->toString() << endl;
            }
        }
    }

    return response.str();
}


void Client::onCliMessage(cli_bridge::cli_connection *c, char *buf, ssize_t len) {
    string msg(buf, (unsigned long) (buf[len - 1] == '\n' ? len - 1 : len));
    msg = strip(msg);
    if (msg.empty()) {
        return;
    }
    try {
        service_arglist_t req;
        service_t *svc = parseRequest(msg, req);
        string result;
        if (!svc) {
            if (msg == string("!list")) {
                result += "\n";
                for (const auto& kv : service_map) {
                    const service_t &s = kv.second;
                    result += std::string("[") + s.owner + " " + s.name + std::string("]\n") + s.service_definition + "\n";
                }
            } else if (msg == string("!help")) {
                result += "\n";
                result += "'!help': Print this help.\n";
                result += "'!list': Get a list of available services.\n";
            } else {
                result = std::string("ERR: Command or service not found: '") + escape(msg) +
                    string("'\n Use !help to get CLI instructions.\n");
            }
        } else {
            log(info, "Calling: %s.%s %s", svc->owner.c_str(), svc->name.c_str(), svc->service_definition.c_str());

            service_arglist_t resp;
            if (svc->callback(req, resp) != 0) {
                throw str_exception(
                        "CliBridge: Internal error in service call. See previous messages in error log for details.");
            }

            result = parseResponse(svc, resp);
        }

        c->write(result.c_str(), result.length());
    } catch (str_exception e) {
        const string &err = format_string("Exception in service call: %s\n%s\n", msg.c_str(), e.what());
        log(warning, "CliBridge: %s", err.c_str());
        c->write(err.c_str(), err.length());
    }
}



void Client::add_service(const robotkernel::service_t &svc) {
    pthread_mutex_lock(&service_map_lock);
    service_map[std::make_pair(svc.owner, svc.name)] = svc;
    pthread_mutex_unlock(&service_map_lock);
}


void Client::remove_service(const robotkernel::service_t &svc) {
    pthread_mutex_lock(&service_map_lock);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        if ((it->first.first == svc.owner) && (it->first.second == svc.name)) {
            service_map.erase(it);
            break;
        }
    }

    pthread_mutex_unlock(&service_map_lock);
}

