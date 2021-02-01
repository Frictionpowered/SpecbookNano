
#include "GlobalDef.h"

#include "Zxdisplay.h"
#include "Zxsound.h"
#include "SpiSwitch.h"
#include "z80.h"

#include "Z80filedecoder.h"


extern Z80 state;
//extern const unsigned char ROM[];
extern unsigned char RAM[];
extern unsigned char KEY[];
extern unsigned char KEMPSTONJOYSTICK;

extern int z80DelayCycle;
extern int timerfreq; //default
extern unsigned char joystickEmulation;
extern unsigned char soundenabled;

extern int16_t batteryPercent;
extern int16_t batteryMilliVolts;

extern bool borderCycle;

extern uint8_t zxDisplayScanToggle;

extern int UpdateInputs(bool doJoystick);
extern void PrintStackSize();

char MiniMenu();
int MainMenu();
int FileMenu(bool spiffs);
int KeyboardMenu();
int JoystickMenu();
int ExtrasMenu();
int Debugger();

int ListDir(const char* currentPath, int startIndex, int numToShow, char* list[], char* ptr);
int FindFileIndex(const char* currentPath, const char* fileName);
uint16_t FindQuicksaves();
int Open(char* fileName, char* currentPath, unsigned char *BUFFER);
int Delete(char* fileName, char* currentPath);
int Copy(char* fileName, char* currentPath, char* targetName);
#ifdef USE_SPIFFS
int ICACHE_FLASH_ATTR ListDirSpiffs(char* currentPath, int startIndex, int numToShow, char* list[], char* ptr);
#endif

int SaveZ80(const char *fileName, bool setLast);
int LoadZ80(const char *fileName, bool setLast);
int TextViewer(SdFile *file, unsigned char *BUFFER, int bufferSize, char* fileName, char* currentPath);

int SaveBuffer(const char *fileName, const char *buffer, int length, bool SD_already_open = false);
int LoadBuffer(const char *fileName, char *buffer, int maxLength);

char MessageInput();
char Message(const char *text, bool wait = true);
//char Message(const __FlashStringHelper* str1, const __FlashStringHelper* str2 = nullptr);
char Message(const char* str1, const char* str2, bool wait = true);
char TextEntry(const char* label, char* inbuf, int maxLength);

void BuildMiniScreen();

//==============================================================
#include <TFT_eSPI.h>

extern TFT_eSPI zxDisplayTft;

extern bool waitForInputClear;
extern bool MustWaitForInputClear();
extern char GetPressedKey();
extern bool IsKeyPressed(char row, char col);


const int z80DelayCycles[] PROGMEM = {0, 1, 2, 4, 8};
const int z80TimerFreqs[] PROGMEM = {10, 25, 50, 75, 100};


SdFat sd;

void ICACHE_FLASH_ATTR SD_Setup()
{
    spiSwitchSet(SD_CS);
   
    //SdFat sd;

    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
    {
        DEBUG_PSTRLN(F("sd.begin OK"));
       
/*        sprintf_P(buf, PSTR("/"));
        if (dirFile.open(buf, O_RDONLY))   //can do path :) e.g "/DCIM/"
        {
            SdFile file;
   
            while (file.openNext(&dirFile, O_RDONLY))
            {
                file.getName(buf, 64);
                Serial.println(buf);
                
                file.close();
            }

            dirFile.close();
            
        } else
        {
            DEBUG_PSTRLN(F("dir.open FAIL"));
        }
*/
    } else
    {
        DEBUG_PSTRLN(F("sd.begin FAIL"));
    }
    
    spiSwitchReset();
}


#define PATH_MAX_LEN    64
#define NUM_SHOW_ENTRIES    12


int ICACHE_FLASH_ATTR BootMenu()
{
    //try to reload last quicksave
    char fileName[16];
    strcpy_P(fileName, PSTR("QuickSave.z80"));
    if (LoadZ80(fileName, false) == 0)
    {
        
    }
    return -1;
}

char ICACHE_FLASH_ATTR MiniMenu()
{
    //TEMP
    //return Message(PSTR("Paused"));
    
    //(try to) quicksave
    char fileName[16];
    strcpy_P(fileName, PSTR("QuickSave.z80"));
    if (SaveZ80(fileName, false) == 0)
    {
        
    }
    
    return Message(PSTR("Paused"), PSTR("Esc:Menu  Fire:Back  F2:Reload"));
}

