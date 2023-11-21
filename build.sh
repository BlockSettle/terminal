#!/bin/bash
python generate.py release --appimage
cd terminal.release
make -j$(nproc)
