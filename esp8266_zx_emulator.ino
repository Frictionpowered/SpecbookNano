
/*
   Denes' improved and optimised "SpecBook Nano" :) ZX Spectrum emulator for ESP8266, ILI9341 display and two PCF8574 multiplexers
   based on https://github.com/aldolo69/esp8266_zxspectrum_emulator
   acknowledgement of others valuable works:
   Marat Fayzullin for z80 cpu emulation
   https://fms.komkon.org/EMUL8/
   https://mikrocontroller.bplaced.net/wordpress/?page_id=756
   https://github.com/uli/Arduino_nowifi
   https://github.com/greiman/SdFat
*/


#include <Arduino.h>

#include "GlobalDef.h"
#include "Zxdisplay.h"
#include "Zxsound.h"
#include "z80.h"
#include "SpiSwitch.h"

extern int MainMenu();

//zx spectrum keyboard buffer (one for each half row, with only the lower 5 bits used)
// [3] 1->5       6<-0 [4]
// [2] Q->T       Y<-P [5]
// [1] A->G       H<-EN [6]
// [0] CS->V      B<-SP [7]
//or
//[0]  SHIFT, Z, X, C, V            [4]  0, 9, 8, 7, 6
//[1]  A, S, D, F, G                [5]  P, O, I, U, Y
//[2]  Q, W, E, R, T                [6]  ENTER, L, K, J, H
//[3]  1, 2, 3, 4, 5                [7]  SPACE, SYM SHFT, M, N, B
unsigned char KEY[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};    //note: INVERTED! 1=off, 0=pressed

//kempston joystick and extra keys
unsigned char KEMPSTONJOYSTICK = 0x00;      //note: NOT inverted
//0 = right
//1 = left
//2 = down
//3 = up
//4 = fire
//5 = fire2
//6 = ESC
//7 = extra


const unsigned char ROM[ROMSIZE] PROGMEM = 
{
    #include "zxrom.h" //original rom
};

unsigned char RAM[RAMSIZE]; //48k

Z80 state;              //emulator state

int z80DelayCycle = 5;  //feels about the right speed (see options array in Menus.cpp)
int timerfreq = 50;     //default
unsigned char soundenabled = 1;     // 0=off, 1=speaker, 2=speaker+tape
unsigned char joystickEmulation = 0;

char zxInterruptPending = 0;        //set by zxDisplay interrupt routine

//could have multiple options, like: None / 1Hz / 50Hz / every loop; all make sense, the latter two to see display performance.
bool borderCycle = false;

#ifdef DEBUG_PRINT_K
const char key2char[] PROGMEM = "^ZXCV???ASDFG???QWERT???12345???09876???POIUY???#LKJH???_sMNB???";
const char joy2char[] PROGMEM = "rldu*/@";
#endif


void ICACHE_FLASH_ATTR printBin(unsigned char c)
{
  for (int b = 7; b >= 0; b--)
  {
    Serial.print(bitRead(c, b));
  }
}

#define SCAN_I2C

#ifdef SCAN_I2C
#include <Wire.h>

//#include <Ticker.h>
//Ticker ticker;

void ICACHE_FLASH_ATTR ScanI2C()
{
  byte error, address;
  int nDevices;
 
  DEBUG_PSTRLN(F("Scanning..."));
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmission to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      DEBUG_PSTR(F("I2C device found at address 0x"));
      if (address < 16)
        DEBUG_PSTR(F("0"));
      Serial.print(address,HEX);
      DEBUG_PSTRLN(F("  !"));
 
      nDevices++;
    }
    else if (error==4)
    {
      DEBUG_PSTR(F("Unknown error at address 0x"));
      if (address < 16)
        DEBUG_PSTR("0");
      Serial.println(address,HEX);
    }    

    //stop ESP-12E from resetting
    ESP.wdtFeed();         
  }
  
  if (nDevices == 0)
  {
    DEBUG_PSTRLN(F("No I2C devices found.\n"));
  }
  else
  {
    DEBUG_PSTRLN(F("Done.\n"));
  }
}
#endif //SCAN_I2C

