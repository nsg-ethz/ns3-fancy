#!/usr/bin/env bash

# More info: https://www.nsnam.org/wiki/Installation#Installation
# pdf with instructions (i did not follow) https://docs.google.com/viewer?a=v&pid=forums&srcid=MTAzNDM3NDI4OTU0NzQzNTg4NTYBMTMxODg5MzUwNTg5MjY3MTQ2ODQBTXY1bl9MWExBd0FKATAuMQEBdjI&authuser=0

# Install xcode, homebrew, etc
xcode-select -install

# Install brew
/usr/bin/ruby -e "$(curl -fsSL
https://raw.githubusercontent.com/Homebrew/install/master/install)"

# Install mercurial
brew install mercurial

# Install legacy headers
sudo open /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg

# Install Qt5

# Install librt
brew install librt

# Install boost lib
# http://macappstore.org/boost/
brew install boost

# configure
CXXFLAGS="-Wall -g -O0" ./waf configure --enable-tests --build-profile=debug --enable-examples --python=/usr/local/bin/python3
# ./waf configure': CXXFLAGS="-Wall -g -O0" ./waf configure --enable-tests --build-profile=optimized --enable-examples
./waf build