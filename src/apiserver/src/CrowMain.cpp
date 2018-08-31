// Copyright 2018 Obelisk Inc.

#include "CgMinerTypes.h"
#include "Crow.h"
#include "handlers/Handlers.h"
#include "utils/HttpStatusCodes.h"
#include "utils/utils.h"
#include <iostream>
#include <stdio.h>
#include <unordered_map>

using namespace std;
using namespace std::chrono_literals;

typedef struct {
  time_t sessionExpirationTime;
} SessionInfo;

#define HASHRATE_POLL_PERIOD 1

extern void sendCgMinerCmd(string command, string param, CgMiner::RequestCallback callback);

static unordered_map<string, SessionInfo> activeSessions;

extern void pollForHashrate();

struct AuthMW {
  struct context {
    bool isAuthenticated;
  };

  template <typename AllContext>
  void before_handle(request &req, response &resp, context &ctx, AllContext &all_ctx) {
    // Don't run the middleware for login and logout
    if (boost::starts_with(req.url, "/api/log")) {
      return;
    }

    auto &cookieCtx = all_ctx.template get<CookieParser>();
    string sessionId = cookieCtx.get_cookie("sessionid");
    if (sessionId.length() > 0) {
      // Lookup the session and see if we know about it.
      unordered_map<string, SessionInfo>::iterator it = activeSessions.find(sessionId);
      if (it != activeSessions.end()) {
        SessionInfo sessionInfo = it->second;
        struct tm tmNow = getTimeNow();
        time_t timeNow = mktime(&tmNow);
        double diff = difftime(timeNow, sessionInfo.sessionExpirationTime);
        if (diff <= 0) {
          // Found it - just return success since the client already has a sessionId cookie
          ctx.isAuthenticated = true;
          // Bump up sessionExpirationTime every time the session is used
          struct tm expTime = makeExpirationTime(SESSION_DURATION_SECS);
          sessionInfo.sessionExpirationTime = mktime(&expTime);
          activeSessions[sessionId] = sessionInfo;
          CROW_LOG_DEBUG << "Bumping session expiration time";
          return;
        }

        // Forget this sessionId since it was in the map
        activeSessions.erase(sessionId);
      }
    }
    ctx.isAuthenticated = false;
  }

  void after_handle(request & /*req*/, response & /*res*/, context &ctx) {}
};

bool isAuthenticated(crow::App<crow::CookieParser, AuthMW> &app, const request &req) {
  auto &ctx = app.get_context<AuthMW>(req);
  return ctx.isAuthenticated;
}