int ICACHE_FLASH_ATTR MainMenu()
{
    // Defined HERE it doesn't use any RAM. As a global string array, even if PROGMEM with F() elements, it does.
    // (unless each element is defined as a _named_ const char* PROGMEM variable, and the array separately as a PROGMEM char*[] array; too fiddly. )
const char* const menu_table[] = 
    { PSTR("1 Open SD")
    , PSTR("2 Open SPIFFS")
    , PSTR("3 Keyboard Layout")
    , PSTR("4 Joystick")
    , PSTR("5 Sound")
    , PSTR("6 CPU Speed")
    , PSTR("7 Timer Freq")
//    , PSTR("8 Screen Refresh Rate")
//    , PSTR("8 Border")
    , PSTR("8 Debugger")
    , PSTR("9 Extras")
    , PSTR("S Save state as Z80")
//    , PSTR("S Save screen as SCR")
    , PSTR("R Reset")
    };
const char* bottomLine = PSTR("Caps+1..9:SAVE  Sym+1..9:LOAD  Esc:Back");    
const char* const sound_table[] = 
    { PSTR("OFF         ")
    , PSTR("SPEAKER     ")
    , PSTR("SPEAKER+TAPE")
    };
const char* const joystick_table[] = 
    { PSTR("KEMPSTON     ")
    , PSTR("SINCLAIR 1   ")
    , PSTR("SINCLAIR 2   ")
    , PSTR("PROTEK/CURSOR")
    , PSTR("QAOPM        ")
    , PSTR("AZOPM        ")
    , PSTR("QZIPM        ")
    , PSTR("QWERT        ")
    , PSTR("CUSTOM       ")    //requires defining which keys
    };
const char* const cpu_delay_table[] = 
    { PSTR("FASTEST")
    , PSTR("FAST   ")
    , PSTR("NORMAL ")
    , PSTR("SLOW   ")
    , PSTR("SLOWEST")
    };

    bool needRender = true;
    char currentItem = 0;
    int16_t lastHighlightedEntry = -1;

    bool keyRepeating = false;
    unsigned long keyRepeatMillis = -1;
    char lastPressedKey = 0;

    zxSoundSet(0);

    {
        char first = MiniMenu();
        
        // esc/menu again opens the full menu; anything else goes back
        if (first == (char)-1 && KEMPSTONJOYSTICK == BUTTON_ESC)
        {//continue to main menu
            waitForInputClear = true;
        } else
        if (first == (char)-1 && KEMPSTONJOYSTICK == BUTTON_FIRE2)
        {//load last file
            char fileName[128];
            int loadedLen = LoadBuffer(F("LastPath.txt"), fileName, 128);
            if (loadedLen > 0)
            {
                char* extension = fileName + strlen(fileName) - 3;
                Serial.println(extension);
                if (!strcasecmp(extension, "Z80"))
                {
                    if (LoadZ80(fileName, false) == 0)
                    {
                        while (MustWaitForInputClear())
                        {
                            UpdateInputs(false);
                            delay(1);   //power saving
                        }
                        return -1;
                    }
                }
            }
            return -1;
        } else
        {//unpause and return to emulator
            return -1;
        }
    }

    //DEBUG_PSTRLN(F("Free heap: "));
    //DEBUG_PRINTLN(ESP.getFreeHeap());
    //DEBUG_PSTRLN(F("Free stack: "));
    //DEBUG_PRINTLN(ESP.getFreeContStack());
    PrintStackSize();
        
    while(true)
    {
        int numItems = sizeof(menu_table) / sizeof(menu_table[0]);
        
        if (needRender)
        {
            spiSwitchSet(TFT_CS);

            char buf[64];

            zxDisplayTft.setTextFont(0);
            zxDisplayTft.setTextSize(1);
            //zxDisplayTft.setTextColor(TFT_MAGENTA, TFT_BLUE);
            zxDisplayTft.setTextDatum(TL_DATUM);

            if (lastHighlightedEntry >= 0)
            {//valid; minimal re-render
                int16_t i = lastHighlightedEntry;
                zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
                strcpy_P(buf, (PGM_P)menu_table[i]);
                zxDisplayTft.drawString(buf, 10, 10+i*16);
                i = currentItem;
                zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLUE);
                strcpy_P(buf, (PGM_P)menu_table[i]);
                zxDisplayTft.drawString(buf, 10, 10+i*16);
            } else
            {//full re-render
                zxDisplayTft.fillScreen(TFT_BLACK);
                    
                for(int i=0; i<numItems; i++)
                {
                    if (i==currentItem)
                        zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLUE);
                    else
                        zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
                    strcpy_P(buf, (PGM_P)menu_table[i]);
                    zxDisplayTft.drawString(buf, 10, 10+i*16);
                }
                
                strcpy_P(buf, (PGM_P)bottomLine);
                zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
                zxDisplayTft.setTextDatum(MC_DATUM);
                zxDisplayTft.drawString(buf, 160, 230);
                
                //how about rendering a little rainbow in the bottom right corner? :)
                //needs 3*2 triangles; maybe use a little progmem table.
                //zxDisplayTft.fillTriangle

                //test 
                BuildMiniScreen();

                //ToDo: display some info about each of the 9 quicksave slots
                //minor problem: have to keep switching between SD and TFT back and forth
                //but BUFFER is not allocated at this stage so we can have a decent* sized buffer
                //MAYBE, if it would cause a noticeable pause, just draw the slots here and refresh them later one by one 
                {
                    zxDisplayTft.setTextDatum(MC_DATUM);
                    char fileName[16];
                    strcpy_P(fileName, PSTR("QuickSaveX.z80"));
                    char fileBuf[16];

                    for(int i=0; i<9; i++)
                    {
                        zxDisplayTft.drawRect(3+i*35, 190-1, 32+2, 24+2, TFT_WHITE);
                    }

                    zxDisplayTft.setTextColor(TFT_YELLOW, TFT_TRANSPARENT)
                    ;
                    uint16_t bits = FindQuicksaves();
                    
                    for(int i=0; i<9; i++)
                    {
                        fileName[9] = (char)('1' + i);

                        //ToDo: try to open quicksave (fileName) and get some info about it
                        //temp way to check if it exists :)
                        //pretty slow (~125ms each), and seems to take the same time whether the file exists or not.
                        //worth running some experiments to see which part is slow and if i can speed it up
                        
                        if (bits & 1<<i)    //if it's zero it doesn't exist
                        {
                            //if (LoadBuffer(fileName, fileBuf, 16) > 0)
                            {
                                zxDisplayTft.fillRect(4+i*35, 190, 32, 24, TFT_LIGHTGREY);
                            }
                        }
                        sprintf_P(buf, PSTR("%d"), 1+i);
                        zxDisplayTft.drawString(buf, 3+i*35+16, 190+12);
                    }
                }
            }
            
            lastHighlightedEntry = currentItem;
            
            zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
            zxDisplayTft.setTextDatum(TL_DATUM);

            strcpy_P(buf, (PGM_P)joystick_table[joystickEmulation]);
            zxDisplayTft.drawString(buf, 150, 10+3*16);
            
            strcpy_P(buf, (PGM_P)sound_table[soundenabled]);
            zxDisplayTft.drawString(buf, 150, 10+4*16);
            
            sprintf_P(buf, PSTR("%d  "), z80DelayCycle);
            for(uint16_t i=0; i<sizeof(z80DelayCycles) / sizeof(z80DelayCycles[0]); i++)
            {
                if (z80DelayCycles[i] == z80DelayCycle)
                {//found
                    strcpy_P(buf, (PGM_P)cpu_delay_table[i]);
                    break;
                }
            }
            zxDisplayTft.drawString(buf, 150, 10+5*16);
            
            sprintf_P(buf, PSTR("%d  "), timerfreq);
            zxDisplayTft.drawString(buf, 150, 10+6*16);

            sprintf_P(buf, PSTR("Batt: %d%% (%d.%dV)"), batteryPercent
                        , batteryMilliVolts/1000, batteryMilliVolts%1000);
            zxDisplayTft.setTextDatum(TR_DATUM);
            zxDisplayTft.drawString(buf, 319, 0);

            needRender = false;
        }
        
        unsigned long now = millis();
        UpdateInputs(false);
            
        if (MustWaitForInputClear())
        {
            if (now < keyRepeatMillis || keyRepeatMillis <= 0)
                continue;   //keep waiting
            //exceeded, start repeating
            keyRepeating = true;
        } else
        {
            keyRepeating = false;
        }

        bool activate = false;
        for(char i=0; i<9; i++)
        {
            if (IsKeyPressed(0, i))
            {
                waitForInputClear = true;
                needRender = true;
                
                if (IsKeyPressed(3, 0)) //caps shift
                {
                    char fileName[16];
                    strcpy_P(fileName, PSTR("QuickSaveX.z80"));
                    fileName[9] = (char)('1' + i);
                    if (SaveZ80(fileName, true) == 0)
                    {
                        waitForInputClear = true;
                        while (MustWaitForInputClear())
                        {
                            UpdateInputs(false);
                            delay(1);   //power saving
                        }
                        return -1;
                    }
                    continue;
                }
                
                if (IsKeyPressed(3, 8)) //symbol shift
                {
                    char fileName[16];
                    strcpy_P(fileName, PSTR("QuickSaveX.z80"));
                    fileName[9] = (char)('1' + i);
                    if (LoadZ80(fileName, true) == 0)
                    {
                        waitForInputClear = true;
                        while (MustWaitForInputClear())
                        {
                            UpdateInputs(false);
                            delay(1);   //power saving
                        }
                        return -1;
                    }
                    continue;
                }

                //no shifts: select menu item (ToDo: and activate)
                currentItem = i;
                //continue;
                activate = true;
            }
        }
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC )
        {
            waitForInputClear = true;
            needRender = true;
            return -1;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_UP
           || IsKeyPressed(1,0) )   //Q
        {
            currentItem = currentItem > 0 ? --currentItem : numItems-1;
            waitForInputClear = true;
            needRender = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        } else
        if (KEMPSTONJOYSTICK & BUTTON_DOWN
           || IsKeyPressed(2,0) )   //A
        {
            currentItem = (++currentItem) % numItems;
            waitForInputClear = true;
            needRender = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        } else
        if (KEMPSTONJOYSTICK & BUTTON_FIRE 
            || IsKeyPressed(2,9)    //enter
            || activate
            )
        {//select
            DEBUG_PSTR(F("SELECT "));
            Serial.println(1+currentItem, DEC);
            waitForInputClear = true;
            needRender = true;
            lastHighlightedEntry = -1;  //full re-render
            keyRepeatMillis = -1;   //do not repeat
            
            switch(1+currentItem)
            {
                case 1:
                    {
                        int ret = FileMenu(false);
                        if (ret == -2)
                        {
                            return -1;
                        }
                    }
                    break;
                    
                case 2:
#ifdef USE_SPIFFS
                    //ToDo: implement spiffs browser
                    {
                        int ret = FileMenu(true);
                        if (ret == -2)
                        {
                            return -1;
                        }
                    }
#endif                    
                    break;
                    
                case 3:
                    KeyboardMenu();
                    break;
                    
                case 4: //joystick
                    joystickEmulation = (++joystickEmulation) % (sizeof(joystick_table)/sizeof(joystick_table[0]));
                    break;
                    
                case 5:
                    soundenabled = (++soundenabled) % (sizeof(sound_table)/sizeof(sound_table[0]));    //0->1->2->0
                    break;
                    
                case 6: //CPU speed
                    {
                        int numOptions = sizeof(z80DelayCycles) / sizeof(z80DelayCycles[0]);
                        for(int16_t i=0; i<numOptions; i++)
                        {
                            if (z80DelayCycles[i] == z80DelayCycle)
                            {//found
                                z80DelayCycle = z80DelayCycles[(i+1) % numOptions];
                                break;
                            }
                        }
                    }
                    break;
                    
                case 7: //timer freq
                    //timerfreq = (((timerfreq-10)+10) % 100) + 10;
                    {
                        int numOptions = sizeof(z80TimerFreqs) / sizeof(z80TimerFreqs[0]);
                        for(int16_t i=0; i<numOptions; i++)
                        {
                            if (z80TimerFreqs[i] == timerfreq)
                            {//found
                                timerfreq = z80TimerFreqs[(i+1) % numOptions];
                                zxDisplaySetIntFrequency(timerfreq);
                                break;
                            }
                        }
                    }
                    break;
                    
                case 8:
                    //borderCycle = !borderCycle;
                    Debugger();
                    break;
                    
                case 9:    //extras
                    ExtrasMenu();
                    break;
                    
                case 10:    //save .z80; ask for name first, needs text input implementation
                    char newName[50];
                    strncpy(newName, F("QuickSave.z80"), 50);
                    if(TextEntry(F("Save as"), newName, 50) == 1)
                    {
                        SaveZ80(newName, true);
                    }
                    break;
                    
                case 11:
                    ResetZ80(&state);
                    return -1;  //also return and watch the memory test
                    //break;
                    
                default:
                    break;
            }
        } else  //end select
        {
            //can perform item-specific controls here, like left-right
            switch(1+currentItem)
            {
                case 4: //joystick
                  {
                    int numJoyOptions = sizeof(joystick_table)/sizeof(joystick_table[0]);
                    if (KEMPSTONJOYSTICK & BUTTON_FIRE2)
                    {
                        //ToDo: set to custom and ask for each button to be pressed
                        joystickEmulation = numJoyOptions-1;
                        
                        waitForInputClear = true;
                        needRender = true;
                        keyRepeatMillis = -1;   //do not repeat
                    } else
                    if (KEMPSTONJOYSTICK & BUTTON_RIGHT)
                    {
                        joystickEmulation = (++joystickEmulation) % numJoyOptions;
                        waitForInputClear = true;
                        needRender = true;
                        keyRepeatMillis = -1;   //do not repeat
                    } else
                    if (KEMPSTONJOYSTICK & BUTTON_LEFT)
                    {
                        joystickEmulation = (--joystickEmulation + numJoyOptions) % numJoyOptions;
                        waitForInputClear = true;
                        needRender = true;
                        keyRepeatMillis = -1;   //do not repeat
                    }
                  }   
                    break;
                    
                default:
                    break;
            }

        }
        
    }//while

    return -1;
}


