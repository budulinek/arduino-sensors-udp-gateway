/* *******************************************************************
   Webserver functions

   recvWeb()
   - receives GET requests for web pages
   - receives POST data from web forms
   - calls processPost
   - sends web pages, for simplicity, all web pages should are numbered (1.htm, 2.htm, ...), the page number is passed to sendPage() function
   - executes actions (such as ethernet restart, reboot) during "please wait" web page

   processPost()
   - processes POST data from forms and buttons
   - updates data.config (in RAM)
   - saves config into EEPROM
   - executes actions which do not require webserver restart

   strToByte(), hex()
   - helper functions for parsing and writing hex data

   ***************************************************************** */

const byte URI_SIZE = 24;   // a smaller buffer for uri
const byte POST_SIZE = 24;  // a smaller buffer for single post parameter + key

// Actions that need to be taken after saving configuration.
enum action_type : byte {
  ACT_NONE,
  ACT_DEFAULT,     // Load default factory settings (but keep MAC address)
  ACT_MAC,         // Generate new random MAC
  ACT_REBOOT,      // Reboot the microcontroller
  ACT_RESET_ETH,   // Ethernet reset
  ACT_CLEAR_LIST,  // Refresh sensors list
  ACT_WEB          // Restart webserver
};
enum action_type action;

// Pages served by the webserver. Order of elements defines the order in the left menu of the web UI.
// URL of the page (*.htm) contains number corresponding to its position in this array.
// The following enum array can have a maximum of 10 elements (incl. PAGE_NONE and PAGE_WAIT)
enum page : byte {
  PAGE_ERROR,  // 404 Error
  PAGE_INFO,
  PAGE_STATUS,
  PAGE_IP,
  PAGE_TCP,
#ifdef ENABLE_ONEWIRE
  PAGE_OW,
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
  PAGE_LIGHT,
#endif /* ENABLE_LIGHT */
#ifdef ENABLE_EXTENDED_WEBUI
  PAGE_TOOLS,
#endif        /* ENABLE_EXTENDED_WEBUI */
  PAGE_WAIT,  // page with "Reloading. Please wait..." message.
  PAGE_DATA,  // data.json
};

// Keys for POST parameters, used in web forms and processed by processPost() function.
// Using enum ensures unique identification of each POST parameter key and consistence across functions.
// In HTML code, each element will apear as number corresponding to its position in this array.
enum post_key : byte {
  POST_NONE,  // reserved for NULL
  POST_DHCP,  // enable DHCP
  POST_MAC,
  POST_MAC_1,
  POST_MAC_2,
  POST_MAC_3,
  POST_MAC_4,
  POST_MAC_5,
  POST_IP,
  POST_IP_1,
  POST_IP_2,
  POST_IP_3,  // IP address         || Each part of an IP address has its own POST parameter.     ||
  POST_SUBNET,
  POST_SUBNET_1,
  POST_SUBNET_2,
  POST_SUBNET_3,  // subnet             || Because HTML code for IP, subnet, gateway and DNS          ||
  POST_GATEWAY,
  POST_GATEWAY_1,
  POST_GATEWAY_2,
  POST_GATEWAY_3,  // gateway            || is generated through one (nested) for-loop,                ||
  POST_DNS,
  POST_DNS_1,
  POST_DNS_2,
  POST_DNS_3,  // DNS                || all these 16 enum elements must be listed in succession!!  ||
  POST_UDP_BROADCAST,
  POST_REM_IP,
  POST_REM_IP_1,
  POST_REM_IP_2,
  POST_REM_IP_3,    // remote IP
  POST_UDP,         // UDP port
  POST_WEB,         // web UI port
  POST_OW_INT_MIN,  // 1-wire sensors read cycle
  POST_OW_INT_MAX,  // 1-wire sensors read cycle
  POST_OW_CHANGE,
  POST_OW_BUS,  // First 1-wire bus
  POST_OW_BUS_L = POST_OW_BUS + OW_MAX_BUSES - 1,
  POST_LIGHT_INT_MIN,  // 1-wire sensors read cycle
  POST_LIGHT_INT_MAX,  // 1-wire sensors read cycle
  POST_LIGHT_CHANGE,
  POST_LIGHT_PIN,  // First BH1750 address pin
  POST_LIGHT_PIN_L = POST_LIGHT_PIN + LIGHT_MAX_SENSORS - 1,
  POST_ACTION,  // actions on Tools page
  POST_LAST,    // must be last
};

