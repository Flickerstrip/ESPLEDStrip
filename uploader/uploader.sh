#!/bin/bash

PORT=$1
COUNT=0

while true; do 
    CHECK=`gtimeout 1 esptool.py --port "$PORT" chip_id`
    if [[ $CHECK =~ "Chip ID" ]]; then
        if [[ $COUNT -ge 0 ]]; then
            ((COUNT++))
            if [[ $COUNT -gt 2 ]]; then
                echo "UPLOADING CODE"
                ./esptool -vv -cd nodemcu -cb 115200 -cp $PORT -cf _firmware.bin

                COUNT=-1
            fi
        fi
    else
        COUNT=0
    fi
done

