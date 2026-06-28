#!/bin/bash
mkdir -p build
gcc pqp_usb_bridge.c pqp_hub.c -lm -pthread -lpthread -lrt -O3 -o ./build/usb_hub

