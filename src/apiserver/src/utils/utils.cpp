// Copyright 2018 Obelisk Inc.

#include "utils.h"
#ifdef __APPLE__
#include <unistd.h>
#else
#include <crypt.h>
#endif
#include "../Crow.h"
#include <cstring>
#include <pwd.h>
#include <resolv.h>
#include <stdio.h>
#include <sys/types.h>

/*
When creating a new user, create a new random salt, store it in a JSON file
{
    "username": {
        salt: '2342342',
        hash: '234234234234234234234234234',
    }
}

Every time user changes password, create a new salt.

To check the password, just append lookup the salt of the username and hash it, then check that the
hash matches.

*/

const unsigned int SALT_LEN = 8;
const unsigned int SALT_PREFIX_LEN = 3;
const unsigned int NUM_SALT_CHARS = 64;
const unsigned int SESSION_ID_LEN = 32;

const string CGMINER_CONFIG_FILE_PATH = "/root/config/cgminer.conf";

static const unsigned char _xcrypt_itoa64[NUM_SALT_CHARS + 1] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void initRand() {
  time_t t;
  srand((unsigned)time(&t));
}

// Create a new random salt for use with crypt()
string genSalt() {
  ostringstream os;
  os << "$6$";

  for (int i = 0; i < SALT_LEN; i++) {
    int r = rand() % 64;
    os << _xcrypt_itoa64[r];
  }
  return os.str();
}

// Create a new random session Id
string genSessionId() {
  ostringstream os;

  for (int i = 0; i < SESSION_ID_LEN; i++) {
    int r = (rand() % 62) + 2;
    os << _xcrypt_itoa64[r];
  }
  return os.str();
}

struct tm getTimeNow() {
  time_t t;
  time(&t);

  struct tm tm;
  gmtime_r(&t, &tm);
  return tm;
}

struct tm makeExpirationTime(int secondsFromNow) {
  struct tm expTime = getTimeNow();
  expTime.tm_sec += secondsFromNow;

  mktime(&expTime);
  return expTime;
}

string formatExpirationTime(struct tm tm) {
  ostringstream os;
  os << put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
  return os.str();
}

// Read the auth JSON file
json::rvalue readAuthFile(string authFilename) {
  try {
    ifstream s(authFilename, ifstream::in);
    stringstream buffer;
    buffer << s.rdbuf();

    if (buffer.str().length() == 0) {
      return json::load("{}");
    }

    auto auth = json::load(buffer.str());
    return auth;
  } catch (...) {
    return json::load("{}");
  }
}

// Write the auth JSON file
void writeAuthFile(string authFilename, string json) {
  ofstream out(authFilename);
  out << json;
  out.close();
}

// See if the given password is valid
bool checkPassword(string username, string password, string authFilename) {
  json::rvalue auth = readAuthFile(authFilename);
  if (!auth.has(username)) {
    return false;
  }

  json::rvalue userInfo = auth[username];

  // Get the salt and hash
  if (!userInfo.has("salt") || !userInfo.has("hash")) {
    return false;
  }

  // string salt = userInfo["salt"].s();
  string hash = userInfo["hash"].s();
  string salt = hash.substr(0, SALT_PREFIX_LEN + SALT_LEN);

  return hash == crypt(password.c_str(), salt.c_str());
}

// Change the password for the named user.  The oldPassword must be correct.
bool changePassword(string username, string oldPassword, string newPassword, string authFilename,
                    bool force) {
  json::rvalue auth = readAuthFile(authFilename);

  // If this is an existing user, then make sure that the oldPassword is correct.
  if (!force && auth.has(username)) {
    json::rvalue userInfo = auth[username];
    // Get the salt and hash
    if (!userInfo.has("salt") || !userInfo.has("hash")) {
      return false;
    }
    string salt = userInfo["salt"].s();
    string existingHash = userInfo["hash"].s();

    string oldPwHash = crypt(oldPassword.c_str(), salt.c_str());
    if (oldPwHash != existingHash) {
      return false;
    }
  }

  // Now set the new password using a new salt
  string salt = genSalt();
  string newPwHash = crypt(newPassword.c_str(), salt.c_str());
  json::wvalue authOut = auth;
  // If this user doesn't exist yet, create the object container
  if (!auth.has(username)) {
    authOut[username] = json::load("{}");
  }
  authOut[username]["salt"] = salt;
  authOut[username]["hash"] = newPwHash;

  // Write out auth info to the file
  writeAuthFile(authFilename, json::dump(authOut));

  return true;
}

#ifdef PAM_AUTH
struct pam_response *reply;

// //function used to get user input
int function_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp,
                          void *appdata_ptr) {
  *resp = reply;
  return PAM_SUCCESS;
}

