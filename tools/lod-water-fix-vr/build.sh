#!/bin/sh
# Cross-build from Linux: pip install ziglang
cd "$(dirname "$0")" && python3 -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++20 -shared -s -Wall -Wextra -o LODWaterFixVR.dll LODWaterFixVR.cpp -lole32 -lshell32 -luuid && rm -f LODWaterFixVR.lib
