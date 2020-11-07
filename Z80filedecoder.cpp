#include <Arduino.h>

#include "GlobalDef.h"
#include "z80.h"
#include "Z80filedecoder.h"


extern Z80 state;
extern unsigned char RAM[];


/*
boolean ICACHE_FLASH_ATTR SetZ80(Z80 *R, z80fileheader *header)
{

  ResetZ80(R);

  const uint8_t *ptr = (uint8_t*)header;
  uint8_t wert1, wert2;
  //uint8_t flag_version = 0;

  //uint16_t header_len;
  //uint16_t akt_adr;

  R->AF.B.h = *(ptr++); // A [0]
  R->AF.B.l = *(ptr++); // F [1]
  R->BC.B.l = *(ptr++); // C [2]
  R->BC.B.h = *(ptr++); // B [3]
  R->HL.B.l = *(ptr++); // L [4]
  R->HL.B.h = *(ptr++); // H [5]

  // PC [6+7]
  wert1 = *(ptr++);
  wert2 = *(ptr++);
  R->PC.W = (wert2 << 8) | wert1;
  if (R->PC.W == 0x0000) {
    return false;

  }

  // SP [8+9]
  wert1 = *(ptr++);
  wert2 = *(ptr++);
  R->SP.W = (wert2 << 8) | wert1;

  R->I = *(ptr++); // I [10]
  R->R = *(ptr++); // R [11]

  // Compressed-Flag und Border [12]
  wert1 = *(ptr++);

  wert2 = ((wert1 & 0x0E) >> 1);

  zxDisplayBorderSet(wert2);


  R->DE.B.l = *(ptr++); // E [13]
  R->DE.B.h = *(ptr++); // D [14]
  R->BC1.B.l = *(ptr++); // C1 [15]
  R->BC1.B.h = *(ptr++); // B1 [16]
  R->DE1.B.l = *(ptr++); // E1 [17]
  R->DE1.B.h = *(ptr++); // D1 [18]
  R->HL1.B.l = *(ptr++); // L1 [19]
  R->HL1.B.h = *(ptr++); // H1 [20]
  R->AF1.B.h = *(ptr++); // A1 [21]
  R->AF1.B.l = *(ptr++); // F1 [22]
  R->IY.B.l = *(ptr++); // Y [23]
  R->IY.B.h = *(ptr++); // I [24]
  R->IX.B.l = *(ptr++); // X [25]
  R->IX.B.h = *(ptr++); // I [26]

  // Interrupt-Flag [27]
  wert1 = *(ptr++);
  if (wert1 != 0) {
    // EI
    R->IFF |= IFF_2 | IFF_EI;
  }
  else {
    // DI
    R->IFF &= ~(IFF_1 | IFF_2 | IFF_EI);
  }
  wert1 = *(ptr++); // nc [28]
  // Interrupt-Mode [29]
  wert1 = *(ptr++);
  if ((wert1 & 0x01) != 0) {
    R->IFF |= IFF_IM1;
  }
  else {
    R->IFF &= ~IFF_IM1;
  }
  if ((wert1 & 0x02) != 0) {
    R->IFF |= IFF_IM2;
  }
  else {
    R->IFF &= ~IFF_IM2;
  }


  R->ICount   = R->IPeriod;
  R->IRequest = INT_NONE;
  R->IBackup  = 0;

  return true;
}


boolean ICACHE_FLASH_ATTR GetZ80(Z80 *R, z80fileheader * header)
{

  uint8_t *ptr = (uint8_t*)header;
  //uint8_t wert1, wert2;
  //uint8_t flag_version = 0;

  //uint16_t header_len;
  //uint16_t akt_adr;

  *(ptr++) = R->AF.B.h; // A [0]
  *(ptr++) = R->AF.B.l; // F [1]
  *(ptr++) = R->BC.B.l; // C [2]
  *(ptr++) = R->BC.B.h; // B [3]
  *(ptr++) = R->HL.B.l; // L [4]
  *(ptr++) = R->HL.B.h; // H [5]


  // PC [6+7]
  *(ptr++) = R->PC.W & 0xff;
  *(ptr++) = (R->PC.W >> 8);

  // SP [8+9]
  *(ptr++) = R->SP.W & 0xff;
  *(ptr++) = (R->SP.W >> 8);


  *(ptr++) = R->I; // I [10]
  *(ptr++) = R->R; // R [11]

  // Compressed-Flag und Border [12]
  *(ptr++) = (R->R & 128 ? 1 : 0) + //bit 7 of R register
             (zxDisplayBorderGet() << 1) +
             32;//compressed
  //          12      1       Bit 0  : Bit 7 of the R-register
  //                        Bit 1-3: Border colour
  //                        Bit 4  : 1=Basic SamRom switched in
  //                        Bit 5  : 1=Block of data is compressed
  //                        Bit 6-7: No meaning



  *(ptr++) = R->DE.B.l; // E [13]
  *(ptr++) = R->DE.B.h; // D [14]
  *(ptr++) = R->BC1.B.l; // C1 [15]
  *(ptr++) = R->BC1.B.h; // B1 [16]
  *(ptr++) = R->DE1.B.l; // E1 [17]
  *(ptr++) = R->DE1.B.h; // D1 [18]
  *(ptr++) = R->HL1.B.l; // L1 [19]
  *(ptr++) = R->HL1.B.h; // H1 [20]
  *(ptr++) = R->AF1.B.h; // A1 [21]
  *(ptr++) = R->AF1.B.l; // F1 [22]
  *(ptr++) = R->IY.B.l; // Y [23]
  *(ptr++) = R->IY.B.h; // I [24]
  *(ptr++) = R->IX.B.l; // X [25]
  *(ptr++) = R->IX.B.h; // I [26]

  // Interrupt-Flag [27]
  *(ptr++) =(IFF_2 | IFF_EI)?1:0;
  ptr++;
  
  // Interrupt-Mode [29]
   *(ptr++)=(R->IFF & IFF_IM1)?1:0+
            (R->IFF & IFF_IM2)?2:0+ 
            64//kempston joystik
            ;

  return true;
}
*/



