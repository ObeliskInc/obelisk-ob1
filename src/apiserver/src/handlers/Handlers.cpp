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

#define FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_GZ "/tmp/upgrade/firmware-upgrade.tar.gz"
#define FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_TAR "/tmp/upgrade/firmware-upgrade.tar"
#define FIRMWARE_UPGRADE_SCRIPT_FILE_PATH "/tmp/upgrade/upgrade.sh"
#define FIRMWARE_UPGRADE_MESSAGE_FILE_PATH "/tmp/upgrade/upgrade-message.txt"

// This is the queue we use to send cgminer requests
extern SafeQueue<CgMiner::Request> gRequestQueue;

string gModel = "";

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
  gRequestQueue.enqueue(cgMinerReq);
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

int findIndexByFieldValue(json::rvalue jsonArr, string fieldName, int value) {
  if (jsonArr.t() == json::type::List) {
    for (int i = 0; i < jsonArr.size(); i++) {
      json::rvalue entry = jsonArr[i];
      if (entry.has(fieldName) && entry[fieldName].i() == value) {
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

#define MAX_HASHRATE_HISTORY_ENTRIES (60) // We keep one hour of history for now
hashrate_t hashrate_history_secs[MAX_HASHRATE_HISTORY_ENTRIES];
int num_hashrate_entries = 0;

// Timer callback to poll for updated hashrates
bool isPollInProgress = false;
void pollForHashrate() {
  CROW_LOG_DEBUG << "Polling for hashrates";
  if (isPollInProgress) {
    // Don't poll again if the last request has not completed
    return;
  }
  isPollInProgress = true;

  sendCgMinerCmd("dashdevs", "", [&](CgMiner::Response cgMinerResp) {
    if (cgMinerResp.error) {
      // No data to append - oh well
      isPollInProgress = false;
      return;
    }

    // No error, so build up the response that the user is expecting
    try {
      // CROW_LOG_DEBUG << "json = " << cgMinerResp.json;

      json::rvalue cgMinerJson = json::load(cgMinerResp.json);
      time_t now = time(0);
      double values[MAX_HASHBOARDS];
      json::rvalue devsArr = cgMinerJson["DEVS"];
      for (int i = 0; i < devsArr.size(); i++) {
        json::rvalue entry = devsArr[i];
        values[i] = entry["mhs5m"].d();
      }


      // See if we need to move the old data
      int entry_index = num_hashrate_entries;
      if (num_hashrate_entries >= MAX_HASHRATE_HISTORY_ENTRIES) {
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
        hashrate_history_secs[entry_index].hashrates[i] = values[i]/1000.0;
      }

    } catch (...) {
    }
    isPollInProgress = false;
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
  int numEntries = 0;
  for (int i = 0; i < args.size(); i++) {
    json::rvalue poolEntry = args[i];
    string url = poolEntry["url"].s();
    string user = poolEntry["worker"].s();
    string pass = poolEntry["password"].s();

    // Don't include entries that have empty URLs, because CGminer will crash, of course
    if (url.length() != 0) {
      json::wvalue entry = json::load("{}");
      entry["url"] = url;
      entry["user"] = user;
      entry["pass"] = pass;
      newPools[numEntries] = to_rvalue(entry);
      numEntries++;
    }
  }

  // Set new pools entry into the wvalue
  newConf["pools"] = to_rvalue(newPools);

  // Write out config file
  writeCgMinerConfig(newConf);

  CROW_LOG_ERROR << "setConfigPools() - Resetting cgminer!";
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

  sendCgMinerCmd("dashpools+dashstats+dashdevs", "", [&](CgMiner::Response cgMinerResp) {
    if (cgMinerResp.error) {
      sendError("Unable to connect to cgminer", HttpStatus_InternalServerError, resp);
      return;
    }

    // CROW_LOG_DEBUG << "RESP=========================================";
    // CROW_LOG_DEBUG << cgMinerResp.json;

    json::rvalue cgJsonResp = json::load(cgMinerResp.json);
    if (!cgJsonResp.has("dashpools") || !cgJsonResp.has("dashstats") || !cgJsonResp.has("dashdevs")) {
      sendError("Invalid response from mining app", HttpStatus_InternalServerError, resp);
      return;
    }

    json::rvalue poolsResp = cgJsonResp["dashpools"][0];
    json::rvalue statsResp = cgJsonResp["dashstats"][0];
    json::rvalue devsResp = cgJsonResp["dashdevs"][0];

    // TODO: See why isCgMinerError() is failing
    // Ensure that all requests succeeded
    // if (isCgMinerError(poolsResp) || isCgMinerError(statsResp) || isCgMinerError(devsResp)) {
    //   CROW_LOG_DEBUG << "Dashboard Error 1";
    //   // sendError("Invalid response from mining app", HttpStatus_InternalServerError, resp);
    //   return;
    // }

    // Prepare our JSON response outline
    json::wvalue jsonResp = json::load("{}");
    json::wvalue hashrateArr = json::load("[]");
    json::wvalue hashboardArr = json::load("[]");
    json::wvalue systemArr = json::load("[]");

    // Handle the pools array - direct copy since it's a custom method in cgminer with just
    // the info we want in the format we want.
    jsonResp["poolStatus"] = poolsResp["POOLS"];

    for (int i = 0; i < num_hashrate_entries; i++) {
      json::wvalue entry = json::load("{}");
      entry["time"] = hashrate_history_secs[i].time;
      uint32_t total = 0;
      for (int hb = 0; hb < MAX_HASHBOARDS; hb++) {

        uint32_t value = hashrate_history_secs[i].hashrates[hb];
        if (value != 0) {
          entry["board" + to_string(hb)] = value;
          total += value;
        }
      }
      entry["total"] = total;
      hashrateArr[i] = to_rvalue(entry);
    }
    jsonResp["hashrateData"] = to_rvalue(hashrateArr);

    // TODO: Implement hashrate polling and then send the data from here

    // Fan speeds
    int fanSpeed0 = 0;
    int fanSpeed1 = 0;

    // Handle the hashboardStatus array
    json::rvalue stats = statsResp["STATS"];
    json::rvalue devs = devsResp["DEVS"];
    json::wvalue hashStatus = json::load("[]");
    int hashboardIndex = 0;
    for (int i = 0; i < stats.size(); i++) {
      json::rvalue statsEntry = stats[i];
      if (statsEntry.has("boardId")) {
        int boardId = statsEntry["boardId"].i();
        json::wvalue entry = json::load("{}");

        entry["numChips"] = statsEntry["numChips"].i();
        entry["numCores"] = statsEntry["numCores"].i();
        entry["boardTemp"] = statsEntry["boardTemp"].d();
        entry["chipTemp"] = statsEntry["chipTemp"].d();
        entry["powerSupplyTemp"] = statsEntry["powerSupplyTemp"].d();
        entry["fanSpeed0"] = statsEntry["fanSpeed0"].i();
        entry["fanSpeed1"] = statsEntry["fanSpeed1"].i();

        // Extract fan speed entry for system info
        if (statsEntry.has("fanSpeed0")) {
          fanSpeed0 = statsEntry["fanSpeed0"].i();
        }
        if (statsEntry.has("fanSpeed1")) {
          fanSpeed1 = statsEntry["fanSpeed1"].i();
        }

        // Try to get corresponding entries from the 
        int devIndex = findIndexByFieldValue(devs, "ASC", i);
        if (devIndex >= 0) {
          json::rvalue devEntry = devs[devIndex];
          entry["status"] = devEntry["status"].s();
          entry["mhsAvg"] = devEntry["mhsAvg"].d();
          entry["mhs1m"] = devEntry["mhs1m"].d();
          entry["mhs5m"] = devEntry["mhs5m"].d();
          entry["mhs15m"] = devEntry["mhs15m"].d();
          entry["accepted"] = devEntry["accepted"].i();
          entry["rejected"] = devEntry["rejected"].i();
        }

        hashboardArr[i] = to_rvalue(entry);
      }
    }

    jsonResp["hashboardStatus"] = to_rvalue(hashboardArr);

    // Handle the systemInfo array
    int i = 0;

    systemArr[i++] = makeSystemInfoEntry("Free Memory", to_string(getFreeMemory()));
    systemArr[i++] = makeSystemInfoEntry("Total Memory", to_string(getTotalMemory()));
    systemArr[i++] = makeSystemInfoEntry("Uptime", getUptime());
    systemArr[i++] = makeSystemInfoEntry("Fan 1 Speed", to_string(fanSpeed0) + " RPM");
    systemArr[i++] = makeSystemInfoEntry("Fan 2 Speed", to_string(fanSpeed1) + " RPM");

    jsonResp["systemInfo"] = to_rvalue(systemArr);
    string str = json::dump(jsonResp);
    // CROW_LOG_DEBUG << "jsonStr=" << str;
    sendJson(str, resp);
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

  // Ensure that the /tmp/upgrade dir exists
  runCmd("mkdir -p /tmp/upgrade");

  // Open file for output
  std::ofstream outfile;
  try {
    if (offset == 0) {
      outfile.open(FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_GZ, std::ofstream::trunc | std::ofstream::binary);
    } else {
      outfile.open(FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_GZ, std::ofstream::app | std::ofstream::binary);
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

void actionResetMinerConfig(string path, json::rvalue &args, const crow::request &req,
                            crow::response &resp) {
  CROW_LOG_DEBUG << "********** actionResetMinerConfig()";

  copyFile("/root/.cgminer/default_cgminer.conf", "/root/.cgminer/cgminer.conf");

  // Restart cgminer so the settings take effect
  sendCgMinerCmd("restart", "", [&](CgMiner::Response cgMinerResp) { resp.end(); });
}

void actionRunUpgrade(string path, json::rvalue &args, const crow::request &req,
                            crow::response &resp) {
  CROW_LOG_DEBUG << "********** actionRunUpgrade()";

  uncompressUpgradeArchive(
    FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_GZ,
    FIRMWARE_UPGRADE_ARCHIVE_FILE_PATH_TAR);

  // Read the upgrade_message.txt file and send its contents as the result
  string message = readFile(FIRMWARE_UPGRADE_MESSAGE_FILE_PATH);

  json::wvalue jsonResp = json::load("{}");
  jsonResp["message"] = message;
  sendJson(json::dump(jsonResp), resp);

  // The watchdog will now see that the upgrade.sh file exists and will kill apiserver and cgminer
  // and start the upgrade by running upgrade.sh.
  
  CROW_LOG_DEBUG << "********** actionRunUpgrade() DONE";
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
    {"resetMinerConfig", actionResetMinerConfig},
    {"runUpgrade", actionRunUpgrade},
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

void sendInfoResp(crow::response &resp) {
    string macAddress = getMACAddr(INTF_NAME);
    string ipAddress = getIpV4(INTF_NAME);

    json::wvalue respJson = json::load("{}");
    respJson["macAddress"] = macAddress;
    respJson["ipAddress"] = ipAddress;
    respJson["model"] = gModel.length() == 0 ? "Obelisk" : gModel;
    respJson["vendor"] = "Obelisk";

    sendJson(json::dump(respJson), resp);
}

void handleInfo(const crow::request &req, crow::response &resp) {
  CROW_LOG_DEBUG << "INFO";

  // Fetch the model only if we didn't already fetch it before
  if (gModel.length() == 0) {
    sendCgMinerCmd("version", "", [&](CgMiner::Response cgMinerResp) {
      if (!cgMinerResp.error) {
        json::rvalue cgMinerJson = json::load(cgMinerResp.json);
        json::rvalue container = cgMinerJson["VERSION"];
        gModel = container[0]["Model"].s();
      }

      sendInfoResp(resp);
    });
  } else {
      sendInfoResp(resp);
  }
}
