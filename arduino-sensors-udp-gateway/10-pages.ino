/* *******************************************************************
   Pages for Webserver

   sendPage()
   - sends the requested page (incl. 404 error and JSON document)
   - displays main page, renders title and left menu using <div> 
   - calls content functions depending on the number (i.e. URL) of the requested web page
   - also displays buttons for some of the pages
   - in order to save flash memory, some HTML closing tags are omitted, new lines in HTML code are also omitted

   contentInfo(), contentStatus(), contentIp(), contentTcp(), contentRtu(), contentTools()
   - render the content of the requested page

   contentWait()
   - renders the "please wait" message instead of the content, will be forwarded to home page after 5 seconds

   tagInputNumber(), tagLabelDiv(), tagButton(), tagDivClose(), tagSpan()
   - render snippets of repetitive HTML code for <input>, <label>, <div>, <button> and <span> tags

   stringPageName()
   - renders repetitive strings for menus, error counters

   jsonVal()
   - provide JSON value to a corresponding JSON key

   ***************************************************************** */

const byte WEB_OUT_BUFFER_SIZE = 64;  // size of web server write buffer (used by StreamLib)

void sendPage(EthernetClient &client, byte reqPage) {
  char webOutBuffer[WEB_OUT_BUFFER_SIZE];
  ChunkedPrint chunked(client, webOutBuffer, sizeof(webOutBuffer));  // the StreamLib object to replace client print
  /*

use HTTP/1.0 because 
- HOSTS: isn't necessary as the Arduino will only host one server.
- in HTTP/1.1 HOSTS: is mandatory (and necessary if you HOST more than one site on one server) and the server should answer a request with "400 Bad Request" if it is missing
- we don't need to send a Content-Length in HTTP/1.0

An advantage of HTTP 1.1 is
- you could keep the connection alive
 
 */
  if (reqPage == PAGE_ERROR) {
    chunked.print(F("HTTP/1.1 404 Not Found\r\n"
                    "\r\n"
                    "404 Not found"));
    chunked.end();
    return;
  } else if (reqPage == PAGE_DATA) {
    chunked.print(F("HTTP/1.1 200\r\n"
                    "Content-Type: application/json\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"));
    chunked.begin();
    chunked.print(F("{"));
    for (byte i = 0; i < JSON_LAST; i++) {
      if (i) chunked.print(F(","));
      chunked.print(F("\""));
      chunked.print(i);
      chunked.print(F("\":\""));
      jsonVal(chunked, i);
      chunked.print(F("\""));
    }
    chunked.print(F("}"));
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
                  "<meta"));
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
                    w - wrapper (includes m + c)
                    m  - navigation menu (left)
                    c - content of a page
                    r - row inside a content
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
#ifdef ENABLE_ONEWIRE
    case PAGE_OW:
      contentOw(chunked);
      break;
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
    case PAGE_LIGHT:
      contentLight(chunked);
      break;
#endif /* ENABLE_LIGHT */
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
#ifdef ENABLE_ONEWIRE
      || reqPage == PAGE_OW
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
      || reqPage == PAGE_LIGHT
#endif /* ENABLE_LIGHT */
  ) {
    chunked.print(F("<p><div class=r><label><input type=submit value='Save & Apply'></label><input type=reset value=Cancel></div>"));
  }
  chunked.print(F("</form>"));
  tagDivClose(chunked);  // close tags <div class=c> <div class=w>
  chunked.end();         // closing tags not required </body></html>
}


//        System Info
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

//        Sensor Status
void contentStatus(ChunkedPrint &chunked) {
  tagLabelDiv(chunked, F("Sensors List"));
  tagButton(chunked, F("Clear"), ACT_CLEAR_LIST);
  tagDivClose(chunked);
#ifdef ENABLE_ONEWIRE
  tagLabelDiv(chunked, F("DS18x20 Temperature Sensors"));
  chunked.print(F("Pin Id<br>"));
  tagSpan(chunked, JSON_ONEWIRE);
  tagDivClose(chunked);
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
  tagLabelDiv(chunked, F("BH1750 Light Sensors"));
  chunked.print(F("Pin<br>"));
  tagSpan(chunked, JSON_LIGHT);
  tagDivClose(chunked);
#endif /* ENABLE_LIGHT */
}