// THIS ACTUALLY WORKS, BUT WE DON'T ACTUALLY WANT TO USE IT AS IT TURNS OUT!
bool checkPasswordPAM(const char *username, const char *password) {
  const struct pam_conv local_conversation = {function_conversation, NULL};
  pam_handle_t *local_auth_handle = NULL; // this gets set by pam_start

  int retval;
  retval = pam_start("su", username, &local_conversation, &local_auth_handle);

  if (retval != PAM_SUCCESS) {
    printf("pam_start returned: %d\n ", retval);
    return 0;
  }

  reply = (struct pam_response *)malloc(sizeof(struct pam_response));

  reply[0].resp = strdup(password);
  reply[0].resp_retcode = 0;
  retval = pam_authenticate(local_auth_handle, 0);

  if (retval != PAM_SUCCESS) {
    // if (retval == PAM_AUTH_ERR) {
    //     printf("Authentication failure.\n");
    // } else {
    //     printf("pam_authenticate returned %d\n", retval);
    // }
    return false;
  }

  // printf("Authenticated.\n");
  retval = pam_end(local_auth_handle, retval);

  if (retval != PAM_SUCCESS) {
    // printf("pam_end returned\n");
    return false;
  }

  return true;
}
#endif

string runCmd(string cmd) {
  FILE *fp = popen(cmd.c_str(), "r");
  string output = "";
  char lineBuffer[1035];

  if (fp == NULL) {
    return output;
  }

  while (fgets(lineBuffer, sizeof(lineBuffer) - 1, fp) != NULL) {
    output.append(lineBuffer);
  }

  return output;
}

// TODO: Could optimize this:
//    free -m | awk '/^Mem/ {print $2 $3 $4}'
// then split on space to get the three numbers all at once and return an array.

string replaceAll(string const &original, string const &from, string const &to) {
  string results;
  string::const_iterator end = original.end();
  string::const_iterator current = original.begin();
  string::const_iterator next = std::search(current, end, from.begin(), from.end());
  while (next != end) {
    results.append(current, next);
    results.append(to);
    current = next + from.size();
    next = std::search(current, end, from.begin(), from.end());
  }
  results.append(current, next);
  return results;
}

string unescapeCgMinerString(string s) {
  string u(s);
  u = replaceAll(u, "\\\\", "\\");
  u = replaceAll(u, "\\|", "|");
  u = replaceAll(u, "\\=", "=");
  u = replaceAll(u, "\\,", ",");
  return u;
}

// Subtract the ‘struct timeval’ values X and Y, storing the result in RESULT.
// Return 1 if the difference is negative, otherwise 0.
void subtractTimespecs(struct timespec *result, const struct timespec *start,
                       const struct timespec *stop) {
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000UL;
  } else {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }

  return;
}

inline uint64_t asNanos(struct timespec *ts) {
  return ts->tv_sec * (uint64_t)1000000000L + ts->tv_nsec;
}

// System info

// Returns free memory as an integer number of kilobytes
int getFreeMemory() {
  string output = runCmd("free -m | awk '/^Mem/ {print $4}'");
  return stoi(output);
}

// Returns free memory as an integer number of kilobytes
int getTotalMemory() {
  string output = runCmd("free -m | awk '/^Mem/ {print $2}'");
  return stoi(output);
}

string getOSName() {
  ostringstream cmd;
  cmd << "cat /etc/os-release | sed -n -r 's/^NAME=\"(.*)\"/\\1/p' | tr -d '\\n'";
  return runCmd(cmd.str());
}

string getOSVersion() {
  ostringstream cmd;
  cmd << "cat /etc/os-release | sed -n -r 's/^VERSION=\"(.*)\"/\\1/p' | tr -d '\\n'";
  return runCmd(cmd.str());
}

void doReboot() { runCmd("reboot"); }

// Network utils
uint32_t subnetPrefixToMask(int prefix) {
  struct in_addr mask;
  memset(&mask, 0, sizeof(mask));
  if (prefix) {
    return ~((1 << (32 - prefix)) - 1);
  } else {
    return 0;
  }
}

string subnetMaskToString(uint32_t mask) {
  int b1 = mask >> 24;
  int b2 = (mask >> 16) & 0xFF;
  int b3 = (mask >> 8) & 0xFF;
  int b4 = mask & 0xFF;
  ostringstream os;
  os << b1 << "." << b2 << "." << b3 << "." << b4;
  return os.str();
}

