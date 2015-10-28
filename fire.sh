#!/bin/bash

git pull origin

sudo modprobe -rf leapmotion && rmmod leapmotion
make && sudo insmod leapmotion.ko
sudo modprobe -rf uvcvideo
dmesg -wk

