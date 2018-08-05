// Copyright 2018 Obelisk Inc.

#include "../Crow.h"
#include "../utils/CgMinerUtils.h"
#include "../utils/utils.h"
#include "catch.h"

using namespace std;
using namespace crow;

const string INTF_NAME = "eth0";

SCENARIO("Test system functions", "[memory]") {
  WHEN("free memory is requested") {
    THEN("it should be over zero") {
      int freeMemory = getFreeMemory();
      REQUIRE(freeMemory > 0);
    }
  }

  WHEN("total memory is requested") {
    THEN("it should be over zero") {
      int totalMemory = getTotalMemory();
      REQUIRE(totalMemory > 0);
    }
  }

  WHEN("OS Name is requested") {
    THEN("it should be Linux Mint") {
      string name = getOSName();
      REQUIRE(name == "Linux Mint");
    }
  }

  WHEN("OS Version is requested") {
    THEN("it should be 18.3 (Sylvia)") {
      string name = getOSVersion();
      REQUIRE(name == "18.3 (Sylvia)");
    }
  }

  WHEN("network settings can be modified") {
    THEN("IP address can be changed") {
      setNetworkInfo(INTF_NAME, "xx", "172.18.156.111", "255.255.254.0", "172.18.156.2", "8.8.8.8",
                     false);

      string ip = getIpV4(INTF_NAME);
      REQUIRE(ip == "172.18.156.111");

      string mask = getSubnetMaskV4(INTF_NAME);
      REQUIRE(mask == "255.255.254.0");

      string gw = getDefaultGateway(INTF_NAME);
      REQUIRE(gw == "172.18.156.2");

      bool dhcpEnabled = getDhcpEnabled(INTF_NAME);
      REQUIRE(dhcpEnabled == false);

      string dns = getDns(INTF_NAME);
      REQUIRE(dns == "8.8.8.8");
    }
  }

  WHEN("expiration time") {
    THEN("it should be in the future (doesn't actually check this!)") {
      struct tm expTime = makeExpirationTime(15 * 60);
      string expTimeStr = formatExpirationTime(expTime);
      CROW_LOG_DEBUG << expTimeStr;
      REQUIRE(true);
    }
  }
}

SCENARIO("Test string functions", "[strings]") {
  WHEN("An escaped string is given to unescapeCgMinerString()") {
    THEN("it should remove escapes") {
      string result1 = unescapeCgMinerString("Foo=\\\\bar\\|baz\\,\\,\\,blah\\=blat");
      REQUIRE(result1 == "Foo=\\bar|baz,,,blah=blat");

      string result2 = unescapeCgMinerString("xx\\\\\\\\yy");
      REQUIRE(result2 == "xx\\\\yy");
    }
  }
}
