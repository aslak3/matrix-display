#!/bin/sh

TOOL_DIR=/usr/local/gcc-arm-embedded-14.2.rel1/

cmake \
  -DCMAKE_C_COMPILER="$TOOL_DIR/bin/arm-none-eabi-gcc" \
  -DCMAKE_CXX_COMPILER="$TOOL_DIR/bin/arm-none-eabi-g++" \
  -DPICO_TOOLCHAIN_PATH="$TOOL_DIR" \
  ..

