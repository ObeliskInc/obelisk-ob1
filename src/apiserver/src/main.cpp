// Copyright 2018 Obelisk Inc.

#include "CgMinerTypes.h"
#include "Crow.h"
#include "utils/HttpStatusCodes.h"
#include "utils/SafeQueue.h"
#include "utils/utils.h"
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <unordered_map>

extern void *runCgMiner(void *);
extern void runCrow(int port);

int main(int argc, char *argv[]) {
  // Check if we should run as a daemon
  if (argc > 1 && strcmp(argv[1], "--daemon") == 0) {
    daemon(0, 0);

    // Since we daemonize ourselves, it is our responsibility to create the pid file for init.d
    // to use for tracking/killing this process.
    pid_t daemon_pid = getpid();
    ostringstream cmd;
    cmd << "echo " << daemon_pid << " > /var/run/apiserver.pid";
    runCmd(cmd.str());
  }

  pthread_t cgMinerThread;
  pthread_create(&cgMinerThread, NULL, runCgMiner, NULL);

  // Force password set (after deleting auth.json)
  // changePassword("admin", "", "admin", AUTH_FILE, true);

  runCrow(8080);

  return 0;
}
