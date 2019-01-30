#! /bin/bash

export MACOSX_DEPLOYMENT_TARGET=10.11.4
export DEV_3RD_ROOT=../../3rd_10.11.4
export CMAKE_OSX_ARCHITECTURES=x86_64

python make_package.py ../..