// Keys for JSON elements, used in: 1) JSON documents, 2) ID of span tags, 3) Javascript.
enum JSON_type : byte {
  JSON_NONE,     // reserved for NULL
  JSON_ONEWIRE,  // List of 1-wire sensors
  JSON_LIGHT,    // List of light sensors (BH1750)
  JSON_LAST,     // Must be the very last element in this array
};


void recvWeb(EthernetClient &client) {
  char uri[URI_SIZE];  // the requested page
  memset(uri, 0, sizeof(uri));
  while (client.available()) {        // start reading the first line which should look like: GET /uri HTTP/1.1
    if (client.read() == ' ') break;  // find space before /uri
  }
  byte len = 0;
  while (client.available() && len < sizeof(uri) - 1) {
    char c = client.read();  // parse uri
    if (c == ' ') break;     // find space after /uri
    uri[len] = c;
    len++;
  }
  while (client.available()) {
    if (client.read() == '\r')
      if (client.read() == '\n')
        if (client.read() == '\r')
          if (client.read() == '\n')
            break;  // find 2 end of lines between header and body
  }
  if (client.available()) {
    processPost(client);  // parse post parameters
  }

  // Get the requested page from URI
  byte reqPage = PAGE_ERROR;  // requested page, 404 error is a default
  if (uri[0] == '/') {
    if (uri[1] == '\0')
      reqPage = 1;  // homepage is a first page (System Info or Sensor Status)
    else if (!strcmp(uri + 2, ".htm")) {
      reqPage = byte(uri[1] - 48);  // Convert single ASCII char to byte
      if (reqPage >= PAGE_WAIT) reqPage = PAGE_ERROR;
    } else if (!strcmp(uri, "/d.json")) {
      reqPage = PAGE_DATA;
    }
  }
  // Actions that require "please wait" page
  if (action == ACT_WEB || action == ACT_MAC || action == ACT_RESET_ETH || action == ACT_REBOOT || action == ACT_DEFAULT) {
    reqPage = PAGE_WAIT;
  }
  // Send page
  sendPage(client, reqPage);

  // Do all actions before the "please wait" redirects (5s delay at the moment)
  if (reqPage == PAGE_WAIT) {
    switch (action) {
      case ACT_WEB:
        for (byte s = 0; s < maxSockNum; s++) {
          disconSocket(s);
        }
        webServer = EthernetServer(data.config.webPort);
        break;
      case ACT_MAC:
      case ACT_RESET_ETH:
        for (byte s = 0; s < maxSockNum; s++) {
          // close all TCP and UDP sockets
          disconSocket(s);
        }
        startEthernet();
        break;
#ifdef ENABLE_EXTENDED_WEBUI
      case ACT_REBOOT:
      case ACT_DEFAULT:
        resetFunc();
        break;
#endif /* ENABLE_EXTENDED_WEBUI */
      default:
        break;
    }
  }
  action = ACT_NONE;
}