unsigned char *_CACHE = NULL;
//variables for buffered file or eeprom decoding
//const unsigned char *z80FileFillCacheAdd;
//SdFile *z80FileFillCachefile;
//SdFile *z80FileFillCachefile2;
unsigned int z80FileLen = 0;
unsigned int z80FileOff = 0;

void ICACHE_FLASH_ATTR z80FileFillCacheStart(SdFile *p, int len)
{
  int readLen = 1024;
  DEBUG_PSTR(" file_read_start ");
  DEBUG_PSTR("0, ");
  DEBUG_PRINTLN(readLen);
  z80FileLen = len;
  p->read(_CACHE , 1024);
  z80FileOff = 1024;
}

void ICACHE_FLASH_ATTR z80FileFillCacheContinue(SdFile *p)
{
  int readLen = (z80FileLen - z80FileOff) >= 512 ? 512 : (z80FileLen - z80FileOff);
  DEBUG_PSTR("file_read ");
  DEBUG_PRINT(z80FileOff);
  DEBUG_PSTR(", ");
  DEBUG_PRINTLN(readLen);
  p->read(_CACHE + 512 , readLen);
  z80FileOff += readLen;
}


//decode v1 .z80 file
void ICACHE_FLASH_ATTR z80FileDecompress(SdFile *file, boolean _isCompressed) 
{
  uint32_t iFileSize = file->fileSize();
  uint32_t offset = 0;  // writeOffset into memory
  int iCacheOffset = 0; // file offset that the first byte of cache represents (silly)

  DEBUG_PSTRLN("startdecode v1");
  DEBUG_PRINTLN(iFileSize);

  if (_isCompressed) DEBUG_PSTRLN("compressed");

  for (unsigned int i = 30; i < iFileSize && offset < RAMSIZE; i++)
  {
    //keep enough buffer
    if (i >= (iCacheOffset + 512))
    {
      DEBUG_PSTR(F("cache_shift "));
      DEBUG_PRINTLN(iCacheOffset);
      memcpy(_CACHE, _CACHE + 512, 512); //shift down
      iCacheOffset += 512;
      //DEBUG_PSTRLN(F("file_read 512"));
      z80FileFillCacheContinue(file);   //read next block
    }

    //check end marker
    if (i < iFileSize  - 4
     && _CACHE[(i) - iCacheOffset] == 0x00
     && _CACHE[(i + 1) - iCacheOffset] == 0xED
     && _CACHE[(i + 2) - iCacheOffset] == 0xED
     && _CACHE[(i + 3) - iCacheOffset] == 0x00)
    {
      DEBUG_PSTRLN(F("00EDED00"));
      return;   //break;
    }

    if (!_isCompressed)
    {
        RAM[offset++] = _CACHE[(i) - iCacheOffset];
        continue;
    }
    
    //compressed
    if (i < iFileSize  - 4
       && _CACHE[(i) - iCacheOffset] == 0xED
       && _CACHE[(i + 1) - iCacheOffset] == 0xED)
      {
        i += 2;
        unsigned int repeat = (unsigned char) _CACHE[(i) - iCacheOffset];
        i++;
        unsigned char val = (unsigned char) _CACHE[(i) - iCacheOffset];

        //DEBUG_PRINTLN(repeat);
        for (unsigned int j = 0; j < repeat && offset < RAMSIZE; j++)
        {
          RAM[offset++] = val;
        }
      }
      else
      {
        RAM[offset++] = _CACHE[(i) - iCacheOffset];
      }
  }//for
  
  DEBUG_PSTRLN("END");
  DEBUG_PSTRLN("ram_offset");
  DEBUG_PRINTLN(offset);
}



