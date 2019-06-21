# OneWire-Multibus-UDP
Arduino sketch to read DS18B20 and DS2438 sensors on multiple 1-wire buses. Fast, non-blocking. Data are send via UDP packets. Based on https://github.com/PavSedl/RAILDUINO-2.0 

## Sensors
Reads DS18B20 sensors (temperature) and DS2438 sensors (temp, Vad, Vcurr). DS2438 not tested yet. Limit is set to 10 sensors per bus.

## Multibus
Supports multiple buses on digital pins. Pins for buses can be configured in the sketch. Number of buses is only limited by available memory. Arduino Nano should work with up to 5 buses with 10 sensors per bus.
Each bus needs pull-up resistor between data and Vcc. Start with 4k7 resistor and continue with lower values until you detect sensors.

## Fast
Conversion request is sent to all buses simultaneously. No artificial delays while waiting for conversion, temperature is sent immediatelly after conversion is finished by all sensors on the bus.

## Non-blocking
DS18B20 sensors are read in async mode. Command requestTemperatures() does not wait for conversion. Arduino can do other stuff while the conversion is in progress.

## Depower between readings (optional)
Depower 1-wire buses between readings to improve reliability and save some power. In my case, switching the power off/on improved reliability - sensors with error are reconnected.
Use digital pin to supply your 1-wire buses. Remember, 1 sensor consumes up to 1,5 mA, Arduino has a limit 40 mA per pin. In order to bypass the current limit, you can try transistor-switch (see the circuit.png).



## UDP
Configure  local IP and remote port in the sketch. By default, UDP packets are send as multicast. Set the boardAddress to distinguish between multiple Arduinos.
UDP syntax:
* DS18B20 1wire sensor packet: `ardu<boardAddress> 1w<OneWireBus> <sensorAddress> temp <tempCelsius>` (example: ardu2 1w1 28ff641dcdc5dd41 temp 25.63)
* DS2438 1wire sensor packet: `ardu<boardAddress> 1w<OneWireBus> <sensorAddress> <tempCelsius> <Vad> <Vcurr>` (example: ardu2 1w2 2612c3102004f 25.44 1.23 0.12)
* Arduino (re)start: `ardu<boardAddress> rst`
* Sensor detected (after start): `ardu<boardAddress> 1w<OneWireBus> <sensorAddress> detected`
* Sensor error: `ardu<boardAddress> 1w<OneWireBus> <sensorAddress> error`
