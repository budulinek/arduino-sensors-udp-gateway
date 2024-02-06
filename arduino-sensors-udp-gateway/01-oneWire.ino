
#if (CONFIG_MAX_SEARCH_FILTERS > 0)
static_assert(CONFIG_MAX_SEARCH_FILTERS >= DSTherm::SUPPORTED_SLAVES_NUM,
              "CONFIG_MAX_SEARCH_FILTERS too small");
#endif

#define PARASITE_POWER_ARG false  // parasite power is not supported by this sketch

typedef struct {
  Timer timerMin;
  Timer timerMax;
  byte state;
  byte retryCnt;  // number of attempts to read
  int32_t oldVal;
} pin_t;

pin_t pins[NUM_DIGITAL_PINS];

// flags for pin.state
#define FORCE_REPORT B10000000  // Force report when max interval is over
#define STATE_MASK B00000111

// flags for owSensors.retryCnt and pins.retryCnt
#define RESULT_SUCCESS B01000000
#define RESULT_SUCCESS_RETRY B00100000

#ifdef ENABLE_DS18X20
Placeholder<OneWireNg_CurrentPlatform> ow;
typedef struct {
  int32_t oldVal;
  byte retryCnt;  // number of attempts to read
} owSensor_t;
owSensor_t owSensors[OW_MAX_SENSORS];
#endif /* ENABLE_DS18X20 */

const byte BH1750_ADDRESS_H = 0x5C;  // Commands sent via Wire (I2C) will be processed by a sensor with address pin set to HIGH
#ifdef ENABLE_BH1750
const byte BH1750_POWER_UP = 0x01;
enum resolution : byte {
  BH1750_RES_HIGH = 0x10,
  BH1750_RES_HIGH2 = 0x11,
  BH1750_RES_LOW = 0x13,
};
#endif /* ENABLE_BH1750 */

#ifdef ENABLE_MAX31865
Placeholder<MAX31865> rtd;
#endif /* ENABLE_MAX31865 */

void searchPin(const byte pin) {

  // detect 1-wire buses
#ifdef ENABLE_DS18X20
  searchPinOw(pin);
#endif /* ENABLE_DS18X20 */

// detect MAX31865 sensors
#ifdef ENABLE_MAX31865
  setAllPins(OUTPUT, HIGH);
  // if (setupRun) delay(10);
  new (&rtd) MAX31865(pin);
  rtd->begin(MAX31865::RTD_2WIRE, MAX31865::FILTER_50HZ);
  if (rtd->getWires() == MAX31865::RTD_2WIRE && rtd->getFilter() == MAX31865::FILTER_50HZ) {  // new sensor found
    data.pinSensor[pin] = SENSOR_RTD;
    updateEeprom();
    // return;
  }
#endif /* ENABLE_MAX31865 */

  // detect light sensors
#ifdef ENABLE_BH1750
  setAllPins(OUTPUT, LOW);
  digitalWrite(pin, HIGH);  // Address to high
  // if (setupRun) delay(10);
  byte error = 1;
  // I2CWrite(BH1750_POWER_UP);  // * "Power On" Command is possible to omit.
  error = I2CWrite(BH1750_RESOLUTION);
  digitalWrite(pin, LOW);
  if (error == 0) {  // new sensor found
    data.pinSensor[pin] = SENSOR_LIGHT;
    updateEeprom();
  }
#endif /* ENABLE_BH1750 */
  setAllPins(INPUT, LOW);
}

void setAllPins(const byte mode, const byte val) {
  for (byte i = 0; i < NUM_DIGITAL_PINS; i++) {
    if (isReserved(i)) continue;                                          // ignore reserved pins
    if (setupRun == false && data.pinSensor[i] != SENSOR_NONE) continue;  // ignore used pins
    pinMode(i, mode);
    digitalWrite(i, val);
  }
}

#define SENSORS_MEM_FULL -1   // oneWireSensors[] memory full
#define SENSORS_MEM_FOUND -2  // sensor already stored in oneWireSensors[] memory
#ifdef ENABLE_DS18X20
bool searchPinOw(const byte pin) {
  new (&ow) OneWireNg_CurrentPlatform(pin, false);
  DSTherm drv(ow);
  ow->searchReset();
  for (const auto& newId : *ow) {
    // check if sensor type is supported by libraries we use
    if (DSTherm::getFamilyName(newId) == NULL) continue;  // not supported
    int16_t sensorNdx = SENSORS_MEM_FULL;
    for (byte j = OW_MAX_SENSORS; j--;) {  // loop all slots in memory
      if (data.ow.ids[j][0] == 0) {
        sensorNdx = j;  // empty slot found, record its index
        continue;       //
      }
      bool match = true;
      for (byte k = 0; k < 8; k++) {
        if (newId[k] != data.ow.ids[j][k]) {  // slot with different ID
          match = false;
          break;
        }
      }
      if (data.ow.pins[j] != pin) match = false;
      if (match) {
        sensorNdx = SENSORS_MEM_FOUND;  // slot with same ID
        break;
      }
    }
    if (sensorNdx == SENSORS_MEM_FULL) {
      // TODO: error sensor not found and memory full
    } else if (sensorNdx >= 0) {                        // empty slot found
                                                        // TODO message new sensor
      drv.writeScratchpad(newId, 0, 0, OW_RESOLUTION);  // Th, Tl (high/low alarm triggers) are set to 0, set resolution
      drv.copyScratchpad(newId, PARASITE_POWER_ARG);
      for (byte l = 0; l < 8; l++) {
        data.ow.ids[sensorNdx][l] = newId[l];
      }
      data.ow.pins[sensorNdx] = pin;
      data.pinSensor[pin] = SENSOR_1WIRE;
      updateEeprom();
    }
  }
}
#endif /* ENABLE_DS18X20 */

