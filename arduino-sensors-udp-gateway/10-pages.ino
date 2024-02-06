const byte WEB_OUT_BUFFER_SIZE = 64;  // size of web server write buffer (used by StreamLib)

/**************************************************************************/
/*!
  @brief Sends the requested page (incl. 404 error and JSON document),
  displays main page, renders title and left menu using, calls content functions
  depending on the number (i.e. URL) of the requested web page.
  In order to save flash memory, some HTML closing tags are omitted,
  new lines in HTML code are also omitted.
  @param client Ethernet TCP client
  @param reqPage Requested page number
*/
/**************************************************************************/
void sendPage(EthernetClient &client, byte reqPage) {
  char webOutBuffer[WEB_OUT_BUFFER_SIZE];
  ChunkedPrint chunked(client, webOutBuffer, sizeof(webOutBuffer));  // the StreamLib object to replace client print
  if (reqPage == PAGE_ERROR) {
    chunked.print(F("HTTP/1.1 404 Not Found\r\n"
                    "\r\n"
                    "404 Not found"));
    chunked.end();
    return;
  } else if (reqPage == PAGE_DATA) {
    chunked.print(F("HTTP/1.1 200\r\n"  // An advantage of HTTP 1.1 is that you can keep the connection alive
                    "Content-Type: application/json\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"));
    chunked.begin();
    chunked.print(F("{\"1\":\""));
    jsonVal(chunked);
    chunked.print(F("\"}"));
    chunked.end();
    return;
  }
  chunked.print(F("HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Transfer-Encoding: chunked\r\n"
                  "\r\n"));
  chunked.begin();
  chunked.print(F("<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta charset=utf-8"));
  if (reqPage == PAGE_WAIT) {  // redirect to new IP and web port
    chunked.print(F(" http-equiv=refresh content=5;url=http://"));
    chunked.print(IPAddress(data.config.ip));
    chunked.print(F(":"));
    chunked.print(data.config.webPort);
  }
  chunked.print(F(">"
                  "<title>Arduino Sensors UDP Gateway</title>"
                  "<style>"
                  /*
                  HTML Tags
                    h1 - main title of the page
                    h4 - text in navigation menu and header of page content
                    a - items in left navigation menu
                    label - first cell of a row in content
                  CSS Classes
                    w - wrapper (flex container, includes .m + .c)
                    m  - navigation menu (left)
                    c - content of a page (includes multiple rows .r)
                    r - row (flex container)
                    l - item
                    g - grid (grid container)
                    f - first item in a row in grid container .g
                    i - short input (byte or IP address octet)
                    n - input type=number
                    s - select input with numbers
                    p - inputs disabled by id=o checkbox
                  CSS Ids
                    o - checkbox which disables other checkboxes and inputs
                  */
                  "body,.m{padding:1px;margin:0;font-family:sans-serif}"
                  "h1,h4{padding:10px}"
                  "h1,.m,h4{background:#0067AC;margin:1px}"
                  ".m,.c{height:calc(100vh - 71px)}"
                  ".m{min-width:20%}"
                  ".c{flex-grow:1;overflow-y:auto}"
                  ".w,.r{display:flex}"
                  ".g{display:grid;grid-template-columns:repeat(auto-fit, minmax(1px, auto));column-gap:10px}"
                  ".f{grid-column-start:1;padding-left:20px}"
                  "a,h1,h4{color:white;text-decoration:none}"
                  ".c h4{padding-left:30%;margin-bottom:20px}"
                  ".r{margin:4px}"
                  "label{width:30%;text-align:right;margin-right:2px}"
                  "input,button,select{margin-top:-2px}"  // improve vertical allignment of input, button and select
                  ".s{text-align:right}"
                  ".s>option{direction:rtl}"
                  ".i{text-align:center;width:3ch;color:black}"
                  ".n{width:8ch}"
                  "</style>"
                  "</head>"
                  "<body"));
#ifdef ENABLE_DHCP
  chunked.print(F(" onload=g(document.getElementById('o').checked)>"
                  "<script>function g(h) {var x = document.getElementsByClassName('p');for (var i = 0; i < x.length; i++) {x[i].disabled = h}}</script"));
#endif /* ENABLE_DHCP */
  if (reqPage == PAGE_STATUS) {
    chunked.print(F("><script>"
                    "var a;"
                    "const b=()=>{"
                    "fetch('d.json')"  // Call the fetch function passing the url of the API as a parameter
                    ".then(e=>{return e.json();a=0})"
                    ".then(f=>{for(var i in f){if(document.getElementById(i))document.getElementById(i).innerHTML=f[i];}})"
                    ".catch(()=>{if(!a){alert('Connnection lost');a=1}})"
                    "};"
                    "setInterval(()=>b(),"));
    chunked.print(FETCH_INTERVAL);
    chunked.print(F(");"
                    "</script"));
  }
  chunked.print(F(">"
                  "<h1>Arduino Sensors UDP Gateway</h1>"
                  "<div class=w>"
                  "<div class=m>"));

  // Left Menu
  for (byte i = 1; i < PAGE_WAIT; i++) {  // PAGE_WAIT is the last item in enum
    chunked.print(F("<h4 "));
    if ((i) == reqPage) {
      chunked.print(F(" style=background-color:#FF6600"));
    }
    chunked.print(F("><a href="));
    chunked.print(i);
    chunked.print(F(".htm>"));
    stringPageName(chunked, i);
    chunked.print(F("</a></h4>"));
  }
  chunked.print(F("</div>"  // <div class=w>
                  "<div class=c>"
                  "<h4>"));
  stringPageName(chunked, reqPage);
  chunked.print(F("</h4>"
                  "<form method=post>"));

  //   PLACE FUNCTIONS PROVIDING CONTENT HERE
  switch (reqPage) {
    case PAGE_INFO:
      contentInfo(chunked);
      break;
    case PAGE_STATUS:
      contentStatus(chunked);
      break;
    case PAGE_IP:
      contentIp(chunked);
      break;
    case PAGE_TCP:
      contentTcp(chunked);
      break;
#ifdef ENABLE_DS18X20
    case PAGE_DS18X20:
      contentOw(chunked);
      break;
#endif /* ENABLE_DS18X20 */
#ifdef ENABLE_BH1750
    case PAGE_BH1750:
      contentBH1750(chunked);
      break;
#endif /* ENABLE_BH1750 */
#ifdef ENABLE_MAX31865
    case PAGE_MAX31865:
      contentRtd(chunked);
      break;
#endif /* ENABLE_MAX31865 */
#ifdef ENABLE_EXTENDED_WEBUI
    case PAGE_TOOLS:
      contentTools(chunked);
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    case PAGE_WAIT:
      contentWait(chunked);
      break;
    default:
      break;
  }

  if (reqPage == PAGE_IP || reqPage == PAGE_TCP
#ifdef ENABLE_DS18X20
      || reqPage == PAGE_DS18X20
#endif /* ENABLE_DS18X20 */
#ifdef ENABLE_BH1750
      || reqPage == PAGE_BH1750
#endif /* ENABLE_BH1750 */
#ifdef ENABLE_MAX31865
      || reqPage == PAGE_MAX31865
#endif /* ENABLE_MAX31865 */
  ) {
    chunked.print(F("<p><div class=r><label><input type=submit value='Save & Apply'></label><input type=reset value=Cancel></div>"));
  }
  chunked.print(F("</form>"));
  tagDivClose(chunked);  // close tags <div class=c> <div class=w>
  chunked.end();         // closing tags not required </body></html>
}