bool linkDown(string intfName) {
  ostringstream cmd;
  cmd << "ip link set " << intfName << " down";
  runCmd(cmd.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

bool linkUp(string intfName) {
  ostringstream cmd;
  cmd << "ip link set " << intfName << " up";
  runCmd(cmd.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

// NOTE: For this code to work, Mint's Automatic DHCP must be disabled,
//       and you need to bring the link down and then up again for it
//       to kill dhclient completely (I did this through the mint GUI,
//       but not sure how to do it on the board since there is no GUI).
bool setIpAndSubnetMaskV4(string intfName, string ip, string subnet) {
  ostringstream cmd;
  cmd << "ifconfig " << intfName << " " << ip << " netmask " << subnet;
  runCmd(cmd.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

bool setDefaultGateway(string intfName, string defaultGateway) {
  // Remove the current default gw by index (assumes there is only 1 set)
  runCmd("route del default gw 0");

  // Set the new gateway
  ostringstream cmd;
  cmd << "route add default gw " << defaultGateway << " " << intfName;
  runCmd(cmd.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

bool setHostname(string hostname) {
  ostringstream cmd;
  cmd << "echo " << hostname << " > /root/config/hostname";
  runCmd(cmd.str());
  return true;
}

bool setDns(string dnsServer) {
  ostringstream cmd;
  cmd << "echo nameserver " << dnsServer << " > /root/config/resolv.conf";
  runCmd(cmd.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

bool setNetworkInfo(string intfName, string hostname, string ipAddress, string subnetMask,
                    string defaultGateway, string dnsServer, bool isDhcpEnabled) {
  bool result = linkDown(intfName);
  if (result) {
    ostringstream config;
    config << "# interface settings generated by obelisk apiserver - CHANGES MAY BE OVERWRITTEN\\n"
           << "auto lo\\n"
           << "auto " << intfName << "\\niface lo inet loopback\\n";

    if (isDhcpEnabled) {
      // Set the given address now that DHCP has been disabled
      config << "iface " << intfName << " inet dhcp\\n";
    } else {
      config << "iface " << intfName << " inet static\\n"
             << "    address " << ipAddress << "\\n    netmask " << subnetMask << "\\n    gateway "
             << defaultGateway << "\\n";
    }

    ostringstream cmd;
    cmd << "printf \"" << config.str() << "\" > /root/config/interfaces" << endl;
    runCmd(cmd.str());

    // Set the hostname
    setHostname(hostname);

    // Set the DNS
    setDns(dnsServer);

    doReboot();
  }
  return result;
}

string getHostname(string intfName) {
  string result = runCmd("hostname");
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getIpV4(string intfName) {
  // OLD COMMAND:
  // cmd << "cat /etc/network/interfaces | sed -En 's/\s+address([ ]+)(.*)/\2/p'";
  ostringstream cmd;
  cmd << "ifconfig " << intfName
      << " | awk '/inet addr:/{split($2,a,\":\"); print a[2]}' | head -1";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getSubnetMaskV4(string intfName) {
  // OLD COMMAND:
  // cmd << "cat /etc/network/interfaces | sed -En 's/\s+netmask([ ]+)(.*)/\2/p'";
  ostringstream cmd;
  cmd << "ifconfig " << intfName << " | awk '/Mask:/{split($4,a,\":\"); print a[2]}' | head -1";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getDefaultGateway(string intfName) {
  // OLD COMMAND:
  // cmd << "cat /etc/network/interfaces | sed -En 's/\s+gateway([ ]+)(.*)/\2/p'";
  ostringstream cmd;
  cmd << "cat /root/config/interfaces | sed -En 's/\\s+gateway([ ]+)(.*)/\\2/p'";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

bool getDhcpEnabled(string intfName) {
  ostringstream cmd;
  cmd << "cat /root/config/interfaces | sed -En 's/iface(.*) " << intfName << " inet (.*)/\\2/p'";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return (result == "dhcp");
}

string getDns(string intfName) {
  ostringstream cmd;
  cmd << "cat /root/config/resolv.conf | sed -En 's/nameserver ([0-9.]+).*/\\1/p' | head -1";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getMACAddr(string intfName) {
  ostringstream cmd;
  cmd << "ifconfig " << intfName << " | sed -En 's/.*HWaddr ([A-F|0-9|:]+).*/\\1/p' | head -1";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getTimezone() {
  ostringstream cmd;
  cmd << "cat /root/config/timezone";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

string getUptime() {
  ostringstream cmd;
  cmd << "uptime | sed -En 's/.*up (.*),  .*/\\1/p' | head -1";
  string result = runCmd(cmd.str());
  if (result.length() > 0) {
    result.pop_back(); // Remove trailing newline
  }
  return result;
}

bool setTimezone(string timezone) {
  ostringstream cmd1;
  cmd1 << "echo " << timezone << " > /root/config/timezone";
  runCmd(cmd1.str());

  ostringstream cmd2;
  cmd2 << "ln -s -f /usr/share/zoneinfo/" << timezone << " /root/config/localtime";
  runCmd(cmd2.str());
  // TODO: Parse result to see if cmd worked
  return true;
}

json::rvalue readCgMinerConfig() {
  ifstream t(CGMINER_CONFIG_FILE_PATH);
  stringstream buffer;
  buffer << t.rdbuf();
  t.close();
  return json::load(buffer.str());
}

void writeCgMinerConfig(json::wvalue &json) {
  ofstream out(CGMINER_CONFIG_FILE_PATH);
  string jsonStr = json::dump(json);
  out << jsonStr;
  out.close();
}