//            IP Settings
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

//            TCP/UDP Settings
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

//            DS18x20 Settings
void contentOw(ChunkedPrint &chunked) {
  codeInterval(chunked, POST_OW_INT_MIN, 1, 3600, data.config.owIntMin, data.config.owIntMax, F("sec"));
  codeChange(chunked, POST_OW_CHANGE, 0, 50, data.config.owChange, F("&#8530;&deg;C"));
  tagLabelDiv(chunked, F("Max Sensors"));
  chunked.print(OW_MAX_SENSORS);
  tagDivClose(chunked);
  codePins(chunked, POST_OW_BUS, OW_MAX_BUSES, data.config.owPins, F("Bus Pin"));
}

//            BH1750 Settings
void contentLight(ChunkedPrint &chunked) {
  codeInterval(chunked, POST_LIGHT_INT_MIN, 1, 600, data.config.lightIntMin, data.config.lightIntMax, F("sec"));
  codeChange(chunked, POST_LIGHT_CHANGE, 0, 200, data.config.lightChange, F("lux"));
  codePins(chunked, POST_LIGHT_PIN, LIGHT_MAX_SENSORS, data.config.lightPins, F("Address Pin"));
}

//            Tools
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

// Functions providing snippets of repetitive HTML code

void codeInterval(ChunkedPrint &chunked, const byte POST_MIN, const uint16_t min, const uint16_t max, const uint16_t configMin, const uint16_t configMax, const __FlashStringHelper *units) {
  tagLabelDiv(chunked, F("Report Interval"));
  chunked.print(F("Min "));
  tagInputNumber(chunked, POST_MIN, min, configMax, configMin, units);
  tagDivClose(chunked);
  tagLabelDiv(chunked, 0);
  chunked.print(F("Max "));
  tagInputNumber(chunked, POST_MIN + 1, configMin, max, configMax, units);
  tagDivClose(chunked);
}

void codeChange(ChunkedPrint &chunked, const byte POST_CHANGE, const byte min, const byte max, const byte config, const __FlashStringHelper *units) {
  tagLabelDiv(chunked, F("Reportable Change"));
  tagInputNumber(chunked, POST_CHANGE, min, max, config, units);
  tagDivClose(chunked);
}

