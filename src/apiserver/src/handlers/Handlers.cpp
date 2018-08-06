// Copyright 2018 Obelisk Inc.

#include "Handlers.h"
#include "../CgMinerTypes.h"
#include "../Crow.h" // Get the log levels
#include "../utils/HttpStatusCodes.h"
#include "../utils/SafeQueue.h"
#include "../utils/base64.h"
#include "../utils/utils.h"
#include <boost/bind.hpp>
#include <sys/reboot.h>
#include <unistd.h>

using namespace std;
using namespace crow;

// This is the queue we use to send cgminer requests
extern SafeQueue<CgMiner::Request> gRequestQueue;

void sendError(string error, int code, crow::response &resp) {
  json::wvalue jsonErr = json::load("{}");
  jsonErr["error"] = error;
  string jsonStr = json::dump(jsonErr);
  resp.code = code;
  resp.write(jsonStr);
  resp.end();
}

void sendJson(string json, crow::response &resp) {
  resp.add_header("Content-Type", "application/json");
  resp.code = 200;
  resp.write(json);
  resp.end();
}

void sendCgMinerCmds(CgMiner::Commands commands, CgMiner::RequestCallback callback) {
  CgMiner::Request cgMinerReq{commands, callback};
  CROW_LOG_DEBUG << "enqueue request";
  gRequestQueue.enqueue(cgMinerReq);
  CROW_LOG_DEBUG << "enqueue request done";
}

void sendCgMinerCmd(string command, string param, CgMiner::RequestCallback callback) {
  CgMiner::Commands cmds;
  cmds.push_back({command, param});

  sendCgMinerCmds(cmds, callback);
}

bool isCgMinerError(json::rvalue json) { return json["STATUS"][0]["STATUS"].s() == "E"; }

// TODO: This is stupid!
json::rvalue to_rvalue(json::wvalue &v) { return json::load(json::dump(v)); }

// Find the index of the entry with the specified name
// the jsonArr must be an array
int indexOfEntryWithId(json::rvalue jsonArr, string id) {
  if (jsonArr.t() == json::type::List) {
    for (int i = 0; i < jsonArr.size(); i++) {
      json::rvalue entry = jsonArr[i];
      if (entry.has(id)) {
        return i;
      }
    }
  }
  return -1;
}

json::rvalue makeSystemInfoEntry(string name, string value) {
  json::wvalue obj = json::load("{}");
  obj["name"] = name;
  obj["value"] = value;
  return to_rvalue(obj);
}

// Structure to store data from cgminer

#define MAX_HASHRATE_HISTORY_ENTRIES (60 * 60) // 1 hour
hashrate_t hashrate_history_secs[MAX_HASHRATE_HISTORY_ENTRIES];
int num_hashrate_entries = 0;

// Timer callback to poll for updated hashrates
void poll_for_hashrate() {
  CROW_LOG_DEBUG << "Polling for hashrates";

  sendCgMinerCmd("dashdevs", "", [&](CgMiner::Response cgMinerResp) {
    if (cgMinerResp.error) {
      // No data to append - oh well
      return;
    }

    // No error, so build up the response that the user is expecting
    try {
      CROW_LOG_DEBUG << "json = " << cgMinerResp.json;

      json::rvalue cgMinerJson = json::load(cgMinerResp.json);
      time_t now = time(0);
      int values[MAX_HASHBOARDS];
      json::rvalue devsArr = cgMinerJson["devs"]["DEVS"];
      for (int i = 0; i < devsArr.size(); i++) {
        json::rvalue entry = devsArr[i];
        values[i] = entry["mhsRolling"].i();
      }

      // See if we need to move the old data
      int entry_index = num_hashrate_entries;
      if (num_hashrate_entries > MAX_HASHRATE_HISTORY_ENTRIES) {
        // Need to slide the other entries down first, then set new data into the last entry
        memmove(hashrate_history_secs, &hashrate_history_secs[1],
                sizeof(hashrate_t) * (MAX_HASHRATE_HISTORY_ENTRIES - 1));
        entry_index = MAX_HASHRATE_HISTORY_ENTRIES - 1;
      } else {
        num_hashrate_entries++;
      }

      // Record the new data
      hashrate_history_secs[entry_index].time = now;
      for (int i = 0; i < devsArr.size(); i++) {
        hashrate_history_secs[entry_index].hashrates[i] = values[i];
      }

    } catch (...) {
      return;
    }
  });
}

