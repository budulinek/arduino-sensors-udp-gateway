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


/*

   signals from Arduino:

     Arduino (re)start:              ardu1 rst
   commands to Arduino:
     read DHT sensor:                ardu1 dht2
     read all DHT sensors:           ardu1 dht
     restart:                        ardu1 rst
   default scan cycles:
     DHT sensors cycle (if change):  5000 ms
     1wire cycle:                    30000 ms

*/

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <EEPROM.h>
#include <TrueRandom.h>         // https://github.com/sirleech/TrueRandom
#include "config.h"
#ifdef USE_ONEWIRE
#include <OneWire.h>                // Onewire https://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h>      // DallasTemperature https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DS2438.h>                 // https://github.com/budulinek/arduino-onewire-DS2438
#endif /* USE_ONEWIRE */
#ifdef USE_DHT
#include <DHT.h>                    // DHT sensor library https://github.com/adafruit/DHT-sensor-library 
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
OneWire *oneWireBus[sizeof(oneWireBusPins)];
DS2438 *oneWireBus2438[sizeof(oneWireBusPins)];
DallasTemperature *oneWireBusDallas[sizeof(oneWireBusPins)];
bool oneWireBusDallasFinished[sizeof(oneWireBusPins)];
// store oneWire sensors
typedef struct {
  byte bus;
  byte addr[8];
  float oldTemp;
  byte errorCnt;
} oneWireSensor;
oneWireSensor oneWireSensors[ONEWIRE_MAX_SENSORS];
byte DS18B20count[sizeof(oneWireBusPins)] = {};
Timer oneWireTimer;
byte oneWireState = 0;
int oneWireRequest = SCHEDULED;
#endif /* USE_ONEWIRE */

#ifdef USE_DHT
DHT *dhtSensor[sizeof(dhtSensorPins)];
// store DHT sensor values
typedef struct {
  float oldTemp;
  float oldHumid;
  byte state;
} dhtSensr;
dhtSensr dhtSensors[sizeof(dhtSensorPins)];
Timer dhtTimer;
byte dhtState = 0;
int dhtRequest = SCHEDULED;
bool dhtError = false;
#endif /* USE_DHT */

#ifdef USE_LIGHT
BH1750FVI *lightSensor[sizeof(lightSensorPins)];
// store lightWire sensors
typedef struct {
  float oldLux;
  byte state;
} lightSensr;
lightSensr lightSensors[sizeof(lightSensorPins)];
Timer lightTimer;
byte lightState = 0;
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

  //      just for me... TODO delete
#define ONEWIRE_POWER_PIN 6
  pinMode(ONEWIRE_POWER_PIN, OUTPUT);
  digitalWrite(ONEWIRE_POWER_PIN, LOW);

  for (byte i = 0; i < sizeof(oneWireBusPins); i++) {
    oneWireBus[i] = new OneWire(oneWireBusPins[i]);
    oneWireBus2438[i] = new DS2438(oneWireBus[i]);
    oneWireBusDallas[i] = new DallasTemperature(oneWireBus[i]);
    oneWireBusDallas[i]->begin();
    oneWireBusDallas[i]->setWaitForConversion(false);
    DS18B20count[i] = 0;
  }
  searchOnewire();
#endif /* USE_ONEWIRE */

#ifdef USE_DHT
#ifdef DHT_POWER_PIN
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);
#endif /* DHT_POWER_PIN */
  for (byte i = 0; i < sizeof(dhtSensorPins); i++) {
    dhtSensor[i] = new DHT(dhtSensorPins[i], DHT_TYPE);
    dhtSensor[i]->begin();
  }
#endif /* USE_DHT */

