#!/bin/bash
if [ ! -d "modules/network/lwNBD" ] || [ ! -f "modules/network/lwNBD/Makefile" ]; then
  rm -rf modules/network/lwNBD
  git clone --depth 1 https://github.com/ps2homebrew/lwNBD.git modules/network/lwNBD || true
fi
if [ -d "modules/network/lwNBD" ]; then
  touch modules/network/lwNBD/.config
  echo "CONFIG_NBD=y" > modules/network/lwNBD/.config
  echo "CONFIG_BLOCK=y" >> modules/network/lwNBD/.config
fi