#ifndef GlobalDef_h
#define GlobalDef_h


// DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY
// best to copy Setup50_ILI9341_ESP8266._h_ into .../Documents/Arduino/libraries/TFT_eSPI/User_Setups/
// change its extension back to .h and included that in User_Setup_Select.h as #include <User_Setups/Setup50_ILI9341_ESP8266.h>
// or look for these lines:
//#define ILI9341_DRIVER
//#define TFT_DC 4
//#define TFT_CS 8
//#define TFT_RST 3

//here only to remember them, they are defined in tft lib:
//#define TFT_DC     D3
//#define TFT_RST    -1
//#define SPI_CLOCK  D5
//#define SPI_MISO   D6
//#define SPI_MOSI   D7
//#define TFT_CS     D8


//#define SD3        10   //(extra GPIO only available on nodemcu)

//D1 and D2 are used for I2C (e.g. PCF8574 I2C multiplexers)
#define SD_CS      D4
#define SPEAKER_CS D0   //not a real CS, directly connected to transistor driving the speaker


//#define SPI_SPEED_TFT   27000000UL 
//#define SPI_SPEED_TFT   40000000UL 
#define SPI_SPEED_TFT   80000000UL 
#define SPI_SPEED_SD    1000000UL
//#define SPI_SPEED_SD    250000UL    //new, for testing the PCB

#define ZXDISPLAYUSEINTERRUPT   // undefine to disable interrupt routine. debug purpose. usually defined
#define BORDERCOLORCYCLE        // define to cycle border color. debug purpose. usually undefined

#define SERIAL_KEYBOARD       //very useful to test functionality when the multiplexer keyboard is not built yet

//#define USE_SPIFFS        // disabled, because spiffs.begin() allocates ~2K of RAM (that spiffs.end() doesn't release!) and it fails

//#define USE_WIFI

//undefine to remove debug code
#define DEBUG_PRINT 

#ifdef DEBUG_PRINT
 #undef DEBUG_PRINT //to avoid a compiler warning
 #define DEBUG_PRINT(x)  Serial.print(x)
 #define DEBUG_PRINTLN(x)  Serial.println(x)
 //Denes' invention: these ones have ACTUALLY ZERO RAM FOOTPRINT! (using F() string still uses 4 bytes)
 //(x must be a string constant but work with or without F() around it)
 #ifndef ESP32
   #define DEBUG_PSTR(x) { const char* t = PSTR(x); Serial.print(FPSTR(t)); }
   #define DEBUG_PSTRLN(x) { const char* t = PSTR(x); Serial.println(FPSTR(t)); }
   //#define DEBUG_PRINT(...)  Serial.print(__VA_ARGS__)
   //#define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
 #else
   #define DEBUG_PSTR(x)    Serial.print(x)
   #define DEBUG_PSTRLN(x)    Serial.println(x)
 #endif
#else
 #define DEBUG_PRINT(...) {}
 #define DEBUG_PRINTLN(...) {}
 #define DEBUG_PSTR(x) {}
 #define DEBUG_PSTRLN(x) {} 
#endif

//#define SPM(_name)  static const char _name[] PROGMEM


//kempston joystick
#define BUTTON_RIGHT 1
#define BUTTON_LEFT 2
#define BUTTON_DOWN 4
#define BUTTON_UP 8
#define BUTTON_FIRE 16
#define BUTTON_FIRE2 32
#define BUTTON_ESC 64
#define BUTTON_EXTRA 128


#endif //GlobalDef_h
