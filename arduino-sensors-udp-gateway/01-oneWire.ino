#ifdef ENABLE_ONEWIRE

#if (CONFIG_MAX_SEARCH_FILTERS > 0)
static_assert(CONFIG_MAX_SEARCH_FILTERS >= DSTherm::SUPPORTED_SLAVES_NUM,
              "CONFIG_MAX_SEARCH_FILTERS too small");
#endif

const bool PARASITE_POWER_ARG = false;  // parasite power is not supported by this sketch

Timer oneWireTimer;  // timer to delay writing statistics to EEPROM

typedef struct {
  Placeholder<OneWireNg_CurrentPlatform> wire;
  Timer minTimer;
  Timer maxTimer;
  byte state;
} owBus_t;

owBus_t owBuses[OW_MAX_BUSES];

typedef struct {
  int32_t oldVal;
  byte retryCnt;  // number of attempts to read
} owSensor_t;

owSensor_t owSensors[OW_MAX_SENSORS];

void startOneWire(const byte i) {
  if (data.config.owPins[i] == NUM_DIGITAL_PINS) return;
  new (&owBuses[i].wire) OneWireNg_CurrentPlatform(data.config.owPins[i], false);
  DSTherm drv(owBuses[i].wire);
#if (CONFIG_MAX_SEARCH_FILTERS > 0)
  drv.filterSupportedSlaves();
#endif
  /*
     * Set common resolution for all sensors.
     * Th, Tl (high/low alarm triggers) are set to 0.
     */
  drv.writeScratchpadAll(0, 0, OW_RESOLUTION);

  /*
     * The configuration above is stored in volatile sensors scratchpad
     * memory and will be lost after power unplug. Therefore store the
     * configuration permanently in sensors EEPROM.
     */
  drv.copyScratchpadAll(PARASITE_POWER_ARG);

  // searchOneWire(i);
}

void clearOneWire(const byte i) {
  // if (data.config.owPins[i] == NUM_DIGITAL_PINS) return; // clear anyway
  for (byte j = 0; j < OW_MAX_SENSORS; j++) {
    if (data.sensors.owBus[j] == i) {
      memset(&owSensors[j], 0, sizeof(owSensors[j]));                  // delete all sensors on the bus
      memset(&data.sensors.owId[j], 0, sizeof(data.sensors.owId[j]));  // delete all sensors on the bus
      data.sensors.owBus[j] = OW_MAX_SENSORS;                          // initial vaue is OW_MAX_SENSORS
    }
  }
  owBuses[i].maxTimer.sleep(0);  // reset timers
  owBuses[i].minTimer.sleep(0);
}

#define SENSORS_MEM_FULL -1   // oneWireSensors[] memory full
#define SENSORS_MEM_FOUND -2  // sensor already stored in oneWireSensors[] memory
void searchOneWire(const byte i) {
  if (data.config.owPins[i] == NUM_DIGITAL_PINS) return;
  owBuses[i].wire->searchReset();
  for (const auto& newId : *owBuses[i].wire) {
    // check if sensor type is supported by libraries we use
    if (DSTherm::getFamilyName(newId) == NULL) continue;  // not supported
    int16_t sensorNdx = SENSORS_MEM_FULL;
    for (byte j = OW_MAX_SENSORS; j--;) {  // loop all slots in memory
      if (data.sensors.owId[j][0] == 0) {
        sensorNdx = j;  // empty slot found, record its index
        continue;       //
      }
      bool match = true;
      for (byte k = 0; k < 8; k++) {
        if (newId[k] != data.sensors.owId[j][k]) {  // slot with different ID
          match = false;
          break;
        }
      }
      if (match) {
        sensorNdx = SENSORS_MEM_FOUND;  // slot with same ID
        if (data.sensors.owBus[j] != i) {
          data.sensors.owBus[j] = i;  // update pin in case the sensor only moved to another bus
          updateEeprom();
        }
        break;
      }
    }
    if (sensorNdx == SENSORS_MEM_FULL) {
      // TODO: error sensor not found and memory full
    } else if (sensorNdx >= 0) {     // empty slot found
                                     // TODO message new sensor
      DSTherm drv(owBuses[i].wire);  // new sensor, set common resolution
      drv.writeScratchpad(newId, 0, 0, OW_RESOLUTION);
      drv.copyScratchpad(newId, PARASITE_POWER_ARG);
      for (byte l = 0; l < 8; l++) {
        data.sensors.owId[sensorNdx][l] = newId[l];
      }
      data.sensors.owBus[sensorNdx] = i;
      updateEeprom();
    }
  }
}

