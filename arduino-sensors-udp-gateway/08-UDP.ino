/* *******************************************************************
   UDP functions

   recvUdp()
   - receives P1P2 command via UDP
   - calls checkCommand

   checkCommand()
   - checks P1P2 command
   - checks availability of queue
   - stores commands into queue or returns an error

***************************************************************** */

void sendUdp(const byte TYPE, const byte sensor) {
// Send packets according to settings
#ifdef ENABLE_EXTENDED_WEBUI
  IPAddress remIp = data.config.remoteIp;
  if (data.config.udpBroadcast) remIp = { 255, 255, 255, 255 };
  Udp.beginPacket(remIp, data.config.udpPort);
#else
  Udp.beginPacket({ 255, 255, 255, 255 }, data.config.udpPort);  // enforce UDP broadcast when the setting is not in WebUI
#endif /* ENABLE_EXTENDED_WEBUI */
  Udp.print(F("{\"Type\":\""));
  switch (TYPE) {
#ifdef ENABLE_ONEWIRE
    case TYPE_1WIRE:
      Udp.print(F("DS18x20\",\"Pin\":\""));
      Udp.print(pinName(data.config.owPins[data.sensors.owBus[sensor]]));
      Udp.print(F("\",\"Id\":\""));
      for (byte j = 0; j < 8; j++) {
        Udp.print(hex(data.sensors.owId[sensor][j]));
      }
      Udp.print(F("\",\"Temperature\":"));
      Udp.print(longTemp(owSensors[sensor].oldVal));
      break;
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
    case TYPE_LIGHT:
      Udp.print(F("BH1750\",\"Pin\":\""));
      Udp.print(pinName(data.config.lightPins[sensor]));
      Udp.print(F("\",\"Illuminance\":"));
      Udp.print(lightSensors[sensor].oldVal);
      break;
#endif /* ENABLE_LIGHT */
    default:
      break;
  }
  Udp.print(F("}"));
  Udp.endPacket();
}
