#!/bin/bash
echo "Testing FTDI serial port transceiver and power control lines."
for i in {0..7}; 
   do dsm_port_config -m RS422 -e POWER_ON -p $i; 
done
echo "Observe that all the LEDs are off or glowing dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do dsm_port_config -m RS422 -e POWER_OFF -p $i;  
done
echo "Observe that the Power LED is glowing bright blue, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do dsm_port_config -m RS485_HALF -e POWER_ON -p $i; 
done
echo "Observe that the RS485_HALF LED is glowing bright green, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do dsm_port_config -m RS232 -e POWER_ON -p $i; 
done
echo "Observe that the RS232 LED is glowing bright amber, and all other LEDs are off or glowing only dimly."
read -p "Press any key to continue..." -n1 -s
for i in {0..7}; 
   do dsm_port_config -m LOOPBACK -e POWER_OFF -p $i; 
done
echo "Observe that all the LEDs are glowing brightly."
read -p "Press any key to continue..." -n1 -s
echo ""
echo "Done testing FTDI serial port transceiver and power control lines."
