/* Use this file  to configure your sensors.
 - Connect your sensor(s) according to schema. Pin numbers in schematics correspond to Arduino Uno / Nano pinouts.
 - Enable sensor type by uncommenting the "USE_..." definition.
 - Specify pin numbers you use for connecting data wires (address wires) from your sensors. If you run out of digital pins, use analog pins A0 to A5 instead (simply use digits 14 to 19).
 - Configure read cycle, hysteresis, resolution (if available) 
*/

#define DEBUG                                          // Print debug to serial

/*  NETWORK SETTINGS

  Shields: W5500, W5200, W5100
  Pins 10, 11, 12, 13 are reserved for the SPI comunication with the ethernet shield.
      
*/
unsigned int listenPort = 10032;                // local listening port
unsigned int sendPort = 10000;                  // local sending port
unsigned int remPort = 10032;                   // remote port
IPAddress ip(192, 168, 1, 32);                  // local IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(192, 168, 1, 10);       // remote IP address
#define ETH_RESET_PIN 7                         // OPTIONAL: Ethernet shield reset pin (deals with power on reset issue of the ethernet shield)
byte boardIdentifier = 2;                       // number identifying the board in UDP output

/* 1-WIRE SETTINGS
  Sensors: DS1820, DS18S20, DS18B20, DS1822, DS1825, DS28EA00
           DS2438

  SENSOR      <->      ARDUINO
  Vcc         <->      5V
  GND         <->      GND
  Data        <->      oneWirePins (one pin for each 1-wire bus, no hard limit on the number of sensors per bus)
                       insert 1kΩ resistor + 5kΩ potentiometer between Vcc and DATA and adjust potentiometer until all sensors are detected

  UDP output examples:
     Sensor detected on bus 2:       ardu1 1w2 2864fc3008082 detected
     Unknown sensor type on bus 3:   ardu1 1w3 0160308084fc2 unknown
     DS18B20 sensor reading:         ardu1 1w2 2864fc3008082 temp 25.4
     DS2438 sensor reading:          ardu1 1w2 2612c3102004f 25.4 1.23 0.12
     Sensor error:                   ardu1 1w2 2864fc3008082 error
*/
// #define USE_ONEWIRE                          // Uncomment to enable 1-wire sensors
byte oneWirePins[] = {2, 3, 4};                 // 1-wire buses. One pin for each 1-wire bus, no hard limit on the number of sensors per bus.
#define ONEWIRE_CYCLE 30000                     // 1-wire sensors read cycle in ms
#define ONEWIRE_MAX_SENSORS 30                  // Maximum number of 1-wire sensors for all buses (limit 255)
#define ONEWIRE_MAX_RETRY 5                     // Number of attempts to read sensor data (within one read cycle). If reached, error message is sent. 
#define ONEWIRE_MAX_ERRORS 5                    // Max number of errors per sensor. If reached, sensor address is deleted from memory.
#define ONEWIRE_RESOLUTION 12                   // Resolution in bits, available options are: 9, 10, 11, or 12 bits,
                                                // which corresponds to increments of 0.5°C, 0.25°C, 0.125°C, and 0.0625°C.
                                                // Decimal points in UDP output are adjusted automatically to 0, 0, 1 and 1 respectively.
#define ONEWIRE_HYSTERESIS 0                    // Hysteresis in °C (send value only if difference between new and old value is larger or equal to hysteresis)

/* DHT SETTINGS
  Sensors: DHT11, DHT12, DHT22 (AM2302), DHT33, DHT44

  SENSOR      <->      ARDUINO
  Vcc         <->      5V
  Data        <->      dhtPins (one pin for each sensor)
  GND         <->      GND

  UDP output examples:
     Sensor 3 reading(temp°C humid%):  ardu1 dht3 temp 25.3 humid 42.0
     Sensor 3 error:                   ardu1 dht3 error
*/
// #define USE_DHT                              // Uncomment to enable DHT sensors
byte dhtPins[] = {14, 15, 16};                  // DHT data pins. One pin for each sensor.
#define DHT_CYCLE 20000                         // DHT sensors read cycle in ms
#define DHT_MAX_RETRY 5                         // Number of attempts to read sensor data before error is sent.
#define DHT_TEMP_HYSTERESIS 0.05                // Hysteresis in °C (send value only if difference between new and old value is larger or equal to hysteresis)
#define DHT_HUMID_HYSTERESIS 0.5                // Hysteresis in % (send value only if difference between new and old value is larger or equal to hysteresis)

/* LIGHT SETTINGS
  SENSOR: BH1750FVI

  SENSOR      <->      ARDUINO
  Vcc         <->      5V
  GND         <->      GND
  SCL         <->      A5/SCL
  SDA         <->      A4/SDA
  ADDR        <->      lightPins (one pin for each sensor)

  UDP output examples:
     Light sensor 1 reading (lux):   ardu1 light1 123.5
     Light sensor 2 error:           ardu1 light2 error
*/
// #define USE_LIGHT                            // Uncomment to enable light sensors
byte lightPins[] = {17, 9};                     // I2C Address pins. One pin for each sensor.
#define LIGHT_CYCLE 1000                        // Light sensors read cycle in ms
#define LIGHT_MAX_RETRY 5                       // Number of attempts to read sensor data before error is sent.
static const float lightResolution = 0.5;       // Resolution in lux, available options: 4, 1 or 0.5 (default is 1 lux)
                                                // Decimal points in UDP output are adjusted automatically.
#define LIGHT_HYSTERESIS 0.5                    // Hysteresis in lux (send  value only if difference between new and old value is larger or equal to hysteresis)

/* RTD SETTINGS
  Sensors: Pt100, Pt1000 (using MAX31865 module)

  MAX31865    <->      ARDUINO
  Vcc         <->      5V
  GND         <->      GND
  SDI         <->      D11 (MOSI)
  SDO         <->      D12 (MISO)
  CLK         <->      D13 (SCK)
  CS          <->      rtdPins (one pin for each MAX31865 module)

  UDP output examples:
     Sensor 3 reading(temp°C):  ardu1 rtd3 temp 25.3
     Sensor 3 error:            ardu1 rtd3 error
*/
// #define USE_RTD              // Uncomment to enable RTD sensors
byte rtdPins[] = {9};           // CS pins, one digital pin for each MAX31865 module
#define RTD_TYPE 1000           // Set 100 for Pt100 sensor, 1000 for Pt1000 (default is 100)
                                // You MUST use corresponding reference resistor (Rref) on your MAX31865 module:
                                //       - For the Pt100 sensor, use 430 ohm 0.1% resistor (marking is 4300)
                                //       - For the Pt1000 sensor, use 4300 ohm 0.1% resistor (marking is 4301)
                                // MAX31865 modules are usually shipped with 430 ohm Rref (intended for Pt100 sensors). If you want to use Pt1000 sensor, replace the Rref resistor manually.
#define RTD_WIRES 3             // Number of wires you use for connecting the RTD sensor to the MAX31865 module (2, 3 or 4). 
                                // For wiring the RTD sensor see https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/rtd-wiring-config 
#define RTD_CYCLE 2000          // RTD sensors read cycle in ms
#define RTD_MAX_RETRY 5         // Number of attempts to read sensor data before error is sent.
#define RTD_HYSTERESIS 0        // Hysteresis in °C (send value only if difference between new and old value is larger or equal to hysteresis). 
                                // Decimal points adjusted automatically
