
#if defined(ENABLE_BH1750) || defined(ENABLE_MAX31865)

void readLight(const byte pin) {
  switch (pins[pin].state & STATE_MASK) {
    case 0:
      {
        pins[pin].retryCnt = 0;  // clears RESULT_SUCCESS and RESULT_SUCCESS_RETRY flags
        pins[pin].state++;
      }
      break;
    case 1:
      {
        if (pins[pin].retryCnt >= MAX_ATTEMPTS) {  // skip sensors with error and successful sensors
          sleepPin(pin);
          break;
        }
        pins[pin].retryCnt++;

        int32_t val = 0;
        byte error = 0;
        uint16_t hysteresis = data.config.change[data.pinSensor[pin]];
        switch (data.pinSensor[pin]) {
#ifdef ENABLE_BH1750
          case SENSOR_LIGHT:
            {
              pinMode(pin, OUTPUT);
              digitalWrite(pin, HIGH);
              Wire.clearWireTimeoutFlag();
              // I2CWrite(BH1750_POWER_UP);  // * "Power On" Command is possible to omit.
              // I2CWrite(BH1750_RESOLUTION);  // Set continuous mode and resolution
              error = I2CWrite(BH1750_RESOLUTION);  // Set continuous mode and resolution
              error |= (Wire.requestFrom(BH1750_ADDRESS_H, byte(2)) != 2);
              val = Wire.read();
              val <<= 8;
              val |= Wire.read();
              val /= 1.2f;
              error |= Wire.getWireTimeoutFlag();
              // error = (val == 54612) || (Wire.getWireTimeoutFlag() == true);
              digitalWrite(pin, LOW);
            }
            break;
#endif /* ENABLE_BH1750 */
#ifdef ENABLE_MAX31865
          case SENSOR_RTD:
            {
              new (&rtd) MAX31865(pin);
              MAX31865::RtdWire myWire;
              if (data.config.rtdWires == 3) {
                myWire = MAX31865::RTD_3WIRE;
              } else {
                myWire = MAX31865::RTD_2WIRE;  // same as RTD_4WIRE
              }
              rtd->begin(myWire, RTD_FILTER, MAX31865::CONV_MODE_CONTINUOUS);
              val = int32_t(rtd->getTemperature(data.config.rtdNominal, data.config.rtdReference) * 1000);
              error = (rtd->getFault() != 0) || (rtd->getResistance(data.config.rtdReference) == 0);
              hysteresis *= 100U;
            }
            break;
#endif /* ENABLE_MAX31865 */
          default:
            break;
        }
        if (error == false) {
          // pins[pin].state &= ~FORCE_REPORT;               // clear FORCE_REPORT flag
          if (pins[pin].retryCnt == 1) {
            pins[pin].retryCnt = RESULT_SUCCESS;  // clears number of retries
          } else {
            pins[pin].retryCnt = RESULT_SUCCESS_RETRY;  // clears number of retries
          }

          if ((abs(val - pins[pin].oldVal) >= hysteresis) || (pins[pin].state & FORCE_REPORT)) {
            pins[pin].oldVal = val;
            sendUdp(pin, 0);
          }
        }
      }
      break;
    default:
      break;
  }
}

byte I2CWrite(byte Data) {
  Wire.beginTransmission(BH1750_ADDRESS_H);
  Wire.write(Data);
  return Wire.endTransmission();
}


#endif