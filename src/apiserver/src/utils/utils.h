// Copyright 2018 Obelisk Inc.

#ifndef UTILS_H
#define UTILS_H

// Utils
#include "../Crow.h"
#include <iostream>
#include <time.h>

using namespace std;
using namespace crow;

// Declarations
void initRand();
string genSalt();
string genSessionId();

struct tm getTimeNow();
struct tm makeExpirationTime(int secondsFromNow);
string formatExpirationTime(struct tm tm);

json::rvalue readAuthFile(string authFilename);
void writeAuthFile(string authFilename, string json);

bool checkPassword(string username, string password, string authFilename);
bool changePassword(string username, string oldPassword, string newPassword, string authFilename,
                    bool force);

string runCmd(string cmd);

void copyFile(string fromPath, string toPath);
string uncompressUpgradeArchive(string gzArchivePath, string tarArchivePath);

// System info
int getFreeMemory();
int getTotalMemory();
string getOSName();
string getOSVersion();
string getFirmwareVersion();
void doReboot();
string replaceAll(string const &original, string const &from, string const &to);
string unescapeCgMinerString(string s);

// Network utils
uint32_t subnetPrefixToMask(int prefix);
string subnetMaskToString(uint32_t mask);

bool linkDown(string intfName);
bool linkUp(string intfName);

bool setIpAndSubnetMaskV4(string intfName, string ip, string subnet);
bool setDefaultGateway(string intfName, string defaultGateway);
bool setDns(string dnsServer);
bool setHostname(string hostname);
bool setNetworkInfo(string intfName, string hostname, string ipAddress, string subnetMask,
                    string defaultGateway, string dnsServer, bool isDhcpEnabled);
bool setTimezone(string timezone);

string getHostname(string intfName);
string getIpV4(string intfName);
string getSubnetMaskV4(string intfName);
string getDefaultGateway(string intfName);
bool getDhcpEnabled(string intfName);
string getDns(string intfName);
string getMACAddr(string intfName);
string getTimezone();
string getUptime();

json::rvalue readCgMinerConfig();
void writeCgMinerConfig(json::wvalue &json);

string readFile(string path);

#endif
