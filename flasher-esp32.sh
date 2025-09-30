#!/bin/sh

esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before=default_reset \
    --after=hard_reset write_flash --flash_mode dio --flash_freq 80m \
    --flash_size 2MB 0x0 bootloader/bootloader.bin 0x10000 \
    matrix-display.bin 0x8000 partition_table/partition-table.bin
