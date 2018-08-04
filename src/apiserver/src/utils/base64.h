// Copyright 2018 Obelisk Inc.
#include <string>
#include <vector>

std::string base64_encode(uint8_t const *buf, unsigned int bufLen);
std::vector<uint8_t> base64_decode(std::string const &encoded_string);
