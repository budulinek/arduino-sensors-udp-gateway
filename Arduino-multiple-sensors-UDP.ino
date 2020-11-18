/*
    Copyright (C) 2019  Ing. Pavel Sedlacek, Dusan Zatkovsky, Milan Brejl, Budulinek
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <EEPROM.h>
#include <TrueRandom.h>         // https://github.com/sirleech/TrueRandom
#include "config.h"
#ifdef USE_ONEWIRE
#include <OneWire.h>                // Onewire https://www.pjrc.com/teensy/td_libs_OneWire.html
#define REQUIRESALARMS false
#include <DallasTemperature.h>      // DallasTemperature https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DS2438.h>                 // https://github.com/budulinek/arduino-onewire-DS2438
#endif /* USE_ONEWIRE */
#ifdef USE_DHT
#include <dhtnew.h>                    // DHTNEW https://github.com/RobTillaart/DHTNEW
#endif /* USE_DHT */
#ifdef USE_LIGHT
#include <BH1750FVI.h>              // BH1750FVI_RT https://github.com/RobTillaart/BH1750FVI_RT
#endif /* USE_LIGHT */

// App will generate unique MAC address, bytes 4, 5 and 6 will hold random value
byte mac[6] = { 0x90, 0xA2, 0xDA, 0x00, 0x00, 0x00 };

#define BUFFER_SIZE 65                           // size of UDP input and output buffers (1 byte for NULL)
#define UDP_TX_PACKET_MAX_SIZE BUFFER_SIZE
char inputPacketBuffer[BUFFER_SIZE];

// strings we use frequently
#define railStr F("ardu")
#define oneWireStr F("1w")
#define detectedStr F("detected")
#define unknownStr F("unknown")
#define errorStr F("error")
#define dropStr F("dropped")
#define dhtStr F("dht")
#define rstStr F("rst")
#define tempStr F("temp")
#define humidStr F("humid")
#define lightStr F("light")

#define SCHEDULED -2
#define ALL -1
#define FINISHED 255

class Timer {
  private:
    unsigned long timestampLastHitMs;
    unsigned long sleepTimeMs;
  public:
    boolean isOver();
    void sleep(unsigned long sleepTimeMs);
};
boolean Timer::isOver() {
  if ((unsigned long)(millis() - timestampLastHitMs) > sleepTimeMs) {
    return true;
  }
  return false;
}
void Timer::sleep(unsigned long sleepTimeMs) {
  this->sleepTimeMs = sleepTimeMs;
  timestampLastHitMs = millis();
}

EthernetUDP udpRecv;
EthernetUDP udpSend;

#ifdef DEBUG
#define dbg(x...) Serial.print(x);
#define dbgln(x...) Serial.println(x);
#else /* DEBUG */
#define dbg(x...) ;
#define dbgln(x...) ;
#endif /* DEBUG */

#ifdef USE_ONEWIRE
OneWire *oneWire[sizeof(oneWirePins)];
DS2438 *ds2438[sizeof(oneWirePins)];
DallasTemperature *dallas[sizeof(oneWirePins)];
// store oneWire sensors
typedef struct {
  byte bus;                // bus to which the sensor is attached
  byte addr[8];
  float oldTemp;
  byte retryCnt;           // number of attempts to read
  byte state;              // state of the reading process for individual sensor
  byte errorCnt;           // number of errors
} oneWireSensor;
oneWireSensor oneWireSensors[ONEWIRE_MAX_SENSORS];
int oneWireRequest = SCHEDULED;                 // request command from UDP (TODO)
Timer oneWireTimer;
#endif /* USE_ONEWIRE */

#ifdef USE_DHT
DHTNEW *dht[sizeof(dhtPins)];
// store DHT sensor values
typedef struct {
  float oldTemp;
  float oldHumid;
  byte retryCnt;           // number of attempts to read
  byte state;              // state of the reading process for individual sensor
} dhtSensor;
dhtSensor dhtSensors[sizeof(dhtPins)];
Timer dhtTimer;
int dhtRequest = SCHEDULED;
#endif /* USE_DHT */

