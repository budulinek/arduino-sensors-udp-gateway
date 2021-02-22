# Arduino-multiple-sensors-UDP 
Arduino sketch which reads multiple types of sensors and sends data via UDP messages. Inspired by https://github.com/PavSedl/RAILDUINO-2.0 

## What is it good for?
Arduino serves as a gateway to integrate various sensors into superordinate systems via UDP messages. The sketch is primarily intended for integrating temperature, humidity and light sensors into Loxone home automation system. Loxone can easily parse UDP messages (use "Virtual UDP Input").

The program is very flexible, it reads any combination of supported sensors. You can easily add new sensors in the future, without editing the main program. You are only limited by the number of pins available on your Arduino.

Because the program employs automatic repeat request as an effective error-control method, it is suitable for directly wiring sensors over longer distances (such as sensors in a house) in slightly noisy environment (interference from mains power lines).

## How to use it?
Pick your favourite sensors. At the moment the following sensors are supported:

* DS1820, DS18S20, DS18B20, DS1822, DS1825, DS28EA00 and DS2438 temperature sensors (via 1-wire bus)
* DHT11, DHT12, DHT22 (AM2302), DHT33, DHT44 temperature and humidity sensors (via data wire)
* BH1750FVI light sensors (via I2C bus)
* Pt100, Pt1000  resistance temperature sensors (using MAX31865 module, via SPI bus)

You can put multiple 1-wire sensors on a single bus. You can have multiple 1-wire buses, DHT, RTD or light sensors. 

Now go to the config.h file where you will find wiring schema and configuration for each sensor type. You only need to:

- Connect your sensor(s) according to the provided schema.
 - Enable sensor type by uncommenting the "USE_..." definition.
 - Specify pin numbers you use for connecting data pins (address pins) of your sensors.
 - Configure read cycle, hysteresis, resolution (if available).

Configure your network settings, upload your sketch to Arduino and watch your sensor data coming in via UDP messages (check the config.h file for UDP output syntax and examples).

If you decide to add new sensor later on, you only need to edit the config.h file and (re)upload the sketch to your Arduino. Do you need additional DHT22 sensor? Connect the sensor's data pin to Arduino, add new pin number to the "dhtPins" array in config.h and upload your program to Arduino. No need to mess with the main program! You are only limited by the amount of flash memory (each sensor type requires additional flash), amount of available SRAM memory (we use it for storing 1-wire sensors' addresses and past sensor readings) and the number of available pins (remember you can also use analog pins as digital ones).

You do not need to worry about delays when reading large number of sensors at the same time. The program is fast and non-blocking.

## Wait... What about communication errors?
What if I have twelve 1-wire temperature sensors, sitting on a single bus in star topology (not recommended), running along mains power line (causing interference)? What if I use the I2C bus for connecting three light sensors over 10 m long wires (I2C bus was not designed to go over such distance)?

Yes, random data transmission errors will occur. But do not worry, your Arduino will handle them! The program employs [automatic repeat request](https://en.wikipedia.org/wiki/Automatic_repeat_request) as an effective error-control method. As a result, we can achieve reliable data transmission (sensor data reading) over an unreliable communication channel (such as 1-wire or I2C over long distances).

Depending on sensor type and wire length, you may also need pull-up resistors (between Vcc and data line) in order to establish communication between Arduino and your sensors.