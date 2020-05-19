/*
    Copyright (C) 2019  Ing. Pavel Sedlacek, Dusan Zatkovsky, Milan Brejl
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
   UDP syntax:        ardu<boardAddress> 1w<OneWireBus> <sensorAddress> temp <tempCelsius>
   signals from Arduino:
     Sensor detected (after start):  ardu1 1w2 2864fc3008082 detected
     DS18B20 1wire sensor packet:    ardu1 1w2 2864fc3008082 temp 25.44
     DS2438 1wire sensor packet:     ardu1 1w2 2612c3102004f 25.44 1.23 0.12
     Sensor error:                   ardu1 1w2 2864fc3008082 error
     DHT sensor (temp°C humid%):     ardu1 dht3 25.3 42.0
     Light sensor (lux):             ardu1 light1 123
     Arduino (re)start:              ardu1 rst
   commands to Arduino:
     read DHT sensor:                ardu1 dht2
     read all DHT sensors:           ardu1 dht
     restart:                        ardu1 rst
   default scan cycles:
     DHT sensors cycle (if change):  5000 ms
     1wire cycle:                    30000 ms

*/

#define dbg(x) Serial.print(x);
// #define dbg(x) ;
#define dbgln(x) Serial.println(x);
// #define dbgln(x) ;

