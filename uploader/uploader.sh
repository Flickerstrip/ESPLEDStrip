#!/bin/bash

#esptool: https://github.com/igrr/esptool-ck/releases
#esptool.py: https://github.com/themadinventor/esptool/releases

BATCH=$1
PORT=$2
COUNT=0

function clearLastLine {
    tput cuu 1 && tput el
}

function displaytime {
  local T=$1
  local D=$((T/60/60/24))
  local H=$((T/60/60%24))
  local M=$((T/60%60))
  local S=$((T%60))
  (( $D > 0 )) && printf '%d days ' $D
  (( $H > 0 )) && printf '%d hours ' $H
  (( $M > 0 )) && printf '%d minutes ' $M
  (( $D > 0 || $H > 0 || $M > 0 )) && printf 'and '
  printf '%d seconds\n' $S
}

TIMEOUT="timeout"

which timeout
if [ $? -ne 0 ]; then
    TIMEOUT="gtimeout"
fi

echo "Programming batch $BATCH on port $PORT"
echo "Start time: `date`"

START=`date +%s`
PROGRAMMED=0

while true; do 
    CHECK=""
    I=1
    echo "Waiting"
    while [[ ! $CHECK =~ "Chip ID" ]]; do
        clearLastLine
        echo -n "Waiting for ESP12"
        for i in `seq $I`; do echo -n .; done
        echo 
        CHECK=`$TIMEOUT 1 ./esptool.py --port "$PORT" chip_id`
        ((I++))
        if [[ $I -gt 3 ]]; then
            I=1
        fi
    done

    fail=0
    I=1
    for i in `seq 3`; do
        CHECK=`$TIMEOUT 1 ./esptool.py --port "$PORT" chip_id`
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
            echo -n .
        fi
#echo -n $c 
    done

    if [ $? -ne 0 ]; then
        echo
        echo "Error uploading firmware!"
        continue;
    fi

    echo

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
    ((PROGRAMMED++))
    CTIME=`date +%s`
    DELTA=$((CTIME-START))
    AVG=$((DELTA/PROGRAMMED))
    echo "Programmed $PROGRAMMED in `displaytime $DELTA`"
    echo "Average time per unit: $AVG seconds";

    echo "Waiting"

    I=1
    while [[ $CHECK =~ "Chip ID" ]]; do
        clearLastLine
        echo -n "Waiting for disconnect"
        for i in `seq $I`; do echo -n .; done
        echo 
        CHECK=`$TIMEOUT 1 ./esptool.py --port "$PORT" chip_id`
        ((I++))
        if [[ $I -gt 3 ]]; then
            I=1
        fi
    done

    echo "Disconnected!"
    echo

done