//--------------------------------------------------------------------------------------------------
// INVENTORY HANDLERS
//--------------------------------------------------------------------------------------------------

void getInventoryVersions(string path, query_string &urlParams, const crow::request &req,
                          crow::response &resp) {
  sendCgMinerCmd("version", "", [&](CgMiner::Response cgMinerResp) {
    if (cgMinerResp.error) {
      sendError(cgMinerResp.errorMsg, HttpStatus_InternalServerError, resp);
      return;
    }

    // No error, so build up the response that the user is expecting
    try {
      json::rvalue cgMinerJson = json::load(cgMinerResp.json);
      json::wvalue jsonResp;
      jsonResp["version"] = cgMinerJson["VERSION"];

      sendJson(json::dump(jsonResp), resp);
    } catch (...) {
      sendError("Invalid JSON object", HttpStatus_InternalServerError, resp);
      return;
    }
  });
}

void getInventorySystem(string path, query_string &urlParams, const crow::request &req,
                        crow::response &resp) {
  string osName = getOSName();
  string osVersion = getOSVersion();
  json::wvalue jsonResp = json::load("{}");
  jsonResp["osName"] = osName;
  jsonResp["osVersion"] = osVersion;
  sendJson(json::dump(jsonResp), resp);
}

/*
TODO: Define a standard handler or a handler wrapper to handle error case
[&](CgMiner::Response cgMinerResp) {
                                // For now, this just returns the exact JSON response from cgminer
                                // TODO: Expand code to convert to a cleaner format.
                                if (!cgMinerResp.isValid) {
                                  sendError("Internal server error", HttpStatus_InternalServerError,
                                            resp);
                                }

                                CROW_LOG_DEBUG << cgMinerResp.json;

                                // Otherwise, just forward the JSON on to the client for now
                                // TODO: Should have a way to just return OK/error
                                sendJson(cgMinerResp.json, resp);
                              }
                              */

