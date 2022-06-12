#!/usr/bin/env bash

# run after pre-requisites are installed
# https://www.nsnam.org/docs/tutorial/html/getting-started.html

./waf clean
CXXFLAGS="-Wall -g -O0" ./waf configure --enable-tests --build-profile=debug --enable-examples --python=/usr/local/bin/python3
./waf build
