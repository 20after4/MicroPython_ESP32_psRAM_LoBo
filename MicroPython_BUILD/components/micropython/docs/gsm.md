# **gsm** Module

---

This module includes full support for using various GSM/GPRS modules connected via UART for Internet access as well as for sending/receiving SMS messages.

2G, 3G or 4G modules can be used, tested with Simcom and Telit modules, but any module should work.

---

### Connecting the GSM module

| ESP32 pin | GSM module | Notes |
| - | - | - |
| TX, any output pin | Rx | GSM module Tx pin |
| RX, any pin | Tx | GSM module Rx pin |
| RTS, any output pin | CTS | GSM module CTS pin, optional, not used in default configuration |
| CTS, any pin | RTS  | GSM module RTS pin, optional, not used in default configuration |
| GND | GND  | Power supply ground |
| ... | Vcc  | GSM module power supply, usualy 3.4~4.2 V (LiPo battery) |

*If rts&cts are set, UART hardware flow controll is set, if not, no flow controll is used*. 

**GSM modules usualy have IO interface pins working in 2.8 or 1.8 V range. When connecting to ESP32 3.3V level shifters must be used**

If the GSM module has 2.8V interface pins (like **SIM800**), the module **output** (Tx) can be connected directly to ESP32 **input** (Rx).

GSM module **input** (Rx) can be connected to the ESP32 **output** (Tx) via schottky diode in a way that diode cathode is facing ESP32 pin. GSM module pin must have pull-up resistor to 2.8V (SIM800 has this resistor integrated).

**UART1** is used for GSM communication. While enabled it cannot be used from other MicroPython modules.

**Some GSM modules have ON/OFF pin**. If used, it must be handled outside of this module.

---

All GSM operations are controlled by the ESP32 task running on the core different of the one MicroPython is running.

After initialization, the GSM can be in two different states:

**online** - connected to the Internet, all network functions works the same way as with Internet connection via WiFi.

**offline** - disconnected from the Internet, SMS functions can be executed as well as any of GSM **AT commands**

While GSM is connected to the Internet, WiFi can be used only in **AP mode**

---

## Methods

---

### gsm.start( tx=pinnum, rx=pinnum, apn="provider_apn" [rts=-1, cts=-1, user="", password="", connect=False, wait=False] )

Start the GSM task, initialize GSM, optionaly start PPPoS and connect to the Internet.

All arguments are **KW** arguments.

*Pins have to be given as* **pin numbers**, *not the machine.Pin objects*

| Argument | Description |
| - | - |
| tx | UART Tx output on ESP32, connected to the GSM Rx input |
| rx | UART Rx input on ESP32, connected to the GSM Tx output |
| apn | Your Internet provider APN |
| rts | optional, default: not used, UART RTS output on ESP32, connected to the GSM CTS input |
| cts | optional, default: not used, UART CTS input on ESP32, connected to the GSM RTS output |
| user | optional, default="", user name if required by the provider |
| password | optional, default="", password if required by the provider |
| connect | optional, default=False, connect to the Internet after GSM initialization if set to True |
| wait | optional, default=False, wait for operation to finish if set to True. If False, return immediately, connection status can be checked with *gsm.status()* |


### gsm.stop()

Disconnect from the Internet if connected, stop the GSM task and free the memory used.


### gsm.status()

Return the current GSM status as tuple of numeric and string values.

Possible return values:

* **(98, Not started)** GSM task not started or initializing
* **(0, Disconnected)** PPPoS started, disconnected from Internet
* **(1, Connected)** PPPoS started, connected to Internet, network functions can be used
* **(89, Idle)** GSM initialized, PPPoS not started, SMS and AT commands can be used


### gsm.connect()

If in **Idle** state, start PPPoS and connect to the Internet.


### gsm.disconnect()

If in **Connected** state, disconnect from Internet and stop PPPoS.


### gsm.sendSMS(gsm_num, msg)

Send SMS message **msg** to the gsm number **gsm_num**. The number has to be in the international format *<counry_code><number>*

Returns **True** if message successfully sent, **False** if not.


### gsm.checkSMS(sort=gsm.SORT_NONE, status=gsm.SMS_ALL)

Chech the number of received messages.

Returns **tuple** of message indexes or *None* if no messages are found.

Returned indexes can be sorted by message time in ascending (**gsm.SORT_ASC**) or descending (**gsm.SORT_DESC**) order.

All, read or unread messages can be checked (**gsm.SMS_ALL**, **gsm.SMS_READ**, **gsm.SMS_UNREAD**).


### gsm.readSMS(idx [,delete])

Read and return the message at index **idx** or *None* if no message is found at that index.

If the optional argument **delete** is *True* delete the message after reading.

The message is returned as tuple:

| Position | Content | Note |
| - | - | - |
| 0 | idx | integer, message index |
| 1 | status | string: **"REC UNREAD"** or **"REC READ"** |
| 2 | from | string, sender's GSM number in international format |
| 3 | time | string, message time as string in GSM format |
| 4 | timeval | integer, message time as unix time |
| 5 | tz | integer, message time zone |
| 6 | msg | string, message text |


### gsm.deleteSMS(idx)

Delete the message at index **idx**. Returns *True* on success or *False* if no message is found at that index.


### gsm.sms_cb(function [,interval])

Register the **callback function** which will be executed on new message arrival.

Message check interval can be set by optional **interval** argiment to 5 ~ 86400 seconds. Default check interval is 60 seconds.

The argument returned to the callback function is tuple of **unread** message indexes sorted in descending order.

```
def smscb(indexes):
    if indexes:
        msg = gsm.readSMS(indexes[0], True)
        if msg:
            print("New message from", msg[2])
            print(msg[6])

gsm.sms_cb(smscb, 30)
```


### gsm.atcmd(cmd, timeout=500, response=None, cmddata=None, printable=False)

Send **AT Command** to the GSM module. Return the response string.

Arguments:

| Argument | Function |
| - | - |
| cmd | command to send to the GSM module. Must start with **AT** or **at**. |
| timeout | time to wait for the response in mili seconds, default: 500 |
| response | wait for the specific response string, default: None |
| cmddata | send additional string to the GSM module **after** the response string is detected, default: None |
| printable | remove all non printable characters from the response, default: False |


```
>>> import gsm
>>> gsm.start(tx=27, rx=21, apn='internet.ht.hr')
D (54121) intr_alloc: Connected src 35 to int 13 (cpu 0)
True
>>> gsm.status()
(98, 'Not started')
>>> gsm.status()
(89, 'Idle')
>>> gsm.atcmd('ATI4')
'\r\nSIM800 R13.08\r\n\r\nOK\r\n'
>>> gsm.atcmd('ATI4',printable=True)
'..SIM800 R13.08....OK..'
>>> 
>>> 
>>> #Send SMS using AT commands
>>> gsm.atcmd("AT+CMGF=1",printable=True)
'..OK..'
>>> 
>>> gsm.atcmd('AT+CMGS="+385119876543"',timeout=1000,response='> ',cmddata='Hi from MicroPython\x1a',printable=True)
'..> ........'
```

### gsm.debug(True | False)

Enable or disable printing GSM task debug messages

