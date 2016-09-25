#!/bin/bash

BATCH=$1
PORT=$2
COUNT=0

function clearLastLine {
    tput cuu 1 && tput el
}

echo "Programming batch $BATCH on port $PORT"

while true; do 
    CHECK=""
    I=1
    echo "Waiting"
    while [[ ! $CHECK =~ "Chip ID" ]]; do
        clearLastLine
        echo -n "Waiting for ESP12"
        for i in `seq $I`; do echo -n .; done
        echo 
        CHECK=`gtimeout 1 esptool.py --port "$PORT" chip_id`
        ((I++))
        if [[ $I -gt 3 ]]; then
            I=1
        fi
    done

    fail=0
    I=1
    for i in `seq 3`; do
        CHECK=`gtimeout 1 esptool.py --port "$PORT" chip_id`
        if [[ ! $CHECK =~ "Chip ID" ]]; then
            fail=1
        fi

        clearLastLine
        echo -n "ESP12 Found, stabilizing"
        for i in `seq $I`; do echo -n .; done
        echo 

        ((I++))
        if [[ $I -gt 3 ]]; then
            I=1
        fi
        sleep .25
    done

    if [[ $fail -eq 1 ]]; then
        continue
    fi

    echo -n "Uploading"
    ./esptool -vv -cd nodemcu -cb 115200 -cp $PORT -cf _firmware.bin | while read -n 1 c
    do
        if [[ "$c" == "." ]]
        then
            echo -n "."
        fi
    done
    echo

    if [ $? -ne 0 ]; then
        continue;
    fi

    sleep .25

    echo "Configuring Flickerstrip.."
    python configure.py "$BATCH" "$PORT"

    if [ $? -ne 0 ]; then
        #try one more time..
        python configure.py "$BATCH" "$PORT"

        if [ $? -ne 0 ]; then
            continue;
        fi
    fi

    echo "Success!"
    echo "Waiting"

    I=1
    while [[ $CHECK =~ "Chip ID" ]]; do
        clearLastLine
        echo -n "Waiting for disconnect"
        for i in `seq $I`; do echo -n .; done
        echo 
        CHECK=`gtimeout 1 esptool.py --port "$PORT" chip_id`
        ((I++))
        if [[ $I -gt 3 ]]; then
            I=1
        fi
    done

    echo "Disconnected!"
    echo

done