/**************************************************************************/
/*!
  @brief System Info

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentInfo(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("SW Version"));
  chunked.print(VERSION[0]);
  chunked.print(F("."));
  chunked.print(VERSION[1]);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Microcontroller"));
  chunked.print(BOARD);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("EEPROM Health"));
  chunked.print(data.eepromWrites);
  chunked.print(F(" Write Cycles"));
  tagDivClose(chunked);
#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("Ethernet Chip"));
  switch (W5100.getChip()) {
    case 51:
      chunked.print(F("W5100"));
      break;
    case 52:
      chunked.print(F("W5200"));
      break;
    case 55:
      chunked.print(F("W5500"));
      break;
    default:  // TODO: add W6100 once it is included in Ethernet library
      chunked.print(F("Unknown"));
      break;
  }
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Ethernet Sockets"));
  chunked.print(maxSockNum);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */

  tagLabelDiv(chunked, F("MAC Address"));
  for (byte i = 0; i < 6; i++) {
    chunked.print(hex(data.mac[i]));
    if (i < 5) chunked.print(F(":"));
  }
  tagDivClose(chunked);

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("DHCP Status"));
  if (!data.config.enableDhcp) {
    chunked.print(F("Disabled"));
  } else if (dhcpSuccess == true) {
    chunked.print(F("Success"));
  } else {
    chunked.print(F("Failed, using fallback static IP"));
  }
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("IP Address"));
  chunked.print(IPAddress(Ethernet.localIP()));
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */
}
/**************************************************************************/
/*!
  @brief P1P2 Status

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentStatus(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Sensors List"));
  tagButton(chunked, F("Clear"), ACT_CLEAR_LIST);
  tagDivClose(chunked);
  chunked.print(F("<div id=1>"));
  jsonVal(chunked);
  chunked.print(F("</div>"));
}

/**************************************************************************/
/*!
  @brief IP Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/

void contentIp(ChunkedPrint &chunked) {

#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("MAC Address"));
  for (byte i = 0; i < 6; i++) {
    tagInputHex(chunked, POST_MAC + i, true, true, data.mac[i]);
    if (i < 5) chunked.print(F(":"));
  }
  tagButton(chunked, F("Randomize"), ACT_MAC);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */

