#!/bin/bash
cd ~/source/portator/publish && rm -r *
cd ~/source/portator/blink && make clean && make -j4
cd ~/source/portator && make
cd ~/source/portator/publish && zipinfo -1 portator
cd ~/source/portator/publish && ./portator list
ls
