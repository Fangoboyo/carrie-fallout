#!/bin/bash

# Stop the default camera app first
adb -H host.docker.internal shell "killall rkipc; killall ispserver; RkLunch-stop.sh" 2>/dev/null

# Open an interactive shell in the root directory
adb -H host.docker.internal shell "cd /root && exec /bin/sh"