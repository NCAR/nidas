#!/bin/bash
echo "Testing FTDI serial port transceiver and power control lines."
for i in {0..7}; 
   do pio $i RS422 on; 
done
echo "Observe that all the LEDs are off or glowing dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do pio $i RS422 off;  
done
echo "Observe that the Power LED is glowing bright blue, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do pio $i RS485_HALF on; 
done
echo "Observe that the RS485_HALF LED is glowing bright green, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do pio $i RS232 on; 
done
echo "Observe that the RS232 LED is glowing bright amber, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do pio $i LOOPBACK off; 
done
echo "Observe that all the LEDs are glowing brightly."
read -p "Press any key to continue..." -n1 -s
echo ""
echo "Done testing FTDI serial port transceiver and power control lines."
