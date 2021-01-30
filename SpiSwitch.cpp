/*in case of new spi devices add here the set/reset code for the new CS pin
 */

#include <Arduino.h>

#include "GlobalDef.h"
#include "SpiSwitch.h"

#include <TFT_eSPI.h> // only included for TFT_CS and SPI

//ToDo: support switching via the PCF8574 multiplexer to save direct GPIOs (except display which is on D8)
//spi devices: display, sdcard, touch (all part of the ili9341 tft_spi)

//char last_CS = -1;

//define cs pins as output and reset them all
void ICACHE_FLASH_ATTR spiSwitchSetup()
{
  pinMode(SD_CS, OUTPUT);
  pinMode(TFT_CS, OUTPUT);

  spiSwitchReset();
}

//turn off all spi devices
void spiSwitchReset()
{
  while (SPI1CMD & SPIBUSY) {};
  
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
  //last_CS = -1;
}

//select one device only
void spiSwitchSet(int cs)
{
  //if (cs == last_CS) return;    //optimisation
  
  spiSwitchReset();
  
  while (SPI1CMD & SPIBUSY) {};
  
  switch (cs)
  {
    case SD_CS:
      digitalWrite(SD_CS, LOW);
      SPI.setFrequency(SPI_SPEED_SD);
      break;
      
    case TFT_CS:
      digitalWrite(TFT_CS, LOW);
      SPI.setFrequency(SPI_SPEED_TFT);
      break;
  }
  
  //last_CS = cs;
}
