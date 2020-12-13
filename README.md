# Remote Touch
Connected copper plates with touch sensing and temperature control
>Made for and tested on:  
>- PSoC5 LP (CY8CKIT-059)  
>- NodeMCU 1.0 (ESP-12E Module)

## Description
Remote Touch is an art piece that connects people across spaces.  
It consists of two frames, each containing a copper plate, that
are connected together through the internet.

When someone touches the copper of one frame, the copper of the
other frame heats up. When they stop touching the copper, the
other frame cools down.

This behaviour allows one person to sense whether or not the other
is touching the copper plate at the same time (and when they stop),
no matter the distance between them.

<img src="remote_touch_frames.png" height="400" />

## Touch and Temperature Control (PSoC)
The PSoC handles touch sensing and temperature control of the copper.
* Touch sensing is done capacitively.
* Heating is done using polyimide heaters.
* Cooling is done using peltiers.

## Communication (NodeMCU)
The NodeMCU handles connecting to WiFi and transmitting/receiving the
touch states.
* WiFi is done using WiFiManager.
* Communication with the other frame is done using a [Blynk](https://blynk.io/) bridge.

The PSoC and NodeMCU communicate with each other via two GPIOs.