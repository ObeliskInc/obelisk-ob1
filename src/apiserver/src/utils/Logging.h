// Copyright 2018 Obelisk Inc.

#ifndef LOGGING_H
#define LOGGING_H

#include "../Crow.h"

class FileAndCerrLogHandler : public ILogHandler {
public:
  void log(std::string message, LogLevel /*level*/) override { std::cerr << message; }
};

#endif