#!/bin/bash
set -e
cd ~/source/portator/publish && rm -rf *
cd ~/source/portator/blink && CFLAGS="-g -O2 -Wno-cast-align" ./configure CC=cosmocc AR=cosmoar --enable-vfs && make clean && make CC=cosmocc AR=cosmoar -j4
cd ~/source/portator && make clean && make
cd ~/source/portator/publish && zipinfo -1 portator
### rm -f /tmp/portator.log
cd ~/source/portator/publish && timeout -s KILL 10 ./portator list
### echo "--- /tmp/portator.log ---"
### cat /tmp/portator.log 2>/dev/null || echo "(no log file)"
