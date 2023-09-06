/*  Advanced settings, extra functions and default config 
*/

/****** FUNCTIONALITY ******/

#define ENABLE_ONEWIRE  // Enable DS18x20 1-wire sensors \
                        // Requires: OneWireNg https://github.com/pstolarz/OneWireNg
#define ENABLE_LIGHT    // Enable BH1750 ambient light sensors \
                        // Requires: BH1750FVI https://github.com/PeterEmbedded/BH1750FVI

// #define ENABLE_EXTENDED_WEBUI  // Enable extended Web UI (additional items and settings), connsumes FLASH memory
// uncomment ENABLE_EXTENDED_WEBUI if you use only one type of sensors or if you have a board with large FLASH memory (Arduino Mega)

// #define ENABLE_DHCP  // Enable DHCP (Auto IP settings), consumes a lot of FLASH memory


/****** DEFAULT CONFIGURATION ******/
/*
  After boot, Arduino loads user settings stored in EEPROM, even if you flash new program to it!
  Arduino loads factory defaults if:
  1) User clicks "Load default settings" in WebUI (factory reset configuration, keeps MAC)
  2) VERSION_MAJOR changes (factory reset configuration AND generates new MAC)
*/

/****** IP Settings ******/
const bool DEFAULT_AUTO_IP = false;  // Default Auto IP setting (only used if ENABLE_DHCP)
#define DEFAULT_STATIC_IP \
  { 192, 168, 1, 254 }  // Default Static IP
#define DEFAULT_SUBMASK \
  { 255, 255, 255, 0 }  // Default Submask
#define DEFAULT_GATEWAY \
  { 192, 168, 1, 1 }  // Default Gateway
#define DEFAULT_DNS \
  { 192, 168, 1, 1 }  // Default DNS Server (only used if ENABLE_DHCP)

/****** TCP/UDP Settings ******/
#define DEFAULT_REMOTE_IP \
  { 192, 168, 1, 22 }                     // Default Remote IP (only used if ENABLE_EXTENDED_WEBUI)
const bool DEFAULT_BROADCAST = true;      // Default UDP Broadcast setting
const uint16_t DEFAULT_UDP_PORT = 10000;  // Default UDP Port
const uint16_t DEFAULT_WEB_PORT = 80;     // Default WebUI Port

/****** DS18x20 Settings ******/
const uint16_t DEFAULT_OW_INT_MIN = 30;   // Default DS18x20 sensors min report interval in seconds
const uint16_t DEFAULT_OW_INT_MAX = 600;  // Default DS18x20 sensors max report interval in seconds
const byte DEFAULT_OW_CHANGE = 1;         // Default Reportable change in 1/10 °C
const byte OW_MAX_SENSORS = 30;           // Maximum number of 1-wire sensors for all buses (must be at leat 1 !!!)
const byte OW_MAX_BUSES = 5;              // Maximum number of 1-wire buses (must be at leat 1 !!!)
                                          // OW_MAX_SENSORS and OW_MAX_BUSES consume RAM memory
                                          // feel free to increase if you only use one type of sensors or if you have a board with large RAM memory (Arduino Mega)

DSTherm::Resolution OW_RESOLUTION = DSTherm::RES_12_BIT; /* DS18x20 sensor resolution (lower resolution => faster conversion time)
                                        OPTIONS:
                                            RES_9_BIT      resolution 0.5°C
                                            RES_10_BIT     resolution 0.25°C
                                            RES_11_BIT     resolution 0.125°C
                                            RES_12_BIT     resolution 0.0625°C
*/

/****** BH1750 Settings ******/
const uint16_t DEFAULT_LIGHT_INT_MIN = 1;   // Default BH1750 sensors min report interval in seconds
const uint16_t DEFAULT_LIGHT_INT_MAX = 60;  // Default BH1750 sensors max report interval in seconds
const byte DEFAULT_LIGHT_CHANGE = 1;        // Default Reportable change in lux
const byte LIGHT_MAX_SENSORS = 5;           // Maximum number of BH1750 sensors (must be at leat 1 !!!)
                                            // LIGHT_MAX_SENSORS consumes RAM memory
                                            // feel free to increase if you only use one type of sensors or if you have a board with a large RAM memory (Arduino Mega)

BH1750FVI::eDeviceMode_t LIGHT_MODE = BH1750FVI::k_DevModeContHighRes2; /* BH1750 sensor mode (continuous/onetime) and resolution (low/high/high2)
                                        OPTIONS:
                                                k_DevModeContLowRes      
                                                k_DevModeContHighRes     
                                                k_DevModeContHighRes2     
                                                k_DevModeOneTimeLowRes    not supported
                                                k_DevModeOneTimeHighRes   not supported
                                                k_DevModeOneTimeHighRes2  not supported
*/

/****** ADVANCED SETTINGS ******/

const byte MAX_ATTEMPTS = 5;  // Number of attempts to read sensor data

const byte MAC_START[3] = { 0x90, 0xA2, 0xDA };  // MAC range for Gheo SA
const byte ETH_RESET_PIN = 7;                    // Ethernet shield reset pin (deals with power on reset issue on low quality ethernet shields)
const uint16_t ETH_RESET_DELAY = 500;            // Delay (ms) during Ethernet start, wait for Ethernet shield to start (reset issue on low quality ethernet shields)
const uint16_t WEB_IDLE_TIMEOUT = 400;           // Time (ms) from last client data after which webserver TCP socket could be disconnected, non-blocking.
const uint16_t TCP_DISCON_TIMEOUT = 500;         // Timeout (ms) for client DISCON socket command, non-blocking alternative to https://www.arduino.cc/reference/en/libraries/ethernet/client.setconnectiontimeout/
const uint16_t TCP_RETRANSMISSION_TIMEOUT = 50;  // Ethernet controller’s timeout (ms), blocking (see https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.setretransmissiontimeout/)
const byte TCP_RETRANSMISSION_COUNT = 3;         // Number of transmission attempts the Ethernet controller will make before giving up (see https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.setretransmissioncount/)
const uint16_t FETCH_INTERVAL = 2000;            // Fetch API interval (ms) for the Sensors Status webpage to renew data from JSON served by Arduino

const byte DATA_START = 96;      // Start address where config and counters are saved in EEPROM
                                 // change to this value will force Arduino to load default settings
const byte EEPROM_INTERVAL = 6;  // Interval (hours) for saving Modbus statistics to EEPROM (in order to minimize writes to EEPROM)
