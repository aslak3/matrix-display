#!/bin/sh

sudo picotool reboot -u -F
sleep 2
sudo picotool load src/matrix-display.uf2
sudo picotool reboot -a -f
