#!/bin/bash
cd ~/source/portator/publish && rm -r *
cd ~/source/portator && make
cd ~/source/portator/publish && zipinfo -1 portator
ls