#include "PCF8574.h"  //streamlined version of https://github.com/xreef/PCF8574_library
PCF8574 mux1(0x20);  //I2C address
PCF8574 mux2(0x21);  //I2C address
byte mux_error = 0;

byte ICACHE_FLASH_ATTR SetupMultiplex()
{
    mux_error = 0;
    
    // Set pinModes
    //on the first, 5+1 is used for keyboard columns, 2 for SPI_CS
    //on the second, all 8 for input keyboard rows (4 left halves, 4 right halves)
    for(int i=0; i<8; i++) 
        mux1.pinMode(i, OUTPUT, HIGH);
    if  (!mux1.begin())
        mux_error |= 1;
    
    for(int i=0; i<8; i++) 
        mux2.pinMode(i, INPUT_PULLUP);
    if  (!mux2.begin())
        mux_error |= 2;
        
    //0 if success
    return mux_error;
}

void ICACHE_FLASH_ATTR ReadMultiplex()
{
   if (mux_error != 0) return;
   
  //test
  //static int pin = 0;
  //Serial.printf("\n pin = %d", pin);
  //pcf8574.digitalWrite(pin, LOW);
  //pin++;
  //if (pin > 7) pin = 0;
  //pcf8574.digitalWrite(pin, HIGH);

    //reset all keyboard column outputs (5+1), leave the remaining two high
    for(int i=0; i<6; i++)
        mux1.digitalWrite(i, HIGH);

    //drop each output one by one and check all inputs; if a button is pressed, that bit is pulled down
    //this gives 6 bytes altogether, the first 5 containing columns of 4 buttons on the left side and 4 in the right side
    // the last byte is all the 8 extra buttons
    for(int i=0; i<6; i++)
    {
        mux1.digitalWrite(i, LOW);  //buttons on this column pull DOWN
        byte keys = ~mux2.digitalReadAll(); //invert to 0 off, 1 on
        
#ifdef DEBUG_PRINT
        //printBin(keys);
#endif        
        mux1.digitalWrite(i, HIGH);
        
        if (i<5)
        {//transpose to zx keyboard format; i wouldn't need this if i wrote mux2 and read mux1, 
            // that would need 8 write+read iterations of 6 bits each instead of 6 reads of 8 bits
            // plus would have to copy the bits of the extra keys one by one
            //(note: it's also INVERTED! 1=off, 0=pressed)
            for(int j = 0; j<8; j++)
            {
#ifdef DEBUG_PRINT_K
                if ((KEY[j] & (1 << i)) != 0
                 && bitRead(keys, j) != 0)
                {//was off and now is on; just pressed
                    Serial.print(pgm_read_byte(&key2char[(j<<3) + i]));
                }
#endif                
                KEY[j] |= (1 << i);
                KEY[j] &= ~(bitRead(keys, j) << i); //un-invert :oP
            }
        } else
        {
#ifdef DEBUG_PRINT_K
            for(int j = 0; j<8; j++)
            {
                if ((KEMPSTONJOYSTICK & (1 << j)) == 0
                 && (keys & (1<<j)) != 0)
                {//was off and now is on; just pressed
                    Serial.print(pgm_read_byte(&joy2char[j]));
                }
            }
#endif                
            KEMPSTONJOYSTICK = keys;    //not inverted
        }
    }
#ifdef DEBUG_PRINT
    //Serial.print(" -> ");
    //for(int i=0; i<8; i++)
    //    printBin(KEY[i]);
    //printBin(KEMPSTONJOYSTICK);
    //Serial.println();
#endif

}

//LiPo voltage table in 5% decrements: 100, 95, 90, ...
const float voltageCurve[] PROGMEM = {
  4.2,
  4.15,
  4.11,
  4.08,
  4.02,
  3.98,
  3.95,
  3.91,
  3.87,
  3.85,
  3.84,
  3.82,
  3.80,
  3.79,
  3.77,
  3.75,
  3.73,
  3.71,
  3.69,
  3.61,
  0
};

//these don't really need to be static globals
int16_t batteryPercent = 0;
int16_t batteryMilliVolts = 0; //*1000

//patch for a bug in the esp8266 core 2.5.2
#define pgm_read_float(addr)            (*reinterpret_cast<const float*>(addr))             //dw fixed


