/*
TJpg_Decoder.cpp

Created by Bodmer 18/10/19
Modified by Denes 07/02/2021
*/

#include "TJpg_Decoder.h"

/***************************************************************************************
** Function name:           TJpg_Decoder
** Description:             Constructor
***************************************************************************************/
ICACHE_FLASH_ATTR TJpg_Decoder::TJpg_Decoder()
{
  // Setup a pointer to this class for static functions
//  thisPtr = this;
}

/***************************************************************************************
** Function name:           ~TJpg_Decoder
** Description:             Destructor
***************************************************************************************/
ICACHE_FLASH_ATTR TJpg_Decoder::~TJpg_Decoder()
{
  // Bye
}

/***************************************************************************************
** Function name:           setJpgScale
** Description:             Set the reduction scale factor (1, 2, 4 or 8)
***************************************************************************************/
void ICACHE_FLASH_ATTR TJpg_Decoder::setSwapBytes(bool swapBytes)
{
  _swap = swapBytes;
}

/***************************************************************************************
** Function name:           setJpgScale
** Description:             Set the reduction scale factor (1, 2, 4 or 8)
***************************************************************************************/
void ICACHE_FLASH_ATTR TJpg_Decoder::setJpgScale(uint8_t scaleFactor)
{
  switch (scaleFactor)
  {
    case 1:
      jpgScale = 0;
      break;
    case 2:
      jpgScale = 1;
      break;
    case 4:
      jpgScale = 2;
      break;
    case 8:
      jpgScale = 3;
      break;
    default:
      jpgScale = 0;
  }
}

/***************************************************************************************
** Function name:           setCallback
** Description:             Set the sketch callback function to render decoded blocks
***************************************************************************************/
void ICACHE_FLASH_ATTR TJpg_Decoder::setDisplayCallback(DisplayCallback callback)
{
  tft_output = callback;
}


/***************************************************************************************
** Function name:           jd_input (declared static)
** Description:             Called by tjpgd.c to get more data
***************************************************************************************/
unsigned int ICACHE_FLASH_ATTR TJpg_Decoder::jd_input(JDEC* jdec, uint8_t* buf, unsigned int len)
{
    TJpg_Decoder *thisPtr = (TJpg_Decoder*)(jdec->device);

    // Avoid running off end of array
    if (thisPtr->array_index + len > thisPtr->array_size) 
    {
      len = thisPtr->array_size - thisPtr->array_index;
    }

    // If buf is valid then copy len bytes to buffer
    //if (buf) memcpy_P(buf, (const uint8_t *)(thisPtr->array_data + thisPtr->array_index), len);
    thisPtr->file_input(buf, len);

    // Move pointer
    thisPtr->array_index += len;

    return len;
}

/***************************************************************************************
** Function name:           jd_output (declared static)
** Description:             Called by tjpgd.c with an image block for rendering
***************************************************************************************/
// Pass image block back to the sketch for rendering, may be a complete or partial MCU
int ICACHE_FLASH_ATTR TJpg_Decoder::jd_output(JDEC* jdec, void* bitmap, JRECT* jrect)
{
  TJpg_Decoder *thisPtr = (TJpg_Decoder*)(jdec->device);

  // Retrieve rendering parameters and add any offset
  int16_t  x = jrect->left + thisPtr->jpeg_x;
  int16_t  y = jrect->top  + thisPtr->jpeg_y;
  uint16_t w = jrect->right  + 1 - jrect->left;
  uint16_t h = jrect->bottom + 1 - jrect->top;

  // Pass the image block and rendering parameters in a callback to the sketch
  return thisPtr->tft_output(x, y, w, h, (uint16_t*)bitmap);
}



JRESULT ICACHE_FLASH_ATTR TJpg_Decoder::drawJpg(int32_t x, int32_t y, uint32_t  data_size, ReadCallback callback, uint8_t* workspace, uint32_t workspace_size)
{
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  array_index = 0;
  array_size  = data_size;
  file_input = callback;

  jpeg_x = x;
  jpeg_y = y;

  jdec.swap = _swap;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, workspace_size, this);

  // Extract image and render
  if (jresult == JDR_OK) {
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  return jresult;
}

JRESULT ICACHE_FLASH_ATTR TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, uint32_t  data_size, ReadCallback callback, uint8_t* workspace, uint32_t workspace_size) 
{
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  array_index = 0;
  array_size  = data_size;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, workspace_size, this);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  return jresult;
}
