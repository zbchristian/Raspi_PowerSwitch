Power Switch for Raspberry Pi based on Digispark Tiny Arduino
=============================================================
Per default the Raspberry Pi is powered via the USB port, but no power switch exists for a controlled shutdown.

This device connects to the GPIO and power pins of the extension port.

An Arduino Tiny (Digispark) controls the 5V supply voltage via an MOSFET switch (up to 3A).
In addition the Arduino 
 - reads the status of the Raspberry Pi on ´GPIO 27´. Pulled low while the Raspberry Pi is running, 
 - sets the shutdown request on ´GPIO 17´ in case the button is pressed, 
 - disconnects the 5V supply from the Raspberry Pi after a predefined timeout.

The device can be powered by a 5V supply, or a DC-DC converter can be added to allow the powering directly from a 12V supply (e.g. car battery). 
The Arduino is supplied by a 3.3V regulator on the board. This ensures, that the signals of the Arduino and the Raspberry Pi are compatible.

The default power button is a touch button connected via a 1MOhm resistor to Port 4 and sensed via Port 2 of the ATTINY85.
For this the package ´CapacitiveSensor´ is required. A standard button can be connected to Port 2 as well.

This device has been developped for the powering of a Raspberry Pi 3a used as Wireless access point (installed is RaspAP) in a very flat case. The Raspberry Pi 
is used in a Camper Van to connect to camp ground wireless networks or LTE/4G. The device is powered in this case by 12V.   
The power required by the device, when the Raspberry Pi is switched off, is about 20mW (ca. 1.5mA@12V).

Required components
-------------------
 - Arduino Tiny - Digispark board. The version with the micro USB port is preferred. This allows to program the board while soldered to the PCB.
 - DC-DC converter in case the the input voltage is higher than 5V.
 - PCB and the required components (SMD components and a pin header)

Board Design
------------
The board connects to the first 14 GPIO pins via a pin header, which is installed to the bottom of the board. 

The design of the board is somewhat special, because a very flat layout is needed to fit into a standard case. 
Therefore the Arduino Tiny is soldered to SMD pads. This is not difficult, but requires some experience. Add an isolating tape to the bottom of the Arduino to avoid shorts. 

On the bottom side of the board a solder jumper sets the source of the power. A closed jumper connects the input voltage from the pin headder directly to the Raspberry Pi and the 
on board regulator. For higher voltages, e.g. 12V from a car battery, a mini DC-DC converter can be soldered to the SMD pads on the bottom side. The output voltage of the DC-DC converter
should be set to 5V.

Changes needed on the Raspberry Pi
----------------------------------
Add the following lines to ´/boot/config.txt´
´´´´
# Request shutdown by setting GPIO 17 to HIGH
dtoverlay=gpio-shutdown,gpio_pin=17,active_low=0,gpio_pull=down
# GPIO 27 flags the shutdown of the system
gpio=27=op,dl
´´´´


Quiescent Power Consumption
--------------------------
The Arduino Tiny will draw up to 10mA when powered by the onboard 5V regulator (78X05). This is mainly due to the quiescent current of the regulator itself , the power LED 
and the 1.5kOhm pull-up resistor connected to port 3 of the ATTINY85 (needed for the USB programming). The ATTINY85 with reduced frequency of about 1MHz
utilizes only a few hundred micro Amps. 

Reduce the required power by
 - run the ATTINY85 at a low frequency (e.g. 1MHz)
 - use an external 3.3V regulator with low quiescent current (here MCP1703). Remove the 78X05 from the board (might not be necessary).
 - either remove the power LED, or change at least the series resistor from 1kOhm to 10kOhm
 - remove the LED attached to Port 1, or increase the series resistor to 10kOhm
 - disconnect the 1.5kOhm resistor from the power rail of the ATTINY85 and connect it to the USB 5V line (USB connector side of the corresponding Schottky diode).

For a 5V powered system, this should bring the qiescent current down well below 1mA at 5V.
With the installed DC-DC converter the current stays at about 1.5mA at 12V (18mW). 
