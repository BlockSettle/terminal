# Supported platforms
Linux x64 (Ubuntu), MacOS X, Windows x64 with VS2015

# Before building (pre-requisites)
## Windows
* Install Python 2.7 (3.6 wasn't fully tested, yet)
* Install cmake
* Install MySQL Connector from https://dev.mysql.com/downloads/windows/installer/ and choose the web installer (select both Connector/C and Connector/C++ for installation)
* Install YASM from https://yasm.tortall.net/Download.html (download Win64 ZIP archive and unpack it to c:\Program Files\yasm)

## Ubuntu
* Install python-pip package (it will install Python)
* Install cmake
* Install Qt5 build pre-requisites by running `apt build-dep qt5-default`
* Install libmysqlclient-dev package
* Install autoconf and libtool packages
* Install yasm package

## MacOS X
To be added later...

## Common to all platforms
Install the following Python modules (pip install):
* wget
* requests

# Cloning the repo
Use your favourite git command to clone this repo, then enter it and execute:
* git submodule init
* git submodule update

# Building BlockSettle terminal

* Run the following command:
`python generate.py [debug]`
(last optional argument is used to enable debugging symbols)

* Go to `terminal.debug` or `terminal.release` dir (depending on the 'debug' argument on the previous step) and type your favourite make command (basically `make -j4`). Windows users should open the BlockSettle.sln file in one of these dirs.

* The binary can then be found in build_terminal dir
