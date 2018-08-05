// Copyright 2018 Obelisk Inc.

#ifndef CGMINERTYPES_H
#define CGMINERTYPES_H

// Define some stuff used for cgminer communication
#include "Crow.h"
#include <functional>
#include <string>

using namespace std;
using namespace crow;

namespace CgMiner {

class Response;

typedef std::function<void(Response &)> RequestCallback;

class Status {
public:
  string status;
  long when;
  int code;
  string msg;
  string description;

  Status(string s, long w, int c, string m, string d)
      : status(s), when(w), code(c), msg(m), description(d) {}
};

typedef vector<pair<string, string>> Commands;

/* Example Response:

{
  "STATUS":[
    {
      "STATUS":"S",
      "When":1528747236,
      "Code":22,
      "Msg":"CGMiner versions",
      "Description":"cgminer 4.9.2"
    }
  ],
  "VERSION":[
    {
      "CGMiner":"4.9.2",
      "API":"3.6"
    }
  ],
  "id":1
}
*/
class Response {
public:
  int error;
  string errorMsg;
  string json; // Specific to each command
  Response(int e, string msg, string j) : error(e), errorMsg(msg), json(j) {}
};

#define CGMINER_SUCCESS 0
#define CGMINER_ERROR -1

class Request {
public:
  Commands commands;
  bool saveAfter;
  // Use lambdas with closure to provide more context
  RequestCallback callback;
  Request(Commands cmds, RequestCallback cb) : commands(cmds), callback(cb) {}
};

} // namespace CgMiner

#endif
