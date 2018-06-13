# Orb

Orb is a USB powered LED lamp with an embedded microcontroller that allows it to be controlled remotely over wifi using HTTP and MQTT. The controller is a Wemos D1 mini that utilises an ESP 8266. 

The USB-A socket can be used to switch USB powered devices on and off such as lamps and fans. It also has a piezo-electric buzzer. Additionally it has an RF 433Mhz reciever that can be dynamically configured to control the devices functions using many EV1527 4CH transmitters.


Here's the a picture of the first Orb. 

<img src="https://user-images.githubusercontent.com/2019989/41097744-677b9d02-6a9c-11e8-8952-004872332f09.jpg" width=400>


Below is a screen shot taken from an iPhone showing the Web page served up from the embedded HTML server.

It's designed to look like an old style TV remote control.

<img src="https://user-images.githubusercontent.com/2019989/41035960-b4d279c0-69d1-11e8-82b0-8630fc84b622.jpg" width=200>

I'll post the mircocontroller C++ code once I've cleaned it up to remove the obsolete bits.
- [ ] Post source code
- [ ] Bill of Materials
- [ ] circuit diagram , contruction notes and photographs
- [ ] videos of an operating lamp

## HTTP endpoints

### Lamp Colour
URL | Paramters | Function
------------ | ------------- | -------------

/colour| value=[r,R,g,G,b,B,w,W,z,Z] | r=red, g=green, b=blue , w=white , z=black ( lc = half, uc = full)

### Animaton Patterns
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


### Animaton Speed
URL | Paramters | Function
------------ | ------------- | -------------
/start |  | start animation cycle
/stop |  | stop animation cycle
/speed | value= | set the animation cycle step time in Milliseconds
/slow |  | animation step size = 200ms
/normal |  | animation step size = 100ms
/fast |  | animation step size = 50ms
/slower |  | slow down animation
/faster |  | speed up animaton

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
/rfmode | mode=[0,1,2,3] | set the behaviour of the lamp when Channel 3 of the RF433 receiver goes low </br> 0 = toggle USB-A  ( default ), 1 = Animation Start/Stop , 2 = Lamp on/off , 3 = Play Melody
/mqtt | mode=[1] | turn the MQTT mode on or off
/ap | mode=[u] | turn Access Point ( AP) mode on or off
/status |  | return state of device in JSON format
/reset | | reset Lamp state
/wfifwipe | | wipe wifi credetials and reset device
