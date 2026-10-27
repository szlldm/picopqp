#!/bin/bash
export PICO_SDK_PATH=../../pico-sdk
export PICO_EXAMPLES_PATH=../../pico-examples
export PICO_EXTRAS_PATH=../../pico-extras
export CMAKE_POLICY_VERSION_MINIMUM=3.5

rm -r build
mkdir -p build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ../
make -j$(nproc)
