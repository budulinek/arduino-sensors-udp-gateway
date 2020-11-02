// SETTINGS
//

#define DEBUG                                          // Print debug to serial

/*  NETWORK SETTINGS

  Shields: W5500, W5200, W5200
  Pins reserved for ethernet shield (on standard Uno or Nano):
      10, 11, 12, 13
*/
unsigned int listenPort = 10032;                       // local listening port
unsigned int sendPort = 10000;                         // local sending port
unsigned int remPort = 10032;                          // remote port
IPAddress ip(192, 168, 1, 32);                          // local IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(192, 168, 1, 10);               // remote IP, you can also use 255, 255, 255, 255 for broadcast (faster)
#define ETH_RESET_PIN 7                                // OPTIONAL: Ethernet shield reset pin (deals with power on reset issue of the ethernet shield)
byte boardIdentifier = 2;                       // number identifying the board in UDP output

/* 1-WIRE SETTINGS
  Sensors: DS1820, DS18S20, DS18B20, DS1822, DS1825, DS28EA00
           DS2438

  SENSOR      <->      ARDUINO
  Vcc         <->      5V
  Data (bus)  <->      oneWireBusPins (digital pins can be used)
                       insert 1kΩ resistor + 5kΩ potentiometer between Vcc and DATA and adjust potentiometer untill all sensors are detected
  GND         <->      GND

  UDP output examples:
     Sensor detected on bus 2:       ardu1 1w2 2864fc3008082 detected
     Unknown sensor type on bus 3:   ardu1 1w3 0160308084fc2 unknown
     DS18B20 sensor reading:         ardu1 1w2 2864fc3008082 temp 25.44
     DS2438 sensor reading:          ardu1 1w2 2612c3102004f 25.44 1.23 0.12
     Sensor error:                   ardu1 1w2 2864fc3008082 error
*/
#define USE_ONEWIRE                                    // Comment out to disable 1-wire sensors completely
byte oneWireBusPins[] = {2, 3, 4};                     // 1-wire buses. Digital pins can be used.
#define ONEWIRE_CYCLE 30000                            // 1-wire sensors read cycle
#define ONEWIRE_MAX_SENSORS 30                         // max 1-wire sensors (limit 255)
#define ONEWIRE_MAX_ERRORS 10                          // max errors per sensor, if reached, sensor address deleted from memory
#define ONEWIRE_RESOLUTION 12                          // DS18B20 resolution (9, 10, 11, or 12 bits)
#define ONEWIRE_HYSTERESIS 0                           // Hysteresis in °C (send value only if difference between new and old value is larger or equal to hysteresis)

/* DHT SETTINGS
  Sensors: DHT22 (AM2302), DHT21 (AM2301), DHT11

  SENSOR      <->      ARDUINO
  Vcc         <->      5V
              <->      OPTIONAL: DHT_POWER_PIN (directly or through transistor switch)
  Data        <->      dhtSensorPins (digital or analog pins can be used)
  GND         <->      GND

  UDP output examples:
     Sensor 3 reading(temp°C humid%):  ardu1 dht3 temp 25.3 humid 42.0
     Sensor error:                     ardu1 dht3 error
*/
#define USE_DHT                                        // Comment out to disable DHT sensors completely
byte dhtSensorPins[] = {14, 15, 16};                   // Digital or analog pins can be used. Analog A0 == pin 14
#define DHT_POWER_PIN 8                                // Optional in order to improve reliability (sensors are depowered between readings in case of reading error)
#define DHT_CYCLE 25000                                // DHT sensors read cycle
#define DHT_TYPE 22                                    // type of DHT sensors (11, 21, or 22)
#define DHT_TEMP_HYSTERESIS 0.05                           // Hysteresis in °C (send value only if difference between new and old value is larger or equal to hysteresis)
#define DHT_HUMID_HYSTERESIS 0.5                           // Hysteresis in % (send value only if difference between new and old value is larger or equal to hysteresis)

/* LIGHT SETTINGS
  SENSOR: BH1750FVI

  SENSOR      <->      ARDUINO
  (it is OK to use I2C bus on longer distances,
  because we use Wire.setWireTimeout to prevent lockups :-))
  Vcc         <->      5V
  GND         <->      GND
  SCL         <->      A5/SCL
  SDA         <->      A4/SDA
  ADDR        <->      lightSensorPins
  UDP output examples:
     Light sensor 1 reading (lux):   ardu1 light1 123.5
     Light sensor 2 error:           ardu1 light2 error
*/
#define USE_LIGHT                                      // Comment out to disable light sensors completely
byte lightSensorPins[] = {17, 9};                      // Address pins 
#define LIGHT_CYCLE 1000                               // light sensors read cycle
#define LIGHT_HYSTERESIS 0.5                           // Hysteresis in lux (send  value only if difference between new and old value is larger or equal to hysteresis)