void  ICACHE_FLASH_ATTR ReadBattery()
{
    int nVoltageRaw = analogRead(A0);
    float fVoltage = (float)nVoltageRaw * 0.0046f; //this needs tweaking depending on the voltage divider  (ideally ~ 4.5/1024)
    batteryMilliVolts = fVoltage*1000;
    batteryPercent = -1;

    int n = sizeof(voltageCurve)/sizeof(voltageCurve[0]);
    int pct = 100;
    for(int i=0; i<n; i++, pct-=5) 
    {
        float f = pgm_read_float(&voltageCurve[i]);
        if (f <= fVoltage)
        {
            batteryPercent = pct;
            break;
        }
    }
    
    //Serial.printf("\nA0=%d  V=%.3f  pct=%d", nVoltageRaw, fVoltage, batteryPercent);
    //delay(100);
}


void  ICACHE_FLASH_ATTR zxSoundSetup()
{
    pinMode(SPEAKER_CS, OUTPUT);
    digitalWrite(SPEAKER_CS, 0);
}

void  zxSoundSet(unsigned char c)   //0x10 = speaker (EAR), 0x08 = tape(MIC)
{
    switch(soundenabled)
    {
        case 0:     //off
            digitalWrite(SPEAKER_CS, 0); break;
        case 1:     //default on
            digitalWrite(SPEAKER_CS, (c & 0x10) != 0); break;
        case 2:     //produce a sound even when we are saving :) interferes with the sound of some games
            digitalWrite(SPEAKER_CS, (c & 0x18) != 0); break;
    }
}

#ifdef USE_SPIFFS
void ICACHE_FLASH_ATTR SpiffsTest()
{
    DEBUG_PSTRLN(F("spiffs test start"));

  DEBUG_PSTRLN(F("Heap size: "));
  Serial.println(ESP.getFreeHeap());
  
    if (!SPIFFS.begin())
    {
        DEBUG_PSTRLN(F("spiffs begin fail"));
        return;
    }
    
    fs::FSInfo fsInfo;
    if (SPIFFS.info(fsInfo))
    {
        DEBUG_PSTRLN(F("spiffs info"));
        Serial.println(fsInfo.totalBytes, DEC);
        Serial.println(fsInfo.usedBytes, DEC);
        Serial.println(fsInfo.blockSize, DEC);
        Serial.println(fsInfo.pageSize, DEC);
        Serial.println(fsInfo.maxOpenFiles, DEC);
        Serial.println(fsInfo.maxPathLength, DEC);
    }
  
    DEBUG_PSTRLN(F("spiffs opendir"));  
    fs::Dir dir = SPIFFS.openDir("/");

    int numEntries = 0;
    while(dir.next())
    {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        
        Serial.print(numEntries, DEC);
        Serial.print("|");
        Serial.print(fileName.c_str());
        Serial.print("|");
        Serial.print(fileSize, DEC);

        fs::File entry = dir.openFile("r");
        Serial.print("|");
        Serial.print(entry.isFile() ? "File " : "-");
        Serial.print(entry.isDirectory() ? "Dir " : "-");
        Serial.print("|");
        Serial.print(entry.name());
        Serial.print("|");
        Serial.print(entry.fullName());
        Serial.print("|");
        Serial.println(entry.size(), DEC);
        entry.close();

        numEntries++;
    }

    SPIFFS.end();
    
    DEBUG_PSTRLN(F("spiffs test end"));
    Serial.println();
}
#endif


#ifdef USE_WIFI
#include "ESP8266WiFi.h"