#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS2438.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DHT.h>
#include <SBWire.h>
#include <BH1750FVI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xED, 0xFE, 0};
unsigned int listenPort = 10032;                                  // listening port
unsigned int sendPort = 10000;                                    // sending port
unsigned int remPort = 10032;                                     // remote port
IPAddress ip(10, 10, 10, 32);                                       // IP address
IPAddress gateway(10, 10, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(10, 10, 10, 10);                      // remote IP

#define BUFFER_SIZE 65                           // size of UDP input and output buffers (1 byte for NULL)
#define UDP_TX_PACKET_MAX_SIZE BUFFER_SIZE
char inputPacketBuffer[BUFFER_SIZE];
char outputPacketBuffer[BUFFER_SIZE];

EthernetUDP udpRecv;
EthernetUDP udpSend;

#define oneWireCycle 30000                                            // 1-wire scan cycle
#define oneWireDepowerCycle 25000                                     // Power 1-wire X milliseconds after beginning 1-wire scan cycle (optional). Must be shorter than oneWireCycle.
#define oneWireSearchCycle 27000                                     // Search new sensors X milliseconds after beginning 1-wire scan cycle
#define dhtSensorCycle 25000                                       // DHT sensors read cycle (reads only if state changes)
#define lightSensorCycle 1000                                       // light sensors read cycle

#define numOfOneWireBuses 3
byte oneWireBusPins[numOfOneWireBuses] = {2, 3, 4};             // 1-wire buses. Digital pins can be used.
#define maxSensors 10                                             // max 1-wire sensors per bus
#define maxErrors 20                                               // max errors per sensor, if reached, arduino resets
#define resolution 12                                             // DS18B20 resolution
#define ONEWIRE_POWER_PIN 6                                          // OPTIONAL: Power (Vcc) to 1-wire buses. Connect Vcc directly to pin or use transistor switch.
#define ETH_RESET_PIN      7                          // OPTIONAL: Ethernet shield reset pin (deals with power on reset issue of the ethernet shield)
#define DHT_RESET_PIN 8                               // OPTIONAL
#define numOfDhtSensors 3
byte dhtSensorPins[numOfDhtSensors] = {14, 15, 16};             //  DHT sensors (dht). Digital or analog pins can be used. Analog A0 == pin 14
#define typeOfDhtSensors 22                                       // type of DHT sensors (11;21;22)
#define numLightSensors 2                                         // BH1750FVI light sensors (VCC  <-> 5V; GND  <-> GND; SDA  <-> A4/SDA; SCL  <-> A5/SCL)
byte lightSensorPins[numLightSensors] = {17, 9};                       // Pins connected to ADDR
BH1750FVI::eDeviceMode_t DEVICEMODE = BH1750FVI::k_DevModeContHighRes;    // Light sensors resolution

byte boardAddress = 2;
byte ethOn = 0;

typedef struct {
  byte addr[8];
  byte errorCnt;
} address;


byte DS2438count[numOfOneWireBuses] = {};
byte DS18B20count[numOfOneWireBuses] = {};
address sensors[numOfOneWireBuses][maxSensors];
OneWire *oneWireBus[numOfOneWireBuses];
DS2438 *oneWireBus2438[numOfOneWireBuses];
DallasTemperature *oneWireBusDallas[numOfOneWireBuses];
DHT *dhtSensor[numOfDhtSensors];
float dhtTemp[numOfDhtSensors];
float dhtHumid[numOfDhtSensors];
BH1750FVI *lightSensor[numLightSensors];

#define railStr F("ardu")
#define oneWireStr F("1w")
#define detectedStr F("detected")
#define errorStr F("error")
#define dropStr F("dropped")
#define dhtStr F("dht")
#define rstStr F("rst")
#define searchStr F("search")
#define tempStr F("temp")
#define humidStr F("humid")
#define lightStr F("light")

class Timer {
  private:
    unsigned long timestampLastHitMs;
    unsigned long sleepTimeMs;
  public:
    boolean isOver();
    void sleep(unsigned long sleepTimeMs);
};

boolean Timer::isOver() {
  if (millis() - timestampLastHitMs < sleepTimeMs) {
    return false;
  }
  timestampLastHitMs = millis();
  return true;
}

void Timer::sleep(unsigned long sleepTimeMs) {
  this->sleepTimeMs = sleepTimeMs;
  timestampLastHitMs = millis();
}

Timer oneWireTimer;
Timer oneWireDepowerTimer;
Timer oneWireSearchTimer;
Timer dhtSensorTimer;
Timer lightSensorTimer;

void setup() {

  Serial.begin(115200);

  pinMode(ONEWIRE_POWER_PIN, OUTPUT);
  digitalWrite(ONEWIRE_POWER_PIN, LOW);
  delay(500);

#ifdef DHT_RESET_PIN
  pinMode(DHT_RESET_PIN, OUTPUT);
  digitalWrite(DHT_RESET_PIN, LOW);
  delay(500);
  digitalWrite(DHT_RESET_PIN, HIGH);
  delay(500);
#endif

#ifdef ETH_RESET_PIN
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  delay(25);
  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(500);
  pinMode(ETH_RESET_PIN, INPUT);
  delay(2000);
#endif


  mac[5] = (0xED + boardAddress);
  Ethernet.begin(mac, ip);

  udpRecv.begin(listenPort);
  udpSend.begin(sendPort);

  sendMsg(rstStr);

  for (byte i = 0; i < numOfOneWireBuses; i++) {
    oneWireBus[i] = new OneWire(oneWireBusPins[i]);
    oneWireBus2438[i] = new DS2438(oneWireBus[i]);
    oneWireBusDallas[i] = new DallasTemperature(oneWireBus[i]);
    oneWireBusDallas[i]->begin();
    oneWireBusDallas[i]->setWaitForConversion(false);
    DS18B20count[i] = 0;
    DS2438count[i] = 0;
  }

  for (byte i = 0; i < numOfDhtSensors; i++) {
    dhtSensor[i] = new DHT(dhtSensorPins[i], typeOfDhtSensors);
  }
  for (auto& sensor : dhtSensor) {
    sensor->begin();
  }
  readDhtSensors("all");

  for (byte i = 0; i < numLightSensors; i++) {
    lightSensor[i] = new BH1750FVI(DEVICEMODE);
    lightSensor[i]->begin();
    pinMode(lightSensorPins[i], OUTPUT);
    digitalWrite(lightSensorPins[i], HIGH);
  }

  lookUpSensors();

  dhtSensorTimer.sleep(dhtSensorCycle);
  lightSensorTimer.sleep(lightSensorCycle);
  oneWireDepowerTimer.sleep(oneWireDepowerCycle);
  oneWireSearchTimer.sleep(oneWireSearchCycle);
  oneWireTimer.sleep(oneWireCycle);
}

void loop() {

  processOnewire();

  triggerDhtSensors();

  readLightSensors();

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

void lookUpSensors() {
  for (byte i = 0; i < numOfOneWireBuses; i++) {
    byte j = 0;
    address newAddr;
    while ((j <= maxSensors) && (oneWireBus[i]->search(newAddr.addr))) {
      if (OneWire::crc8(newAddr.addr, 7) == newAddr.addr[7]) {

        bool old = false;
        for (byte s = 0; s <= maxSensors; s++) {
          int check;
          check = memcmp(newAddr.addr, sensors[i][s].addr, sizeof(newAddr.addr));
          if (check == 0) {
            old = true;
            break;
          }
        }
        if (old == false) {
          for (byte t = 0; t <= maxSensors; t++) {
            if (sensors[i][t].addr[0] == 0) {
              for (byte x = 0; x < 8; x++) {
                sensors[i][t].addr[x] = newAddr.addr[x];
              }
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(i + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(sensors[i][t].addr);
              sendStr += " ";
              sendStr += detectedStr;
              sendMsg(sendStr);
              if (sensors[i][t].addr[0] == 38) {
                DS2438count[i]++;
              } else {
                DS18B20count[i]++;
              }
              break;
            }
          }
        }
      }
      j++;
    }
    oneWireBus[i]->reset_search();
    oneWireBusDallas[i]->setResolution(resolution);
  }
}

void processOnewire() {
  static byte oneWireState = 0;
  static byte oneWireBusCnt = 0;
  static byte oneWireSensorCnt = 0;

  switch (oneWireState)
  {
    case 0:
      if (!oneWireDepowerTimer.isOver()) {
        return;
      }
      oneWireBusCnt = 0;
      oneWireSensorCnt = 0;
      oneWireState++;
      break;
    case 1:     // Power on buses
#ifdef ONEWIRE_POWER_PIN
      digitalWrite(ONEWIRE_POWER_PIN, LOW);
#endif
      oneWireState++;
      break;
    case 2:     // Check for new sensors
      if (!oneWireSearchTimer.isOver()) {
        return;
      }
      lookUpSensors();
      oneWireState++;
      break;
    case 3:
      if (!oneWireTimer.isOver()) {
        return;
      }
      oneWireState++;
      break;
    case 4:     // Read DS2438
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        if ((oneWireSensorCnt < maxSensors)) {
          if (sensors[oneWireBusCnt][oneWireSensorCnt].addr[0] != 0 && sensors[oneWireBusCnt][oneWireSensorCnt].addr[0] == 38) {
            oneWireBus2438[oneWireBusCnt]->begin();
            oneWireBus2438[oneWireBusCnt]->update(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
            if (!oneWireBus2438[oneWireBusCnt]->isError()) {
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(oneWireBusCnt + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
              sendStr += " ";
              sendStr += String(oneWireBus2438[oneWireBusCnt]->getTemperature(), 2);
              sendStr += " ";
              sendStr += String(oneWireBus2438[oneWireBusCnt]->getVoltage(DS2438_CHA), 2);
              sendStr += " ";
              sendStr += String(oneWireBus2438[oneWireBusCnt]->getVoltage(DS2438_CHB), 2);
              sendMsg(sendStr);
              sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt = 0;
            } else {
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(oneWireBusCnt + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
              sendStr += " ";
              sendStr += errorStr;
              sendStr += " ";
              sendStr += String(sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt + 1, DEC);
              sendMsg(sendStr);
              sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt++;
              if (sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt == maxErrors) {
                String sendStr;
                sendStr += oneWireStr;
                sendStr += String(oneWireBusCnt + 1, DEC);
                sendStr += " ";
                sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
                sendStr += " ";
                sendStr += dropStr;
                sendMsg(sendStr);
                for (byte x = 0; x < 8; x++) {
                  sensors[oneWireBusCnt][oneWireSensorCnt].addr[x] = 0;
                }
                sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt = 0;
              }
            }
          }
          oneWireSensorCnt++;
        } else {
          oneWireSensorCnt = 0;
          oneWireBusCnt++;
        }
      } else {
        oneWireBusCnt = 0;
        oneWireState++;
      }
      break;
    case 5:     // Request temperature DS18B20
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        oneWireBusDallas[oneWireBusCnt]->requestTemperatures();
        oneWireBusCnt++;
      } else {
        oneWireBusCnt = 0;
        oneWireState++;
      }
      break;
    case 6:     // Read DS18B20
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        if ((!oneWireBusDallas[oneWireBusCnt]->isConversionComplete()) && (DS18B20count[oneWireBusCnt] != 0)) {
          return;
        }
        if ((oneWireSensorCnt < maxSensors)) {
          if (sensors[oneWireBusCnt][oneWireSensorCnt].addr[0] != 0 && sensors[oneWireBusCnt][oneWireSensorCnt].addr[0] != 38) {
            float tempC = oneWireBusDallas[oneWireBusCnt]->getTempC(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
            if ((tempC != -127) && (tempC != 85)) {
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(oneWireBusCnt + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
              sendStr += " ";
              sendStr += tempStr;
              sendStr += " ";
              sendStr += String(tempC, 2);
              sendMsg(sendStr);
              sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt = 0;
            } else {
              String sendStr;
              sendStr += oneWireStr;
              sendStr += String(oneWireBusCnt + 1, DEC);
              sendStr += " ";
              sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
              sendStr += " ";
              sendStr += errorStr;
              sendStr += " ";
              sendStr += String(sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt + 1, DEC);
              sendMsg(sendStr);
              sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt++;
              if (sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt == maxErrors) {
                String sendStr;
                sendStr += oneWireStr;
                sendStr += String(oneWireBusCnt + 1, DEC);
                sendStr += " ";
                sendStr += oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt].addr);
                sendStr += " ";
                sendStr += dropStr;
                sendMsg(sendStr);
                for (byte x = 0; x < 8; x++) {
                  sensors[oneWireBusCnt][oneWireSensorCnt].addr[x] = 0;
                }
                sensors[oneWireBusCnt][oneWireSensorCnt].errorCnt = 0;
              }
            }
          }
          oneWireSensorCnt++;
        } else {
          oneWireSensorCnt = 0;
          oneWireBusCnt++;
        }
      } else {
        oneWireBusCnt = 0;
        oneWireState++;
      }
      break;
    case 7:     // Power off buses
      #ifdef ONEWIRE_POWER_PIN
        digitalWrite(ONEWIRE_POWER_PIN, HIGH);        // Power off
        for (byte i = 0; i < numOfOneWireBuses; i++) {
          oneWireBus[i]->depower();
        }
      #endif
      oneWireState++;
      break;
    case 8:     // Sleep all timers
      oneWireDepowerTimer.sleep(oneWireDepowerCycle);
      oneWireSearchTimer.sleep(oneWireSearchCycle);
      oneWireTimer.sleep(oneWireCycle);
      oneWireState = 0;
      break;
  }
}

void triggerDhtSensors() {
  if (!dhtSensorTimer.isOver()) {
    return;
  }
  dhtSensorTimer.sleep(dhtSensorCycle);
  readDhtSensors("all");
}

void readDhtSensors(String request) {
  int n = 0;
  for (byte i = 0; i < numOfDhtSensors; i++) {
    dhtTemp[i] = dhtSensor[i]->readTemperature();
    dhtHumid[i] = dhtSensor[i]->readHumidity();
    if (!isnan(dhtTemp[i]) && !isnan(dhtHumid[i])) {
      ++n;
      if (request == "all") {
        String sendStr;
        sendStr += dhtStr;
        sendStr += String(i + 1, DEC);
        sendStr += " ";
        sendStr += tempStr;
        sendStr += " ";
        sendStr += String(dhtTemp[i], 1);
        sendStr += " ";
        sendStr += humidStr;
        sendStr += " ";
        sendStr += String(dhtHumid[i], 1);
        sendMsg(sendStr);
      } else {
        if (i + 1 == request.toInt()) {
          String sendStr;
          sendStr += dhtStr;
          sendStr += String(i + 1, DEC);
          sendStr += " ";
          sendStr += tempStr;
          sendStr += " ";
          sendStr += String(dhtTemp[i], 1);
          sendStr += " ";
          sendStr += humidStr;
          sendStr += " ";
          sendStr += String(dhtHumid[i], 1);
          sendMsg(sendStr);
        }
      }
    }
  }
  if (n < numOfDhtSensors) {
    delay(100);
    resetFunc();
  }
}

void readLightSensors() {
  if (!lightSensorTimer.isOver()) {
    return;
  }
  lightSensorTimer.sleep(lightSensorCycle);
  for (byte i = 0; i < numLightSensors; i++) {
    digitalWrite(lightSensorPins[i], LOW);
    uint16_t lux = lightSensor[i]->GetLightIntensity();
    if (lux != 54612) {
      String sendStr;
      sendStr += lightStr;
      sendStr += String(i + 1, DEC);
      sendStr += " ";
      sendStr += String(lux);
      sendMsg(sendStr);
    } else {
      String sendStr;
      sendStr += lightStr;
      sendStr += String(i + 1, DEC);
      sendStr += " ";
      sendStr += errorStr;
      sendMsg(sendStr);
    }
    digitalWrite(lightSensorPins[i], HIGH);
  }
}

void sendMsg(const String& sendStr) {
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    udpSend.beginPacket(sendIpAddress, remPort);
    udpSend.print(railStr);
    udpSend.print(String(boardAddress));
    udpSend.print(" ");
    udpSend.print(sendStr);
    udpSend.endPacket();
  }
  dbg(F("Sending packet: "));
  dbg(railStr);
  dbg(String(boardAddress));
  dbg(" ");
  dbgln(sendStr);
}

boolean receivePacket(String *cmd) {
  int packetSize = udpRecv.parsePacket();
  if (packetSize) {
    memset(inputPacketBuffer, 0, sizeof(inputPacketBuffer));
    udpRecv.read(inputPacketBuffer, BUFFER_SIZE);
    *cmd = String(inputPacketBuffer);
    String boardAddressRailStr;
    boardAddressRailStr += railStr;
    boardAddressRailStr += String(boardAddress);
    if (cmd->startsWith(boardAddressRailStr)) {
      cmd->replace(boardAddressRailStr, "");
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
        readDhtSensors("all");
      } else {
        cmd.replace(dhtStr, "");
        readDhtSensors(cmd);
      }
    }
  }
}
