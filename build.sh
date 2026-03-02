#!/bin/bash

# Create a build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Navigate into the build directory
cd build

# Run CMake to generate build files
cmake ..

# Compile the code
cmake --build .
