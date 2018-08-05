// Copyright 2018 Obelisk Inc.

#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "../Crow.h"
#include "../utils/utils.h"
#include "catch.h"

using namespace std;
using namespace crow;

SCENARIO("Write and read JSON auth files", "[json_auth]") {
  GIVEN("Write JSON to a file") {
    initRand();
    const string authFilename = "test_auth1.json";
    std::remove(authFilename.c_str());
    json::rvalue outAuth = json::load("{ \"admin\": \"1234ABCD\", \"someone\": \"DEADBEEF\" }");

    WHEN("file is written and read back") {
      writeAuthFile(authFilename, json::dump(outAuth));
      json::rvalue inAuth = readAuthFile(authFilename);

      THEN("the JSON has the correct keys and values") {
        REQUIRE(inAuth.has("admin"));
        REQUIRE(inAuth.has("someone"));
        REQUIRE(inAuth["admin"] == "1234ABCD");
        REQUIRE(inAuth["someone"] == "DEADBEEF");
      }
    }

    std::remove(authFilename.c_str());
  }
}

SCENARIO("Lookup valid and invalid usernames/password", "[auth]") {
  GIVEN("An empty auth.json") {
    initRand();
    const string authFilename = "test_auth2.json";
    std::remove(authFilename.c_str());

    WHEN("file does not exist") {
      bool result = checkPassword("admin", "admin", authFilename);
      THEN("the result is false") { REQUIRE(result == false); }
    }

    WHEN("a password is changed with force") {
      bool result = changePassword("admin", "", "admin", authFilename, true);

      THEN("that user can then be authenticated") {
        bool result = checkPassword("admin", "admin", authFilename);
        REQUIRE(result == true);
      }
      THEN("but a non-existent user cannot be authenticated") {
        bool result = checkPassword("some_dude", "admin", authFilename);
        REQUIRE(result == false);
      }
    }

    WHEN("a password is changed") {
      bool result = changePassword("test", "admin", "yyy", authFilename, false);

      THEN("the old password no longer works") {
        bool result = checkPassword("admin", "admin", authFilename);
        REQUIRE(result == false);
      }
      THEN("but the new password does work") {
        bool result = checkPassword("admin", "yyy", authFilename);
        REQUIRE(result == false);
      }
    }

    std::remove(authFilename.c_str());
  }
}