void readPin(const byte pin) {
  if (pins[pin].timerMax.isOver()) {
    pins[pin].state |= FORCE_REPORT;
    pins[pin].timerMax.sleep((data.config.intervalMax[data.pinSensor[pin]] - data.config.intervalMin[data.pinSensor[pin]]) * 1000UL);
  }
  if (pins[pin].timerMin.isOver() == false || isReserved(pin)) {
    return;
  }
  switch (data.pinSensor[pin]) {
    case SENSOR_NONE:
      {
        sleepPin(pin);
        searchPin(pin);
      }
      break;
#ifdef ENABLE_DS18X20
    case SENSOR_1WIRE:
      {
        readOneWire(pin);
      }
      break;
#endif /* ENABLE_DS18X20 */
#ifdef ENABLE_BH1750
    case SENSOR_LIGHT:
      {
        readLight(pin);
      }
      break;
#endif /* ENABLE_BH1750 */
#ifdef ENABLE_MAX31865
    case SENSOR_RTD:
      {
        // readRtd(pin);
        readLight(pin);
      }
      break;
#endif /* ENABLE_MAX31865 */
    default:
      break;
  }
}

#ifdef ENABLE_DS18X20
void readOneWire(const byte pin) {
  switch (pins[pin].state & STATE_MASK) {
    case 0:  // Initialize conversion for all dallas temp sensors on a bus
      {
        searchPinOw(pin);
        new (&ow) OneWireNg_CurrentPlatform(pin, false);
        DSTherm drv(ow);
        drv.convertTempAll(0, PARASITE_POWER_ARG);
        for (byte j = 0; j < OW_MAX_SENSORS; j++) {
          if (data.ow.pins[j] == pin) {
            // clears RESULT_SUCCESS and RESULT_SUCCESS_RETRY flags
            if (owSensors[j].retryCnt == MAX_ATTEMPTS) {
              owSensors[j].retryCnt = MAX_ATTEMPTS - 1;  // allow only one attempt for disconnected sensors
            } else {
              owSensors[j].retryCnt = 0;
            }
          }
        }
        pins[pin].timerMin.sleep(drv.getConversionTime(OW_RESOLUTION));
        pins[pin].state++;
      }
      break;
    case 1:
      {
        new (&ow) OneWireNg_CurrentPlatform(pin, false);
        DSTherm drv(ow);
        if (ow->readBit() == 1 || pins[pin].timerMin.isOver()) {
          // conversion on the bus finished, read sensors one by one
          Placeholder<DSTherm::Scratchpad> scrpd;
          bool finished = true;
          for (byte j = 0; j < OW_MAX_SENSORS; j++) {
            if (data.ow.pins[j] != pin                       // skip empty slots and sensors from other buses
                || (owSensors[j].retryCnt >= MAX_ATTEMPTS))  // skip sensors with error and successful sensors
              continue;
            owSensors[j].retryCnt++;
            byte result = drv.readScratchpad(data.ow.ids[j], scrpd);
            bool empty = true;  // check if scratchpad is empty
            int32_t val;
            const uint8_t* scrpd_raw = scrpd->getRaw();
            for (byte k = 0; k < DSTherm::Scratchpad::LENGTH; k++) {
              if (scrpd_raw[k] != 0) {
                empty = false;
                val = scrpd->getTemp();
              }
            }
            if (result == OneWireNg::EC_SUCCESS && empty == false && val != 85000) {
              // pins[pin].state &= ~FORCE_REPORT;          // clear FORCE_REPORT flag
              if (owSensors[j].retryCnt == 1) {
                owSensors[j].retryCnt = RESULT_SUCCESS;  // clears number of retries
              } else {
                owSensors[j].retryCnt = RESULT_SUCCESS_RETRY;  // clears number of retries
              }
              if ((abs(val - owSensors[j].oldVal) >= (data.config.change[SENSOR_1WIRE] * 100L)) || (pins[pin].state & FORCE_REPORT)) {
                owSensors[j].oldVal = val;
                sendUdp(pin, j);
              }
            } else {
              // TODO: msg soft error
              drv.convertTemp(data.ow.ids[j], 0, PARASITE_POWER_ARG);
              pins[pin].timerMin.sleep(drv.getConversionTime(OW_RESOLUTION));
              finished = false;
            }
          }
          if (finished) {
            sleepPin(pin);
          }
        }
      }
      break;
    default:
      break;
  }
}
#endif /* ENABLE_DS18X20 */

// sleep pin (depending on pinType) and set pin state to 0
void sleepPin(const byte pin) {
  pins[pin].state = 0;  // clear FORCE_REPORT flag
  pins[pin].timerMin.sleep(data.config.intervalMin[data.pinSensor[pin]] * 1000UL);
}

// returns true if pin is reserved for Wire (I2C), SPI or Ethernet connections
bool isReserved(const byte pin) {
  // SPI pins used by the ethernet shield
  if (pin == PIN_SPI_SS || pin == PIN_SPI_MOSI || pin == PIN_SPI_MISO || pin == PIN_SPI_SCK || pin == ETH_RESET_PIN
#ifdef ENABLE_BH1750
      || pin == PIN_WIRE_SDA || pin == PIN_WIRE_SCL
#endif /* ENABLE_BH1750 */
  ) {
    return true;
  } else {
    return false;
  }
}