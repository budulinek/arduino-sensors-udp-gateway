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
   signals:
     DS18B20 1wire sensor packet:    ardu1 1w2 2864fc3008082 temp 25.44
     DS2438 1wire sensor packet:     ardu1 1w2 2612c3102004f 25.44 1.23 0.12
     Arduino (re)start:              ardu1 rst
     Sensor detected (after start):  ardu1 1w2 2864fc3008082 detected
     Sensor error:                   ardu1 1w2 2864fc3008082 error
   default scan cycles:
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


byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0};
unsigned int sendPort = 10000;                                    // sending port
unsigned int remPort = 10009;                                     // remote port
IPAddress ip(192, 168, 1, 81);                                       // IP address of your Arduino
// IPAddress gateway(192, 168, 1, 80);
// IPAddress subnet(255, 255, 255, 0);
IPAddress sendIpAddress(255, 255, 255, 255);

#define inputPacketBufferSize UDP_TX_PACKET_MAX_SIZE
char inputPacketBuffer[UDP_TX_PACKET_MAX_SIZE];
#define outputPacketBufferSize 100
char outputPacketBuffer[outputPacketBufferSize];

EthernetUDP udpSend;

#define oneWireCycle 30000                                            // 1-wire scan cycle
#define oneWireDepowerCycle 29000                                     // Depower period after reading 1-wire (optional). Must be shorter than oneWireCycle.

#define numOfOneWireBuses 5
int oneWireBusPins[numOfOneWireBuses] = {2, 3, 4, 5, 6};             // 1-wire buses. Digital pins can be used.
int oneWirePowerPin = {8};                                          // Power (Vcc) to 1-wire buses (optional). Connect Vcc directly to pin or use transistor switch.

byte boardAddress = 2;


String boardAddressStr;
String boardAddressRailStr;
String railStr = "ardu";
String oneWireStr = "1w";
String detectedStr = "detected";
String errorStr = "error";
String rstStr = "rst";
String tempStr = "temp";

unsigned long startMillis;

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

#define maxSensors 10
#define resolution 12
int DS2438count[numOfOneWireBuses] = {};
int DS18B20count[numOfOneWireBuses] = {};
byte sensors[numOfOneWireBuses][maxSensors][8];
OneWire *oneWireBus[numOfOneWireBuses];
DS2438 *oneWireBus2438[numOfOneWireBuses];
DallasTemperature *oneWireBusDallas[numOfOneWireBuses];

void setup() {

  Serial.begin(115200);

  boardAddressStr = String(boardAddress);
  boardAddressRailStr = railStr + String(boardAddress);

  if (oneWirePowerPin) {
    pinMode(oneWirePowerPin, OUTPUT);
    digitalWrite(oneWirePowerPin, HIGH);
    startMillis = millis();
    while (millis() - startMillis < 500);
    digitalWrite(oneWirePowerPin, LOW);
    startMillis = millis();
    while (millis() - startMillis < 500);
  }

  mac[5] = (0xED + boardAddress);
  Ethernet.begin(mac, ip);

  udpSend.begin(sendPort);

  for (int i = 0; i < numOfOneWireBuses; i++) {
    oneWireBus[i] = new OneWire(oneWireBusPins[i]);
    oneWireBus2438[i] = new DS2438(oneWireBus[i]);
    oneWireBusDallas[i] = new DallasTemperature(oneWireBus[i]);
    oneWireBusDallas[i]->begin();
    oneWireBusDallas[i]->setWaitForConversion(false);
    DS18B20count[i] = 0;
    DS2438count[i] = 0;
  }

  sendMsg(rstStr);
  lookUpSensors();

}

void loop() {

  processOnewire();

}

void (* resetFunc) (void) = 0;

String oneWireAddressToString(byte addr[]) {
  String s = "";
  for (int i = 0; i < 8; i++) {
    s += String(addr[i], HEX);
  }
  return s;
}

void lookUpSensors() {
  for (int i = 0; i < numOfOneWireBuses; i++) {
    byte j = 0, k = 0, l = 0, m = 0;
    while ((j <= maxSensors) && (oneWireBus[i]->search(sensors[i][j]))) {
      if (!OneWire::crc8(sensors[i][j], 7) != sensors[i][j][7]) {
        sendMsg(oneWireStr + String(i + 1, DEC) + " " + oneWireAddressToString(sensors[i][j]) + " " + detectedStr);
        if (sensors[i][j][0] == 38) {
          k++;
        } else {
          m++;
        }
      }
      j++;
    }
    DS2438count[i] = k;
    DS18B20count[i] = m;
    oneWireBusDallas[i]->setResolution(resolution);
  }
}