//decode v2 .z80 file
void ICACHE_FLASH_ATTR z80FileDecompressV2(SdFile *file, int iDataSize, int iDataOffset)
{
  uint32_t offset = 0; // write offset into memory
  int iCacheOffset = 0;

  int nextBlock = 0;
  int nextBlockSize = 0;

  DEBUG_PSTRLN("startdecode v2 ");
  DEBUG_PRINTLN(iDataSize);

  for (int i = iDataOffset; i < iDataSize ; i++)
  {

    if (nextBlockSize <= 0)
    { //look for another block because this one is over
      nextBlock = 0;
      DEBUG_PSTRLN("ram_offset");
      DEBUG_PRINTLN(offset);
    }

    if (nextBlock == 0)
    { //get block size and address

      nextBlockSize = _CACHE[(i) - iCacheOffset] + (_CACHE[(i+1) - iCacheOffset] << 8);
      DEBUG_PSTR("NEXTBLOCK:");
      DEBUG_PRINTLN(_CACHE[(i + 2) - iCacheOffset]); //"page number"
      DEBUG_PRINTLN(nextBlockSize); //compressed size

      switch (_CACHE[(i + 2) - iCacheOffset])
      {
        case 4:
          DEBUG_PSTRLN("0x8000");
          offset = 0x8000 - ROMSIZE;
          nextBlock = 4;
          break;
        case 5:
          DEBUG_PSTRLN("0xc000");
          offset = 0xC000 - ROMSIZE;
          nextBlock = 5;
          break;
        case 8:
          DEBUG_PSTRLN("0x4000");
          offset = 0x4000 - ROMSIZE;
          nextBlock = 8;
          break;
      default:
          DEBUG_PSTRLN("????ERROR");
          return;
      }

      DEBUG_PRINTLN(offset);
      //DEBUG_PRINTLN(nextBlock);
      i += 3;
    }

    if (i >= (iCacheOffset + 512))
    { //cache empty
      DEBUG_PSTRLN("cache_shift");
      DEBUG_PRINTLN(iCacheOffset);
      memcpy(_CACHE, _CACHE + 512, 512); //shift down
      iCacheOffset += 512;
      z80FileFillCacheContinue(file);   //read next block
    }

    if (  nextBlockSize  >= 4
       && _CACHE[(i) - iCacheOffset] == 0xED 
       && _CACHE[(i + 1) - iCacheOffset] == 0xED )
     {//repeat block
        i += 2;
        nextBlockSize -= 2;
        unsigned int repeat = (unsigned char) _CACHE[(i) - iCacheOffset];
        i++;
        nextBlockSize -= 1;
        unsigned char val = (unsigned char) _CACHE[(i) - iCacheOffset];

        //DEBUG_PRINTLN(repeat);
        for (int j = 0; j < repeat; j++)
        {
          if (offset < RAMSIZE)   
            RAM[offset] = val;
          offset++;
        }
     }
     else
     {
        if (offset < RAMSIZE) 
            RAM[offset] = _CACHE[(i) - iCacheOffset];
        offset++;
     }

    nextBlockSize -= 1;
  }
  
  DEBUG_PSTRLN("END");
  DEBUG_PSTRLN("ram_offset");
  DEBUG_PRINTLN(offset);
}