#ifdef USE_LIGHT
  Wire.begin();
  Wire.setWireTimeout(25000, false);    // I2C timeout 25ms to prevent lockups (latest Wire.h needed for this function)
  for (byte i = 0; i < sizeof(lightSensorPins); i++) {
    lightSensor[i] = new BH1750FVI(0x5C);   // 0x5C means that sensor will be read if address pin is set to HIGH
    lightSensor[i]->powerOn();
    lightSensor[i]->setContHigh2Res();
    pinMode(lightSensorPins[i], OUTPUT);
    digitalWrite(lightSensorPins[i], LOW);
    lightSensors[i].oldLux = -1;
  }
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
  for (byte i = 0; i < sizeof(oneWireBusPins); i++) {
    byte newAddr[8];
    while (oneWireBus[i]->search(newAddr)) {
      if (OneWire::crc8(newAddr, 7) == newAddr[7]) {
        // check if sensor type is supported by libraries we use
        if (oneWireBusDallas[i]->validFamily(newAddr) || (newAddr[0] == 38)) {
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
                if (oneWireSensors[t].addr[0] != 38) DS18B20count[i]++;
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
    oneWireBus[i]->reset_search();
    oneWireBusDallas[i]->setResolution(ONEWIRE_RESOLUTION);
  }
#endif /* USE_ONEWIRE */
}

void readOnewire() {
#ifdef USE_ONEWIRE
  if (!oneWireTimer.isOver()) {
    return;
  }
  switch (oneWireState) {
    case 0:     // Check for new sensors
      searchOnewire();
      oneWireState++;
      break;
    case 1:     // Read DS2438
      // TODO check if reading DS2438 is blocking or not
      for (byte j = 0; j < sizeof(oneWireBusPins); j++) {
        for (byte i = 0; i < ONEWIRE_MAX_SENSORS; i++) {
          if (oneWireSensors[i].addr[0] == 38 && oneWireSensors[i].bus == j) {
            oneWireBus2438[j]->begin();
            oneWireBus2438[j]->update(oneWireSensors[i].addr);
            String sendStr;
            sendStr += oneWireStr;
            sendStr += String(j + 1, DEC);
            sendStr += " ";
            sendStr += oneWireAddressToString(oneWireSensors[i].addr);
            sendStr += " ";
            if (!oneWireBus2438[j]->isError()) {
              float temp = oneWireBus2438[j]->getTemperature();
              if (ONEWIRE_HYSTERESIS && (abs(temp - oneWireSensors[i].oldTemp) <= ONEWIRE_HYSTERESIS)) continue;
              sendStr += String(temp, 2);
              sendStr += " ";
              sendStr += String(oneWireBus2438[j]->getVoltage(DS2438_CHA), 2);
              sendStr += " ";
              sendStr += String(oneWireBus2438[j]->getVoltage(DS2438_CHB), 2);
              oneWireSensors[i].errorCnt = 0;
            } else if (oneWireSensors[i].errorCnt == ONEWIRE_MAX_ERRORS) {
              sendStr += dropStr;
              sendMsg(sendStr);
              for (byte x = 0; x < 8; x++) {
                oneWireSensors[i].addr[x] = 0;
              }
              oneWireSensors[i].errorCnt = 0;
              oneWireSensors[i].bus = 0;
            } else {
              sendStr += errorStr;
              oneWireSensors[i].errorCnt++;
            }
            sendMsg(sendStr);
          }
        }
      }
      oneWireState++;
      break;
    case 2:     // sends command for all devices on all buses to perform a temperature conversion
      for (byte j = 0; j < sizeof(oneWireBusPins); j++) {
        oneWireBusDallas[j]->requestTemperatures();
        oneWireBusDallasFinished[j] = false;
      }
      oneWireState++;
      break;
    case 3:     // Read DS18B20
      for (byte j = 0; j < sizeof(oneWireBusPins); j++) {
        if (((oneWireBusDallasFinished[j] == false) && oneWireBusDallas[j]->isConversionComplete()) || !DS18B20count[j]) {
          oneWireBusDallasFinished[j] = true;
          // Conversion is complete, we can fetch temperature for all sensors on the bus
          for (byte i = 0; i < ONEWIRE_MAX_SENSORS; i++) {
            if (oneWireSensors[i].addr[0] != 0 && oneWireSensors[i].addr[0] != 38 && oneWireSensors[i].bus == j) {
              float tempC = oneWireBusDallas[j]->getTempC(oneWireSensors[i].addr);
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(j + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(oneWireSensors[i].addr);
              sendStr += " ";
              if ((tempC != -127) && (tempC != 85)) {
                oneWireSensors[i].errorCnt = 0;
                if (ONEWIRE_HYSTERESIS && (abs(tempC - oneWireSensors[i].oldTemp) <= ONEWIRE_HYSTERESIS)) continue;
                sendStr += tempStr;
                sendStr += " ";
                sendStr += String(tempC, 2);
                oneWireSensors[i].oldTemp = tempC;
              } else if (oneWireSensors[i].errorCnt == ONEWIRE_MAX_ERRORS) {
                sendStr += dropStr;
                for (byte x = 0; x < 8; x++) {
                  oneWireSensors[i].addr[x] = 0;
                }
                oneWireSensors[i].errorCnt = 0;
                oneWireSensors[i].bus = 0;
                DS18B20count[j]--;
              } else {
                sendStr += errorStr;
                oneWireSensors[i].errorCnt++;
              }
              sendMsg(sendStr);
            }
          }
        }
      }
      for (byte j = 0; j < sizeof(oneWireBusPins); j++) {
        // still waiting for some buses to convert, return
        if (oneWireBusDallasFinished[j] == false) return;
      }
      oneWireState++;
    case 4: // all done, power off and sleep
      for (byte i = 0; i < sizeof(oneWireBusPins); i++) {
        oneWireBus[i]->depower();       // not sure if both commands are necessary
        oneWireBus[i]->reset();
      }
      oneWireTimer.sleep(ONEWIRE_CYCLE - 1000);
      oneWireState = 0;
      break;
  }
#endif /* USE_ONEWIRE */
}

void readDht() {
#ifdef USE_DHT
  if (!dhtTimer.isOver()) {
    return;
  }
  switch (dhtState) {
    case 0:     // Power on buses, if you use transistor switch, the logic is reversed
#ifdef DHT_POWER_PIN
      if (DHT_POWER_PIN) digitalWrite(DHT_POWER_PIN, HIGH);
      dhtTimer.sleep(1000);
#endif /* DHT_POWER_PIN */
      dhtState++;
      break;
    case 1: // Read DHT sensors
      for (byte i = 0; i < sizeof(dhtSensorPins); i++) {
        float temp = dhtSensor[i]->readTemperature();
        float humid = dhtSensor[i]->readHumidity();
        String sendStr;
        sendStr += dhtStr;
        sendStr += String(i + 1, DEC);
        sendStr += " ";
        if (!isnan(temp) && !isnan(humid)) {
          if (DHT_TEMP_HYSTERESIS && ((abs(temp - dhtSensors[i].oldTemp) <= DHT_TEMP_HYSTERESIS) && (abs(humid - dhtSensors[i].oldHumid) <= DHT_HUMID_HYSTERESIS))) continue;
          dhtSensors[i].oldTemp = temp;
          dhtSensors[i].oldHumid = humid;
          sendStr += tempStr;
          sendStr += " ";
          sendStr += String(temp, 1);
          sendStr += " ";
          sendStr += humidStr;
          sendStr += " ";
          sendStr += String(humid, 1);
          sendMsg(sendStr);
        } else {
          sendStr += errorStr;
          dhtError = true;
        }
        sendMsg(sendStr);
      }
      dhtState++;
      break;
    case 2: // Power off in case of error, sleep
#ifdef DHT_POWER_PIN
      if (dhtError == true) digitalWrite(DHT_POWER_PIN, LOW);
      dhtTimer.sleep(DHT_CYCLE - 1000);
#else /* DHT_POWER_PIN */
      dhtTimer.sleep(DHT_CYCLE);
#endif /* DHT_POWER_PIN */
      dhtError = false;
      dhtState = 0;
      break;
  }
#endif /* USE_DHT */
}

void readLight() {
#ifdef USE_LIGHT
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    Wire.end();
    Wire.begin();
    // reinitialize everything (not sure if all steps are actually needed)
    //    Wire.setWireTimeout(25000, false);    // I2C timeout 25ms to prevent lockups (latest Wire.h needed for this function)
    //    for (byte i = 0; i < sizeof(lightSensorPins); i++) {
    //      lightSensor[i] = new BH1750FVI(0x5C);   // 0x5C means that sensor will be read if address pin is set to HIGH
    //      lightSensor[i]->powerOn();
    //      lightSensor[i]->setContHigh2Res();
    //    }
    String sendStr;
    sendStr += lightStr;      // we do not know which sensor caused the error
    sendStr += " ";
    sendStr += errorStr;
    sendMsg(sendStr);
  }
  if (!lightTimer.isOver()) {
    return;
  }
  switch (lightState) {
    case 0:
      for (byte i = 0; i < sizeof(lightSensorPins); i++) {
        if (lightSensors[i].state == FINISHED) continue;     // sensor is finished, continue with the next one
        switch (lightSensors[i].state) {
          case 0:                       // Set address pin, take a short nap
            digitalWrite(lightSensorPins[i], HIGH);
            lightTimer.sleep(100);
            lightSensors[i].state++;
            break;
          case 1:
            if (lightSensor[i]->isReady()) {
              float lux = lightSensor[i]->getLux();
              String sendStr;
              sendStr += lightStr;
              sendStr += String(i + 1, DEC);
              sendStr += " ";
              if (lux != 54612) {
                if (LIGHT_HYSTERESIS && (abs(lux - lightSensors[i].oldLux) <=  LIGHT_HYSTERESIS)) {
                  lightSensors[i].state++;
                  break;
                }
                lightSensors[i].oldLux = lux;
                sendStr += String(lux, 1);
              } else {
                sendStr += errorStr;
              }
              sendMsg(sendStr);
              lightSensors[i].state++;
            }
            break;
          case 2: // Sensor done, set the address pin
            digitalWrite(lightSensorPins[i], LOW);
            lightSensors[i].state = FINISHED;
            break;
        }
        break;    // Break the for loop (deal with next sensor only after the previous finishes reading
      }
      for (byte i = 0; i < sizeof(lightSensorPins); i++) {
        // still waiting for some sensors to finish, return
        if (lightSensors[i].state != FINISHED) return;
      }
      lightState++;
      break;
    case 1: // all light sensors finished reading, sleep
      lightTimer.sleep(LIGHT_CYCLE);
      lightState = 0;
      for (byte i = 0; i < sizeof(lightSensorPins); i++) {
        lightSensors[i].state = 0;
        digitalWrite(lightSensorPins[i], LOW);    // just to make sure all sensors' address is low
      }
      break;
  }
#endif /* USE_LIGHT */
}

void sendMsg(const String& sendStr) {
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

boolean receivePacket(String *cmd) {
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