int ICACHE_FLASH_ATTR ListDir(const char* currentPath, int startIndex, int numToShow, char* list[], char* ptr)
{
    int index = 0;
    int numEntries = 0;
    int numShown = 0;

    DEBUG_PSTR(F("list dir "));
    DEBUG_PRINTLN(currentPath);
    
    spiSwitchSet(SD_CS);
   
    //SdFat sd;
    SdFile dirFile;
    
    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
                  
    {
        if (dirFile.open(currentPath, O_RDONLY))   //can do path :) e.g "/DCIM/"
        {
            SdFile file;
   
            while (file.openNext(&dirFile, O_RDONLY))
            {
                if (file.isHidden())
                {//skip
                    file.close();
                    continue;
                }
                //ToDo: filter to name mask if i typed in any
            
                ++numEntries;
    
                if (startIndex > index++)
                {//not there yet
                    file.close();
                    continue;
                }
                
                /*if (++numShown > numToShow)
                {
                    file.close();
                    continue;   //keep counting
                    //break;
                }*/
        
                if (numShown < numToShow)
                {
                    list[numShown++] = ptr;
                    file.getName(ptr, 64);
                    //Serial.print(ptr);
                    ptr += strlen(ptr) + 1;
                }
                
                //test recursion
                //if (file.isSubDir())
                //{
                    //ListDir(file, 0, 32);
                //}
        
                file.close();
            }

            dirFile.close();
        }
    }

    spiSwitchSet(TFT_CS);

    //fill the rest with empties
    while (numShown < numToShow)
    {
        list[numShown++] = NULL;
    }
    
    //Serial.println(numEntries);
    return numEntries;  //number of all files in the folder
}

int ICACHE_FLASH_ATTR FindFileIndex(const char* currentPath, const char* fileName)
{
    int index = 0;

    DEBUG_PSTR(F("find_begin "));
    DEBUG_PRINTLN(currentPath);
    DEBUG_PRINTLN(fileName);
    
    spiSwitchSet(SD_CS);
   
    //SdFat sd;
    SdFile dirFile;
    
    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
                  
    {
        if (dirFile.open(currentPath, O_RDONLY))   //can do path :) e.g "/DCIM/"
        {
            SdFile file;

            char buffer[64];
   
            while (file.openNext(&dirFile, O_RDONLY))
            {
                if (file.isHidden())
                {//skip
                    file.close();
                    continue;
                }
   
                file.getName(buffer, 64);
                if (!strncmp(buffer, fileName, 64))
                {//found it
                    file.close();
                    dirFile.close();
                    spiSwitchSet(TFT_CS);
                    DEBUG_PRINTLN(index);
                    return index;
                }

                ++index;
                
                file.close();
            }
            dirFile.close();
        }
    }

    spiSwitchSet(TFT_CS);
    
    return 0;
}

