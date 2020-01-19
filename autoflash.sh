#!/bin/bash

# States
# 0 = waiting
# 1 = Found Uno but with DFU disable
# 2 = DFU has been enabled, time to flash

_STATE=0

echo "Waiting device"
while true
do
  if [ $_STATE -eq 0 ]; then
    if lsusb | grep -q "Hori Co., Ltd"; then
      _STATE=1
      clear
      echo "Device found"
      echo "Waiting for short on reset"
    fi
  elif [ $_STATE -eq 1 ]; then
    if lsusb | grep -q "Atmel Corp."; then
      _STATE=2
    fi
  elif [ $_STATE -eq 2 ]; then
    echo "Flashing"
    make flash
    echo "done"
    echo "Waiting device"
    _STATE=0
  fi
done