#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("Auto IP"));
  chunked.print(F("<input type=hidden name="));
  chunked.print(POST_DHCP, HEX);
  chunked.print(F(" value=0>"
                  "<input type=checkbox id=o name="));
  chunked.print(POST_DHCP, HEX);
  chunked.print(F(" onclick=g(this.checked) value=1"));
  if (data.config.enableDhcp) chunked.print(F(" checked"));
  chunked.print(F("> DHCP"));
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */

  byte *tempIp;
  for (byte j = 0; j < 3; j++) {
    switch (j) {
      case 0:
        tagLabelDiv(chunked, F("Static IP"));
        tempIp = data.config.ip;
        break;
      case 1:
        tagLabelDiv(chunked, F("Submask"));
        tempIp = data.config.subnet;
        break;
      case 2:
        tagLabelDiv(chunked, F("Gateway"));
        tempIp = data.config.gateway;
        break;
      default:
        break;
    }
    tagInputIp(chunked, POST_IP + (j * 4), tempIp);
    tagDivClose(chunked);
  }
#ifdef ENABLE_DHCP
  tagLabelDiv(chunked, F("DNS Server"));
  tagInputIp(chunked, POST_DNS, data.config.dns);
  tagDivClose(chunked);
#endif /* ENABLE_DHCP */
}

