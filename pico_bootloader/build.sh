#!/bin/bash
export PICO_SDK_PATH=../../pico-sdk
export PICO_EXAMPLES_PATH=../../pico-examples
export PICO_EXTRAS_PATH=../../pico-extras

rm -r build
mkdir -p build
cd build
cmake ../
make -j$(nproc)