int  ICACHE_FLASH_ATTR z80FileLoad(SdFile *file, unsigned char CACHE[], uint16_t bufferSize)
{
    _CACHE = CACHE;
    
  z80fileheader* header;
  z80fileheader2* header2;
  //int offset = 30;
  int filesize = file->fileSize();
  DEBUG_PRINTLN(filesize);
  
  z80FileFillCacheStart(file, filesize);

  header = (z80fileheader*)CACHE;
  header2 = (z80fileheader2*)(CACHE+30);

  ResetZ80(&state );

  if (header->_PC != 0)
  { //v1
    DEBUG_PSTRLN("ver1");
    
    DEBUG_PSTR("PC ");
    Serial.println(header->_PC);
    SetZ80(&state, header);

    z80FileDecompress(file, (header->_Flags1 & 0x20) ? true : false);   //compressed or not
  } else
  {
    //DEBUG_PRINTLN("ver2/3");
    if (header2->_header2len == 23)
    {
        DEBUG_PSTRLN("ver2");
    }else
    {
        DEBUG_PSTRLN("ver3");
    }
    DEBUG_PRINTLN(header2->_header2len);
    
    DEBUG_PSTR("PC ");
    Serial.println(header2->_PC);
    header->_PC = header2->_PC;
    SetZ80(&state, header);
    
    int offset = 30 + header2->_header2len + 2;
    z80FileDecompressV2(file, filesize, offset);//, &z80FileFillCacheContinue);
  }

    DEBUG_PSTRLN("done.");
    Serial.println(state.PC.W);

  _CACHE = NULL;
  return 0;
}


//SAVE_____________________________________________

void ICACHE_FLASH_ATTR z80FileSaveStart(SdFile *file, int offset)
{
    z80FileOff = offset;
}

int ICACHE_FLASH_ATTR z80FileSaveAdd(SdFile *file, unsigned char c )
{
  if (z80FileOff >= BUFFERSIZE-1)
  {
    if (file->write(_CACHE , z80FileOff) != z80FileOff)
    {
      DEBUG_PSTRLN(F("WRITE KO"));
      return -1;
    }
     
    DEBUG_PSTRLN(F("WRITE:"));
    DEBUG_PRINTLN(z80FileOff);
    z80FileOff = 0;
  }
  
  _CACHE[z80FileOff] = c;
  
  z80FileOff++;
  
  return 0;
}

