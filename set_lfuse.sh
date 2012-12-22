#!/bin/sh

sudo avrdude -p attiny45 -P usb     -c usbtiny -U lfuse:w:0x61:m