void ICACHE_FLASH_ATTR WiFi_Connect() 
{
    WiFi.forceSleepWake();
    delay(1);
    // Bring up the WiFi connection
    WiFi.mode(WIFI_STA);
    WiFi.begin("XXX", "XXX");

    // Wait until the connection has been confirmed before continuing
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        DEBUG_PSTR(".");
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////////
void ICACHE_FLASH_ATTR setup() 
{
#ifdef DEBUG_PRINT
  Serial.begin(115200);
#endif

    //Serial.print(F("\n\nThe board name is: "));
    //const char* title = PSTR("\n\nThe board name is: ");
    //Serial.print(FPSTR(title));
    DEBUG_PSTR("\n\nThe board name is: ");
    Serial.println(ARDUINO_BOARD);  
    //Serial.print(F("Chip ID: 0x"));
    DEBUG_PSTR(F("Chip ID: 0x"));
    Serial.println(ESP.getChipId(), HEX);

    //Serial.println(F("led = ") + String(LED_BUILTIN));

/*
    //flash size identification for SPIFFS. disabled since SPIFFS doesn't fit into RAM anyway right now
    uint32_t realSize = ESP.getFlashChipRealSize();
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();

    Serial.printf(F("Flash real id:   %08X\n"), ESP.getFlashChipId());
    Serial.printf(F("Flash real size: %u\n\n"), realSize);

    Serial.printf(F("Flash ide  size: %u\n"), ideSize);
    Serial.printf(F("Flash ide speed: %u\n"), ESP.getFlashChipSpeed());
    Serial.printf(F("Flash ide mode:  %s\n"), (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
*/
#ifdef USE_WIFI
    DEBUG_PSTRLN("Shutdown_WiFi");
    WiFi.mode( WIFI_OFF );
    WiFi.forceSleepBegin();
#endif

    DEBUG_PSTRLN("Setup_I2C");
    //delay(100);

    Wire.begin(D2, D1);   //SDA, SCL
    Wire.setClock(400000L);
    
#ifdef SCAN_I2C
    ScanI2C();
#endif
    SetupMultiplex();
    if (mux_error != 0)
        DEBUG_PSTRLN("Multiplexer Error!");

  DEBUG_PSTRLN(F("Setup_CPU"));

  //SET 160MHZ
  REG_SET_BIT(0x3ff00014, BIT(0));
  ets_update_cpu_frequency(160);
  DEBUG_PSTRLN(F("Setup_160"));

  DEBUG_PSTRLN(F("Setup_SPI"));
  spiSwitchSetup();

  //DEBUG_PSTRLN(F("Setup_Snd"));
  zxSoundSetup();

  DEBUG_PSTRLN(F("Setup_Z80"));
  ResetZ80(&state);

  DEBUG_PSTRLN(F("Setup_Display"));
  zxDisplaySetup(RAM);

  DEBUG_PSTRLN(F("Heap size: "));
  Serial.println(ESP.getFreeHeap());
}



static unsigned long ulLastKeyboardUpdate = 0;

#ifdef SERIAL_KEYBOARD
static unsigned long ulResetSerialKeyboard = 0;
static char nextKey = 0;

void ICACHE_FLASH_ATTR simulate_key(char cRead, char cCheck, unsigned char cBit, unsigned char k)
{
  if (cRead == cCheck)
  {
    Serial.println(cRead);
    
    KEY[k] &= ~cBit;
    ulResetSerialKeyboard = millis() + 50;  //hold it down for this long
  }
}

const char serial_key_table[] PROGMEM = "1234567890qwertyuiopasdfghjkl\\>zxcvbnm< ";

inline void PressKey(char row, char col)
{
    int byteIndex = col<5 ? (3-row) : (4+row);
    int bitIndex = col<5 ? col : 9-col;
    
    KEY[byteIndex] &= ~(1<<bitIndex);
    ulResetSerialKeyboard = millis() + 50;  //hold it down for this long
}

void ICACHE_FLASH_ATTR ProcessSerialKey(char cRead)
{
    for(int row=0; row<4; row++)
    {
        for(int col=0; col<10; col++)
        {
            if (cRead == pgm_read_byte(&serial_key_table[row*10+col]))
            {
                PressKey(row, col);
                break;
            }
        }
    }

    //a few alternative keycodes
    if (cRead == 'C') PressKey(3, 0);   //caps
    if (cRead == 'S') PressKey(3, 8);   //sym
    if (cRead == '_') PressKey(3, 9);   //space
    if (cRead == '#') PressKey(2, 9);   //enter
    
/*
    simulate_key(cRead, '1', BUTTON_1, 3);
    simulate_key(cRead, '2', BUTTON_2, 3);
    simulate_key(cRead, '3', BUTTON_3, 3);
    simulate_key(cRead, '4', BUTTON_4, 3);
    simulate_key(cRead, '5', BUTTON_5, 3);

    simulate_key(cRead, 'q', BUTTON_Q, 2);
    simulate_key(cRead, 'w', BUTTON_W, 2);
    simulate_key(cRead, 'e', BUTTON_E, 2);
    simulate_key(cRead, 'r', BUTTON_R, 2);
    simulate_key(cRead, 't', BUTTON_T, 2);

    simulate_key(cRead, 'a', BUTTON_A, 1);
    simulate_key(cRead, 's', BUTTON_S, 1);
    simulate_key(cRead, 'd', BUTTON_D, 1);
    simulate_key(cRead, 'f', BUTTON_F, 1);
    simulate_key(cRead, 'g', BUTTON_G, 1);

    simulate_key(cRead, '>', BUTTON_CS, 0);
    simulate_key(cRead, 'z', BUTTON_Z, 0);
    simulate_key(cRead, 'x', BUTTON_X, 0);
    simulate_key(cRead, 'c', BUTTON_C, 0);
    simulate_key(cRead, 'v', BUTTON_V, 0);
    
    simulate_key(cRead, '0', BUTTON_0, 4);
    simulate_key(cRead, '9', BUTTON_9, 4);
    simulate_key(cRead, '8', BUTTON_8, 4);
    simulate_key(cRead, '7', BUTTON_7, 4);
    simulate_key(cRead, '6', BUTTON_6, 4);

    simulate_key(cRead, 'y', BUTTON_Y, 5);
    simulate_key(cRead, 'u', BUTTON_U, 5);
    simulate_key(cRead, 'i', BUTTON_I, 5);
    simulate_key(cRead, 'o', BUTTON_O, 5);
    simulate_key(cRead, 'p', BUTTON_P, 5);

    simulate_key(cRead, 'h', BUTTON_H, 6);
    simulate_key(cRead, 'j', BUTTON_J, 6);
    simulate_key(cRead, 'k', BUTTON_K, 6);
    simulate_key(cRead, 'l', BUTTON_L, 6);
    simulate_key(cRead, '\\', BUTTON_EN, 6);

    simulate_key(cRead, 'b', BUTTON_B, 7);
    simulate_key(cRead, 'n', BUTTON_N, 7);
    simulate_key(cRead, 'm', BUTTON_M, 7);
    simulate_key(cRead, '<', BUTTON_SS, 7);
    simulate_key(cRead, ' ', BUTTON_SP, 7);

    //Denes' alternate keys
    simulate_key(cRead, 'C', BUTTON_CS, 0);
    simulate_key(cRead, 'S', BUTTON_SS, 7);
    simulate_key(cRead, '_', BUTTON_SP, 7);
    simulate_key(cRead, '#', BUTTON_EN, 6);
*/
    
    if (cRead == '|' || cRead == ',' || cRead == ';')
    {
        //separators; release all and wait (to allow entire sequences to be sent at once)
        nextKey = 2;
        ulResetSerialKeyboard = millis() + 50;
    }
}

void ICACHE_FLASH_ATTR UpdateSerialKeyboard(unsigned long ulNow)
{
  //simulation of zx keyboard from serial data
  // it can type shift+key combinations
  // examples:  ><z is BEEP, >0 is Delete, \ is Enter
  // longer sequences need separators to simulate releasing all buttons for a short while before pressing the next

  if (ulNow > ulResetSerialKeyboard && ulResetSerialKeyboard)
  {
    //Serial.println("rsk_");
    ulResetSerialKeyboard = 0;
    KEMPSTONJOYSTICK = 0;
    KEY[0] = KEY[1] = KEY[2] = KEY[3] = KEY[4] = KEY[5] = KEY[6] = KEY[7] = 0xff;
    if (nextKey > 0)
    {
        nextKey -= 1;
        if (nextKey > 0)
        {//wait once more, until rom has also processed the empty keyboard buffer
          ulResetSerialKeyboard = millis() + 50;    //hold it released for this long
        }
    }
  }
  if (nextKey > 0)  //do not type the next one yet
    return;     

  if (Serial.available())
  {
    char cRead = Serial.read();
    
    DEBUG_PSTR("Key ");
    Serial.println(cRead);

    ProcessSerialKey(cRead);
  }
}
#endif


bool waitForInputClear = false;

bool ICACHE_FLASH_ATTR MustWaitForInputClear()
{
    if (!waitForInputClear) return false;
    if (KEMPSTONJOYSTICK != 0x00) return true;
    for(int i=0; i<8; i++)  
        if (KEY[i] != 0xFF) return true;
    //Serial.println("released");
    waitForInputClear = false;
    return false;
}

const char pressed_key_table_base[] PROGMEM = "1234567890qwertyuiopasdfghjkl\n^zxcvbnm` ";
const char pressed_key_table_caps[] PROGMEM = "!@#$%&'()\bQWERTYUIOPASDFGHJKL\r^ZXCVBNM` ";
const char pressed_key_table_sym[]  PROGMEM = "!@#$%&'()_QWE<>[]I;\"~|\\{}^-+=\n^:£?/*,.` ";

char ICACHE_FLASH_ATTR GetPressedKey()
{
    char caps_shift = bitRead(KEY[0], 0) ^ 1;
    char sym_shift = bitRead(KEY[7], 1) ^ 1;
    for(int row=0; row<4; row++)
    {
        for(int col=0; col<10; col++)
        {
            int i = row*10 + col;
            int byteIndex = col<5 ? (3-row) : (4+row);
            int bitIndex = col<5 ? col : 9-col;
            if (bitRead(KEY[byteIndex], bitIndex) == 0)
            {
                if (byteIndex==0 && bitIndex==0) {}//skip, it's the caps shift
                else
                if (byteIndex==7 && bitIndex==1) {}//skip, it's the symbol shift
                else
                if (caps_shift) return pgm_read_byte(&pressed_key_table_caps[i]);
                else
                if (sym_shift) return pgm_read_byte(&pressed_key_table_sym[i]);
                else
                return pgm_read_byte(&pressed_key_table_base[i]);
            }
        }
    }
    //if no button is pressed, return the status of the two shifts
    return (caps_shift<<1) | sym_shift;
}

bool ICACHE_FLASH_ATTR IsKeyPressed(char row, char col)
{
    //int i = row*10 + col;
    int byteIndex = col<5 ? (3-row) : (4+row);
    int bitIndex = col<5 ? col : 9-col;
    return (bitRead(KEY[byteIndex], bitIndex) == 0);
}

const char key_check_table[] PROGMEM = "1234567890QWERTYUIOPASDFGHJKL#\\ZXCVBNM// ";

bool ICACHE_FLASH_ATTR IsKeyPressed(char key)   //a *bit* slower but more convenient to use
{
    for(int row=0; row<4; row++)
    {
        for(int col=0; col<10; col++)
        {
            if (pgm_read_byte(&key_check_table[row*10+col]) == key)
            {
                return IsKeyPressed(row, col);
            }
        }
    }
    return false;
}

//(potentially) update all keyboards, multiplex or serial
int ICACHE_FLASH_ATTR UpdateInputs(bool doJoystick)   //it probably starts in ram anyway
{
    ESP.wdtFeed();
    unsigned long ulNow = millis();

    // Update Denes' dual multiplex keyboard
    
    //no need to update within 20 millisec (not even the serial keyboard)
    if (ulNow < ulLastKeyboardUpdate + 20)
    {
        return ulLastKeyboardUpdate + 20 - ulNow;   //report time left so i can for example use it for delay()
    }

#ifdef SERIAL_KEYBOARD
    //serial keyboard input
    UpdateSerialKeyboard(ulNow);
#endif
    
    //if (ulNow > ulLastKeyboardUpdate + 20  //50 times a second
#ifdef SERIAL_KEYBOARD    
    if (ulResetSerialKeyboard != 0)       //try not to interfere with serial keyboard; wait
    {
        return 1;
    }
#endif

    ulLastKeyboardUpdate = ulNow;
    ReadMultiplex();

    //ToDo: apply joystick->key emulation depending on joystick option(s); sinclair, cursor, etc
    //Kempston     RLDUF
    //Sinclair 1   67890->LRDUF
    //Sinclair 2   12345->LRDUF
    //Protek/cursor 56780->LDURF
    //QAOPM(->UDLRF)
    //AZOPM(->UDLRF)
    //QZIPM(->UDLRF) (horace)
    //QWERT(->LRUDF) (pssst) 
    //CUSTOM

    if (doJoystick)
    switch(joystickEmulation)
    {
        case 0:
            break;
        case 1: //sinclair 1
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[4] &= ~(1<<4);
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[4] &= ~(1<<3);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[4] &= ~(1<<2);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[4] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[4] &= ~(1<<0);
            break;
        case 2: //sinclair 2
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[3] &= ~(1<<4);
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[3] &= ~(1<<3);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[3] &= ~(1<<2);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[3] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[3] &= ~(1<<0);
            break;
        case 3: //cursor
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[3] &= ~(1<<4);
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[4] &= ~(1<<2);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[4] &= ~(1<<4);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[4] &= ~(1<<3);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[4] &= ~(1<<0);
            break;
        case 4: //QAOPM(->UDLRF)
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[5] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[5] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[1] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[2] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[7] &= ~(1<<2);
            break;
        case 5: //AZOPM(->UDLRF)
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[5] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[5] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[0] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[1] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[7] &= ~(1<<2);
            break;
        case 6: //QZIPM(->UDLRF) (horace)
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[5] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[5] &= ~(1<<2);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[0] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[2] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[7] &= ~(1<<2);
            break;
        case 7: //QWERT(->LRDUF) (pssst) 
            if (bitRead(KEMPSTONJOYSTICK, 0))   KEY[2] &= ~(1<<1);
            if (bitRead(KEMPSTONJOYSTICK, 1))   KEY[2] &= ~(1<<0);
            if (bitRead(KEMPSTONJOYSTICK, 2))   KEY[2] &= ~(1<<2);
            if (bitRead(KEMPSTONJOYSTICK, 3))   KEY[2] &= ~(1<<3);
            if (bitRead(KEMPSTONJOYSTICK, 4))   KEY[2] &= ~(1<<4);
            break;
        case 8: //CUSTOM
            break;
        default:
            break;
    }

    ReadBattery();
    
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////
void loop() 
{
#ifdef USE_SPIFFS_TEST
    SpiffsTest();
    delay(1000);
    return;
#endif

    UpdateInputs(true);

//extern volatile unsigned int zxDisplay_frame_counter_50hz;
  //DEBUG_PRINTLN(zxDisplay_frame_counter_50hz);

//    static int loops = 0;
//    Serial.printf("\n Loop %d %d ", loops++, ongoingtask);

//#define DEBUG_PRINT_KEYBOARD
#ifdef DEBUG_PRINT_KEYBOARD
    //debug print keyboard status
    for(int i=0; i<8; i++)
    {
        printBin(KEY[i]);
        DEBUG_PSTR("-");
    }
    printBin(KEMPSTONJOYSTICK);
    Serial.println();
    delay(100);
    //return;
#endif
    
    //ESP.wdtFeed(); // feeds the watchdog (added trying to make it run on wemos D1 mini; was fine on NodeMCU without it)
    //delay(1000);
    //ExecZ80(&state, 50000);   //how many cycles to run
    //return;

    if (!MustWaitForInputClear()
    && (KEMPSTONJOYSTICK & BUTTON_ESC) ) //'ESC special key
    {
        DEBUG_PSTRLN("ESC");
        waitForInputClear = true;
        
        zxSoundSet(0);
        zxDisplayStop();
        MainMenu();
        zxDisplayStart();
        
        return;
    }
    
    ExecZ80(&state, 50000);   //how many cycles to run
        
    if (zxInterruptPending) //set=1 by screen routine at the end of draw
    {
        zxInterruptPending = 0;
    
        IntZ80(&state, INT_IRQ);

#ifdef BORDERCOLORCYCLE
        if (borderCycle)
        {
            static char border = 0;
            //static char cnt = 0;
            //cnt++;
            //if (cnt == 50)
            {       //this way it changes every 50000 z80 cycles
              //cnt = 0;
              zxDisplayBorderSet(border);
              border = (++border) & 0x7;
            }
        }
#endif
    }
    
    return;
}

//EOF____________________________________________________________