// This function stores POST parameter values in data.config.
// Most changes are saved and applied immediatelly, some changes (IP settings, web server port, reboot) are saved but applied later after "please wait" page is sent.
void processPost(EthernetClient &client) {
  while (client.available()) {
    char post[POST_SIZE];
    byte len = 0;
    while (client.available() && len < sizeof(post) - 1) {
      char c = client.read();
      if (c == '&') break;
      post[len] = c;
      len++;
    }
    post[len] = '\0';
    char *paramKey = post;
    char *paramValue = post;
    while (*paramValue) {
      if (*paramValue == '=') {
        paramValue++;
        break;
      }
      paramValue++;
    }
    if (*paramValue == '\0')
      continue;  // do not process POST parameter if there is no parameter value
    byte paramKeyByte = strToByte(paramKey);
    uint16_t paramValueUint = atol(paramValue);
    switch (paramKeyByte) {
      case POST_NONE:  // reserved, because atoi / atol returns NULL in case of error
        break;
#ifdef ENABLE_DHCP
      case POST_DHCP:
        {
          data.config.enableDhcp = byte(paramValueUint);
        }
        break;
      case POST_DNS ... POST_DNS_3:
        {
          data.config.dns[paramKeyByte - POST_DNS] = byte(paramValueUint);
        }
        break;
#endif /* ENABLE_DHCP */
      case POST_IP ... POST_IP_3:
        {
          action = ACT_RESET_ETH;  // this RESET_ETH is triggered when the user changes anything on the "IP Settings" page.
                                   // No need to trigger RESET_ETH for other cases (POST_SUBNET, POST_GATEWAY etc.)
                                   // if "Randomize" button is pressed, action is set to ACT_MAC
          data.config.ip[paramKeyByte - POST_IP] = byte(paramValueUint);
        }
        break;
      case POST_SUBNET ... POST_SUBNET_3:
        {
          data.config.subnet[paramKeyByte - POST_SUBNET] = byte(paramValueUint);
        }
        break;
      case POST_GATEWAY ... POST_GATEWAY_3:
        {
          data.config.gateway[paramKeyByte - POST_GATEWAY] = byte(paramValueUint);
        }
        break;
#ifdef ENABLE_EXTENDED_WEBUI
      case POST_MAC ... POST_MAC_5:
        {
          data.mac[paramKeyByte - POST_MAC] = strToByte(paramValue);
        }
        break;
      case POST_REM_IP ... POST_REM_IP_3:
        {
          data.config.remoteIp[paramKeyByte - POST_REM_IP] = byte(paramValueUint);
        }
        break;
      case POST_UDP_BROADCAST:
        data.config.udpBroadcast = byte(paramValueUint);
        break;
#endif /* ENABLE_EXTENDED_WEBUI */
      case POST_UDP:
        {
          data.config.udpPort = paramValueUint;
          Udp.stop();
          Udp.begin(data.config.udpPort);
        }
        break;
      case POST_WEB:
        {
          if (paramValueUint != data.config.webPort) {  // continue only if the value changed
            data.config.webPort = paramValueUint;
            action = ACT_WEB;
          }
        }
        break;
#ifdef ENABLE_ONEWIRE
      case POST_OW_INT_MIN:
        {
          data.config.owIntMin = paramValueUint;
        }
        break;
      case POST_OW_INT_MAX:
        data.config.owIntMax = paramValueUint;
        break;
      case POST_OW_CHANGE:
        data.config.owChange = byte(paramValueUint);
        break;
      case POST_OW_BUS ... POST_OW_BUS_L:
        {
          owBuses[paramKeyByte - POST_OW_BUS].minTimer.sleep(0);  // reset timers whenever the form on the BH1750 Settings page changes
          owBuses[paramKeyByte - POST_OW_BUS].maxTimer.sleep(0);
          byte newVal = byte(paramValueUint);
          if (data.config.owPins[paramKeyByte - POST_OW_BUS] != newVal && isUsed(newVal) == false) {
            data.config.owPins[paramKeyByte - POST_OW_BUS] = newVal;
            clearOneWire(paramKeyByte - POST_OW_BUS);
            startOneWire(paramKeyByte - POST_OW_BUS);
          }
        }
        break;
#endif /* ENABLE_ONEWIRE */

#ifdef ENABLE_LIGHT
      case POST_LIGHT_INT_MIN:
        {
          data.config.lightIntMin = paramValueUint;
        }
        break;
      case POST_LIGHT_INT_MAX:
        data.config.lightIntMax = paramValueUint;
        break;
      case POST_LIGHT_CHANGE:
        data.config.lightChange = byte(paramValueUint);
        break;
      case POST_LIGHT_PIN ... POST_LIGHT_PIN_L:
        {
          light[paramKeyByte - POST_LIGHT_PIN].minTimer.sleep(0);  // reset timers whenever the form on the BH1750 Settings page changes
          light[paramKeyByte - POST_LIGHT_PIN].maxTimer.sleep(0);
          byte newVal = byte(paramValueUint);
          if (data.config.lightPins[paramKeyByte - POST_LIGHT_PIN] != newVal && isUsed(newVal) == false) {
            data.config.lightPins[paramKeyByte - POST_LIGHT_PIN] = newVal;
            clearLight(paramKeyByte - POST_LIGHT_PIN);
            startLight(paramKeyByte - POST_LIGHT_PIN);
          }
        }
        break;
#endif /* ENABLE_LIGHT */

      case POST_ACTION:
        action = action_type(paramValueUint);
        break;
      default:
        break;
    }
  }
  switch (action) {
    case ACT_CLEAR_LIST:
      resetSensors();
      break;
#ifdef ENABLE_EXTENDED_WEBUI
    case ACT_DEFAULT:
      data.config = DEFAULT_CONFIG;
      // set all pins to "--"
      memset(data.config.owPins, NUM_DIGITAL_PINS, sizeof(data.config.owPins));
      memset(data.config.lightPins, NUM_DIGITAL_PINS, sizeof(data.config.lightPins));
      resetSensors();
      break;
    case ACT_MAC:
      generateMac();
      break;
#endif /* ENABLE_EXTENDED_WEBUI */
    default:
      break;
  }
  // new parameter values received, save them to EEPROM
  updateEeprom();  // it is safe to call, only changed values (and changed error and data counters) are updated
}

