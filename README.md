# Orb

Orb is a USB powered LED lamp containing an embedded microcontroller with WiFi running a HTTP server. The server provides simple HMTL based configuration and UI pages suitable for mobile devices. 

The lamp can also be controlled remotely using a set of HTTP endpoints and MQTT commands. 
 

A USB-A socket on the side of case can be used to switch USB powered devices on and off such as LED fairy lights and low powered fans. It also contains a piezo-electric buzzer. Additionally it has an RF 433Mhz reciever that can be dynamically configured to control the devices functions using many EV1527 4CH based transmitters.


Here's the a picture of the first Orb. See the wiki pages for more pictures.

<img src="https://user-images.githubusercontent.com/2019989/41097744-677b9d02-6a9c-11e8-8952-004872332f09.jpg" width=400>


Below is a screen shot taken from an iPhone showing the simple UI served up from the embedded HTML server.

It's designed to look like an old style TV remote control.

<img src="https://user-images.githubusercontent.com/2019989/41035960-b4d279c0-69d1-11e8-82b0-8630fc84b622.jpg" width=200>

## Wifi Setup

Upon reset, the system will try to reconnect to the last WiFi network it successfully connected to, if that fails then it starts an **Access Point** WiFi on SSID "**Led Orb Config**". Use a phone to connect to this WiFi network and the following screen will load.

<img src="https://user-images.githubusercontent.com/2019989/41467247-12a55b4e-70e9-11e8-91ba-a7382eb94dc4.PNG" width=200>

Select "configure Wifi" and a screen similiar the following will appear:

<img src="https://user-images.githubusercontent.com/2019989/41467740-f99be756-70ea-11e8-916a-b99053641e74.png" width=200>

Select the SSID or type it in and add the password.
You can also configure the MQTT server details here if required.
Press save and the device will switch off the AP WiFi and connect to the supplied WiFi network.


## HTTP endpoints

### Lamp Colour
URL | Paramters | Function
------------ | ------------- | -------------
/colour| value=[r,R,g,G,b,B,w,W,z,Z] | r=red, g=green, b=blue , w=white , z=black ( lc = half, uc = full)

### Sequencer Patterns
URL | Paramters | Function
------------ | ------------- | -------------
/flash |  | flash
/blink |  | blink once
/fade | value= | fade to black
/ramp |  | ramp to white
/alternate |  | alternate between Red and Green
/rainbow |  | rotate through hues
/disco |  | random colours
/sos |  | green SOS sequence
/pattern | pattern= | play sequence of colours ie. pattern=rGzWw


### Sequencer Speed
URL | Paramters | Function
------------ | ------------- | -------------
/start |  | start sequencer cycle
/stop |  | stop sequencer 
/speed | value= | set the sequencer step time in Milliseconds
/slow |  | sequencer step size = 200ms
/normal |  | sequencer step size = 100ms
/fast |  | sequencer step size = 50ms
/slower |  | slow down sequencer
/faster |  | speed up sequencer

### Buzzer and USB-A Socket
URL | Paramters | Function
------------ | ------------- | -------------
/beep |  | short beep on the piezo buzzer
/melody |  | play melody on the piezo buzzer
/usb |  | toggle USB-A power
/usbon |  | turn USB-A power on
/usboff |  | turn USB-A power off

### Configurations and Modes
URL | Paramters | Function
------------ | ------------- | -------------
/rfmode | mode=[0,1,2,3] | set the behaviour of the lamp when Channel 3 of the RF433 receiver goes low </br> 0 = toggle USB-A  ( default ), 1 = Sequencer Start/Stop , 2 = Lamp on/off , 3 = Play Melody
/mqtt | mode=[1] | turn the MQTT mode on or off
/ap | mode=[u] | turn Access Point ( AP) mode on or off
/status |  | return state of device in JSON format
/reset | | reset Lamp state
/wfifwipe | | wipe wifi credetials and reset device

## MQTT functions

If there is an MQTT server name configured then the system will try to establish a connection every 10 seconds.

Once connected it will listen on the topic "orbs/deviceCommand" for commands ( TODO: add hostname to topic )
The device will also post JSON status messages to the topic "orbs/deviceStatus" on every HTTP request 

### MQTT commands

command | function
------------ | ------------- 
3 | toggle RF433 SW3 function 
a | toggle sequencer start/stop
s | fixed 
f | flash
u | rainbow sequence
r | red 
R | red 
g | green 
G | green  
b | blue 
B | blue 
w | white 
W | white 

## Documentation TODOs:
- [x] Post source code
- [x] Bill of Materials ( on the wiki )
- [ ] circuit diagram , contruction notes and photographs
- [ ] videos of an operating lamp