int ICACHE_FLASH_ATTR z80FileSaveEnd(SdFile *file)
{
  if (z80FileOff > 0)
  {
    if (file->write(_CACHE , z80FileOff) != z80FileOff)
    {
      DEBUG_PSTRLN(F("WRITE KO END"));
      return -1;
    }
    DEBUG_PSTRLN(F("WRITE:"));
    DEBUG_PRINTLN(z80FileOff);
  }

  file->flush();
  file->close();

  DEBUG_PSTRLN(F("WRITE END"));
  return 0;
}


//save z80 file. file format 1 with 30 bytes header
int  ICACHE_FLASH_ATTR z80FileSave(SdFile *file, byte CACHE[], uint16_t bufferSize)
{
    _CACHE = CACHE;

  int offset;
  int counter;

  GetZ80(&state, (z80fileheader *)(CACHE));
  z80FileSaveStart(file, 30);

  DEBUG_PSTRLN("SAVE start");

  for (offset = 0; offset < RAMSIZE - 4;)
  {
    //The compression method is very simple: it replaces repetitions of at least five equal bytes by a four-byte code ED ED xx yy,
    //which stands for "byte yy repeated xx times". Only sequences of length at least 5 are coded.
    //The exception is sequences consisting of ED's; if they are encountered, even two ED's are encoded into ED ED 02 ED.

    //Finally, every byte directly following a single ED is not taken into a block,
    //for example ED 6*00 is not encoded into ED ED ED 06 00 but into ED 00 ED ED 05 00. The block is terminated by an end marker, 00 ED ED 00.

    if (offset > 1 && RAM[offset - 2] != 0xed && RAM[offset - 1] == 0xed && RAM[offset] != 0xed)
    {
      if(z80FileSaveAdd(file, RAM[offset])) return -1;
      offset++;
      continue;
    }

    if (RAM[offset] == 0xED &&
        RAM[offset] == RAM[offset + 1]
       )
    {
      //ED compression

      //up to ff bytes...
      for (counter = 0; counter < 254; counter++)
      {
        if ( RAM[offset + counter] != RAM[offset + counter + 1]) break;
        if (offset + counter + 1 == RAMSIZE) break;
      }

      counter++;
      if(z80FileSaveAdd(file, 0xED )) return -1;
      if(z80FileSaveAdd(file, 0xED )) return -1;
      if(z80FileSaveAdd(file, counter )) return -1;
      if(z80FileSaveAdd(file, RAM[offset] )) return -1;
      offset += counter;

      continue;
    }

    if (RAM[offset] == RAM[offset + 1] &&
        RAM[offset] == RAM[offset + 2] &&
        RAM[offset] == RAM[offset + 3] &&
        RAM[offset] == RAM[offset + 4]
       )
    {
      //non ED compression

      //up to ff bytes...
      for (counter = 0; counter < 254; counter++)
      {
        if ( RAM[offset + counter] != RAM[offset + counter + 1]) break;
        if (offset + counter + 1 == RAMSIZE) break;
      }

      counter++;
      if(z80FileSaveAdd(file, 0xED )) return -1;
      if(z80FileSaveAdd(file, 0xED )) return -1;
      if(z80FileSaveAdd(file, counter )) return -1;
      if(z80FileSaveAdd(file, RAM[offset] )) return -1;
      offset += counter;
      continue;
    }

    if(z80FileSaveAdd(file, RAM[offset] )) return -1;
    offset++;
  }

  for (; offset < RAMSIZE;)
  {
    if(z80FileSaveAdd(file, RAM[offset++] )) return -1;
  }

  if(z80FileSaveAdd(file, 0 )) return -1;
  if(z80FileSaveAdd(file, 0xED )) return -1;
  if(z80FileSaveAdd(file, 0xED )) return -1;
  if(z80FileSaveAdd(file, 0 )) return -1;

  int result = z80FileSaveEnd(file);

  return result;
}


