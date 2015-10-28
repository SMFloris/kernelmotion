#!/bin/bash

modprobe -rf leapmotion
rmmod leapmotion
pkill dmesg
pkill dmesg