# msp430-midi-controller

A code example for MSP430 series microcontroller (MSP430G2553) that uses an ultrasonic sensor (HC-SR04) and a potentiometer (foot pedal in this case) to send signals for MIDI control.

# introduction

MIDI controllers can be awfully expensive. This code gives you the flexibility of designing your own MIDI controller using potentiometers and demonstrates the ability to use other devices, eg. an ultrasonic sensors, capacitive touches, joysticks, and variety of other sensors, as MIDI controllers.

# working

In this particular code example I used a 'stranger SVP2 stereo volume pedal', bought from Amazon India. It changes it's resistance of output from 2k to 9k ohms on changing the foot control. 
A potentiometer or a similar pedal can be used using a voltage divider. For the selection of secondary resistance in voltage divider, use the formula R=sqrt(Rmin*Rmax), where Rmin and Rmax are min and maximum resistances of the pedal or knob setting on a full swing. 

Further, to match it to give full scale output of MIDI control (from 0 to 127), a calibration algo is incorporated, that changes the upper and lower thresholds on the press of a button (see code).

The Ultrasonic sensor detects the distance of the hand or obstacle and accordingly sends the MIDI control signal. It's upper and lower threshold distances can be changed in the code, or a method similar to calibration of the foot pedal can be deployed here also.

# UART to MIDI
there are 2 options here:

1. Use the MSP430Launchpad (that has the FTDI chip integrated) to connect to the system as virtual COM over USB and then use 'Hairless MIDI Serial Bridge'(http://projectgus.github.io/hairless-midiserial/) at 9600 baud to forward the MIDI data to a virtual driver like 'LoopBe1' (http://www.nerds.de/en/loopbe1.html). You can then receive this data from your virtual  port in your DAW.

2. Use a MIDI-to-USB converter cable and connect the MIDI Female Connector to the UART Tx Pins and GND according to the pinout (refer other ources from internet). Use opto-couplers or voltage level buffers (eg. IC 4050), if needed. There weren't needed in my case. Also, set the UART Baud to 31250 for the microcontroller (see code).
