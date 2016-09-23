#!/bin/bash

echo "opening port.."
exec 3<> /dev/cu.usbserial-AL00P7CO
echo "waiting for arduino.."
sleep 3

echo "sending command"
echo "mac" >&3

echo "receiving response"
cat <&3

echo "closing"
exec 3>&-