uint16_t ICACHE_FLASH_ATTR FindQuicksaves()
{
    uint16_t bits = 0;

    DEBUG_PSTRLN(F("FindQuickSaves"));
    PrintStackSize();

    spiSwitchSet(SD_CS);
   
    //SdFat sd;
    SdFile dirFile;
    
    if (!sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
                  
    {
        spiSwitchSet(TFT_CS);
        return 0;
    }
    
    if (!dirFile.open("/", O_RDONLY))   //can do path :) e.g "/DCIM/"
    {
        spiSwitchSet(TFT_CS);
        return 0;
    }
    
    SdFile file;

    char fileName[64];
    strcpy_P(fileName, PSTR("QuickSaveX.z80"));

    char buffer[64];
    
    while (file.openNext(&dirFile, O_RDONLY))
    {
        if (file.isHidden())
        {//skip
            file.close();
            continue;
        }

        if (file.isSubDir())
        {//skip
            file.close();
            continue;
        }

        file.getName(buffer, 64);
        if (strlen(buffer) != strlen(fileName)
         || strncmp(buffer, fileName, 9)  //"QuickSave"
         || strncmp(buffer+10, fileName+10, 4))
        {//not a quicksave
            file.close();
            continue;
        }

        int index = buffer[9] - '1';
        bits |= 1<<index;
       
        file.close();
    }
    
    dirFile.close();

    spiSwitchSet(TFT_CS);
    
    return bits;
}

int ICACHE_FLASH_ATTR OpenSub(SdFile &file, char* fileName, char* currentPath)
{
    char fullPath[256]; //only allocated while absolutely necessary
    strncpy(fullPath, currentPath, 256);
    strncat(fullPath, fileName, 256);

    Serial.println(fullPath);

    DEBUG_PSTRLN(F("open"));
    int openresult = file.open(fullPath, O_RDONLY);   //can do path :) e.g "/DCIM/"

    //have to save it here as it's in the BUFFER and will be overwritten
    if (openresult && !file.isSubDir())
        SaveBuffer(F("LastPath.txt"), fullPath, strlen(fullPath)+1, true);
    
    return openresult;
}

int ICACHE_FLASH_ATTR Open(char* fileName, char* currentPath, unsigned char* BUFFER)
{
    spiSwitchSet(SD_CS);
    
    //SdFat sd;   //this seems to take 624 bytes on the stack

    DEBUG_PSTRLN(F("open_begin"));
    PrintStackSize();
    
    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
    {
        SdFile file;
        
        char* extension = fileName + strlen(fileName) - 3;
        Serial.println(extension);
        
        int openresult = OpenSub(file, fileName, currentPath);
        {
            //char fullPath[256]; //only allocated while absolutely necessary
            //strncpy(fullPath, currentPath, 256);
            //strncat(fullPath, fileName, 256);
    
            //Serial.println(fullPath);

            //DEBUG_PSTRLN(F("open"));
            //openresult = file.open(fullPath, O_RDONLY);   //can do path :) e.g "/DCIM/"

            //have to save it here as it's in the BUFFER and will be overwritten
            //if (openresult && !file.isSubDir())
            //    SaveBuffer(F("LastPath.txt"), fullPath, strlen(fullPath)+1, true);
        }

        if (openresult)
        {
          if (file.isSubDir())
            {//dir: enter
                DEBUG_PSTRLN(F("Dir enter "));
                strcat(currentPath, fileName);
                strcat_P(currentPath, F("/"));
                file.close();
                spiSwitchSet(TFT_CS);
                return 2;
            }

            //note: not necessary at the END :oP but it will do for now
            //if (!strcasecmp(extension, "Z80"))
            if (!strcmp_P(extension, PSTR("Z80"))
             || !strcmp_P(extension, PSTR("z80")))
            {
                DEBUG_PSTRLN(F(".Z80 load "));
                PrintStackSize();
                z80FileLoad(&file, BUFFER, BUFFERSIZE);
                PrintStackSize();
                
                DEBUG_PSTRLN(F("close"));
                file.close();
                spiSwitchSet(TFT_CS);
                return 1;
            } else
            //if (!strcasecmp(extension, "SCR"))
            if (!strcmp_P(extension, PSTR("SCR"))
             || !strcmp_P(extension, PSTR("scr")))
            {//screen dump
                DEBUG_PSTRLN(F(".SCR load "));
                file.read(RAM, 6912);
                file.close();
                spiSwitchSet(TFT_CS);
                return 1;
            } else
            if (!strcasecmp(extension, "SNA"))
            //if (!strcmp_P(extension, PSTR("SNA"))
            // || !strcmp_P(extension, PSTR("sna")))
            {//snapshot
                DEBUG_PSTRLN(F(".SNA load "));
                SnaFileLoad(&file, BUFFER, BUFFERSIZE);
                file.close();
                spiSwitchSet(TFT_CS);
                return 1;
            } else
            //if (!strcasecmp(extension, "POK"))
            if (!strcmp_P(extension, PSTR("POK")))
            {//pokes
                //ToDo: collect the options in the .pok file into a little list to pick from, and allow to toggle each one separately
            } else
            if (!strcmp_P(extension, PSTR("txt"))
             || !strcmp_P(extension, PSTR("TXT")))
            {
                DEBUG_PSTRLN(F(".TXT viewer "));
                //TextViewer(&file, BUFFER);
                int pathLen = strlen(currentPath);
                //filename is destroyed here
                TextViewer(&file, BUFFER + pathLen+1, BUFFERSIZE - (pathLen+1), fileName, currentPath);
                
                DEBUG_PSTRLN(F("close"));
                file.close();
                spiSwitchSet(TFT_CS);
                //return 1;
                return 0;
            } else
            {
                DEBUG_PSTRLN(F("unsupported"));
            }
            
            file.close();
        }
    }

    spiSwitchSet(TFT_CS);
    return 0;  
}

int ICACHE_FLASH_ATTR Delete(char* fileName, char* currentPath)
{
    spiSwitchSet(SD_CS);
    
    //SdFat sd;

    DEBUG_PSTRLN(F("delete_begin"));
    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
    {
        char fullPath[256]; //only allocated while absolutely necessary
        strncpy(fullPath, currentPath, 256);
        strncat(fullPath, fileName, 256);

        Serial.println(fullPath);

        //sd.remove(fullPath);  //doesn't seem to work

        SdFile file;
        if (!file.open(fullPath, O_READ | O_WRITE))
        {
            DEBUG_PSTRLN(F("open fail"));
            spiSwitchSet(TFT_CS);
            return -2;
        }

        if (!file.remove())
        {
            DEBUG_PSTRLN(F("remove fail"));
            spiSwitchSet(TFT_CS);
            return -3;
        }
    }
    
    spiSwitchSet(TFT_CS);
    return 0;
}

int ICACHE_FLASH_ATTR Copy(char* fileName, char* currentPath, char* targetName)
{
    return 0;
}

#ifdef USE_SPIFFS
int ICACHE_FLASH_ATTR ListDirSpiffs(char* currentPath, int startIndex, int numToShow, char* list[], char* ptr)
{
    int index = 0;
    int numEntries = 0;
    int numShown = 0;

    DEBUG_PSTR(F("list dir spiffs "));
    DEBUG_PRINTLN(currentPath);

    if (!SPIFFS.begin())
    {
        DEBUG_PSTRLN(F("spiffs begin fail"));
        return 0;
    }
    
    fs::Dir dir = SPIFFS.openDir("/");//currentPath);
    
    while(dir.next())
    {
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        
        Serial.print(numEntries, DEC);
        Serial.println(fileName.c_str());

        ++numEntries;

        if (startIndex > index++)
        {//not there yet
            //entry.close();
            continue;
        }
      
        fs::File entry = dir.openFile("r");

        //Serial.println(entry.name());

        if (numShown < numToShow)
        {
            list[numShown++] = ptr;
            //file.getName(ptr, 64);
            strncpy(ptr, fileName.c_str(), 64);
            
            //Serial.print(ptr);
            ptr += strlen(ptr) + 1;
        }
        
        entry.close();    
    }

    SPIFFS.end();
    
    Serial.print("num entries:");
    Serial.println(numEntries, DEC);
    return numEntries;  //number of all files in the folder
}
#endif

int ICACHE_FLASH_ATTR FileMenu(bool spiffs)
{
const char* bottomLine = PSTR("Fire:Open  Caps+0:Delete  Fire2:Menu  Esc:Back");
    
    unsigned char BUFFER[BUFFERSIZE];    //only allocate it here when it's needed to keep more dynamic memory free
    char* currentPath = (char*)&BUFFER;
    bool navNeedsInit = true;
    bool navNeedsRead = true;
    bool navNeedsRender = true;
    int lastHighlightedEntry = -1;

    char* navList[NUM_SHOW_ENTRIES];
    char numNavEntries = 0;
    int navStartIndex = -1;      //in dir
    int navCurrentIndex = 0;    //in dir

    bool keyRepeating = false;
    unsigned long keyRepeatMillis = -1;
    char lastPressedKey = 0;

    //DEBUG_PSTRLN(F("Free heap: "));
    //DEBUG_PRINTLN(ESP.getFreeHeap());
    //DEBUG_PSTRLN(F("Free stack: "));
    //DEBUG_PRINTLN(ESP.getFreeContStack());
    PrintStackSize();

    while(true)
    {
        if (navNeedsInit)
        {   //init my variables
            navNeedsInit = false;

            navCurrentIndex = 0;
            numNavEntries = 0;
            
            int loadedLen = LoadBuffer(F("LastPath.txt"), currentPath, BUFFERSIZE);
            if (loadedLen > 0)
            {//success
                DEBUG_PSTR(F("LastPath="));
                DEBUG_PRINTLN(currentPath);
                PrintStackSize();
                
                //separate path and filename
                char* lastDash = currentPath + loadedLen-2;
                while(lastDash >= currentPath && *lastDash != '/')
                {
                    lastDash--;
                }
                
                char fileName[64];
                
                //find navCurrentIndex of the fileName in the folder
                if (lastDash < currentPath)
                {//not found; filename starts at currentPath
                    strncpy(fileName, currentPath, 64);
                    navCurrentIndex = FindFileIndex(F("/"), fileName);
                    strcpy_P(currentPath, F("/"));      //start in the root folder
                } else
                {//filename starts at lastDash+1
                    strncpy(fileName, lastDash+1, 64);
                    *(lastDash+1) = 0;
                    navCurrentIndex = FindFileIndex(currentPath, fileName);
                }
                PrintStackSize();
                
                numNavEntries = navCurrentIndex;    //workaround for the check ;)
            } else
            {
                //currentPath = (char*)&BUFFER;
                strcpy_P(currentPath, F("/"));      //start in the root folder
            }
            
            for(unsigned int i=0; i<sizeof(navList) / sizeof(navList[0]); i++)
                navList[i] = NULL;
                
            navNeedsRead = true;
            navNeedsRender = true;
        }
    
        //make sure it's valid
        if (navCurrentIndex > numNavEntries)
            navCurrentIndex = 0;
        //scroll the shown "window" to contain navCurrentIndex; if necessary, trigger a re-read
        int navPage = navCurrentIndex / NUM_SHOW_ENTRIES;
        if (navStartIndex != navPage * NUM_SHOW_ENTRIES)
        {
            navStartIndex = navPage * NUM_SHOW_ENTRIES;
            navNeedsRead = true;
        }
    
        if (navNeedsRender)
        {
            navNeedsRender = false;
            
            //check if also needs to refresh directory page
            if (navNeedsRead)
            {
                navNeedsRead = false;

#ifdef USE_SPIFFS
                if (spiffs)
                    numNavEntries = ListDirSpiffs(currentPath, navStartIndex, NUM_SHOW_ENTRIES, navList, (char*)&BUFFER + strlen(currentPath)+2);
                else
#endif                
                    numNavEntries = ListDir(currentPath, navStartIndex, NUM_SHOW_ENTRIES, navList, (char*)&BUFFER + strlen(currentPath)+2);
                
                lastHighlightedEntry = -1;
                PrintStackSize();
            }
        
            spiSwitchSet(TFT_CS);

            zxDisplayTft.setTextFont(0);
            zxDisplayTft.setTextSize(1);
            zxDisplayTft.setTextDatum(TL_DATUM);

            if (lastHighlightedEntry >= 0)
            {//minimal refresh; don't clear, only re-render the last and the new entry
                int i = lastHighlightedEntry;
                zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
                zxDisplayTft.drawString(navList[i], 10, 20+i*16);
                i = navCurrentIndex - navStartIndex;
                zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLUE);
                zxDisplayTft.drawString(navList[i], 10, 20+i*16);
            } else
            {
                zxDisplayTft.fillScreen(TFT_BLACK);
                
                for(int i=0; i<NUM_SHOW_ENTRIES; i++)
                {
                    if (navList[i] != NULL)
                    {
                        if (i == navCurrentIndex - navStartIndex)
                            zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLUE);
                        else
                            zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
                        zxDisplayTft.drawString(navList[i], 10, 20+i*16);
                    }
                }
                
                {
                    char buf[128];
                    strcpy_P(buf, (PGM_P)bottomLine);
                    zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    zxDisplayTft.setTextDatum(MC_DATUM);
                    zxDisplayTft.drawString(buf, 160, 230);
                }
            }
            lastHighlightedEntry = navCurrentIndex - navStartIndex;
            
            zxDisplayTft.setTextDatum(TL_DATUM);
            zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
            {
                char buf[32];
                strcpy_P(buf, spiffs ? PSTR("SPIFFS Directory") : PSTR("SD Directory"));
                zxDisplayTft.drawString(buf, 120, 0);
                sprintf_P(buf, PSTR("P%d: %d/%d "), navPage, navCurrentIndex, numNavEntries);
                zxDisplayTft.drawString(buf, 250, 0);
                zxDisplayTft.drawString(currentPath, 0, 0);
            }
        }
    
        unsigned long now = millis();
        UpdateInputs(false);
        delay(1);   //power saving
            
        if (MustWaitForInputClear())
        {
            if (now < keyRepeatMillis || keyRepeatMillis <= 0)
                continue;   //keep waiting
            //exceeded, start repeating
            keyRepeating = true;
        } else
        {
            keyRepeating = false;
        }

        char pressedKey = GetPressedKey();
        if (pressedKey != lastPressedKey 
         && pressedKey > 0x3)   //don't print shifts; i can detect if they are pressed or released if i need to
        {
            //ToDo: either set it as filter (enough as a first letter),
            // or simply try to find the first entry from the current one that starts with it 
            // (make sure don't get into an infinite loop if there isn't any)
        }
        lastPressedKey = pressedKey;

        if (KEMPSTONJOYSTICK & BUTTON_ESC )
        {
            waitForInputClear = true;
            keyRepeatMillis = -1;
            navNeedsRender = true;
            
            //up a folder
            int len = strlen(currentPath);
            if (len <= 1)
                return -1;  //back to main menu
            char* lastDash = currentPath + len-2;
            while(lastDash > currentPath && *lastDash != '/')
            {
                lastDash--;
            }

            char folderName[64];
            strncpy(folderName, lastDash+1, 64);
            folderName[strlen(folderName)-1] = 0; //remove closing '/'
           
            *(lastDash+1) = 0;  //terminate currentpath
            Serial.println(currentPath);
            Serial.println(folderName);

            //ToDo: find the index of the folder we just exited but this will do for now
            //navCurrentIndex = 0;
            navCurrentIndex = FindFileIndex(currentPath, folderName);
            numNavEntries = navCurrentIndex;    //workaround for the check ;)
            
            navNeedsRead = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_UP
           || IsKeyPressed(1,0) )   //Q
        {
            navCurrentIndex = navCurrentIndex > 0 ? --navCurrentIndex : numNavEntries-1;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            navNeedsRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_DOWN
           || IsKeyPressed(2,0) )   //A
        {
            navCurrentIndex = (++navCurrentIndex) % numNavEntries;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            navNeedsRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_RIGHT )   //page down
        {
            navCurrentIndex = (navCurrentIndex + NUM_SHOW_ENTRIES) % numNavEntries;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            navNeedsRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_LEFT )    //page up
        {
            //navCurrentIndex = navCurrentIndex > 0 ? --navCurrentIndex : numNavEntries-1;
            navCurrentIndex = (navCurrentIndex - NUM_SHOW_ENTRIES + numNavEntries) % numNavEntries;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            navNeedsRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            //ToDo: if folder, step into. if (supported) file, load
            int i = navCurrentIndex - navStartIndex;
            DEBUG_PSTR(F("LOAD "));
            Serial.println(navList[i]);
            waitForInputClear = true;
            keyRepeatMillis = -1;
            navNeedsRender = true;
            navNeedsRead = true;
            
            switch (Open(navList[i], currentPath, BUFFER))
            {
                case 0: //nothing happened or failed
                    break;
                case 1: //load success
                    //Note: this destroys the buffer, including currentPath!
                    DEBUG_PSTRLN(F("loaded."));
                    return -2;
                case 2: //entered/exited folder
                    navCurrentIndex = 0;
                    navNeedsRead = true;
                    break;
                default:
                    navNeedsInit = true;
                    break;
            }
        } else
        if (KEMPSTONJOYSTICK & BUTTON_FIRE2 )    //"right click"
        {
            int i = navCurrentIndex - navStartIndex;
            char key = Message(PSTR("File"), PSTR("D:Delete  R:Rename  C:Copy"));
            //the latter two need a name entry dialog
            //to delete a file, sd.remove("test.txt")
            //to rename, https://codebender.cc/example/SdFat/rename#rename.ino
            //to copy, use a buffer
            Serial.println(key, DEC);
            switch(key)
            {
                case 'd':
                    Delete(navList[i], currentPath);
                    break;
                case 'r':
                    //ToDo: ask for new name; copy and delete
                    break;
                case 'c':
                    //ask for target name
                    char newName[50];
                    strncpy(newName, navList[i], 50);
                    if(TextEntry(F("Copy to"), newName, 50) == 1)
                    {
                        //ToDo
                    }
                    break;
                default:
                    break;
            }
            waitForInputClear = true;
            keyRepeatMillis = -1;
            navNeedsRender = true;
            lastHighlightedEntry = -1;  //full re-render
        } else
        {
            //ToDo: any other button on the keyboard to type name (display entry field) and instant search+jump
        }    
    }
    return -1;
}