#ifdef USE_LIGHT
BH1750FVI *bh1750[sizeof(lightPins)];
byte lightDecPlaces = 1;
// store lightWire sensors
typedef struct {
  float oldLux;
  byte retryCnt;           // number of attempts to read
  byte state;              // state of the reading process for individual sensor
} lightSensor;
lightSensor lightSensors[sizeof(lightPins)];
Timer lightTimer;
int lightRequest = SCHEDULED;
#endif /* USE_LIGHT */

void setup() {

  Serial.begin(115200);
  // TODO: debug info summarizing all settings

#ifdef ETH_RESET_PIN
  if (ETH_RESET_PIN) {
    pinMode(ETH_RESET_PIN, OUTPUT);
    digitalWrite(ETH_RESET_PIN, LOW);
    delay(25);
    digitalWrite(ETH_RESET_PIN, HIGH);
    delay(500);
    pinMode(ETH_RESET_PIN, INPUT);
    delay(2000);
  }
#endif /* ETH_RESET_PIN */

  // MAC stored in EEPROM
  if (EEPROM.read(1) == '#') {
    for (int i = 3; i < 6; i++)
      mac[i] = EEPROM.read(i);
  }
  // MAC not set, generate random value
  else {
    for (int i = 3; i < 6; i++) {
      mac[i] = TrueRandom.randomByte();
      EEPROM.write(i, mac[i]);
    }
    EEPROM.write(1, '#');
  }
  Ethernet.begin(mac, ip);
  udpRecv.begin(listenPort);
  udpSend.begin(sendPort);
  sendMsg(rstStr);

#ifdef USE_ONEWIRE
  for (byte i = 0; i < sizeof(oneWirePins); i++) {
    oneWire[i] = new OneWire(oneWirePins[i]);
    ds2438[i] = new DS2438(oneWire[i]);
    dallas[i] = new DallasTemperature(oneWire[i]);
    dallas[i]->begin();
    dallas[i]->setWaitForConversion(false);
    dallas[i]->setCheckForConversion(true);
  }
  searchOnewire();
#endif /* USE_ONEWIRE */

#ifdef USE_DHT
  for (byte i = 0; i < sizeof(dhtPins); i++) {
    dht[i] = new DHTNEW(dhtPins[i]);
    dht[i]->getType();
  }
#endif /* USE_DHT */

#ifdef USE_LIGHT
  Wire.begin();
  Wire.setWireTimeout(25000, false);    // I2C timeout 25ms to prevent lockups, no need to do HW reset. Latest Wire.h needed for this function.
  // Unlike DHT sensors (which have separate DATA pins), all light sensors share the same data line (I2C bus).
  // Unlike onewire sensors, light sensors are not individually addressable, they can only be addressed by setting the address pin LOW or HIGH.
  // Therefore, before communicating via I2C, we have to make sure that only one sensor has address pin set to HIGH.
  for (byte i = 0; i < sizeof(lightPins); i++) {
    pinMode(lightPins[i], OUTPUT);
    digitalWrite(lightPins[i], LOW);
  }
  for (byte i = 0; i < sizeof(lightPins); i++) {
    digitalWrite(lightPins[i], HIGH);
    delay(20);                         // just a precaution
    bh1750[i] = new BH1750FVI(BH1750FVI_ALT_ADDRESS);        // means that bh1750 commands will be processed by a sensor with address pin set to HIGH
    bh1750[i]->powerOn();
    if (lightResolution == 0.5) bh1750[i]->setContHigh2Res();
    else if (lightResolution == 4) bh1750[i]->setContLowRes();
    else bh1750[i]->setContHighRes();
    digitalWrite(lightPins[i], LOW);
    lightSensors[i].oldLux = -1;
  }
  if (lightResolution == 0.5) lightDecPlaces = 1;
#endif /* USE_LIGHT */
}

void loop() {

  readOnewire();
  readDht();
  readLight();

  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    processCommands();
  }

}

void (* resetFunc) (void) = 0;

String oneWireAddressToString(byte addr[]) {
  String s = "";
  for (byte i = 0; i < 8; i++) {
    if (addr[i] < 16) s += "0";
    s += String(addr[i], HEX);
  }
  return s;
}