void processOnewire() {
  static byte oneWireState = 0;
  static byte oneWireBusCnt = 0;
  static byte oneWireSensorCnt = 0;
  static byte errorCnt = 0;

  switch (oneWireState)
  {
    case 0:
      if (!oneWireTimer.isOver()) {
        return;
      }
      oneWireTimer.sleep(oneWireCycle);
      oneWireDepowerTimer.sleep(oneWireDepowerCycle);
      oneWireBusCnt = 0;
      oneWireSensorCnt = 0;
      errorCnt = 0;
      oneWireState++;
      break;
    case 1:
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        if ((oneWireSensorCnt < maxSensors)) {
          if (sensors[oneWireBusCnt][oneWireSensorCnt][0] != 0 && sensors[oneWireBusCnt][oneWireSensorCnt][0] == 38) {
            oneWireBus2438[oneWireBusCnt]->begin();
            oneWireBus2438[oneWireBusCnt]->update(sensors[oneWireBusCnt][oneWireSensorCnt]);
            if (!oneWireBus2438[oneWireBusCnt]->isError()) {
              sendMsg(oneWireStr + String(oneWireBusCnt + 1, DEC) + " " + oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt]) + " " + String(oneWireBus2438[oneWireBusCnt]->getTemperature(), 2) + " " + String(oneWireBus2438[oneWireBusCnt]->getVoltage(DS2438_CHA), 2) + " " + String(oneWireBus2438[oneWireBusCnt]->getVoltage(DS2438_CHB), 2));
            } else {
              sendMsg(oneWireStr + String(oneWireBusCnt + 1, DEC) + " " + oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt]) + " " + errorStr);
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
    case 2:
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        oneWireBusDallas[oneWireBusCnt]->requestTemperatures();
        oneWireBusCnt++;
      } else {
        oneWireBusCnt = 0;
        oneWireState++;
      }
      break;
    case 3:
      if ((oneWireBusCnt < numOfOneWireBuses)) {
        if ((!oneWireBusDallas[oneWireBusCnt]->isConversionComplete()) && (DS18B20count[oneWireBusCnt] != 0)) {
          return;
        }
        if ((oneWireSensorCnt < maxSensors)) {
          if (sensors[oneWireBusCnt][oneWireSensorCnt][0] != 0 && sensors[oneWireBusCnt][oneWireSensorCnt][0] != 38) {
            if ((errorCnt < 5)) {
              float tempC = oneWireBusDallas[oneWireBusCnt]->getTempC(sensors[oneWireBusCnt][oneWireSensorCnt]);
              if ((tempC != -127)) {
                sendMsg(oneWireStr + String(oneWireBusCnt + 1, DEC) + " " + oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt]) + " " + tempStr + " " + String(tempC, 2));
                errorCnt = 0;
              } else {
                errorCnt++;
                return;
              }
            } else {
              sendMsg(oneWireStr + String(oneWireBusCnt + 1, DEC) + " " + oneWireAddressToString(sensors[oneWireBusCnt][oneWireSensorCnt]) + " " + errorStr);
            }
          }
          errorCnt = 0;
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
    case 4:
      if (oneWirePowerPin) {
        digitalWrite(oneWirePowerPin, HIGH);        // Power off
        for (int i = 0; i < numOfOneWireBuses; i++) {
          oneWireBus[i]->depower();
        }
        oneWireState++;
      } else {
        oneWireState = 0;
      }
      break;
    case 5:
      if (!oneWireDepowerTimer.isOver()) {
        return;
      }
      digitalWrite(oneWirePowerPin, LOW);
      oneWireState = 0;
      break;
  }
}

void sendMsg(String message) {
  message = railStr + boardAddressStr + " " + message;
  message.toCharArray(outputPacketBuffer, outputPacketBufferSize);
  if (Ethernet.hardwareStatus() != EthernetNoHardware) {
    udpSend.beginPacket(sendIpAddress, remPort);
    udpSend.write(outputPacketBuffer, message.length());
    udpSend.endPacket();
  }
  dbgln(message);
}
