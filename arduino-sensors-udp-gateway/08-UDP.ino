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

// Keys for JSON elements, used in: 1) JSON documents, 2) ID of span tags, 3) Javascript.
enum COLUMN_t : byte {
  COLUMN_SENSOR,
  COLUMN_PINTYPE,
  COLUMN_PIN,
  COLUMN_I2C,
  COLUMN_ID,
  COLUMN_VALUE,
  COLUMN_UNIT,
  COLUMN_STATUS,
  COLUMN_LAST,  // Must be the very last element in this array
};

void sendUdp(const byte pin, const byte id) {
// Send packets according to settings
#ifdef ENABLE_EXTENDED_WEBUI
  IPAddress remIp = data.config.remoteIp;
  if (data.config.udpBroadcast) remIp = { 255, 255, 255, 255 };
  Udp.beginPacket(remIp, data.config.udpPort);
#else
  Udp.beginPacket({ 255, 255, 255, 255 }, data.config.udpPort);  // enforce UDP broadcast when the setting is not in WebUI
#endif /* ENABLE_EXTENDED_WEBUI */
  Udp.print(F("{"));
  for (byte column = 0; column < COLUMN_LAST; column++) {
    if (cell(column, pin, id)[0] != '\0') {
      if (column > 0) Udp.print(F(","));
      Udp.print(F("\""));
      Udp.print((const __FlashStringHelper *)stringColumn(column));
      Udp.print(F("\":\""));
      Udp.print(cell(column, pin, id));
      Udp.print(F("\""));
    }
  }
  Udp.print(F("}"));
  Udp.endPacket();
}
