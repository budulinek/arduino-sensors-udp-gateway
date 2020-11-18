# Arduino-multiple-sensors-UDP 
Arduino sketch which reads multiple types of sensors and sends data via UDP packets. Based on https://github.com/PavSedl/RAILDUINO-2.0 

## UDP
See config for details on the UDP syntax for each sensor type.

## Multiple sensors,  multiple Onewire buses
Reads any combination of the following sensors:
* Onewire Dallas temperature sensors
* DHT temperature and humidity sensors
* BH1750FVI light sensors

You can put multiple onewire sensors on a single bus. You can have multiple onewire buses, DHT sensors or light sensors. For detailed list of sensors see config. 

## Easy to use
All you need is to specify which Arduino pins you use for connecting your sensors. For more details on connections, see config file.

## Fast, non-blocking
Conversion request is sent to all buses simultaneously. No artificial delays while waiting for conversion, all sensors are read in async mode. 

## Reliable, fail-safe
In case of error, new reading is attempted. Fail-safe I2C bus - feel free to use the I2C bus for long distances (10m successfully tested)!