/**************************************************************************/
/*!
  @brief TCP/UDP Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentTcp(ChunkedPrint &chunked) {

#ifdef ENABLE_EXTENDED_WEBUI
  tagLabelDiv(chunked, F("Remote IP"));
  tagInputIp(chunked, POST_REM_IP, data.config.remoteIp);
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Send and Receive UDP"));
  static const __FlashStringHelper *optionsList[] = {
    F("Only to/from Remote IP"),
    F("To/From Any IP (Broadcast)")
  };
  tagSelect(chunked, POST_UDP_BROADCAST, optionsList, 2, data.config.udpBroadcast);
  tagDivClose(chunked);
#endif /* ENABLE_EXTENDED_WEBUI */

  tagLabelDiv(chunked, F("UDP Port"));
  tagInputNumber(chunked, POST_UDP, 1, 65535, data.config.udpPort, F(""));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("WebUI Port"));
  tagInputNumber(chunked, POST_WEB, 1, 65535, data.config.webPort, F(""));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief DS18x20 Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentOw(ChunkedPrint &chunked) {
  codeInterval(chunked, SENSOR_1WIRE, 1, 3600);
  codeChange(chunked, SENSOR_1WIRE, 0, 100, F("⅒°C"));
  tagLabelDiv(chunked, F("Max Sensors"));
  chunked.print(OW_MAX_SENSORS);
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief BH1750 Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentBH1750(ChunkedPrint &chunked) {
  codeInterval(chunked, SENSOR_LIGHT, 1, 600);
  codeChange(chunked, SENSOR_LIGHT, 0, 100, F("lux"));
}

/**************************************************************************/
/*!
  @brief MAX31865 Settings

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentRtd(ChunkedPrint &chunked) {
  codeInterval(chunked, SENSOR_RTD, 1, 3600);
  codeChange(chunked, SENSOR_RTD, 0, 100, F("⅒°C"));
  tagLabelDiv(chunked, F("RTD Wires"));
  tagInputNumber(chunked, POST_RTD_WIRES, 2, 4, data.config.rtdWires, F(""));
  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Nominal RTD Resistance"));
  chunked.print(F("Pt"));

#ifdef ENABLE_EXTENDED_WEBUI
  // Costly alternative with select options
  const uint16_t NOM_RESISTANCES[] = { 100, 500, 1000 };
  chunked.print(F("<select class=s name="));
  chunked.print(POST_RTD_NOMINAL, HEX);
  chunked.print(F(">"));
  for (byte i = 0; i < (sizeof(NOM_RESISTANCES) / 2); i++) {
    chunked.print(F("<option value="));
    chunked.print(NOM_RESISTANCES[i]);
    if (data.config.rtdNominal == NOM_RESISTANCES[i]) chunked.print(F(" selected"));
    chunked.print(F(">"));
    chunked.print(NOM_RESISTANCES[i]);
    chunked.print(F("</option>"));
  }
  chunked.print(F("</select> Ohm"));
#else
  tagInputNumber(chunked, POST_RTD_NOMINAL, 100, 1000, data.config.rtdNominal, F("Ohm"));
#endif /* ENABLE_EXTENDED_WEBUI */

  tagDivClose(chunked);
  tagLabelDiv(chunked, F("Reference Resistor"));
  tagInputNumber(chunked, POST_RTD_REFERENCE, 100, 10000, data.config.rtdReference, F("Ohm"));
  tagDivClose(chunked);
}

/**************************************************************************/
/*!
  @brief Tools

  @param chunked Chunked buffer
*/
/**************************************************************************/
void contentTools(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, 0);
  tagButton(chunked, F("Load Default Settings"), ACT_DEFAULT);
  chunked.print(F(" (static IP: "));
  chunked.print(IPAddress(DEFAULT_CONFIG.ip));
  chunked.print(F(")"));
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  tagButton(chunked, F("Reboot"), ACT_REBOOT);
  tagDivClose(chunked);
}

void contentWait(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, 0);
  chunked.print(F("Reloading. Please wait..."));
  tagDivClose(chunked);
}

void codeInterval(ChunkedPrint &chunked, const byte TYPE, const uint16_t min, const uint16_t max) {
  const __FlashStringHelper *units = F("sec");
  tagLabelDiv(chunked, F("Reporting Interval"));
  chunked.print(F("Min "));
  tagInputNumber(chunked, POST_INTERVAL_MIN + TYPE, min, data.config.intervalMax[TYPE], data.config.intervalMin[TYPE], units);
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  chunked.print(F("Max "));
  tagInputNumber(chunked, POST_INTERVAL_MAX + TYPE, data.config.intervalMin[TYPE], max, data.config.intervalMax[TYPE], units);
  tagDivClose(chunked);
}

