/*
TJpg_Decoder.h

JPEG Decoder for Arduino using TJpgDec:
http://elm-chan.org/fsw/tjpgd/00index.html

Incorporated into an Arduino library by Bodmer 18/10/19
Modified by Denes 07/02/2021
*/

#ifndef TJpg_Decoder_H
  #define TJpg_Decoder_H

  #include "TJpg_Config.h"
  #include "Arduino.h"
  #include "tjpgd.h"
//------------------------------------------------------------------------------

typedef bool (*DisplayCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data);
typedef unsigned int (*ReadCallback)(uint8_t* buf, unsigned int len);

class TJpg_Decoder 
{

private:

public:

  TJpg_Decoder();
  ~TJpg_Decoder();

  //static functions to be called by JDEC
  static int jd_output(JDEC* jdec, void* bitmap, JRECT* jrect);
  static unsigned int jd_input(JDEC* jdec, uint8_t* buf, unsigned int len);

  //functions i can call from the above two
  DisplayCallback tft_output = nullptr;
  ReadCallback file_input = nullptr;

  void setJpgScale(uint8_t scale);
  void setDisplayCallback(DisplayCallback callback);

  JRESULT drawJpg(int32_t x, int32_t y, uint32_t  data_size, ReadCallback callback, uint8_t* workspace, uint32_t workspace_size);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, uint32_t  data_size, ReadCallback callback, uint8_t* workspace, uint32_t workspace_size);

  void setSwapBytes(bool swap);

  bool _swap = false;

  uint32_t array_index = 0;
  uint32_t array_size  = 0;

  // Must align workspace to a 32 bit boundary
  //ToDo: use a buffer passed in by the caller instead
  //uint8_t* workspace;//[TJPGD_WORKSPACE_SIZE] __attribute__((aligned(4)));

  int16_t jpeg_x = 0;
  int16_t jpeg_y = 0;

  uint8_t jpgScale = 0;

};

extern TJpg_Decoder TJpgDec;

#endif // TJpg_Decoder_H
