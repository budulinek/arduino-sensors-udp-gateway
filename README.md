# Arduino-multiple-sensors-UDP 
Arduino sketch which reads multiple types of sensors and sends data via UDP packets. Based on https://github.com/PavSedl/RAILDUINO-2.0 

## Multiple sensors,  multiple Onewire busses
Reads any combination of these sensor types:
* Onewire Dallas temperature sensors
* DHT temperature and humidity sensors
* BH1750FVI light sensors

For detailed list of sensors see config. You can put multiple onewire sensors on a single bus. You can have multiple onewire buses, DHT sensors or light sensors. All you need is to specidy pins in the config file. For more details on connections, see config file.

## Fast, non-blocking
Conversion request is sent to all buses simultaneously. No artificial delays while waiting for conversion, all sensors are read in async mode. 

## UDP
See config for details on the UDP syntax for each sensor type.