void codeChange(ChunkedPrint &chunked, const byte TYPE, const byte min, const byte max, const __FlashStringHelper *units) {
  tagLabelDiv(chunked, F("Reportable Change"));
  tagInputNumber(chunked, POST_CHANGE + TYPE, min, max, data.config.change[TYPE], units);
  tagDivClose(chunked);
}

void tagSelect(ChunkedPrint &chunked, const byte name, const __FlashStringHelper *options[], const byte numOptions, byte value) {
  chunked.print(F("<select class=s name="));
  chunked.print(name, HEX);
  chunked.print(F(">"));
  for (byte i = 0; i < numOptions; i++) {
    chunked.print(F("<option value="));
    chunked.print(i);
    if (value == i) chunked.print(F(" selected"));
    chunked.print(F(">"));
    chunked.print(options[i]);
    chunked.print(F("</option>"));
  }
  chunked.print(F("</select>"));
}

/**************************************************************************/
/*!
  @brief <input type=number>

  @param chunked Chunked buffer
  @param name Name POST_
  @param min Minimum value
  @param max Maximum value
  @param value Current value
  @param units Units (string)
*/
/**************************************************************************/
void tagInputNumber(ChunkedPrint &chunked, const byte name, const byte min, uint16_t max, uint16_t value, const __FlashStringHelper *units) {
  chunked.print(F("<input class='s n' required type=number name="));
  chunked.print(name, HEX);
  chunked.print(F(" min="));
  chunked.print(min);
  chunked.print(F(" max="));
  chunked.print(max);
  chunked.print(F(" value="));
  chunked.print(value);
  chunked.print(F("> ("));
  chunked.print(min);
  chunked.print(F("~"));
  chunked.print(max);
  chunked.print(F(") "));
  chunked.print(units);
}

/**************************************************************************/
/*!
  @brief <input>
  IP address (4 elements)

  @param chunked Chunked buffer
  @param name Name POST_
  @param ip IP address from data.config
*/
/**************************************************************************/
void tagInputIp(ChunkedPrint &chunked, const byte name, const byte ip[]) {
  for (byte i = 0; i < 4; i++) {
    chunked.print(F("<input name="));
    chunked.print(name + i, HEX);
    chunked.print(F(" class='p i' required maxlength=3 pattern='^(&bsol;d{1,2}|1&bsol;d&bsol;d|2[0-4]&bsol;d|25[0-5])$' value="));
    chunked.print(ip[i]);
    chunked.print(F(">"));
    if (i < 3) chunked.print(F("."));
  }
}

/**************************************************************************/
/*!
  @brief <input>
  HEX string (2 chars)

  @param chunked Chunked buffer
  @param name Name POST_
  @param required True if input is required
  @param printVal True if value is shown
  @param value Value
*/
/**************************************************************************/
void tagInputHex(ChunkedPrint &chunked, const byte name, const bool required, const bool printVal, const byte value) {
  chunked.print(F("<input name="));
  chunked.print(name, HEX);
  if (required) {
    chunked.print(F(" required"));
  }
  chunked.print(F(" minlength=2 maxlength=2 class=i pattern='[a-fA-F&bsol;d]+' value='"));
  if (printVal) {
    chunked.print(hex(value));
  }
  chunked.print(F("'>"));
}

/**************************************************************************/
/*!
  @brief <label><div>

  @param chunked Chunked buffer
  @param label Label string
*/
/**************************************************************************/
void tagLabelDiv(ChunkedPrint &chunked, const __FlashStringHelper *label) {
  chunked.print(F("<div class=r>"));
  chunked.print(F("<label> "));
  if (label) {
    chunked.print(label);
    chunked.print(F(":"));
  }
  chunked.print(F("</label><div>"));
}

/**************************************************************************/
/*!
  @brief <button>

  @param chunked Chunked buffer
  @param flashString Button string
  @param value Value to be sent via POST
*/
/**************************************************************************/
void tagButton(ChunkedPrint &chunked, const __FlashStringHelper *flashString, byte value) {
  chunked.print(F(" <button name="));
  chunked.print(POST_ACTION, HEX);
  chunked.print(F(" value="));
  chunked.print(value);
  chunked.print(F(">"));
  chunked.print(flashString);
  chunked.print(F("</button>"));
}