void runCrow(int port) {
  // Initialize syslog for this thread
  setlogmask(LOG_UPTO(LOG_NOTICE));
  openlog("apiserver/crow", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
  syslog(LOG_NOTICE, "STARTING apiserver/crow thread");

  CROW_LOG_DEBUG << "runCrow()";
  App<CookieParser, AuthMW> app;
  app.tick(60s, pollForHashrate);

  int counter = 0;
  CROW_ROUTE(app, "/api/counter")
  ([&]() {
    std::ostringstream os;
    os << counter;
    counter++;
    return crow::response(os.str());
  });

  CROW_ROUTE(app, "/api/login").methods("POST"_method)([&](const request &req) {
    // See if the session cookie is already present
    auto &ctx = app.get_context<CookieParser>(req);
    string sessionId = ctx.get_cookie("sessionid");
    if (sessionId.length() > 0) {
      // Lookup the session and see if we know about it.
      unordered_map<string, SessionInfo>::iterator it = activeSessions.find(sessionId);
      if (it != activeSessions.end()) {
        SessionInfo sessionInfo = it->second;
        struct tm tmNow = getTimeNow();
        time_t timeNow = mktime(&tmNow);
        double diff = difftime(timeNow, sessionInfo.sessionExpirationTime);
        if (diff <= 0) {
          // Found it - just return success since the client already has a sessionId cookie
          return response(HttpStatus_OK);
        }

        // Forget this sessionId since it was in the map
        activeSessions.erase(sessionId);
      }
    }

    auto jsonReq = json::load(req.body);
    if (!jsonReq || !jsonReq.has("username") || !jsonReq.has("password")) {
      return crow::response(HttpStatus_Unauthorized);
    }

    string username = jsonReq["username"].s();
    string password = jsonReq["password"].s();

    // TODO: Right now this just decides if the user is authorized.  There is no concept
    //       of access levels.
    if (!checkPassword(username, password, AUTH_FILE)) {
      return crow::response(HttpStatus_Unauthorized);
    }

    // Add a cookie to the header
    stringstream sessionCookie;
    struct tm expTime = makeExpirationTime(SESSION_DURATION_SECS);
    string expTimeStr = formatExpirationTime(expTime);
    sessionId = genSessionId();
    sessionCookie << "sessionid=" << sessionId; // << "; Expires=" << expTimeStr;

    // Remember the sessionId for later
    SessionInfo newSessionInfo = {mktime(&expTime)};
    activeSessions.emplace(sessionId, newSessionInfo);

    crow::response resp;
    CROW_LOG_DEBUG << "Set-Cookie: " << sessionCookie.str();
    resp.add_header("Set-Cookie", sessionCookie.str());
    resp.code = HttpStatus_OK;
    return resp;
  });

  CROW_ROUTE(app, "/api/logout")
      .methods("POST"_method)([&](const request &req, crow::response &resp) {
        // See if the session cookie is already present
        auto &ctx = app.get_context<CookieParser>(req);
        string sessionId = ctx.get_cookie("sessionid");
        string cookieName = "sessionid";
        string cookieValue = "";
        ctx.set_cookie(cookieName, cookieValue);
        resp.end();
      });

  CROW_ROUTE(app, "/api/currUser")
      .methods("GET"_method)([&](const request &req, crow::response &resp) {
        if (!isAuthenticated(app, req)) {
          CROW_LOG_DEBUG << "NOT LOGGED IN";
          resp.code = HttpStatus_Unauthorized;
          resp.end();
          return;
        }

        // TODO: Make the model dynamic and correct
        sendJson("{\"username\": \"admin\", \"deviceModel\": \"SC1|DCR1\"}", resp);
      });

  // NOTE: This is a non-authenticated endpoint, so only info that should be known
  //       publicly should be added here.
  CROW_ROUTE(app, "/api/info").methods("GET"_method)([&](const request &req, crow::response &resp) {
    CROW_LOG_DEBUG << "/api/info";
    handleInfo(req, resp);
  });

  CROW_ROUTE(app, "/api/<path>")
      .methods("GET"_method)([&](const request &req, crow::response &resp, string path) {
        if (!isAuthenticated(app, req)) {
          CROW_LOG_DEBUG << "NOT LOGGED IN";
          resp.code = HttpStatus_Unauthorized;
          resp.end();
          return;
        }

        CROW_LOG_DEBUG << "path='" << path << "'";
        handleGet(path, req, resp);
        CROW_LOG_DEBUG << "handleGet() DONE";
      });

  CROW_ROUTE(app, "/api/action/<path>")
      .methods("POST"_method)([&](const request &req, crow::response &resp, string path) {
        if (!isAuthenticated(app, req)) {
          CROW_LOG_DEBUG << "NOT LOGGED IN";
          resp.code = HttpStatus_Unauthorized;
          resp.end();
          return;
        }

        CROW_LOG_DEBUG << "handleAction() path='" << path << "'";
        handleAction(path, req, resp);
        CROW_LOG_DEBUG << "handleAction() DONE";
      });

  CROW_ROUTE(app, "/api/<path>")
      .methods("POST"_method)([&](const request &req, crow::response &resp, string path) {
        if (!isAuthenticated(app, req)) {
          CROW_LOG_DEBUG << "NOT LOGGED IN";
          resp.code = HttpStatus_Unauthorized;
          resp.end();
          return;
        }

        CROW_LOG_DEBUG << "handleSet() path='" << path << "'";
        handleSet(path, req, resp);
        CROW_LOG_DEBUG << "handleSet() DONE";
      });

  app.port(port).multithreaded().run();
}
