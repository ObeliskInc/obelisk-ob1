// Copyright 2018 Obelisk Inc.

#include "../Crow.h"

using namespace std;
using namespace crow;

#ifdef __APPLE__
#define AUTH_FILE "./auth.json"
#else
#define AUTH_FILE "/root/auth.json"
#endif
#define INTF_NAME "eth0"

#define SESSION_DURATION_SECS (15 * 60) // 15 minutes
#define MAX_POOLS 3
#define MAX_HASHBOARDS 3

typedef struct hashrate_t {
  time_t time;
  double hashrates[MAX_HASHBOARDS];
} hashrate_t;

typedef std::function<void(std::string, query_string &, const crow::request &, crow::response &)>
    PathHandlerForGet;

typedef std::function<void(std::string, json::rvalue &, const crow::request &, crow::response &)>
    PathHandlerForSet;

typedef std::function<void(std::string, json::rvalue &, const crow::request &, crow::response &)>
    PathHandlerForAction;

void handleGet(string &path, const crow::request &req, crow::response &resp);

void handleSet(string &path, const crow::request &req, crow::response &resp);

void handleAction(string &path, const crow::request &req, crow::response &resp);

void handleInfo(const crow::request &req, crow::response &resp);

void sendError(string error, int code, crow::response &resp);

void sendJson(string json, crow::response &resp);
