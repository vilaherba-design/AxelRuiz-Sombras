#!/bin/sh
# Off-game tests (Linux, g++ or clang++ with C++23). Optional args: <Shaders012_VR.fxp> <docs/catalog/Shaders012_VR.csv>
set -e
D="$(dirname "$0")"
CXX=${CXX:-g++}
$CXX -std=c++23 -O2 -Wall -Wextra -o /tmp/sombras_tests "$D/test.cpp" "$D/../src/Config.cpp"
/tmp/sombras_tests "$@"
