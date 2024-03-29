/* Arduino-based sensor gateway

  Version history
  v0.9 2019-05-26 Initial commit, multiple 1-wire buses
  v1.0 2020-05-19 Add support for DHT sensors and light sensors
  v1.1 2020-11-02 Config file
  v1.2 2020-11-18 Better DHT sensors management
  v2.0 2021-02-22 Support for Pt100, Pt1000 RTD sensors, 
  v3.0 2024-02-06 Substantial rewrite of the code, add web interface, settings stored in EEPROM.

*/

const byte VERSION[] = { 3, 0 };

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <utility/w5100.h>
#include <EEPROM.h>
#include <StreamLib.h>  // StreamLib https://github.com/jandrassy/StreamLib

#include "OneWireNg_CurrentPlatform.h"  // OneWireNg https://github.com/pstolarz/OneWireNg
#include "drivers/DSTherm.h"
#include "utils/Placeholder.h"  // also used by other sensors (not just OneWire) in order to prevent class object heap allocation

#include "advanced_settings.h"

#ifdef ENABLE_DS18X20
#define SHOW_COLUMN_PIN
#define SHOW_COLUMN_ID
#endif /* ENABLE_DS18X20 */

#include <Wire.h>
#ifdef ENABLE_BH1750
#define SHOW_COLUMN_PIN
#define SHOW_COLUMN_I2C
#endif /* ENABLE_BH1750 */

#ifdef ENABLE_MAX31865
#include <MAX31865_NonBlocking.h>
#define SHOW_COLUMN_PIN
#endif /* ENABLE_MAX31865 */

// these are used by CreateTrulyRandomSeed() function
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>

enum sensor_t : byte {
  SENSOR_NONE,   // reserved for null
  SENSOR_1WIRE,  // 1-Wire (DS18x20)
  SENSOR_LIGHT,  // Light (BH1750FVI)
  SENSOR_RTD,    // RTD temperature (MAX31865)
  SENSOR_DHT,    // TODO, reserved for DHT22
  SENSOR_LAST,   // Must be the very last element in this array
};

typedef struct {
  byte ip[4];
  byte subnet[4];
  byte gateway[4];
  byte dns[4];      // only used if ENABLE_DHCP
  bool enableDhcp;  // only used if ENABLE_DHCP
  byte remoteIp[4];
  bool udpBroadcast;
  uint16_t udpPort;
  uint16_t webPort;
  uint16_t intervalMin[SENSOR_LAST];  // minimum report interval in seconds (different for each sensor type)
  uint16_t intervalMax[SENSOR_LAST];  // maximum report interval in seconds (different for each sensor type)
  byte change[SENSOR_LAST];           // reportable change
  byte rtdWires;
  uint16_t rtdNominal;
  uint16_t rtdReference;
} config_t;

const config_t DEFAULT_CONFIG = {
  DEFAULT_STATIC_IP,
  DEFAULT_SUBMASK,
  DEFAULT_GATEWAY,
  DEFAULT_DNS,
  DEFAULT_AUTO_IP,
  DEFAULT_REMOTE_IP,
  DEFAULT_BROADCAST,
  DEFAULT_UDP_PORT,
  DEFAULT_WEB_PORT,
  { PIN_SEARCH_INTERVAL,
    DEFAULT_OW_MIN,
    DEFAULT_LIGHT_INT_MIN,
    DEFAULT_RTD_INT_MIN },
  { PIN_SEARCH_INTERVAL,
    DEFAULT_OW_MAX,
    DEFAULT_LIGHT_INT_MAX,
    DEFAULT_RTD_INT_MAX },
  { 0,
    DEFAULT_OW_CHANGE,
    DEFAULT_LIGHT_CHANGE,
    DEFAULT_RTD_CHANGE },
  DEFAULT_RTD_WIRES,
  DEFAULT_RTD_NOMINAL,
  DEFAULT_RTD_REFERENCE,
};

// pins and IDs of all detected 1-wire sensors
typedef struct {
  byte pins[OW_MAX_SENSORS];
  byte ids[OW_MAX_SENSORS][8];
} ow_t;

typedef struct {
  uint32_t eepromWrites;             // Number of Arduino EEPROM write cycles
  byte major;                        // major version
  byte mac[6];                       // MAC Address (initial value is random generated)
  config_t config;                   // configuration variables modified via Web UI
  byte pinSensor[NUM_DIGITAL_PINS];  // type of sensor detected on each pin
  ow_t ow;                           // pins and IDs of all detected 1-wire sensors (TODO: only if 1-wire enabled?)
} data_t;

data_t data;

/****** ETHERNET ******/

byte maxSockNum = MAX_SOCK_NUM;

#ifdef ENABLE_DHCP
bool dhcpSuccess = false;
#endif /* ENABLE_DHCP */

EthernetUDP Udp;
EthernetServer webServer(DEFAULT_CONFIG.webPort);

/****** TIMERS AND STATE MACHINE ******/

class Timer {
private:
  uint32_t timestampLastHitMs;
  uint32_t sleepTimeMs;
public:
  boolean isOver();
  void sleep(uint32_t sleepTimeMs);
};
boolean Timer::isOver() {
  if ((uint32_t)(millis() - timestampLastHitMs) > sleepTimeMs) {
    return true;
  }
  return false;
}
void Timer::sleep(uint32_t sleepTimeMs) {
  this->sleepTimeMs = sleepTimeMs;
  timestampLastHitMs = millis();
}

Timer eepromTimer;  // timer to delay writing statistics to EEPROM


/****** RUN TIME AND DATA COUNTERS ******/

volatile uint32_t seed1;  // seed1 is generated by CreateTrulyRandomSeed()
volatile int8_t nrot;
uint32_t seed2 = 17111989;  // seed2 is static

/****** SETUP: RUNS ONCE ******/
bool setupRun = true;
void setup() {

  CreateTrulyRandomSeed();
  EEPROM.get(DATA_START, data);
  // is configuration already stored in EEPROM?
  if (data.major != VERSION[0]) {
    data.major = VERSION[0];
    // load default configuration from flash memory
    data.config = DEFAULT_CONFIG;
    generateMac();
    resetSensors();
    updateEeprom();
  }
  startEthernet();
#ifdef ENABLE_BH1750
  Wire.begin();
  Wire.setWireTimeout(3000, true);  // I2C timeout to prevent lockups, latest Wire.h needed for this function.
#endif                              /* ENABLE_BH1750 */

  // initial detection of sensors (more thorough)
  for (byte i = 0; i < NUM_DIGITAL_PINS; i++) {
    if (isReserved(i)) continue;  // ignore reserved pins
    searchPin(i);
    // delay(5);
  }
  setupRun = false;
}

/****** LOOP ******/

void loop() {

  // recvUdp();

  for (byte i = 0; i < NUM_DIGITAL_PINS; i++) {
    readPin(i);
  }

  manageSockets();

#ifdef ENABLE_EXTENDED_WEBUI
  if (EEPROM_INTERVAL > 0 && eepromTimer.isOver() == true) {
    updateEeprom();
  }
#endif /* ENABLE_EXTENDED_WEBUI */
#ifdef ENABLE_DHCP
  maintainDhcp();
#endif /* ENABLE_DHCP */
}
