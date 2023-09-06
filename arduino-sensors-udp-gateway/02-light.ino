
#ifdef ENABLE_LIGHT

// store lightWire sensors
typedef struct {
  BH1750FVI *wire;
  Timer minTimer;
  Timer maxTimer;
} light_t;

light_t light[LIGHT_MAX_SENSORS];

// store lightWire sensors
typedef struct {
  uint16_t oldVal;
  byte retryCnt;  // number of attempts to read
  byte state;
} lightSensor_t;

lightSensor_t lightSensors[LIGHT_MAX_SENSORS];

BH1750FVI::eDeviceAddress_t LIGHT_ADDRESS = BH1750FVI::k_DevAddress_H;  // Commands sent via Wire (I2C) will be processed by a sensor with address pin set to HIGH

void startLight(const byte i) {
  // Unlike DHT sensors (which have separate DATA pins), all light sensors share the same data line (I2C bus).
  // Unlike onewire sensors, light sensors are not individually addressable, they can only be addressed by setting the address pin LOW or HIGH.
  // We use address pin as "chip select", therefore we have to make sure that only one sensor has address pin set to HIGH.
  if (data.config.lightPins[i] == NUM_DIGITAL_PINS) return;
  light[i].wire = new BH1750FVI(data.config.lightPins[i], LIGHT_ADDRESS, LIGHT_MODE);
  light[i].wire->begin();
  digitalWrite(data.config.lightPins[i], LOW);
}

void clearLight(const byte i) {
  // if (data.config.lightPins[i] == NUM_DIGITAL_PINS) return; // clear anyway
  memset(&lightSensors[i], 0, sizeof(lightSensors[i]));
  data.sensors.lightConnected[i] = false;
  light[i].maxTimer.sleep(0);  // reset timers
  light[i].minTimer.sleep(0);
}

void searchLight(const byte i) {
  if (data.config.lightPins[i] == NUM_DIGITAL_PINS) return;
  digitalWrite(data.config.lightPins[i], HIGH);
  Wire.beginTransmission(BH1750FVI::k_DevAddress_H);
  byte error = Wire.endTransmission();
  if (error == 0 && data.sensors.lightConnected[i] == false) {  // new sensor found
    data.sensors.lightConnected[i] = true;
    updateEeprom();
  }
  digitalWrite(data.config.lightPins[i], LOW);
}

void readLight(const byte i) {
  if (data.config.lightPins[i] == NUM_DIGITAL_PINS) return;
  if (light[i].maxTimer.isOver()) {
    lightSensors[i].retryCnt = lightSensors[i].retryCnt | FORCE_REPORT;  // Add FORCE_REPORT flag for all sensors
    light[i].maxTimer.sleep((data.config.lightIntMax - data.config.lightIntMin) * 1000UL);
  }
  if (light[i].minTimer.isOver() == false) {
    return;
  }
  switch (lightSensors[i].state) {
    case 0:  // STATE_IDLE
      searchLight(i);
      if (data.sensors.lightConnected[i]) {
        lightSensors[i].retryCnt = (lightSensors[i].retryCnt & FORCE_REPORT) | B00000000;  // keep FORCE_REPORT flag and clear RESULT_SUCCESS and RESULT_SUCCESS_RETRY flags
      }
      lightSensors[i].state++;
      break;
    case 1:  // STATE_CONVERTING
      {
        if (data.sensors.lightConnected[i] == false                          // skip if no sensor is (was) connected
            || (lightSensors[i].retryCnt & RESULT_SUCCESS)                   // skip successful sensors
            || ((lightSensors[i].retryCnt & RETRY_MASK) == MAX_ATTEMPTS)) {  // skip sensors with error
          lightSensors[i].state = STATE_IDLE;
          light[i].minTimer.sleep(data.config.lightIntMin * 1000UL);
          break;
        }
        digitalWrite(data.config.lightPins[i], HIGH);
        lightSensors[i].retryCnt++;
        Wire.clearWireTimeoutFlag();
        uint16_t val = light[i].wire->GetLightIntensity();
        if (val != 54612 && Wire.getWireTimeoutFlag() == false) {
          if ((abs(val - lightSensors[i].oldVal) >= data.config.lightChange) || (lightSensors[i].retryCnt & FORCE_REPORT)) {
            lightSensors[i].oldVal = val;
            sendUdp(TYPE_LIGHT, i);
          }
          lightSensors[i].retryCnt = RESULT_SUCCESS;  // clears FORCE_REPORT flag and number of retries
          if ((lightSensors[i].retryCnt & RETRY_MASK) > 1) {
            lightSensors[i].retryCnt = RESULT_SUCCESS_RETRY;  // clears FORCE_REPORT flag and number of retries
          }
        }
        digitalWrite(data.config.lightPins[i], LOW);
      }
      break;
    default:
      break;
  }
}

#endif /* ENABLE_LIGHT */