//large letter ok keys, mode L/mode C
//smaller and simpler storage, making use of these being single characters
const char key_table[] PROGMEM = "1234567890QWERTYUIOPASDFGHJKL  ZXCVBNM  ";

int ICACHE_FLASH_ATTR KeyboardMenu()
{
	//white, mode K
	const char* const key_table_cmd[] = 
    { 
        PSTR("!"), PSTR("@"), PSTR("#"), PSTR("$"), PSTR("%"), PSTR("&"), PSTR("'"), PSTR("("), PSTR(")"), PSTR("_"),
        PSTR("PLOT"), PSTR("DRAW"), PSTR("REM"), PSTR("RUN"), PSTR("RAND"), PSTR("RETURN"), PSTR("IF"), PSTR("INPUT"), PSTR("POKE"), PSTR("PRINT"),
        PSTR("NEW"), PSTR("SAVE"), PSTR("DIM"), PSTR("FOR"), PSTR("GOTO"), PSTR("GOSUB"), PSTR("LOAD"), PSTR("LIST"), PSTR("LET"), PSTR("ENTER"),
        PSTR("CAPS"), PSTR("COPY"), PSTR("CLEAR"), PSTR("CONT"), PSTR("CLS"), PSTR("BORDER"), PSTR("NEXT"), PSTR("PAUSE"), PSTR("SYMBOL"), PSTR("BREAK")
    };
    
	//green, above, mode E (without shift)
	const char* const key_table_above[] =
	{
		PSTR("EDIT"), PSTR("C_LOCK"), PSTR("TRUV"), PSTR("INVV"), PSTR("<"), PSTR("v"), PSTR("^"), PSTR(">"), PSTR("GRAPHICS"), PSTR("DELETE"),
		PSTR("SIN"), PSTR("COS"), PSTR("TAN"), PSTR("INT"), PSTR("RND"), PSTR("STR$"), PSTR("CHR$"), PSTR("CODE"), PSTR("PEEK"), PSTR("TAB"),
		PSTR("READ"), PSTR("RESTORE"), PSTR("DATA"), PSTR("SGN"), PSTR("ABS"), PSTR("SQR"), PSTR("VAL"), PSTR("LEN"), PSTR("USR"), PSTR(""),
		PSTR(""), PSTR("LN"), PSTR("EXP"), PSTR("LPRINT"), PSTR("LLIST"), PSTR("BIN"), PSTR("INKEY$"), PSTR("PI"), PSTR(""), PSTR("")
	};

	//red, below (mode E with either shift)
	const char* const key_table_below[] =
	{
		PSTR("DEF FN"), PSTR("FN"), PSTR("LINE"), PSTR("OPEN#"), PSTR("CLOSE#"), PSTR("MOVE"), PSTR("ERASE"), PSTR("POINT"), PSTR("CAT"), PSTR("FORMAT"),
		PSTR("ASN"), PSTR("ACS"), PSTR("ATN"), PSTR("VERIFY"), PSTR("MERGE"), PSTR("["), PSTR("]"), PSTR("IN"), PSTR("OUT"), PSTR("(C)"),
		PSTR("~"), PSTR("|"), PSTR("\\"), PSTR("{"), PSTR("}"), PSTR("CIRCLE"), PSTR("VAL$"), PSTR("SCRN$"), PSTR("ATTR"), PSTR(""),
		PSTR(""), PSTR("BEEP"), PSTR("INK"), PSTR("PAPER"), PSTR("FLASH"), PSTR("BRIGHT"), PSTR("OVER"), PSTR("INV"), PSTR(""), PSTR("")
	};
  
    //red, on (with symbol shift)
    const char* const key_table_on_red[] =
    {
        PSTR("!"), PSTR("@"), PSTR("#"), PSTR("$"), PSTR("%"), PSTR("&"), PSTR("'"), PSTR("("), PSTR(")"), PSTR("_"),
        PSTR("<="), PSTR("<>"), PSTR(">="), PSTR("<"), PSTR(">"), PSTR("AND"), PSTR("OR"), PSTR("AT"), PSTR(";"), PSTR("\""),
        PSTR("STOP"), PSTR("NOT"), PSTR("STEP"), PSTR("TO"), PSTR("THEN"), PSTR("^"), PSTR("-"), PSTR("+"), PSTR("="), PSTR(""),
        PSTR(""), PSTR(":"), PSTR("£"), PSTR("?"), PSTR("/"), PSTR("*"), PSTR(","), PSTR("."), PSTR("SYMBOL"), PSTR("")
    };

    //colours over the top row, mode G (after CAPS+9)
    const char* const key_table_above_2[] = 
    { 
        PSTR("BLUE"), PSTR("RED"), PSTR("MAGENTA"), PSTR("GREEN"), PSTR("CYAN"), PSTR("YELLOW"), PSTR("WHITE"), PSTR(""), PSTR(""), PSTR("BLACK"),
    };
  

    {
        spiSwitchSet(TFT_CS);
        
        zxDisplayTft.fillScreen(TFT_BLACK);
        zxDisplayTft.setTextFont(0);
        zxDisplayTft.setTextSize(1);
        zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
        zxDisplayTft.setTextDatum(TL_DATUM);

        char buf[32];
        strcpy_P(buf, F("ZX Spectrum Keyboard Layout"));
        zxDisplayTft.drawString(buf, 150, 0);

        int row_offset[] = {0,10,20,0};
        
        for(int i=0; i<40; i++)
        {
            int16_t row = i/10;
            int16_t x = row_offset[row] + (i%10) * 30;
            int16_t y = 50 + row * 40;

            zxDisplayTft.fillRoundRect(x,y, 24, 20, 3, TFT_DARKGREY);
            
            zxDisplayTft.setTextColor(TFT_WHITE, TFT_DARKGREY);
            zxDisplayTft.setTextSize(2);
            //strcpy_P(buf, key_table[i]);
            buf[0] = pgm_read_byte(&key_table[i]);
            buf[1] = 0;
            zxDisplayTft.drawString(buf, x+3, y+3);
            
            zxDisplayTft.setTextSize(1);
            strcpy_P(buf, key_table_cmd[i]);
            zxDisplayTft.drawString(buf, x+10, y+10);
            
            strcpy_P(buf, key_table_above[i]);
            zxDisplayTft.setTextColor(TFT_GREEN, TFT_BLACK);
            zxDisplayTft.drawString(buf, x+0, y-10);
            
            strcpy_P(buf, key_table_below[i]);
            zxDisplayTft.setTextColor(TFT_RED, TFT_BLACK);
            zxDisplayTft.drawString(buf, x+0, y+22);
            
        }
    }

    char lastPressedKey = 0;
    
    while(true)
    {
        UpdateInputs(false);
        delay(1);   //power saving

        char pressedKey = GetPressedKey();
        if (pressedKey != lastPressedKey 
         && pressedKey > 0x3)   //don't print shifts; i can detect if they are pressed or released if i need to
        {
            Serial.print(pressedKey);
        }
        lastPressedKey = pressedKey;
        
        
        if (MustWaitForInputClear()) continue;
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC
         || KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            waitForInputClear = true;
            return -1;
        }
    }
           
    return -1;
}
/*
int ICACHE_FLASH_ATTR JoystickMenu()
{
    spiSwitchSet(TFT_CS);
    
    zxDisplayTft.fillScreen(TFT_BLACK);
    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
    zxDisplayTft.setTextDatum(TL_DATUM);

    {
        char buf[32];
        strcpy_P(buf, F("Joystick Config"));
        zxDisplayTft.drawString(buf, 150, 0);
    }

    //ToDo: show the current key assignment,
    // press what keys the joystick should simulate (incl. fire 1/2/...3?)

    waitForInputClear = true;
    
    while(true)
    {
        UpdateInputs(false);
        delay(1);   //power saving
        
        if (MustWaitForInputClear()) continue;
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC
         || KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            waitForInputClear = true;
            return -1;
        }
    }
    
    return -1;
}
*/


//Waits for input release and first press, returns which button (or -1 if it was joystick)
char ICACHE_FLASH_ATTR MessageInput()
{
    waitForInputClear = true;
    
    while(true)
    {
        UpdateInputs(false);
        delay(1);   //power saving
        
        if (MustWaitForInputClear()) continue;
        
        if (GetPressedKey() != 0
         || KEMPSTONJOYSTICK != 0)
        {
            //waitForInputClear = true;
            return -1;
        }
    }
  
    //waitForInputClear = true;
    return KEMPSTONJOYSTICK ? -1 : GetPressedKey();
}