void searchOnewire() {
#ifdef USE_ONEWIRE
  for (byte i = 0; i < sizeof(oneWirePins); i++) {
    byte newAddr[8];
    oneWire[i]->reset_search();
    while (oneWire[i]->search(newAddr)) {
      if (OneWire::crc8(newAddr, 7) == newAddr[7]) {
        // check if sensor type is supported by libraries we use
        if (dallas[i]->validFamily(newAddr) || (newAddr[0] == 38)) {
          bool old = false;
          // Check if sensor address is already stored in memory
          for (byte s = 0; s < ONEWIRE_MAX_SENSORS; s++) {
            int check;
            check = memcmp(newAddr, oneWireSensors[s].addr, sizeof(newAddr));
            if (check == 0) {
              old = true;
              break;
            }
          }
          if (old == false) {
            for (byte t = 0; t < ONEWIRE_MAX_SENSORS; t++) {
              if (oneWireSensors[t].addr[0] == 0) {
                for (byte x = 0; x < 8; x++) {
                  oneWireSensors[t].addr[x] = newAddr[x];
                }
                oneWireSensors[t].bus = i;
                String sendStr;
                sendStr += oneWireStr;
                sendStr += String(i + 1, DEC);
                sendStr += " ";
                sendStr += oneWireAddressToString(oneWireSensors[t].addr);
                sendStr += " ";
                sendStr += detectedStr;
                sendMsg(sendStr);
                break;
              }
            }
          }
        } else {
          String sendStr;
          sendStr += oneWireStr;
          sendStr += String(i + 1, DEC);
          sendStr += " ";
          sendStr += oneWireAddressToString(newAddr);
          sendStr += " ";
          sendStr += unknownStr;
          sendMsg(sendStr);
        }
      }
    }
    dallas[i]->setResolution(ONEWIRE_RESOLUTION);
  }
#endif /* USE_ONEWIRE */
}

