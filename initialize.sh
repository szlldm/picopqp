#!/bin/bash

# init submodules
git submodule update --init --recursive

PICO_SDK_URL="https://github.com/raspberrypi/pico-sdk.git"
TARGET_DIR="pico-sdk"
TAG="1.5.1"

# Check if directory already exists
if [ -d "$TARGET_DIR" ]; then
	echo "Directory '$TARGET_DIR' already exists."
else
	echo "Cloning repository..."
	git clone "$PICO_SDK_URL" "$TARGET_DIR"
	cd "$TARGET_DIR" || exit 1
	git checkout "$TAG"
	git submodule update --init
	cd ..
fi

