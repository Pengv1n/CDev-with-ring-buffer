#!/bin/bash
sudo /bin/bash -c "echo "$(TZ=Europe/Moscow date +%T)" >> /dev/circ_cdev0"

