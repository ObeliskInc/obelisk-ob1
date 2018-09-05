// Copyright 2018 Obelisk Inc.

#include "CgMinerTypes.h"
#include "Crow.h"
#include "utils/CgMinerUtils.h"
#include "utils/SafeQueue.h"
#include "utils/utils.h"
#include <array>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <stdio.h>
#include <string>
#include <syslog.h>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;
using namespace crow;

SafeQueue<CgMiner::Request> gRequestQueue;

class CgMinerClient {
public:
  CgMinerClient(string addr, string p) : address(addr), port(p), exit(false) {}

  void start() {
    // Initialize syslog for this thread
    setlogmask(LOG_UPTO(LOG_NOTICE));
    openlog("apiserver/cgminer", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "STARTING apiserver/cgminer thread");

    while (!exit) {
    wait:
      CgMiner::Request req = gRequestQueue.dequeue();
      CgMiner::Response lastResp(0, "", "");

      for (auto cmd : req.commands) {
        // CROW_LOG_DEBUG << "Connecting for cgminer command: " << cmd.first << ", " << cmd.second;
        if (!resolveAndConnect()) {
          CgMiner::Response resp = {CGMINER_ERROR, "Unable to connect to obelisk-miner", ""};
          req.callback(resp);
          goto wait;
        }

        // CROW_LOG_DEBUG << "Calling sendCommand(" << cmd.first << ", " << cmd.second << ")";
        CgMiner::Response resp = sendCommand(cmd.first, cmd.second);
        lastResp = resp;
        // CROW_LOG_DEBUG << "Send/receive complete";
        // CROW_LOG_DEBUG << "Data = " << resp.json;
        // int i = 0;
        // while (i < resp.json.length()) {
        //   CROW_LOG_DEBUG << resp.json.substr(i, 80);
        //   i += 80;
        // }
      }

      // Call the callback with the last response now that all commands are done
      // TODO: Could instead return an array of responses if we need access to the individual
      // resps.
      req.callback(lastResp);
    }
  }

private:
#define MAX_RETRIES 5
  bool resolveAndConnect() {
    boost::system::error_code error;
    tcp::resolver::iterator it;
    tcp::resolver::query q{address, port};

    // Resolve
    int retryCount = 0;
    while (retryCount < MAX_RETRIES) {
      it = resolv.resolve(q, error);
      if (!error) {
        break;
      }

      CROW_LOG_DEBUG << "Waiting to resolve...";
      usleep(1000000);
      retryCount++;
    }
    if (retryCount == MAX_RETRIES) {
      return false;
    }

    // Connect
    retryCount = 0;
    while (retryCount < MAX_RETRIES) {
      tcp_socket.connect(*it, error);
      if (!error) {
        return true;
      }
      CROW_LOG_DEBUG << "Waiting for miner API connection...";
      usleep(1000000);
      retryCount++;
    }
    return false;
  }

  CgMiner::Response sendCommand(string command, string param) {
    json::wvalue jsonReq = json::load("{}");
    if (param.length() > 0) {
      jsonReq["parameter"] = param;
    }
    jsonReq["command"] = command;
    string strReq = json::dump(jsonReq);
    CROW_LOG_DEBUG << "Sending: " << strReq;
    boost::system::error_code error;

    size_t bytesSent = tcp_socket.send(buffer(strReq.c_str(), strReq.length()), 0, error);
    if (error || bytesSent != strReq.length()) {
      CROW_LOG_DEBUG << "error=" << error << " bytesSent=" << bytesSent;
      CgMiner::Response resp{CGMINER_ERROR, "Invalid number of bytes sent", ""};
      return resp;
    }

    bool readComplete = false;
    string partialResponse = "";
    int numReadAttempts = 0;
    while (!readComplete) {
      size_t bytesRead = tcp_socket.read_some(buffer(responseArr), error);

      string responseStr(responseArr.begin(), responseArr.begin() + bytesRead);
      partialResponse += responseStr;

      // CROW_LOG_DEBUG << "partialResponse: `" << partialResponse
      //      << "`"
      //         " bytesRead = "
      //      << bytesRead << "  error = " << error;

      try {
        // Parse to check if it's valid JSON
        json::rvalue json = json::load(partialResponse);

        // The partialResponse is actually a full response, so return it
        if (!error && json) {
          CgMiner::Response resp{CGMINER_SUCCESS, "", partialResponse};
          // Other side already closed, but cleanup our side too
          tcp_socket.close();
          return resp;
        }

        // Some error or invalid json
        CgMiner::Response resp{CGMINER_ERROR, "Invalid JSON received", ""};
        return resp;

      } catch (const std::exception& exc) {
        // Parsing exception means we still need to read more data
        CROW_LOG_DEBUG << "Waiting for more JSON data...";
        numReadAttempts++;
        if (numReadAttempts > 5) {
          readComplete = true;
        }
      }
    }

    // If we have tried this many times, but haven't read the json, we are probably
    // out of sync somehow.
    CgMiner::Response resp{CGMINER_ERROR, "Invalid response", ""};
    return resp;
  }

private:
  string address;
  string port;
  bool exit;
  io_service ioservice;
  tcp::resolver resolv{ioservice};
  tcp::socket tcp_socket{ioservice};
  std::array<char, 4096> responseArr;
};

void *runCgMiner(void *) {
  while (true) {
    try {
      CROW_LOG_DEBUG << "runCgMiner()";
      CgMinerClient client("127.0.0.1", "4028");
      client.start();
    } catch(const std::exception& exc) {
      CROW_LOG_ERROR << "CgMinerMain EXCEPTION: " << exc.what();
    }
  }
  return NULL;
}