/**************************************************************************/
/*!
  @brief </div>

  @param chunked Chunked buffer
*/
/**************************************************************************/
void tagDivClose(ChunkedPrint &chunked) {
  chunked.print(F("</div>"
                  "</div>"));  // <div class=r>
}

/**************************************************************************/
/*!
  @brief Menu item strings

  @param chunked Chunked buffer
  @param item Page number
*/
/**************************************************************************/
void stringPageName(ChunkedPrint &chunked, byte item) {
  switch (item) {
      // #ifdef ENABLE_EXTENDED_WEBUI
    case PAGE_INFO:
      chunked.print(F("System Info"));
      break;
      // #endif /* ENABLE_EXTENDED_WEBUI */
    case PAGE_STATUS:
      chunked.print(F("Sensor Status"));
      break;
    case PAGE_IP:
      chunked.print(F("IP Settings"));
      break;
    case PAGE_TCP:
      chunked.print(F("TCP/UDP Settings"));
      break;
#ifdef ENABLE_DS18X20
    case PAGE_DS18X20:
      chunked.print(F("DS18x20 Settings"));
      break;
#endif /* ENABLE_DS18X20 */
#ifdef ENABLE_BH1750
    case PAGE_BH1750:
      chunked.print(F("BH1750 Settings"));
      break;
#endif /* ENABLE_BH1750 */
#ifdef ENABLE_MAX31865
    case PAGE_MAX31865:
      chunked.print(F("MAX31865 Settings"));
      break;
#endif /* ENABLE_MAX31865 */
#ifdef ENABLE_EXTENDED_WEBUI
    case PAGE_TOOLS:
      chunked.print(F("Tools"));
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    default:
      break;
  }
}

/**************************************************************************/
/*!
  @brief Provide JSON value to the JSON key (there is only one JSON key). The value is printed
  in <span> and in JSON document fetched on the background.
  @param chunked Chunked buffer
*/
/**************************************************************************/
void jsonVal(ChunkedPrint &chunked) {
  chunked.print(F("<div class=g>"));
  for (int8_t pin = -1; pin < NUM_DIGITAL_PINS; pin++) {  // use pin == NUM_DIGITAL_PINS (last loop) for I2C sensors not attached to particular pin
    for (byte id = 0; id < OW_MAX_SENSORS; id++) {
      if (pin < 0 && id > 0) continue;
      if (pin >= 0
          && ((data.pinSensor[pin] != SENSOR_1WIRE && id > 0)
              || (data.pinSensor[pin] == SENSOR_1WIRE && data.ow.pins[id] != pin)
              || isReserved(pin)                         // do not show reserved pins
              || (data.pinSensor[pin] == SENSOR_NONE)))  // do not show available pins
        continue;
      for (byte column = 0; column < COLUMN_LAST; column++) {
#ifndef SHOW_COLUMN_PIN
        if (column == COLUMN_PINTYPE || column == COLUMN_PIN) continue;
#endif
#ifndef SHOW_COLUMN_I2C
        if (column == COLUMN_I2C) continue;
#endif
#ifndef SHOW_COLUMN_ID
        if (column == COLUMN_ID) continue;
#endif
        if (column == COLUMN_SENSOR) {  // Sensor column is always the first one
          chunked.print(F("<div  class=f>"));
        } else {
          chunked.print(F("<div>"));
        }
        if (pin == -1) {  // first row (header with column labels)
          chunked.print((const __FlashStringHelper *)stringColumn(column));
        } else {
          chunked.print(cell(column, pin, id));
        }
        chunked.print(F("</div>"));
      }
    }
  }
  chunked.print(F("</div>"));
}