void readOnewire() {
#ifdef USE_ONEWIRE
  if (!oneWireTimer.isOver()) {
    return;
  }
  for (byte i = 0; i < ONEWIRE_MAX_SENSORS; i++) {
    if (oneWireSensors[i].state == FINISHED) continue;     // This sensor is finished, continue the for loop with the next one (probably not needed here).
    switch (oneWireSensors[i].state) {
      case 0:          // Initialize conversion for all dallas temp sensors on a bus
        if (dallas[oneWireSensors[i].bus]->validFamily(oneWireSensors[i].addr)) {
          dallas[oneWireSensors[i].bus]->requestTemperatures();
          // all dallas temp sensors on the same bus can go to next state
          for (byte j = 0; j < ONEWIRE_MAX_SENSORS; j++) {
            if ((dallas[oneWireSensors[j].bus]->validFamily(oneWireSensors[j].addr)) && (oneWireSensors[j].bus == oneWireSensors[i].bus)) {
              oneWireSensors[j].state++;
            }
          }
        } else if (oneWireSensors[i].addr[0] == 38) {
          oneWireSensors[i].state++;
        }
        break;
      case 1:          // Read sensors
        // read dallas temp sensors in case conversion is completed
        if (dallas[oneWireSensors[i].bus]->validFamily(oneWireSensors[i].addr) && dallas[oneWireSensors[i].bus]->isConversionComplete()) {
          float tempC = dallas[oneWireSensors[i].bus]->getTempC(oneWireSensors[i].addr);
          String sendStr;
          sendStr += oneWireStr;
          sendStr += String(oneWireSensors[i].bus + 1, DEC);
          sendStr += " ";
          sendStr += oneWireAddressToString(oneWireSensors[i].addr);
          sendStr += " ";
          if ((tempC != -127) && (tempC != 85)) {
            oneWireSensors[i].errorCnt = 0;
            oneWireSensors[i].retryCnt = 0;
            if (ONEWIRE_HYSTERESIS && (abs(tempC - oneWireSensors[i].oldTemp) <= ONEWIRE_HYSTERESIS)) {
              oneWireSensors[i].state++;
              break;                  // break the switch-case, in order to avoid sendMsg(sendStr)
            }
            oneWireSensors[i].oldTemp = tempC;
            sendStr += tempStr;
            sendStr += " ";
            sendStr += String(tempC, 2);
          } else if (oneWireSensors[i].retryCnt == ONEWIRE_MAX_RETRY) {   // error detected, ONEWIRE_MAX_RETRY treshold reached
            sendStr += errorStr;
            oneWireSensors[i].retryCnt = 0;
            oneWireSensors[i].errorCnt++;
            if (oneWireSensors[i].errorCnt == ONEWIRE_MAX_ERRORS) { // error detected, ONEWIRE_MAX_ERRORS treshold reached
              // delete sensor
              for (byte x = 0; x < 8; x++) {
                oneWireSensors[i].addr[x] = 0;
              }
              oneWireSensors[i].errorCnt = 0;
              oneWireSensors[i].bus = 0;
              sendStr += dropStr;
            }
          } else {                             // TODO be more aggressive in case of error (new requestTemperatures or even depower and reset?)
            oneWireSensors[i].retryCnt++;
            dallas[oneWireSensors[i].bus]->requestTemperaturesByAddress(oneWireSensors[i].addr);
            break;
          }
          sendMsg(sendStr);
          oneWireSensors[i].state = FINISHED;
        } else if (oneWireSensors[i].addr[0] == 38) {       // read DS2438
          ds2438[oneWireSensors[i].bus]->begin();
          ds2438[oneWireSensors[i].bus]->update(oneWireSensors[i].addr);
          String sendStr;
          sendStr += oneWireStr;
          sendStr += String(oneWireSensors[i].bus + 1, DEC);
          sendStr += " ";
          sendStr += oneWireAddressToString(oneWireSensors[i].addr);
          sendStr += " ";
          if (!ds2438[oneWireSensors[i].bus]->isError()) {
            oneWireSensors[i].errorCnt = 0;
            oneWireSensors[i].retryCnt = 0;
            float temp = ds2438[oneWireSensors[i].bus]->getTemperature();
            if (ONEWIRE_HYSTERESIS && (abs(temp - oneWireSensors[i].oldTemp) <= ONEWIRE_HYSTERESIS)) {
              oneWireSensors[i].state++;
              break;                  // break the switch-case, in order to avoid sendMsg(sendStr)
            }
            sendStr += String(temp, 2);
            sendStr += " ";
            sendStr += String(ds2438[oneWireSensors[i].bus]->getVoltage(DS2438_CHA), 2);
            sendStr += " ";
            sendStr += String(ds2438[oneWireSensors[i].bus]->getVoltage(DS2438_CHB), 2);
            oneWireSensors[i].oldTemp = temp;
          } else if (oneWireSensors[i].retryCnt == ONEWIRE_MAX_RETRY) {   // error detected, ONEWIRE_MAX_RETRY treshold reached
            sendStr += errorStr;
            oneWireSensors[i].retryCnt = 0;
            oneWireSensors[i].errorCnt++;
            if (oneWireSensors[i].errorCnt == ONEWIRE_MAX_ERRORS) { // error detected, ONEWIRE_MAX_ERRORS treshold reached
              // delete sensor
              for (byte x = 0; x < 8; x++) {
                oneWireSensors[i].addr[x] = 0;
              }
              oneWireSensors[i].errorCnt = 0;
              oneWireSensors[i].bus = 0;
              sendStr += dropStr;
            }
          } else {                             // TODO be more aggressive in case of error (new requestTemperatures or even depower and reset?)
            oneWireSensors[i].retryCnt++;
            break;
          }
          sendMsg(sendStr);
          oneWireSensors[i].state = FINISHED;
        }
        break;
    }
  }
  for (byte i = 0; i < ONEWIRE_MAX_SENSORS; i++) {
    // still waiting for some recognised sensors to finish, return
    if ((dallas[oneWireSensors[i].bus]->validFamily(oneWireSensors[i].addr) || (oneWireSensors[i].addr[0] == 38)) && (oneWireSensors[i].state != FINISHED)) return;
  }
  // all onewire sensors finished reading, search new sensors, reset state and sleep
  searchOnewire();
  for (byte i = 0; i < ONEWIRE_MAX_SENSORS; i++) {
    oneWireSensors[i].state = 0;
  }
  for (byte i = 0; i < sizeof(oneWirePins); i++) {
    oneWire[i]->depower();       // not sure if both commands are necessary
    oneWire[i]->reset();
  }
  oneWireTimer.sleep(ONEWIRE_CYCLE);
#endif /* USE_ONEWIRE */
}