void readOnewire(const byte i) {
  if (data.config.owPins[i] == NUM_DIGITAL_PINS) return;
  if (owBuses[i].maxTimer.isOver()) {
    for (byte j = 0; j < OW_MAX_SENSORS; j++) {
      if (data.sensors.owBus[j] == i) {
        owSensors[j].retryCnt = owSensors[j].retryCnt | FORCE_REPORT;  // Add FORCE_REPORT flag for all sensors on the bus
      }
    }
    owBuses[i].maxTimer.sleep((data.config.owIntMax - data.config.owIntMin) * 1000UL);
  }
  if (owBuses[i].minTimer.isOver() == false) {
    return;
  }
  DSTherm drv(owBuses[i].wire);
  switch (owBuses[i].state) {
    case 0:  // STATE_IDLE: Initialize conversion for all dallas temp sensors on a bus
      searchOneWire(i);
      drv.convertTempAll(0, PARASITE_POWER_ARG);
      for (byte j = 0; j < OW_MAX_SENSORS; j++) {
        if (data.sensors.owBus[j] == i) {
          // keep FORCE_REPORT flag and clear RESULT_SUCCESS and RESULT_SUCCESS_RETRY flags
          if (owSensors[j].retryCnt & RETRY_MASK == MAX_ATTEMPTS) {
            owSensors[j].retryCnt = (MAX_ATTEMPTS - 1) | (owSensors[j].retryCnt & FORCE_REPORT);  // allow only one attempt for disconnected sensors
          } else {
            owSensors[j].retryCnt = (owSensors[j].retryCnt & FORCE_REPORT) | B00000000;
          }
        }
      }
      owBuses[i].state++;
      owBuses[i].minTimer.sleep(drv.getConversionTime(OW_RESOLUTION));
      break;
    case 1:  // STATE_CONVERTING
      if (owBuses[i].wire->readBit() == 1 || owBuses[i].minTimer.isOver()) {
        // conversion on the bus finished, read sensors one by one
        Placeholder<DSTherm::Scratchpad> scrpd;
        bool finished = true;
        for (byte j = 0; j < OW_MAX_SENSORS; j++) {
          if (data.sensors.owBus[j] != i                                // skip empty slots and sensors from other buses
              || (owSensors[j].retryCnt & RESULT_SUCCESS)               // skip successful sensors
              || (owSensors[j].retryCnt & RETRY_MASK == MAX_ATTEMPTS))  // skip sensors with error
            continue;
          owSensors[j].retryCnt++;  // safe to do without mask
          byte result = drv.readScratchpad(data.sensors.owId[j], scrpd);
          bool empty = true;  // check if scratchpad is empty
          const uint8_t* scrpd_raw = scrpd->getRaw();
          for (byte k = 0; k < DSTherm::Scratchpad::LENGTH; k++) {
            if (scrpd_raw[k] != 0) empty = false;
          }
          if (result == OneWireNg::EC_SUCCESS && empty == false) {
            int32_t val = scrpd->getTemp();
            if ((abs(val - owSensors[j].oldVal) >= (data.config.owChange * 100UL)) || (owSensors[j].retryCnt & FORCE_REPORT)) {
              owSensors[j].oldVal = val;
              sendUdp(TYPE_1WIRE, j);
            }
            owSensors[j].retryCnt = RESULT_SUCCESS;  // clears FORCE_REPORT flag and number of retries
            if ((owSensors[j].retryCnt & RETRY_MASK) > 1) {
              owSensors[j].retryCnt = RESULT_SUCCESS_RETRY;  // clears FORCE_REPORT flag and number of retries
            }
          } else if ((owSensors[j].retryCnt & RETRY_MASK) < MAX_ATTEMPTS) {
            // TODO: msg soft error
            drv.convertTemp(data.sensors.owId[j], 0, PARASITE_POWER_ARG);
            owBuses[i].minTimer.sleep(drv.getConversionTime(OW_RESOLUTION));
            finished = false;
          }
        }
        if (finished) {
          owBuses[i].state = STATE_IDLE;
          owBuses[i].minTimer.sleep(data.config.owIntMin * 1000UL);
        }
      }
      break;
    default:
      break;
  }
}

#endif /* ENABLE_ONEWIRE */