void codePins(ChunkedPrint &chunked, const byte POST_PIN, const byte MAX, const byte *config, const __FlashStringHelper *label) {
  for (byte i = 0; i < MAX; i++) {
    tagLabelDiv(chunked, label);
    chunked.print(F("<select class=s name="));
    chunked.print(POST_PIN + i, HEX);
    chunked.print(F(">"));
    for (byte j = 0; j <= NUM_DIGITAL_PINS; j++) {
      // SPI pins used by the ethernet shield
      if (j == PIN_SPI_SS || j == PIN_SPI_MOSI || j == PIN_SPI_MISO || j == PIN_SPI_SCK || j == ETH_RESET_PIN) continue;
#ifdef ENABLE_LIGHT
      // Wire (I2C) pins used by light sensors
      if (j == PIN_WIRE_SDA || j == PIN_WIRE_SCL) continue;
#endif /* ENABLE_LIGHT */
      if (isUsed(j) && j != config[i]) continue;
      chunked.print(F("<option value="));
      chunked.print(j);
      if (j == config[i]) chunked.print(F(" selected"));
      chunked.print(F(">"));
      chunked.print(pinName(j));
      chunked.print(F("</option>"));
    }
    chunked.print(F("</select>"));
    tagDivClose(chunked);
  }
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

void tagLabelDiv(ChunkedPrint &chunked, const __FlashStringHelper *label) {
  chunked.print(F("<div class=r>"));
  chunked.print(F("<label> "));
  if (label) {
    chunked.print(label);
    chunked.print(F(":"));
  }
  chunked.print(F("</label><div>"));
}

void tagButton(ChunkedPrint &chunked, const __FlashStringHelper *flashString, byte value) {
  chunked.print(F(" <button name="));
  chunked.print(POST_ACTION, HEX);
  chunked.print(F(" value="));
  chunked.print(value);
  chunked.print(F(">"));
  chunked.print(flashString);
  chunked.print(F("</button>"));
}

void tagDivClose(ChunkedPrint &chunked) {
  chunked.print(F("</div>"
                  "</div>"));  // <div class=r>
}

void tagSpan(ChunkedPrint &chunked, const byte JSONKEY) {
  chunked.print(F("<span id="));
  chunked.print(JSONKEY);
  chunked.print(F(">"));
  jsonVal(chunked, JSONKEY);
  chunked.print(F("</span>"));
}

// Menu item strings
void stringPageName(ChunkedPrint &chunked, byte item) {
  switch (item) {
    case PAGE_INFO:
      chunked.print(F("System Info"));
      break;
    case PAGE_STATUS:
      chunked.print(F("Sensor Status"));
      break;
    case PAGE_IP:
      chunked.print(F("IP Settings"));
      break;
    case PAGE_TCP:
      chunked.print(F("TCP/UDP Settings"));
      break;
#ifdef ENABLE_ONEWIRE
    case PAGE_OW:
      chunked.print(F("DS18x20 Settings"));
      break;
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
    case PAGE_LIGHT:
      chunked.print(F("BH1750 Settings"));
      break;
#endif /* ENABLE_LIGHT */
#ifdef ENABLE_EXTENDED_WEBUI
    case PAGE_TOOLS:
      chunked.print(F("Tools"));
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    default:
      break;
  }
}

void stringStats(ChunkedPrint &chunked, const byte stat) {
  if ((stat & RETRY_MASK) == MAX_ATTEMPTS) {
    chunked.print(F(" Error"));
  } else if (stat & RESULT_SUCCESS_RETRY) {
    chunked.print(F(" OK after retry"));
  } else if (stat & RESULT_SUCCESS) {
    chunked.print(F(" OK"));
  } else {
    chunked.print(F(" Processing"));
  }
}

void jsonVal(ChunkedPrint &chunked, const byte JSONKEY) {
  switch (JSONKEY) {
#ifdef ENABLE_ONEWIRE
    case JSON_ONEWIRE:
      for (byte i = 0; i < OW_MAX_BUSES; i++) {
        bool noSensor = true;
        for (byte j = 0; j < OW_MAX_SENSORS; j++) {
          if (data.sensors.owBus[j] == i) {
            noSensor = false;
            chunked.print(pinName(data.config.owPins[i]));
            chunked.print(F("&emsp; "));
            for (byte k = 0; k < 8; k++) {
              chunked.print(hex(data.sensors.owId[j][k]));
            }
            chunked.print(F(" &emsp;"));
            chunked.print(longTemp(owSensors[j].oldVal));
            chunked.print(F("&deg;C&emsp;"));
            stringStats(chunked, owSensors[j].retryCnt);
            chunked.print(F("<br>"));
          }
        }
        if (noSensor) {
          chunked.print(pinName(data.config.owPins[i]));
          chunked.print(F("&emsp;--<br>"));
        }
      }
      break;
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
    case JSON_LIGHT:
      for (byte i = 0; i < LIGHT_MAX_SENSORS; i++) {
        chunked.print(pinName(data.config.lightPins[i]));
        chunked.print(F("&emsp;"));
        if (data.sensors.lightConnected[i]) {
          chunked.print(lightSensors[i].oldVal);
          chunked.print(F(" lux&emsp;"));
          stringStats(chunked, lightSensors[i].retryCnt);
        } else {
          chunked.print(F("--"));
        }
        chunked.print(F("<br>"));
      }
      break;
#endif /* ENABLE_LIGHT */
    default:
      break;
  }
}

// checks if a pin number is used by other sensors or not
bool isUsed(const byte i) {
  if (i == NUM_DIGITAL_PINS) return false;
#ifdef ENABLE_ONEWIRE
  for (byte j = 0; j < OW_MAX_BUSES; j++) {
    if (i == data.config.owPins[j]) {
      return true;
    }
  }
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
  for (byte j = 0; j < LIGHT_MAX_SENSORS; j++) {
    if (i == data.config.lightPins[j]) {
      return true;
    }
  }
#endif /* ENABLE_LIGHT */
  return false;
}