char ICACHE_FLASH_ATTR Message(const char *text, bool wait)    //ToDo: option to return immediately (e.g. "Loading")
{
    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
    zxDisplayTft.setTextDatum(MC_DATUM);
    int w = zxDisplayTft.textWidth(text);
    int h = zxDisplayTft.fontHeight();

    zxDisplayTft.fillRect(160-(w+16)/2, 120-(h+16)/2, w+16, h+16, TFT_BLACK);
    zxDisplayTft.drawRect(160-(w+16)/2+1, 120-(h+16)/2+1, w+14, h+14, TFT_WHITE);
    zxDisplayTft.drawString(text, 160, 120);

    if (wait)
    {
        return MessageInput();
    }
    
    return 0;
}


//char ICACHE_FLASH_ATTR Message(const __FlashStringHelper* str1, const __FlashStringHelper* str2)
char ICACHE_FLASH_ATTR Message(const char* str1, const char* str2, bool wait)
{
    int numLines = 0;
    if (str1 != nullptr)   numLines++;
    if (str2 != nullptr)   numLines++;
    
    //int length = strlen_P((PGM_P)str); // cast it to PGM_P, which is basically const char *
    //if (length == 0) return 0;

    char buf[128];

    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    int _h = zxDisplayTft.fontHeight();
    int h = _h * numLines;

    strncpy_P(buf, (PGM_P)str2, sizeof(buf));
    int w1 = zxDisplayTft.textWidth(buf);
    strncpy_P(buf, (PGM_P)str1, sizeof(buf));
    int w2 = zxDisplayTft.textWidth(buf);
    int w = max(w1, w2);

    zxDisplayTft.fillRect(160-(w+16)/2, 120-(h+16)/2, w+16, h+16, TFT_BLACK);
    zxDisplayTft.drawRect(160-(w+16)/2+1, 120-(h+16)/2+1, w+14, h+14, TFT_WHITE);

    if (numLines==1)
    {
        zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
        zxDisplayTft.setTextDatum(MC_DATUM);
        zxDisplayTft.drawString(buf, 160, 120);
    } else
    {
        zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
        zxDisplayTft.setTextDatum(TC_DATUM);
        zxDisplayTft.drawString(buf, 160, 120-_h);
        
        strncpy_P(buf, (PGM_P)str2, sizeof(buf));
        zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
        zxDisplayTft.setTextDatum(TC_DATUM);
        zxDisplayTft.drawString(buf, 160, 120);
    }

    if (wait)
    {
        return MessageInput();
    }
       
    return 0;
}

char ICACHE_FLASH_ATTR TextEntry(const char* label, char* inbuf, int maxLength)
{
    //char buf[64];

    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    int _h = zxDisplayTft.fontHeight();
    int h = _h * 2;

    #define CHAR_WIDTH (5+1)

    //strncpy_P(buf, (PGM_P)label, sizeof(buf));
    int w1 = zxDisplayTft.textWidth(label);
    int w2 = maxLength * CHAR_WIDTH;
    int w = max(w1, w2);

    zxDisplayTft.fillRect(160-(w+16)/2, 120-(h+16)/2, w+16, h+16, TFT_BLACK);
    zxDisplayTft.drawRect(160-(w+16)/2+1, 120-(h+16)/2+1, w+14, h+14, TFT_WHITE);

    int x0 = 160-(w+8)/2;
    zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
    zxDisplayTft.setTextDatum(TL_DATUM);
    zxDisplayTft.drawString(label, x0, 120-_h);
    
    zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
    zxDisplayTft.setTextDatum(TL_DATUM);
    
    waitForInputClear = true;

    unsigned long keyRepeatMillis = -1;
    bool keyRepeating = false;
    
    int curPos = strlen(inbuf);
    char lastKey = 0;
    while(true)
    {
        zxDisplayTft.fillRect(x0, 120, w, h, TFT_BLACK);
        zxDisplayTft.drawString(inbuf, x0, 120);
    
        zxDisplayTft.drawString("_", x0 + CHAR_WIDTH*curPos, 120);
        
        unsigned long now = millis();
        UpdateInputs(false);
        delay(1);   //power saving
        
        if (MustWaitForInputClear())
        {
            if (now < keyRepeatMillis || keyRepeatMillis <= 0)
                continue;   //keep waiting
            //exceeded, start repeating
            keyRepeating = true;
        } else
        {
            keyRepeating = false;
        }

        if (KEMPSTONJOYSTICK & BUTTON_ESC)
        {
            waitForInputClear = true;
            return -1;
        }
        if (KEMPSTONJOYSTICK & BUTTON_FIRE)
        {
            waitForInputClear = true;
            return 0;
        }
        if (KEMPSTONJOYSTICK & BUTTON_FIRE)
        {
            waitForInputClear = true;
            return -2;
        }
        
        if (KEMPSTONJOYSTICK & BUTTON_LEFT)
        {
            if (curPos > 0) curPos--;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            continue;
        }
        if (KEMPSTONJOYSTICK & BUTTON_RIGHT)
        {
            if (curPos < strlen(inbuf)) curPos++;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
            continue;
        }
       
        lastKey = GetPressedKey();
        switch (lastKey)
        {
            case 0: //none
                break;
                
            case 1: //sym
            case 2: //caps
            case 3: //caps+sym
                break;
                
            case '\n':  //enter
                return 1;
            case '\r':  //shift+enter
                return 2;
                
            case '\b':  //del
                inbuf[curPos] = 0;
                strncpy(&inbuf[curPos], &inbuf[curPos+1], maxLength-curPos);
                curPos--;
                waitForInputClear = true;
                keyRepeatMillis = now + (keyRepeating ? 100 : 500);
                break;
                
            default:
                if (strlen(inbuf) < maxLength-1)
                {
                    inbuf[curPos++] = lastKey;
                    inbuf[curPos] = 0;  //temp
                }
                waitForInputClear = true;
                keyRepeating = false;
                keyRepeatMillis = now + 500;
                break;
        }
    }
    
    return 0;   //can't really happen
}

int ICACHE_FLASH_ATTR SaveZ80(const char *fileName, bool setLast)
{
    unsigned char BUFFER[BUFFERSIZE];    //only allocate it here when it's needed to keep more dynamic memory free
    
    //SdFat sd;
    SdFile file;

    spiSwitchSet(SD_CS);
    
    DEBUG_PSTR(F("saveZ80_begin "));
    DEBUG_PRINTLN(fileName);
    
    if (!sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0))) 
    {
        spiSwitchSet(TFT_CS);
        DEBUG_PSTRLN(F("FAIL"));
        return -1;
    }

    if (setLast)
    {
        SaveBuffer(F("LastPath.txt"), fileName, strlen(fileName)+1, true);
    }

    if (!file.open(fileName, O_TRUNC | O_RDWR | O_CREAT))
    {
        DEBUG_PSTRLN(F("OPEN FAIL"));
        spiSwitchSet(TFT_CS);
        return -2;
    }

    if ( z80FileSave(&file, BUFFER, BUFFERSIZE) != 0)
    {
        DEBUG_PSTRLN(F("SAVE FAIL"));
        spiSwitchSet(TFT_CS);
        return -3;
    }

    DEBUG_PSTRLN(F("saved."));
    //z80filesave already closes the file
    //file.close();
    
    spiSwitchSet(TFT_CS);
    return 0;
}

int ICACHE_FLASH_ATTR LoadZ80(const char *fileName, bool setLast)
{
    unsigned char BUFFER[BUFFERSIZE];    //only allocate it here when it's needed to keep more dynamic memory free
    
    //SdFat sd;
    SdFile file;

    spiSwitchSet(SD_CS);
    
    DEBUG_PSTR(F("loadZ80_begin "));
    DEBUG_PRINTLN(fileName);
    
    if (!sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0))) 
    {
        spiSwitchSet(TFT_CS);
        DEBUG_PSTRLN(F("FAIL"));
        return -1;
    }

    if (!file.open(fileName, O_RDONLY))   //can do path :) e.g "/DCIM/"
    {
        DEBUG_PSTRLN(F("OPEN FAIL"));
        spiSwitchSet(TFT_CS);
        return -2;
    }

    if ( z80FileLoad(&file, BUFFER, BUFFERSIZE) != 0)
    {
        DEBUG_PSTRLN(F("LOAD FAIL"));
        spiSwitchSet(TFT_CS);
        return -3;
    }

    DEBUG_PSTRLN(F("loaded."));
    file.close();
    
    spiSwitchSet(TFT_CS);
    return 0;   //success
}

__inline char* strnchr(char* string, char chr, int maxLength)
{
    char* c = string;
    
    while(maxLength-- > 0 && *c != chr && *c != 0)
        c++;
    if (*c==chr)
        return c;
    return 0;
}