void readDht() {
#ifdef USE_DHT
  if (!dhtTimer.isOver()) {
    return;
  }
  for (byte i = 0; i < sizeof(dhtPins); i++) {
    if (dhtSensors[i].state == FINISHED) continue;     // This sensor is finished, continue the for loop with the next one.
    switch (dhtSensors[i].state) {
      case 0:                       // Initialize new reading
        dht[i]->powerUp();
        dhtSensors[i].state++;
        break;
      case 1:                       // Wait for the reading to finish and process the results
        { // yes, we need these brackets
          int chk = dht[i]->read();
          if (chk != DHTLIB_WAITING_FOR_READ) {
            String sendStr;
            sendStr += dhtStr;
            sendStr += String(i + 1, DEC);
            sendStr += " ";
            if (chk == DHTLIB_OK) {
              dhtSensors[i].retryCnt = 0;
              if (DHT_TEMP_HYSTERESIS && ((abs(dht[i]->getTemperature() - dhtSensors[i].oldTemp) <= DHT_TEMP_HYSTERESIS) && (abs(dht[i]->getHumidity() - dhtSensors[i].oldHumid) <= DHT_HUMID_HYSTERESIS))) {
                dhtSensors[i].state++;
                break;                  // break the switch-case, in order to avoid sendMsg(sendStr);
              }
              dhtSensors[i].oldTemp = dht[i]->getTemperature();     // getTemperature() only returns stored value, does not perform new reading (we can thus call it any times we want)
              dhtSensors[i].oldHumid = dht[i]->getHumidity();
              sendStr += tempStr;
              sendStr += " ";
              sendStr += String(dhtSensors[i].oldTemp, 1);
              sendStr += " ";
              sendStr += humidStr;
              sendStr += " ";
              sendStr += String(dhtSensors[i].oldHumid, 1);
            } else if (dhtSensors[i].retryCnt == DHT_MAX_RETRY) {   // error detected, treshold reached
              dhtSensors[i].retryCnt = 0;
              sendStr += errorStr;
            } else {
              dhtSensors[i].retryCnt++;
              dhtSensors[i].state = 0;      // repeat the first step (incl. short nap)
              return;        // do not send message yet, retry the same sensor
            }
            sendMsg(sendStr);
            dhtSensors[i].state++;
          }
          break;
        }
      case 2: // Sensor done, set the data pin to LOW
        dht[i]->powerDown();
        dhtSensors[i].state = FINISHED;
        break;
    }
  }
  for (byte i = 0; i < sizeof(dhtPins); i++) {
    // still waiting for some sensors to finish, return
    if (dhtSensors[i].state != FINISHED) return;
  }
  // all dht sensors finished reading, reset state and sleep
  for (byte i = 0; i < sizeof(dhtPins); i++) {
    dhtSensors[i].state = 0;
  }
  dhtTimer.sleep(DHT_CYCLE);
#endif /* USE_DHT */
}