void resetSensors() {
#ifdef ENABLE_ONEWIRE
  for (byte i = 0; i < OW_MAX_BUSES; i++) {
    clearOneWire(i);
  }
#endif /* ENABLE_ONEWIRE */
#ifdef ENABLE_LIGHT
  for (byte i = 0; i < LIGHT_MAX_SENSORS; i++) {
    clearLight(i);
  }
#endif /* ENABLE_LIGHT */
}

// takes 2 chars, 1 char + null byte or 1 null byte
byte strToByte(const char myStr[]) {
  if (!myStr) return 0;
  byte x = 0;
  for (byte i = 0; i < 2; i++) {
    char c = myStr[i];
    if (c >= '0' && c <= '9') {
      x *= 16;
      x += c - '0';
    } else if (c >= 'A' && c <= 'F') {
      x *= 16;
      x += (c - 'A') + 10;
    } else if (c >= 'a' && c <= 'f') {
      x *= 16;
      x += (c - 'a') + 10;
    }
  }
  return x;
}

// from https://github.com/RobTillaart/printHelpers
char __printbuffer[8];
char *hex(byte val) {
  memset(__printbuffer, 0, sizeof(__printbuffer));
  char *buffer = __printbuffer;
  byte digits = 2;
  buffer[digits] = '\0';
  while (digits > 0) {
    byte v = val & 0x0F;
    val >>= 4;
    digits--;
    buffer[digits] = (v < 10) ? '0' + v : ('A' - 10) + v;
  }
  return buffer;
}

// converts temp (signed long) to temperature
char *longTemp(int32_t val) {
  memset(__printbuffer, 0, sizeof(__printbuffer));
  char *buffer = __printbuffer;
  char *end;
  uint16_t decimal = abs(val % 1000);
  itoa((val / 1000), buffer, 10);
  end = buffer + strlen(buffer);
  *end++ = '.';
  if (decimal < 100) *end++ = '0';
  if (decimal < 10) *end++ = '0';
  itoa(decimal, end, 10);
  return buffer;
}

// converts pin number
char *pinName(const byte i) {
  memset(__printbuffer, 0, sizeof(__printbuffer));
  char *buffer = __printbuffer;
  if (i == NUM_DIGITAL_PINS) {
    memset(buffer, '-', 2);
  } else if (i >= PIN_A0 && i < PIN_A0 + NUM_ANALOG_INPUTS) {
    *buffer = 'A';
    itoa(i - PIN_A0, buffer + 1, 10);
  } else {
    itoa(i, buffer, 10);
  }
  return buffer;
}