void getInventoryAsicCount(string path, query_string &urlParams, const crow::request &req,
                           crow::response &resp) {
  sendCgMinerCmd("asccount", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getInventoryDevices(string path, query_string &urlParams, const crow::request &req,
                         crow::response &resp) {
  sendCgMinerCmd("devs", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

//--------------------------------------------------------------------------------------------------
// CONFIG GET HANDLERS
//--------------------------------------------------------------------------------------------------

void getConfigPools(string path, query_string &urlParams, const crow::request &req,
                    crow::response &resp) {
  json::rvalue conf = readCgMinerConfig();

  if (!conf.has("pools")) {
    json::rvalue result = json::load("[]");
    sendJson(json::dump(result), resp);
    return;
  }

  json::rvalue pools = conf["pools"];
  json::wvalue result = json::load("[]");
  for (int i = 0; i < pools.size(); i++) {
    json::rvalue entry = pools[i];
    json::wvalue resEntry = json::load("{}");
    resEntry["worker"] = entry["user"].s();
    resEntry["url"] = entry["url"].s();
    resEntry["password"] = entry["pass"].s();
    result[i] = to_rvalue(resEntry);
  }
  sendJson(json::dump(result), resp);
}

void getConfigNetwork(string path, query_string &urlParams, const crow::request &req,
                      crow::response &resp) {
  json::wvalue jsonResp = json::load("{}");
  jsonResp["hostname"] = getHostname(INTF_NAME);
  jsonResp["ipAddress"] = getIpV4(INTF_NAME);
  jsonResp["subnetMask"] = getSubnetMaskV4(INTF_NAME);
  jsonResp["gateway"] = getDefaultGateway(INTF_NAME);
  jsonResp["dnsServer"] = getDns(INTF_NAME);
  jsonResp["dhcpEnabled"] = getDhcpEnabled(INTF_NAME);
  jsonResp["macAddress"] = getMACAddr(INTF_NAME);
  string jsonStr = json::dump(jsonResp);

  sendJson(jsonStr, resp);
}

void getConfigSystem(string path, query_string &urlParams, const crow::request &req,
                     crow::response &resp) {
  json::wvalue jsonResp = json::load("{}");
  jsonResp["timezone"] = getTimezone();
  string jsonStr = json::dump(jsonResp);

  sendJson(jsonStr, resp);
}

void getConfigMining(string path, query_string &urlParams, const crow::request &req,
                     crow::response &resp) {
  json::wvalue jsonResp = json::load("{}");
  // jsonResp["optimizationMode"] = getOptimizationMode();
  string jsonStr = json::dump(jsonResp);

  sendJson(jsonStr, resp);
}

void getConfigConfig(string path, query_string &urlParams, const crow::request &req,
                     crow::response &resp) {
  sendCgMinerCmd("config", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

//--------------------------------------------------------------------------------------------------
// CONFIG SET HANDLERS
//--------------------------------------------------------------------------------------------------
void setConfigPools(string path, json::rvalue &args, const crow::request &req,
                    crow::response &resp) {
  if (args.t() != json::type::List) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  // Read .cgminer/cgminer.conf
  json::rvalue conf = readCgMinerConfig();

  // Create a wvalue from the rvalue
  json::wvalue newConf;
  json::wvalue newPools = json::load("[]");
  try {
    newConf = conf;
  } catch (...) {
    newConf = json::load("{}");
  }

  // Build up new pools entries as an array
  for (int i = 0; i < args.size(); i++) {
    json::rvalue poolEntry = args[i];
    string url = poolEntry["url"].s();
    string user = poolEntry["worker"].s();
    string pass = poolEntry["password"].s();

    json::wvalue entry = json::load("{}");
    entry["url"] = url;
    entry["user"] = user;
    entry["pass"] = pass;
    newPools[i] = to_rvalue(entry);
  }

  // Set new pools entry into the wvalue
  newConf["pools"] = to_rvalue(newPools);

  // Write out config file
  writeCgMinerConfig(newConf);

  sendCgMinerCmd("restart", "", [&](CgMiner::Response cgMinerResp) { resp.end(); });
}

void setConfigNetwork(string path, json::rvalue &args, const crow::request &req,
                      crow::response &resp) {
  if (args.t() != json::type::Object) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  bool dhcpEnabled = args["dhcpEnabled"].b();
  string hostname = args["hostname"].s();
  string ipAddress = args["ipAddress"].s();
  string subnetMask = args["subnetMask"].s();
  string defaultGateway = args["gateway"].s();
  string dnsServer = args["dnsServer"].s();

  if (!setNetworkInfo(INTF_NAME, hostname, ipAddress, subnetMask, defaultGateway, dnsServer,
                      dhcpEnabled)) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  resp.end();
  return;
}

void setConfigSystem(string path, json::rvalue &args, const crow::request &req,
                     crow::response &resp) {
  if (args.t() != json::type::Object) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  string timezone = args["timezone"].s();
  setTimezone(timezone);

  resp.end();
  return;
}

void setConfigMining(string path, json::rvalue &args, const crow::request &req,
                     crow::response &resp) {
  if (args.t() != json::type::Object) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  // TODO: Implement setConfigMining()

  resp.end();
  return;
}

void setConfigAsic(string path, json::rvalue &args, const crow::request &req,
                   crow::response &resp) {
  if (args.t() != json::type::List) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  CgMiner::Commands cmds;
  for (int i = 0; i < args.size(); i++) {
    json::rvalue entry = args[i];
    int asicNum = entry["n"].i();
    string opt = entry["opt"].s();
    string val = entry["val"].s();

    CgMiner::Commands cmds;
    ostringstream params;
    params << asicNum << "," << opt << "," << val;
    cmds.push_back({"ascset", params.str()});
  }

  cmds.push_back({"save", ""});
  sendCgMinerCmds(cmds, [&](CgMiner::Response cgMinerResp) { resp.end(); });
}

//--------------------------------------------------------------------------------------------------
// STATUS HANDLERS (CGMINER)
//--------------------------------------------------------------------------------------------------

void getStatusDashboard(string path, query_string &urlParams, const crow::request &req,
                        crow::response &resp) {
  /*
string json =
"{\"hashrateData\":[{\"time\":1530657000000,\"board1\":1660,\"board2\":1420,\"board3\":"
"900,\"total\":3980},"
"{\"time\":1530657300000,\"board1\":1469,\"board2\":1574,\"board3\":800,\"total\":"
"3843},"
"{\"time\":1530657600000,\"board1\":1602,\"board2\":900,\"board3\":700,\"total\":"
"3202},"
"{\"time\":1530657900000,\"board1\":1610,\"board2\":1200,\"board3\":1000,"
"\"total\":3810},"
"{\"time\":1530658200000,\"board1\":907,\"board2\":1700,\"board3\":1100,\"total\":"
"3707},"
"{\"time\":1530658500000,\"board1\":1860,\"board2\":1654,\"board3\":1050,"
"\"total\":4564}],"
"\"poolStatus\":[{\"url\":\"http://"
"us-east.luxor.tech:3333\",\"worker\":\"abc\",\"status\":\"Active\",\"rejected\":"
"0,"
"\"accepted\":79},"
"{\"url\":\"http://"
"us-west.luxor.tech:3333\",\"worker\":\"abc\",\"status\":\"Offline\",\"rejected\":"
"0,"
"\"accepted\":78},"
"{\"url\":\"http://"
"us-east.siamining.com:3333\",\"worker\":\"abc\",\"status\":\"Active\","
"\"rejected\":0,"
"\"accepted\":123}],"
"\"hashboardStatus\":["
"{\"hashrate\":407,\"status\":\"Active\",\"accepted\":45,\"rejected\":0,"
"\"boardTemp\":82,"
"\"chipTemp\":86},"
"{\"hashrate\":486,\"status\":\"Error\",\"accepted\":54,\"rejected\":0,"
"\"boardTemp\":81,"
"\"chipTemp\":85},"
"{\"hashrate\":569,\"status\":\"Active\",\"accepted\":41,\"rejected\":0,"
"\"boardTemp\":"
"83,\"chipTemp\":87}],"
"\"systemInfo\":["
"{\"name\":\"Free Memory\",\"value\":\"123 MB\"},{\"name\":\"Total "
"Memory\",\"value\":\"999 MB\"},"
"{\"name\":\"Uptime\",\"value\":\"19:47:21\"},{\"name\":\"Fan 1 "
"Speed\",\"value\":\"2400 RPM\"},"
"{\"name\":\"Fan 2 Speed\",\"value\":\"2469 RPM\"},{\"name\":\"Board 1 "
"Temp\",\"value\":\"80 C\"},"
"{\"name\":\"Board 2 Temp\",\"value\":\"82 C\"},{\"name\":\"Board 3 "
"Temp\",\"value\":\"85 C\"}]}";

sendJson(json, resp);

*/

  sendCgMinerCmd("dashpools+dashstats+dashdevs", "", [&](CgMiner::Response cgMinerResp) {
    CROW_LOG_DEBUG << "RESP=========================================";
    CROW_LOG_DEBUG << cgMinerResp.json;
    json::rvalue cgJsonResp = json::load(cgMinerResp.json);
    if (!cgJsonResp.has("pools") || !cgJsonResp.has("stats") || !cgJsonResp.has("devs")) {
      sendError("Invalid response from mining app", HttpStatus_InternalServerError, resp);
    }

    CROW_LOG_DEBUG << "Dashboard 1";
    json::rvalue poolsResp = cgJsonResp["pools"];
    json::rvalue statsResp = cgJsonResp["stats"];
    json::rvalue devsResp = cgJsonResp["devs"];
    CROW_LOG_DEBUG << "Dashboard 2";

    // Ensure that all requests succeeded
    if (isCgMinerError(poolsResp) || isCgMinerError(statsResp) || isCgMinerError(devsResp)) {
      CROW_LOG_DEBUG << "Dashboard Error 1";
      sendError("Invalid response from mining app", HttpStatus_InternalServerError, resp);
    }

    // Prepare our JSON response outline
    json::wvalue jsonResp = json::load("{}");
    json::wvalue hashrateArr = json::load("[]");
    json::wvalue hashboardArr = json::load("[]");
    json::wvalue systemArr = json::load("[]");
    CROW_LOG_DEBUG << "Dashboard 3";

    // Handle the pools array - direct copy since it's a custom method in cgminer with just
    // the info we want in the format we want.
    jsonResp["poolStatus"] = poolsResp["POOLS"];
    CROW_LOG_DEBUG << "Dashboard 7";

    // Handle the hashrateData array - local data
    // string hashrateData =
    //     "[{\"time\":1530657000000,\"board1\":1660,\"board2\":1420,\"board3\":"
    //     "900,\"total\":3980},"
    //     "{\"time\":1530657300000,\"board1\":1469,\"board2\":1574,\"board3\":800,\"total\":"
    //     "3843},"
    //     "{\"time\":1530657600000,\"board1\":1602,\"board2\":900,\"board3\":700,\"total\":"
    //     "3202},"
    //     "{\"time\":1530657900000,\"board1\":1610,\"board2\":1200,\"board3\":1000,"
    //     "\"total\":3810},"
    //     "{\"time\":1530658200000,\"board1\":907,\"board2\":1700,\"board3\":1100,\"total\":"
    //     "3707},"
    //     "{\"time\":1530658500000,\"board1\":1860,\"board2\":1654,\"board3\":1050,"
    //     "\"total\":4564}]";
    CROW_LOG_DEBUG << "Dashboard 8";

    for (int i = 0; i < num_hashrate_entries; i++) {
      json::wvalue entry = json::load("{}");
      entry["time"] = hashrate_history_secs[i].time;
      uint32_t total = 0;
      for (int hb = 0; hb < MAX_HASHBOARDS; hb++) {
        uint32_t value = hashrate_history_secs[i].hashrates[hb];
        if (value != 0) {
          entry["board" + hb] = value;
          total += value;
        }
      }
      entry["total"] = total;
      hashrateArr[i] = to_rvalue(entry);
    }
    jsonResp["hashrateData"] = to_rvalue(hashrateArr);

    CROW_LOG_DEBUG << "Dashboard 9";
    // TODO: Implement hashrate polling and then send the data from here

    // Fan speeds
    int fanSpeed0 = 0;
    int fanSpeed1 = 0;

    // Handle the hashboardStatus array
    json::rvalue stats = statsResp["STATS"];
    json::wvalue hashStats = json::load("[]");
    int hashboardIndex = 0;
    for (int i = 0; i < stats.size(); i++) {
      // Copy over just the Obelisk entries, not the POOL entries
      json::rvalue entry = stats[i];
      string name = entry["id"].s();
      if (name.find("Obelisk") == 0) {
        // This is a hashboard stats entry
        hashStats[hashboardIndex++] = entry;

        // Extract fan speed entry for system info
        if (entry.has("fanSpeed0")) {
          fanSpeed0 = entry["fanSpeed0"].i();
        }
        if (entry.has("fanSpeed1")) {
          fanSpeed1 = entry["fanSpeed1"].i();
        }

        // TODO: Remove fan speeds from 'entry' - MINOR
      }
    }
    jsonResp["hashboardStatus"] = to_rvalue(hashStats);

    CROW_LOG_DEBUG << "Dashboard 27";

    // Handle the systemInfo array
    int i = 0;

    CROW_LOG_DEBUG << "Dashboard 28";
    systemArr[i++] = makeSystemInfoEntry("Free Memory", to_string(getFreeMemory()));
    systemArr[i++] = makeSystemInfoEntry("Total Memory", to_string(getTotalMemory()));
    systemArr[i++] = makeSystemInfoEntry("Uptime", getUptime());
    systemArr[i++] = makeSystemInfoEntry("Fan 1 Speed", to_string(fanSpeed0) + " RPM");
    systemArr[i++] = makeSystemInfoEntry("Fan 2 Speed", to_string(fanSpeed1) + " RPM");
    jsonResp["systemInfo"] = to_rvalue(systemArr);
    CROW_LOG_DEBUG << "Dashboard 29";
    sendJson(json::dump(jsonResp), resp);
  });
}

void getStatusDeviceDetails(string path, query_string &urlParams, const crow::request &req,
                            crow::response &resp) {
  sendCgMinerCmd("devdetails", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getStatusSummary(string path, query_string &urlParams, const crow::request &req,
                      crow::response &resp) {
  sendCgMinerCmd("summary", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getStatusAsic(string path, query_string &urlParams, const crow::request &req,
                   crow::response &resp) {
  sendCgMinerCmd("asc", urlParams.get("n"),
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getStatusStats(string path, query_string &urlParams, const crow::request &req,
                    crow::response &resp) {
  sendCgMinerCmd("stats", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getStatusCoinMining(string path, query_string &urlParams, const crow::request &req,
                         crow::response &resp) {
  sendCgMinerCmd("coin", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

void getStatusLockStats(string path, query_string &urlParams, const crow::request &req,
                        crow::response &resp) {
  sendCgMinerCmd("lockstats", "",
                 [&](CgMiner::Response cgMinerResp) { sendJson(cgMinerResp.json, resp); });
}

//--------------------------------------------------------------------------------------------------
// STATUS HANDLERS (API SERVER)
//--------------------------------------------------------------------------------------------------

void getStatusMemory(string, query_string &urlParams, const crow::request &req,
                     crow::response &resp) {
  int freeMemory = getFreeMemory();
  int totalMemory = getTotalMemory();
  json::wvalue jsonResp = json::load("{}");
  jsonResp["freeMemory"] = freeMemory;
  jsonResp["totalMemory"] = totalMemory;
  string jsonStr = json::dump(jsonResp);
  sendJson(jsonStr, resp);
}

//--------------------------------------------------------------------------------------------------
// ACTION HANDLERS
//--------------------------------------------------------------------------------------------------

void actionChangePassword(string path, json::rvalue &args, const crow::request &req,
                          crow::response &resp) {
  string username = args["username"].s();
  string oldPassword = args["oldPassword"].s();
  string newPassword = args["newPassword"].s();

  if (!changePassword(username, oldPassword, newPassword, AUTH_FILE, false)) {
    resp.code = HttpStatus_Unauthorized;
    resp.end();
    return;
  }

  resp.end();
}

void actionRestartApiServer(string path, json::rvalue &args, const crow::request &req,
                            crow::response &resp) {
  resp.end();

  // TODO: Do we need to wait here before exiting so that the success code is sent?
  // We're not actually calling this at the moment anyway...

  // Exit the API Server.  The watchdog should kick in to restart us
  CROW_LOG_DEBUG << "Calling exit()";
  exit(EXIT_SUCCESS);
}

void actionRestartMiningApp(string path, json::rvalue &args, const crow::request &req,
                            crow::response &resp) {
  sendCgMinerCmd("restart", "", [&](CgMiner::Response cgMinerResp) { resp.end(); });
}

void actionReboot(string path, json::rvalue &args, const crow::request &req, crow::response &resp) {
  CROW_LOG_DEBUG << "actionReboot() called";
  resp.end();

  // Reboot the entire computer
  CROW_LOG_DEBUG << "Calling reboot()";

  // This works even when not running the app under sudo
  doReboot();

  // The following way did not work unless the server was run as "sudo ./bin/apiserver"
  //
  // sync();
  // setuid(0);
  // int ret = reboot(RB_AUTOBOOT);
  // CROW_LOG_DEBUG << "ret=" << ret << " errno=" << errno;
}

void actionClearStats(string path, json::rvalue &args, const crow::request &req,
                      crow::response &resp) {
  sendCgMinerCmd("zero", "all,false", [&](CgMiner::Response cgMinerResp) { resp.end(); });
}

void actionEnableAsic(string path, json::rvalue &args, const crow::request &req,
                      crow::response &resp) {
  string asicNum = args["n"].s();
  if (args["enabled"].s() == "true") {
    sendCgMinerCmd("ascenable", asicNum, [&](CgMiner::Response cgMinerResp) { resp.end(); });
  } else {
    sendCgMinerCmd("ascdisable", asicNum, [&](CgMiner::Response cgMinerResp) { resp.end(); });
  }
}

#ifdef __APPLE__
#define FIRMWARE_FILE_PATH "./firmware_upgrade.bin"
#else
#define FIRMWARE_FILE_PATH "/root/firmware_upgrade.bin"
#endif

/* File Upload:
 *   filename: string
 *   offset: number of bytes into the file that this chunk starts
 *   totalSize: number of bytes in the total file (NOT the size of the base64-encoded data)
 *   data: base64-encoded string
 *
 * If the offset is zero, create the firmware file and truncate it.
 * If the offset is non-zero, open the file, seek to the offset and write/append.
 * If end of file is reached, ensure that the file stat size matches the totalSize
 */
void actionUploadFirmwareFileFragment(string path, json::rvalue &args, const crow::request &req,
                                      crow::response &resp) {
  CROW_LOG_DEBUG << "********** actionUploadFirmwareFileFragment()";
  int64_t offset = args["offset"].i(); // Used only to detect first fragment
  string data = args["data"].s();

  // Decode the base64-encoded string data to bytes
  vector<uint8_t> binData = base64_decode(data);
  if (binData.size() == 0) {
    resp.code = HttpStatus_BadRequest;
    resp.end();
    return;
  }

  // Open file for output
  std::ofstream outfile;
  try {
    if (offset == 0) {
      outfile.open(FIRMWARE_FILE_PATH, std::ofstream::trunc | std::ofstream::binary);
    } else {
      outfile.open(FIRMWARE_FILE_PATH, std::ofstream::app | std::ofstream::binary);
    }
  } catch (...) {
    // Catch exceptions and return an error
    resp.code = HttpStatus_InternalServerError;
    resp.end();
    return;
  }

  // Write the binary data
  outfile.write((const char *)&binData[0], binData.size());
  outfile.close();

  resp.end();
}

// inventory/ - things about the miner that "just are" and cannot be modified by set requests
//              (e.g., number of boards, number of ASICs per board, serial numbers, etc.)
// config/    - things about the miner that can be modified and read back (e.g., pool
// settings,
//              IP address, DHCP enabled, etc.).
// status/    - things about the miner that cannot be specifically set, but that do change
// over time
//              (e.g., control board temperature, hashrates, uptime, etc.)
map<string, PathHandlerForGet> pathHandlerMapForGet = {
    // Inventory
    {"inventory/versions", getInventoryVersions},
    {"inventory/devices", getInventoryDevices},
    {"inventory/asicCount", getInventoryAsicCount},
    {"inventory/system", getInventorySystem},

    // Config
    {"config/config", getConfigConfig},
    {"config/pools", getConfigPools},
    {"config/network", getConfigNetwork},
    {"config/system", getConfigSystem},
    {"config/mining", getConfigMining},

    // Status
    {"status/dashboard", getStatusDashboard},
    {"status/memory", getStatusMemory},
    {"status/devDetails", getStatusDeviceDetails},
    {"status/summary", getStatusSummary},
    {"status/asic", getStatusAsic},
    {"status/stats", getStatusStats},
    {"status/coinMining", getStatusCoinMining},
    {"status/lockStats", getStatusLockStats},
};

map<string, PathHandlerForSet> pathHandlerMapForSet = {
    {"config/network", setConfigNetwork}, {"config/system", setConfigSystem},
    {"config/mining", setConfigMining},   {"config/pools", setConfigPools},
    {"config/asic", setConfigAsic},
};

map<string, PathHandlerForAction> pathHandlerMapForAction = {
    {"changePassword", actionChangePassword},
    {"restartMiningApp", actionRestartMiningApp},
    {"restartApiServer", actionRestartApiServer},
    {"reboot", actionReboot},
    {"clearStats", actionClearStats},
    {"enableAsic", actionEnableAsic},
    {"uploadFirmwareFile", actionUploadFirmwareFileFragment},
};

void handleGet(string &path, const crow::request &req, crow::response &resp) {
  CROW_LOG_DEBUG << "GET: " << path;
  crow::query_string urlParams = req.url_params;

  auto it = pathHandlerMapForGet.find(path);
  if (it == pathHandlerMapForGet.end()) {
    // Not found
    sendError("Path not found", HttpStatus_NotFound, resp);
    return;
  }

  auto handler = it->second;
  handler(path, urlParams, req, resp);
}

void handleSet(string &path, const crow::request &req, crow::response &resp) {
  json::rvalue args = json::load(req.body);

  auto it = pathHandlerMapForSet.find(path);
  if (it == pathHandlerMapForSet.end()) {
    // Not found
    sendError("Path not found", HttpStatus_NotFound, resp);
    return;
  }

  auto handler = it->second;
  handler(path, args, req, resp);
}

void handleAction(string &path, const crow::request &req, crow::response &resp) {
  json::rvalue args = json::load(req.body);

  auto it = pathHandlerMapForAction.find(path);
  if (it == pathHandlerMapForAction.end()) {
    // Not found
    sendError("Path not found", HttpStatus_NotFound, resp);
    return;
  }

  auto handler = it->second;
  handler(path, args, req, resp);
}