void readLight() {
#ifdef USE_LIGHT
  if (!lightTimer.isOver()) {
    return;
  }
  for (byte i = 0; i < sizeof(lightPins); i++) {
    if (lightSensors[i].state == FINISHED) continue;     // This sensor is finished, continue the for loop with the next one.
    if (Wire.getWireTimeoutFlag()) {                // Check I2C bus for errors.
      Wire.clearWireTimeoutFlag();
      Wire.end();
      Wire.begin();       // Re-initialise the I2C bus after failure. It seems that it is not necessary to re-initialize sensors themselves.
      String sendStr;
      sendStr += lightStr;
      sendStr += String(i + 1, DEC);
      sendStr += " ";
      sendStr += errorStr;
      sendMsg(sendStr);
    }
    switch (lightSensors[i].state) {
      case 0:                       // Set address pin, take a short nap
        for (byte j = 0; j < sizeof(lightPins); j++) {
          if (i == j) continue;
          digitalWrite(lightPins[j], LOW);    // only precaution, just to make sure all other sensors are LOW
        }
        digitalWrite(lightPins[i], HIGH);
        lightTimer.sleep(20);         // just a precaution
        lightSensors[i].state++;
        break;
      case 1:
        if (bh1750[i]->isReady()) {
          float lux = bh1750[i]->getLux();
          String sendStr;
          sendStr += lightStr;
          sendStr += String(i + 1, DEC);
          sendStr += " ";
          if (bh1750[i]->getError() == BH1750FVI_OK) {
            if (LIGHT_HYSTERESIS && (abs(lux - lightSensors[i].oldLux) <=  LIGHT_HYSTERESIS)) {
              lightSensors[i].retryCnt = 0;
              lightSensors[i].state++;
              break;                  // break the switch-case, in order to avoid sendMsg(sendStr);
            }
            lightSensors[i].oldLux = lux;
            sendStr += String(lux, lightDecPlaces);
          } else if (lightSensors[i].retryCnt == LIGHT_MAX_RETRY) {   // error detected, treshold reached
            sendStr += errorStr;
          } else {        // error detected
            lightSensors[i].retryCnt++;
            lightSensors[i].state = 0;      // repeat the first step (incl. short nap)
            return;        // do not send message yet, retry the same sensor
          }
          sendMsg(sendStr);
          lightSensors[i].retryCnt = 0;   // because new reading was sent or LIGHT_MAX_RETRY was reached
          lightSensors[i].state++;
        }
        break;
      case 2: // Sensor done, set the address pin to LOW
        digitalWrite(lightPins[i], LOW);
        lightSensors[i].state = FINISHED;
        break;
    }
    break;    // Break the for loop, because we can only talk to next sensor after the previous sensor is FINISHED (and its addr pin is set to LOW)!
  }
  for (byte i = 0; i < sizeof(lightPins); i++) {
    // still waiting for some sensors to finish, return
    if (lightSensors[i].state != FINISHED) return;
  }
  // all light sensors finished reading, reset state and sleep
  for (byte i = 0; i < sizeof(lightPins); i++) {
    lightSensors[i].state = 0;
  }
  lightTimer.sleep(LIGHT_CYCLE);
#endif /* USE_LIGHT */
}

void sendMsg(const String & sendStr) {
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    udpSend.beginPacket(sendIpAddress, remPort);
    udpSend.print(railStr);
    udpSend.print(String(boardIdentifier));
    udpSend.print(" ");
    udpSend.print(sendStr);
    udpSend.endPacket();
  }
  dbg(F("Sending packet: "));
  dbg(railStr);
  dbg(String(boardIdentifier));
  dbg(" ");
  dbgln(sendStr);
}

boolean receivePacket(String * cmd) {
  int packetSize = udpRecv.parsePacket();
  if (packetSize) {
    memset(inputPacketBuffer, 0, sizeof(inputPacketBuffer));
    udpRecv.read(inputPacketBuffer, BUFFER_SIZE);
    *cmd = String(inputPacketBuffer);
    String boardIdentifierRailStr;
    boardIdentifierRailStr += railStr;
    boardIdentifierRailStr += String(boardIdentifier);
    if (cmd->startsWith(boardIdentifierRailStr)) {
      cmd->replace(boardIdentifierRailStr, "");
      cmd->trim();
      return true;
    }
  }
  return false;
}


void processCommands() {
  String cmd;
  if (receivePacket(&cmd)) {
    dbg(F("Received packet: "));
    dbgln(cmd);
    if (cmd == rstStr) {
      delay(100);
      resetFunc();
    } else if (cmd.startsWith(dhtStr)) {
      if (cmd == dhtStr) {
        //        readDhtSensors("all");
      } else {
        cmd.replace(dhtStr, "");
        //        readDhtSensors(cmd);
      }
    }
  }
}