int ICACHE_FLASH_ATTR TextViewer(SdFile *file, unsigned char *BUFFER, int bufferSize, char* fileName, char* currentPath)
{
    int filesize = file->fileSize();
    DEBUG_PRINTLN(filesize);
    
    bool needRead = true;
    bool needRender = true;

    int readStart = 0;
    int readLength = 0;
    int readLineIndex = 0;
    int posX = 0;
    int posY = 0;

    bool keyRepeating = false;
    unsigned long keyRepeatMillis = -1;

    waitForInputClear = true;

    //file->close();

    while(true)
    {
        if (needRead)
        {
            needRead = false;

            DEBUG_PSTR(F("seek "));
            DEBUG_PRINTLN(readStart);
            
            spiSwitchSet(SD_CS);
/*            
            SdFat sd;
            DEBUG_PSTRLN(F("open_begin"));
            if (sd.begin(SD_CS, SPISettings(
                          SPI_SPEED_SD,
                          MSBFIRST,
                          SPI_MODE0)))
            {
                int openresult = 0;
                {
                    char* fullPath = (char*)BUFFER;     //can override BUFFER here, we are going to refill it right after
                    strncpy(fullPath, currentPath, bufferSize);
                    strncat(fullPath, fileName, bufferSize);
            
                    Serial.println(fullPath);
                    
                    DEBUG_PSTRLN(F("open"));
                    openresult = (file->open(fullPath, O_RDONLY));   //can do path :) e.g "/DCIM/"
                }

                if (openresult)
                {
                    file->seekSet(readStart);
                    readLength = file->read(BUFFER, bufferSize-1);
                    BUFFER[bufferSize-1] = 0;
                    DEBUG_PSTR(F("read "));
                    DEBUG_PRINTLN(readLength);
                    file->close();
                }
            }
*/            
            file->seekSet(readStart);
            readLength = file->read(BUFFER, bufferSize-1);
            BUFFER[bufferSize-1] = 0;
            DEBUG_PSTR(F("read "));
            DEBUG_PRINTLN(readLength);
        }

        if (needRender)
        {
            needRender = false;
            
            spiSwitchSet(TFT_CS);
            
            zxDisplayTft.fillScreen(TFT_BLACK);
            zxDisplayTft.setTextFont(0);
            zxDisplayTft.setTextSize(1);
            zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
            zxDisplayTft.setTextDatum(TL_DATUM);

#define lineHeight      10
#define maxCharsPerLine 50      //text wrap

            char* lineStart = (char*)BUFFER;
            //render line by line
            int line = readLineIndex;
            int charsLeft = readLength-1;
            bool wasOffScreen = posY + line*lineHeight < -lineHeight;
            int firstDrawnLineStart = readStart;
            int firstDrawnLineIndex = line;

            while(charsLeft > 0)
            {
                if (posY + line*lineHeight >= 240)  //stop if went outside the bottom of screen (take scroll into account)
                    break;
                char* eol = strnchr(lineStart, '\n', min(charsLeft, maxCharsPerLine));
                if (eol == 0)
                {//not found; render what's left
                    eol = lineStart + min(charsLeft, maxCharsPerLine);
                }
                char atEol = *eol; //save it to support wrap
                *eol = 0;
                if (posY + line*lineHeight >= -lineHeight)   //don't bother rendering above screen (if we scrolled down)
                {
                    zxDisplayTft.drawString(lineStart, posX, posY + line*lineHeight);
                    if (wasOffScreen)
                    {
                        wasOffScreen = false;
                        firstDrawnLineStart = readLineIndex + (int)(lineStart - (char*)BUFFER);
                        firstDrawnLineIndex = line;
                    }
                }
                *eol = atEol;
                line++;
                if (atEol != '\n')
                    eol--;
                charsLeft -= eol+1 - lineStart;
                lineStart = eol+1;
            }
            //DEBUG_PRINTLN(posY + line*10);
            //DEBUG_PRINTLN(charsLeft);

            //ToDo: if rendering reaches the end (on the bottom, due to posY) AND there's more in the file,
            // increase readStart (to the beginning of the first rendered line) and trigger a reload of 
/*            
* disabled for now; once switched spi to screen the file is lost and the file name was overwritten in the buffer so can't reopen it
* (switching back and forth will also need solving for an image viewer)
            if (posY + line*10 < 240 && charsLeft <= 0 && readStart + readLength < filesize)
            {
                readStart = firstDrawnLineStart;
                readLineIndex = firstDrawnLineIndex;
                needRead = true;
                DEBUG_PSTR(F("need to read from "));
                DEBUG_PRINTLN(readStart);
            }
*/            
            // OR, if the rendering (on top) starts lower than the top of the window (due to posY) and there's more before (readStart > 0),
            // decrease loadStart (non-trivial without knowing the length of previous lines, but can make an estimate) and trigger a reload
        }
        
        unsigned long now = millis();
        UpdateInputs(false);
        delay(1);   //power saving
            
        if (MustWaitForInputClear())
        {
            if (now < keyRepeatMillis || keyRepeatMillis <= 0)
                continue;   //keep waiting
            //exceeded, start repeating
            keyRepeating = true;
        } else
        {
            keyRepeating = false;
        }
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC
         || KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            waitForInputClear = true;
            return -1;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_DOWN )
        {
            posY -= 5;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 25 : 250);
            needRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_UP )
        {
            posY += 5;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 25 : 250);
            needRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_RIGHT )   //page down
        {
            posX -= 5;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 25 : 250);
            needRender = true;
        } else
        if (KEMPSTONJOYSTICK & BUTTON_LEFT )    //page up
        {
            posX += 5;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 25 : 250);
            needRender = true;
        }
    }
    
    return 0;
}

//return 0 if success
int ICACHE_FLASH_ATTR SaveBuffer(const char *fileName, const char *buf, int len, bool SD_already_open)
{
    //SdFat sd;
    SdFile file;

    DEBUG_PSTR(F("save_begin "));
    DEBUG_PRINTLN(fileName);
    
    if (!SD_already_open)
    {
        spiSwitchSet(SD_CS);
        if (!sd.begin(SD_CS, SPISettings(
                      SPI_SPEED_SD,
                      MSBFIRST,
                      SPI_MODE0))) 
        {
            spiSwitchSet(TFT_CS);
            DEBUG_PSTRLN(F("FAIL"));
            return -1;
        }
    }

    DEBUG_PSTRLN(F("open"));
    if (!file.open(fileName, O_CREAT | O_WRITE))
    {
        DEBUG_PSTRLN(F("OPEN FAIL"));
        if (!SD_already_open)
            spiSwitchSet(TFT_CS);
        return -2;
    }

    DEBUG_PSTRLN(F("write"));
    //this part is quite slow
    if (file.write(buf, len) != len)
    {
        DEBUG_PSTRLN(F("SAVE FAIL"));
        if (!SD_already_open)
            spiSwitchSet(TFT_CS);
        return -3;
    }

    DEBUG_PSTRLN(F("flush"));
    file.flush();
    DEBUG_PSTRLN(F("saved."));
    file.close();

    if (!SD_already_open)
        spiSwitchSet(TFT_CS);
        
    return 0;
}

//return length loaded
int ICACHE_FLASH_ATTR LoadBuffer(const char *fileName, char *buf, int maxLength)
{
    //SdFat sd;
    SdFile file;

    DEBUG_PSTR(F("load_begin "));
    DEBUG_PRINTLN(fileName);

    //unsigned long millis1 = millis();
    
    spiSwitchSet(SD_CS);
    
    //unsigned long millis2 = millis();

    //this takes ~20-25ms
    if (!sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0))) 
    {
        spiSwitchSet(TFT_CS);
        DEBUG_PSTRLN(F("FAIL"));
        return -1;
    }

    //unsigned long millis3 = millis();

    // this takes ~100ms!
    if (!file.open(fileName, O_RDONLY))
    {
        //unsigned long millis4 = millis();
        
        DEBUG_PSTRLN(F("OPEN FAIL"));
        spiSwitchSet(TFT_CS);
/*
    Serial.printf(" %d %d %d \n"
                , millis2 - millis1
                , millis3 - millis2
                , millis4 - millis3
                );
*/        
        return -2;
    }

    //unsigned long millis4 = millis();

    //0ms
    int filesize = file.fileSize();
    //DEBUG_PRINTLN(filesize);

    //unsigned long millis5 = millis();

    int len = min(filesize, maxLength);
    //~5ms
    int readLength = file.read(buf, len);
    //DEBUG_PRINTLN(readLength);

    //unsigned long millis6 = millis();

    //0ms
    file.close();

    //unsigned long millis7 = millis();
    
    spiSwitchSet(TFT_CS);

    //unsigned long millis8 = millis();
/*
    Serial.printf(" %d %d %d %d %d %d %d \n"
                , millis2 - millis1
                , millis3 - millis2
                , millis4 - millis3
                , millis5 - millis4
                , millis6 - millis5
                , millis7 - millis6
                , millis8 - millis7
                );
*/
    
    return readLength;
}

//lookup table of how many bits are on if the byte value is...
const byte numBitsOn[256] PROGMEM = {
     0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
     1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
     1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
     1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
     2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
     3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
     3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
     4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8 };
     
void ICACHE_FLASH_ATTR BuildMiniScreen()
{
//    unsigned char BUFFER[32*24];    //one byte(pixel) for each 8x8 block

    DEBUG_PSTRLN(F("MINI_SCREEN"));
    
    for(int b=0; b<3; b++)  //bank 0,1,2
    {
        for(int y=0; y<8; y++)
        {
            for(int x=0; x<32; x++)
            {
                int blockAddr = b*32*8*8 + y*32 + x;    //address of the top byte of the block
                byte num1s = 0;
                for(int i=0; i<8; i++)
                {
                    byte mem = RAM[blockAddr + i*32*8];
                    num1s += pgm_read_byte(&numBitsOn[mem]);
                    //Serial.printf(" %d", mem);
                }
                //num1s = 0..64;  can use 0..3F
                byte alpha = num1s<0x40 ? (num1s<<2) : 0xFF;

                int colAddr = (b*8+y)*32 + x;
                byte col = RAM[32*192 + colAddr];   //0x1800
                //bits: [flash|bright|bbb|fff]   (GRB GRB)

                if ((col & 0x80) && (zxDisplayScanToggle & 16))
                {//swap colours
                    byte fc = col&0x7;
                    byte bc = (col>>3)&0x7;
                    col = col&0xC0 + fc<<3 + bc;
                }
                
                uint16_t R = ((col&0x02) ? alpha : 0) + ((col&0x10) ? 255-alpha : 0);
                uint16_t G = ((col&0x04) ? alpha : 0) + ((col&0x20) ? 255-alpha : 0);
                uint16_t B = ((col&0x01) ? alpha : 0) + ((col&0x08) ? 255-alpha : 0);

                if (col&0x40 == 0)
                {//not bright: half values
                    R = R>>1;
                    G = G>>1;
                    B = B>>1;
                }

                //the TFT_eSPI colour format is 16 bits (2 bytes), 565 RGB: [RRRRRGGG|GGGBBBBB]
                uint16_t col16 = 0;
                //col16 += (R >> 3) << 11;
                //col16 += (G >> 2) << 5;
                //col16 += (B >> 3);

                //Serial.printf("\n %d, %d, %d  %d,%d,%d  %d", num1s, alpha, col, R,G,B, col16);
                //ESP.wdtFeed();
               
                //need a max 8-bit colour palette to store the image in only one byte per pixel
                // https://en.wikipedia.org/wiki/8-bit_color
                // RRRGGGBB
                uint16_t col8 = 0;
                col8 += (R >> 5) << 5;
                col8 += (G >> 5) << 2;
                col8 += (B >> 6);
                
//                BUFFER[(b*8+y)*32 + x] = col8;

                //332RGB col to 565RGB conversion:
                col16 = (col8 & 0xE0) << 8;
                col16 += (col8 & 0x1C) << 6;
                col16 += (col8 & 0x03) << 3;
                
                zxDisplayTft.drawPixel(319-35+x, 150+(b*8+y), col16);
            }
            //Serial.println();
        }
    }
}