int  ICACHE_FLASH_ATTR SnaFileLoad(SdFile *file, unsigned char CACHE[], uint16_t bufferSize)
{
    DEBUG_PSTRLN("load_SNA");
    
  int filesize = file->fileSize();
  DEBUG_PRINTLN(filesize);    //must be 27 + 49152

  ResetZ80(&state );
  
  Snafileheader* header = (Snafileheader*)CACHE;   
  //DEBUG_PRINTLN(sizeof(Snafileheader));
  int readLength;
  readLength = file->read(header, 27);  //because sizeof(Snafileheader)) == 28;
  DEBUG_PRINTLN(readLength);

  //set up registers
  {
      Z80 *R = &state;
      const uint8_t *ptr = (uint8_t*)header;
      uint8_t wert1, wert2;

      R->I = *(ptr++); // I [10]
      
      R->HL1.B.l = *(ptr++); // L1 [19]
      R->HL1.B.h = *(ptr++); // H1 [20]
      R->DE1.B.l = *(ptr++); // E1 [17]
      R->DE1.B.h = *(ptr++); // D1 [18]
      R->BC1.B.l = *(ptr++); // C1 [15]
      R->BC1.B.h = *(ptr++); // B1 [16]
      R->AF1.B.l = *(ptr++); // F1 [22]
      R->AF1.B.h = *(ptr++); // A1 [21]
      
      R->HL.B.l = *(ptr++); // L [4]
      R->HL.B.h = *(ptr++); // H [5]    
      R->DE.B.l = *(ptr++); // E [13]
      R->DE.B.h = *(ptr++); // D [14]
      R->BC.B.l = *(ptr++); // C [2]
      R->BC.B.h = *(ptr++); // B [3]
      R->IY.B.l = *(ptr++); // Y [23]
      R->IY.B.h = *(ptr++); // I [24]
      R->IX.B.l = *(ptr++); // X [25]
      R->IX.B.h = *(ptr++); // I [26]

      // Interrupt-Flag
      wert1 = *(ptr++);
      if (bitRead(wert1, 2)) {
        // EI
        R->IFF |= IFF_2 | IFF_EI;
      }
      else {
        // DI
        R->IFF &= ~(IFF_1 | IFF_2 | IFF_EI);
      }

      R->R = *(ptr++); // R [11]

      R->AF.B.l = *(ptr++); // F [1]
      R->AF.B.h = *(ptr++); // A [0]
    
      wert1 = *(ptr++);
      wert2 = *(ptr++);
      R->SP.W = (wert2 << 8) | wert1;

      // Interrupt-Mode [29]
      wert1 = *(ptr++);
      if (wert1 == 0) {
        R->IFF &= ~(IFF_IM1 | IFF_IM2);
      } else
      if (wert1 == 1) {
        R->IFF |= IFF_IM1;
      } else
      if (wert1 == 2) {
        R->IFF |= IFF_IM2;
      } else {
      }
      
      wert1 = *(ptr++);
      wert2 = ((wert1 & 0x07));
      zxDisplayBorderSet(wert2);

      //pop PC from the stack(SP), simulating a RETN
      //ptr = &RAM[R->SP.W - ROMSIZE];  //16384
      //wert1 = *(ptr++);
      //wert2 = *(ptr++);
      //R->PC.W = (wert2 << 8) | wert1;
      //R->SP.W += 2;
      
      //RetnZ80(R);
      R->PC.W = 0x72; // address of a RETN in ROM
    }
    
    readLength = file->read(RAM, RAMSIZE);
    DEBUG_PRINTLN(readLength);

    return 0;
}

int ICACHE_FLASH_ATTR LoadScreenPreview(const char *fileName, int posX, int posY)
{
    unsigned char BUFFER[256];
    unsigned char IMAGE[32*24];

    //reuse what's possible from LoadZ80 (or SNA)
    //it's ok if i can only deal with v1 first

    //also, the buffer refill can be made more efficient! 
    // it only needs to keep up to 4 continuous bytes, so can read much beyond half (up to length-4)
    // and shift down only the last 4! 
    
    return 0;
}