char __cellbuffer[17];
char *cell(const byte column, const byte pin, const byte id) {
  memset(__cellbuffer, '\0', sizeof(__cellbuffer));
  char *buffer = __cellbuffer;
  const __FlashStringHelper *pData = 0;
  byte sensor = data.pinSensor[pin];
  switch (column) {
    case COLUMN_SENSOR:
      switch (sensor) {
        case SENSOR_1WIRE: pData = F("DS18x20"); break;
        case SENSOR_LIGHT: pData = F("BH1750"); break;
        case SENSOR_RTD: pData = F("MAX31865"); break;
        default: break;
      }
      break;
    case COLUMN_PINTYPE:
      switch (sensor) {
        case SENSOR_1WIRE: pData = F("1WIRE"); break;
        case SENSOR_LIGHT: pData = F("ADDR"); break;
        case SENSOR_RTD: pData = F("CS"); break;
        default: break;
      }
      break;
    case COLUMN_PIN:
      if (pin >= PIN_A0 && pin < PIN_A0 + NUM_ANALOG_INPUTS) {
        *buffer = 'A';
        itoa(pin - PIN_A0, buffer + 1, 10);
      } else {
        itoa(pin, buffer, 10);
      }
      break;
    case COLUMN_I2C:
      if (sensor == SENSOR_LIGHT) {
        // strcat_P(buffer, PSTR("0x"));
        strcat(buffer, "0x");
        strcat(buffer, hex(BH1750_ADDRESS_H));
      }
      break;
    case COLUMN_ID:
      if (sensor == SENSOR_1WIRE) {
        for (byte k = 0; k < 8; k++) {
          strcat(buffer, hex(data.ow.ids[id][k]));
        }
      }
      break;
    case COLUMN_VALUE:
      if (sensor == SENSOR_LIGHT) {
        ltoa(pins[pin].oldVal, buffer, 10);
      } else {
        int32_t val = 0;
#ifdef ENABLE_DS18X20
        if (sensor == SENSOR_1WIRE) val = owSensors[id].oldVal;
#endif /* ENABLE_DS18X20 */
        if (sensor == SENSOR_RTD) val = pins[pin].oldVal;
        char *end;
        uint16_t decimal = abs(val % 1000);
        itoa((val / 1000), buffer, 10);
        end = buffer + strlen(buffer);
        *end++ = '.';
        if (decimal < 100) *end++ = '0';
        if (decimal < 10) *end++ = '0';
        itoa(decimal, end, 10);
      }
      break;
    case COLUMN_UNIT:
      switch (sensor) {
        case SENSOR_1WIRE:
        case SENSOR_RTD: pData = F("°C"); break;
        case SENSOR_LIGHT: pData = F("lux"); break;
        default: break;
      }
      break;
    case COLUMN_STATUS:
      {
        byte stat = pins[pin].retryCnt;
#ifdef ENABLE_DS18X20
        if (sensor == SENSOR_1WIRE) {
          stat = owSensors[id].retryCnt;
        }
#endif /* ENABLE_DS18X20 */
        switch (stat) {
          case MAX_ATTEMPTS: pData = F("Error"); break;
          case RESULT_SUCCESS_RETRY: pData = F("OKish"); break;
          case RESULT_SUCCESS: pData = F("OK"); break;
          default: pData = F("Wait"); break;
        }
      }
      break;
    default:
      break;
  }
  if (pData) {
    byte index = 0;
    PGM_P ptr = (PGM_P)pData;
    while ((buffer[index] = pgm_read_byte_near(ptr + index)) != '\0') ++index;
  }
  return buffer;
}


const char *stringColumn(byte column) {
  switch (column) {
    case COLUMN_SENSOR: return PSTR("Sensor");
    case COLUMN_PINTYPE: return PSTR("PinType");
    case COLUMN_PIN: return PSTR("Pin");
    case COLUMN_I2C: return PSTR("I2CAddress");
    case COLUMN_ID: return PSTR("1WireID");
    case COLUMN_VALUE: return PSTR("Value");
    case COLUMN_UNIT: return PSTR("Unit");
    case COLUMN_STATUS: return PSTR("Status");
    default: return PSTR("");
  }
}