int ICACHE_FLASH_ATTR ExtrasMenu()
{
    DEBUG_PSTRLN(F("EXTRAS"));
    
    spiSwitchSet(TFT_CS);
    
    zxDisplayTft.fillScreen(TFT_BLACK);
    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
    zxDisplayTft.setTextDatum(TL_DATUM);

    {
        char buf[64];
        strcpy_P(buf, PSTR("ESP8266 Extras"));
        zxDisplayTft.drawString(buf, 150, 0);

        //ToDo: display diagnostics about the ESP8266 (e.g. heap, stack, frame rate, time, i2c, spi, sdcard, etc),
        //tests (e.g. inputs, display, wifi), 
        //maybe allow full reset
    }
    
    waitForInputClear = true;

    while(true)
    {
        UpdateInputs(false);
        delay(1);   //power saving
        
        if (MustWaitForInputClear()) continue;
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC
         || KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            waitForInputClear = true;
            return -1;
        }
    }
    
    return -1;
}


inline byte RdZ80(word16 A)        
{
  return (A < ROMSIZE ? pgm_read_byte(ROM + A) : RAM[A - ROMSIZE]);
}

int ICACHE_FLASH_ATTR Debugger()
{
    bool keyRepeating = false;
    unsigned long keyRepeatMillis = -1;
    char lastPressedKey = 0;
    bool needRender = true;
    
    word16 mem_addr = 0;

    char buf[64];
    
    DEBUG_PSTRLN(F("DEBUG"));

    //ToDo: display CPU state (registers), memory, and disassembly
    //allow standard debug: single stepping, step over/into/out, change memory
    
    spiSwitchSet(TFT_CS);
    
    zxDisplayTft.fillScreen(TFT_BLACK);
    zxDisplayTft.setTextFont(0);
    zxDisplayTft.setTextSize(1);
    zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
    zxDisplayTft.setTextDatum(TL_DATUM);

    {
        strcpy_P(buf, PSTR("Z80 Debugger"));
        zxDisplayTft.setTextColor(TFT_YELLOW, TFT_BLACK);
        zxDisplayTft.setTextDatum(TC_DATUM);
        zxDisplayTft.drawString(buf, 150, 0);

        strcpy_P(buf, PSTR("Spc:Step  Esc:Back"));
        zxDisplayTft.setTextDatum(MC_DATUM);
        zxDisplayTft.drawString(buf, 160, 230);
    }
    
    waitForInputClear = true;

    while(true)
    {
        if (needRender)
        {
            zxDisplayTft.setTextColor(TFT_WHITE, TFT_BLACK);
            zxDisplayTft.setTextDatum(TL_DATUM);
    
            sprintf_P(buf, PSTR("AF %04X"), state.AF.W);
            zxDisplayTft.drawString(buf, 10, 15);
            sprintf_P(buf, PSTR("BC %04X"), state.BC.W);
            zxDisplayTft.drawString(buf, 10, 25);
            sprintf_P(buf, PSTR("DE %04X"), state.DE.W);
            zxDisplayTft.drawString(buf, 10, 35);
            sprintf_P(buf, PSTR("HL %04X"), state.HL.W);
            zxDisplayTft.drawString(buf, 10, 45);
    
            sprintf_P(buf, PSTR("IX %04X"), state.IX.W);
            zxDisplayTft.drawString(buf, 60, 15);
            sprintf_P(buf, PSTR("IY %04X"), state.IY.W);
            zxDisplayTft.drawString(buf, 60, 25);
            sprintf_P(buf, PSTR("PC %04X"), state.PC.W);
            zxDisplayTft.drawString(buf, 60, 35);
            sprintf_P(buf, PSTR("SP %04X"), state.SP.W);
            zxDisplayTft.drawString(buf, 60, 45);
    
            sprintf_P(buf, PSTR("AF'%04X"), state.AF1.W);
            zxDisplayTft.drawString(buf, 110, 15);
            sprintf_P(buf, PSTR("BC'%04X"), state.BC1.W);
            zxDisplayTft.drawString(buf, 110, 25);
            sprintf_P(buf, PSTR("DE'%04X"), state.DE1.W);
            zxDisplayTft.drawString(buf, 110, 35);
            sprintf_P(buf, PSTR("HL'%04X"), state.HL1.W);
            zxDisplayTft.drawString(buf, 110, 45);
        
            sprintf_P(buf, PSTR("IFF %02X"), state.IFF);
            zxDisplayTft.drawString(buf, 10, 60);
            sprintf_P(buf, PSTR("I %02X"), state.I);
            zxDisplayTft.drawString(buf, 60, 60);
            sprintf_P(buf, PSTR("R %02X"), state.R);
            zxDisplayTft.drawString(buf, 110, 60);

            //code around PC
            for(int i=0; i<14; i++)
            {
                word16 addr = state.PC.W + i;
                byte b = RdZ80(addr);
                sprintf_P(buf, PSTR("%02X"), b);
                zxDisplayTft.drawString(buf, 10, 80+i*10);
            }

            //memory
            sprintf_P(buf, PSTR("%04X"), mem_addr);
            zxDisplayTft.drawString(buf, 170, 15);
            for(int y=0; y<20; y++)
            {
                for(int x=0; x<8; x++)
                {
                    word16 addr = mem_addr + y*8+x;
                    byte b = RdZ80(addr);
                    sprintf_P(buf, PSTR("%02X"), b);
                    zxDisplayTft.drawString(buf, 170+x*16, 25+y*10);
                }
            }
        }
        
        unsigned long now = millis();
        UpdateInputs(false);
        delay(1);   //power saving
            
        if (MustWaitForInputClear())
        {
            if (now < keyRepeatMillis || keyRepeatMillis <= 0)
                continue;   //keep waiting
            //exceeded, start repeating
            keyRepeating = true;
        } else
        {
            keyRepeating = false;
        }

        char pressedKey = GetPressedKey();
        switch(pressedKey)
        {
            case 0: //nothing
                break;
                
            case ' ':   //step
                DEBUG_PSTRLN(F("step"));
                ExecZ80(&state, 1);
                waitForInputClear = true;
                keyRepeatMillis = now + (keyRepeating ? 100 : 500);
                break;
            
            default:
                waitForInputClear = true;
                keyRepeatMillis = -1;
                break;
        }
        lastPressedKey = pressedKey;

        if (KEMPSTONJOYSTICK & BUTTON_UP)
        {
            mem_addr -= 8;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        }
        if (KEMPSTONJOYSTICK & BUTTON_DOWN)
        {
            mem_addr += 8;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        }
        if (KEMPSTONJOYSTICK & BUTTON_LEFT)
        {
            mem_addr -= 8 * 20;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        }
        if (KEMPSTONJOYSTICK & BUTTON_RIGHT)
        {
            mem_addr += 8 * 20;
            waitForInputClear = true;
            keyRepeatMillis = now + (keyRepeating ? 100 : 500);
        }
        
        if (KEMPSTONJOYSTICK & BUTTON_ESC
         || KEMPSTONJOYSTICK & BUTTON_FIRE )    //select
        {
            waitForInputClear = true;
            return -1;
        }
    }
    
    return -1;
}


int ICACHE_FLASH_ATTR Test_SD()
{
    char buf[64];

    DEBUG_PSTRLN(F("TEST_SD..."));

    zxDisplayStop();
    
    spiSwitchSet(SD_CS);
   
    //SdFat sd;
    SdFile dirFile;

    if (sd.begin(SD_CS, SPISettings(
                  SPI_SPEED_SD,
                  MSBFIRST,
                  SPI_MODE0)))
    {
        sprintf_P(buf, PSTR("/"));
        if (dirFile.open(buf, O_RDONLY))   //can do path :) e.g "/DCIM/"
        {
            SdFile file;
   
            while (file.openNext(&dirFile, O_RDONLY))
            {
                file.getName(buf, 64);
                Serial.println(buf);
                
                file.close();
            }

            dirFile.close();
        } else
        {
            DEBUG_PSTRLN(F("dir.open FAIL"));
        }
    } else
    {
        DEBUG_PSTRLN(F("sd.begin FAIL"));
    }

    spiSwitchSet(TFT_CS);

    zxDisplayStart();
    
    DEBUG_PSTRLN(F("TEST_SD_END."));    
}
