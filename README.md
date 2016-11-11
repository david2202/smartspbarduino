# Installation (Linux)

1. Connect the shield to the Arduino
2. Power the Arduino from an external power source (i.e. not the PC USB port)
3. Connect the shield to a PC serial port with a mini USB cable
4. Power on the shield by pressing the power button
5. In Linux enter the command:
~~~
lsusb
~~~
6. You should see something like the following:
~~~
Bus 002 Device 002: ID 8087:8000 Intel Corp.
Bus 002 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 001 Device 002: ID 8087:8008 Intel Corp.
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 004 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 003 Device 010: ID 05c6:9000 Qualcomm, Inc. SIMCom SIM5218 modem
Bus 003 Device 005: ID 0461:4d15 Primax Electronics, Ltd Dell Optical Mouse
Bus 003 Device 004: ID 413c:2010 Dell Computer Corp. Keyboard
Bus 003 Device 002: ID 413c:1003 Dell Computer Corp. Keyboard Hub
Bus 003 Device 006: ID 0cf3:0036 Atheros Communications, Inc.
Bus 003 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
~~~
7. Enter the command:
~~~
dmesg | grep tty
~~~
8. You should see something like the following:
~~~
[139140.055304] usb 3-8: GSM modem (1-port) converter now attached to ttyUSB0
[139140.055366] usb 3-8: GSM modem (1-port) converter now attached to ttyUSB1
[139140.055452] usb 3-8: GSM modem (1-port) converter now attached to ttyUSB2
[139140.055509] usb 3-8: GSM modem (1-port) converter now attached to ttyUSB3
[139140.055567] usb 3-8: GSM modem (1-port) converter now attached to ttyUSB4
~~~
9. The 3G shield is now connected to the PC
10. Download a serial terminal program such as gtkterm:
~~~
sudo apt-get install gtkterm
~~~
11. Open the terminal using sudo to avoid any permission issues:
~~~
sudo gtkterm
~~~
12. Select Configuration > Port
13. Select /dev/ttyUSB2 as the port
14. Select 115,200 as the Baud rate
15. Select none as the Parity
16. Select 8 as the bits
17. Select 1 as the stopbits
18. Select none as the Flow Control
19. Click on OK
20. Select Configuration > Local Echo
19. Enter the command ATI:
~~~
ATI
Manufacturer: SIMCOM INCORPORATED
Model: SIMCOM_SIM5216A
Revision: SIM5216A_V1.5
IMEI: 359769034498003
+GCAP: +CGSM,+FCLASS,+DS

OK
~~~
20. The above shows that you are now communicating with the 3G shield.
21. Enter the command to permanently change the baud rate to 9600, suitable for use by the Arduino software serial:
~~~
AT+IPREX=19200
~~~
22. Change RX and TX jumper pins on 3G shield to pins 6 (RX) and 7 (TX).  If you don't do this you won't be able to upload or communicated with the Arduino from the PC.


~~~
AT+CREG? // Check that we are connected to the telephone network
AT+CGSOCKCONT=1,"IP","telstra.wap" // Configure the connection type and APN
AT+CSOCKAUTH=1,0 // Set the authentication to none
AT+CHTTPACT="www.ebay.com.au",80
GET http://www.ebay.com.au HTTP/1.1
Host: www.ebay.com.au
Content-Length: 0

<ctrl>Z

{"grams": 505, "degreesC": 21.2}
~~~
