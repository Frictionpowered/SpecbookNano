/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.h                         **/
/**                                                         **/
/** This file contains declarations relevant to emulation   **/
/** of Z80 CPU.                                             **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2007                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#ifndef Z80_H
#define Z80_H

#include <Arduino.h>

#include "Zxdisplay.h"
#include "Zxsound.h"
#include "Z80filedecoder.h" //not pretty, two-way dependency

#define ROMSIZE 16384   //0x4000
#define RAMSIZE 49152   //0xC000
extern unsigned char RAM[];
extern const unsigned char ROM[];
extern unsigned char KEY[];
extern unsigned char KEMPSTONJOYSTICK;
extern int z80DelayCycle;   //delay loop


// Defines von UB (WICHTIG!!)
#define  EXECZ80
#define  LSB_FIRST


#ifdef __cplusplus
extern "C" {
#endif

                               /* Compilation options:       */
/* #define DEBUG */            /* Compile debugging version  */
/* #define LSB_FIRST */        /* Compile for low-endian CPU */
/* #define MSB_FIRST */        /* Compile for hi-endian CPU  */

                               /* LoopZ80() may return:      */
#define INT_RST00   0x00C7     /* RST 00h                    */
#define INT_RST08   0x00CF     /* RST 08h                    */
#define INT_RST10   0x00D7     /* RST 10h                    */
#define INT_RST18   0x00DF     /* RST 18h                    */
#define INT_RST20   0x00E7     /* RST 20h                    */
#define INT_RST28   0x00EF     /* RST 28h                    */
#define INT_RST30   0x00F7     /* RST 30h                    */
#define INT_RST38   0x00FF     /* RST 38h                    */
#define INT_IRQ     INT_RST38  /* Default IRQ opcode is FFh  */
#define INT_NMI     0xFFFD     /* Non-maskable interrupt     */
#define INT_NONE    0xFFFF     /* No interrupt required      */
#define INT_QUIT    0xFFFE     /* Exit the emulation         */

                               /* Bits in Z80 F register:    */
#define S_FLAG      0x80       /* 1: Sign; result negative   */
#define Z_FLAG      0x40       /* 1: Zero: result is zero    */
#define Y_FLAG      0x20       /* 1: Copy of bit 5 of result */
#define H_FLAG      0x10       /* 1: Halfcarry/Halfborrow    */
#define X_FLAG      0x08       /* 1: Copy of bit 3 of result */
#define P_FLAG      0x04       /* 1: Parity (result is even) */
#define V_FLAG      0x04       /* 1: Overflow occured        */     // same as P_FLAG!!!
#define N_FLAG      0x02       /* 1: Subtraction occured     */
#define C_FLAG      0x01       /* 1: Carry/Borrow occured    */

                               /* Bits in IFF flip-flops:    */
#define IFF_1       0x01       /* IFF1 flip-flop             */
#define IFF_IM1     0x02       /* 1: IM1 mode                */
#define IFF_IM2     0x04       /* 1: IM2 mode                */
#define IFF_2       0x08       /* IFF2 flip-flop             */
#define IFF_EI      0x20       /* 1: EI pending              */
#define IFF_HALT    0x80       /* 1: CPU HALTed              */

/** Simple Datatypes *****************************************/
/** NOTICE: sizeof(byte)=1 and sizeof(word16)=2             **/
/*************************************************************/
#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef uint8_t byte;
#endif
#ifndef WORD_TYPE_DEFINED
#define WORD_TYPE_DEFINED
typedef uint16_t word16;
#endif
typedef int8_t offset;

/** Structured Datatypes *************************************/
/** NOTICE: #define LSB_FIRST for machines where least      **/
/**         significant byte goes first.                    **/
/*************************************************************/
typedef union
{
#ifdef LSB_FIRST
  struct { byte l,h; } B;
#else
  struct { byte h,l; } B;
#endif
  word16 W;
} pair;

typedef struct
{
  pair AF,BC,DE,HL,IX,IY,PC,SP;       /* Main registers      */
  pair AF1,BC1,DE1,HL1;               /* Shadow registers    */
  byte IFF,I;                         /* Interrupt registers */
  byte R;                             /* Refresh register    */

  int IPeriod,ICount; /* Set IPeriod to number of CPU cycles */
                      /* between calls to LoopZ80()          */
  int IBackup;        /* Private, don't touch                */
  word16 IRequest;      /* Set to address of pending IRQ       */
  byte IAutoReset;    /* Set to 1 to autom. reset IRequest   */
  byte TrapBadOps;    /* Set to 1 to warn of illegal opcodes */
  word16 Trap;          /* Set Trap to address to trace from   */
  byte Trace;         /* Set Trace=1 to start tracing        */
  void *User;         /* Arbitrary user data (ID,RAM*,etc.)  */
} Z80;

/** ResetZ80() ***********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with RunZ80(). It sets registers to  **/
/** their initial values.                                   **/
/*************************************************************/
void ResetZ80(register Z80 *R);

/** ExecZ80() ************************************************/
/** This function will execute given number of Z80 cycles.  **/
/** It will then return the number of cycles left, possibly **/
/** negative, and current register values in R.             **/
/*************************************************************/
#ifdef EXECZ80
int ExecZ80(register Z80 *R,register int RunCycles);
#endif

/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
void IntZ80(register Z80 *R,register word16 Vector);

/** RunZ80() *************************************************/
/** This function will run Z80 code until an LoopZ80() call **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
#ifndef EXECZ80
word16 RunZ80(register Z80 *R);
#endif

/** RdZ80()/WrZ80() ******************************************/
/** These functions are called when access to RAM occurs.   **/
/** They allow to control memory access.                    **/
/************************************ TO BE WRITTEN BY USER **/
void WrZ80(register word16 Addr,register byte Value);
byte RdZ80(register word16 Addr);

/** InZ80()/OutZ80() *****************************************/
/** Z80 emulation calls these functions to read/write from  **/
/** I/O ports. There can be 65536 I/O ports, but only first **/
/** 256 are usually used.                                   **/
/************************************ TO BE WRITTEN BY USER **/
void OutZ80(register word16 Port,register byte Value);
byte InZ80(register word16 Port);

/** PatchZ80() ***********************************************/
/** Z80 emulation calls this function when it encounters a  **/
/** special patch command (ED FE) provided for user needs.  **/
/** For example, it can be called to emulate BIOS calls,    **/
/** such as disk and tape access. Replace it with an empty  **/
/** macro for no patching.                                  **/
/************************************ TO BE WRITTEN BY USER **/
void PatchZ80(register Z80 *R);

/** DebugZ80() ***********************************************/
/** This function should exist if DEBUG is #defined. When   **/
/** Trace!=0, it is called after each command executed by   **/
/** the CPU, and given the Z80 registers. Emulation exits   **/
/** if DebugZ80() returns 0.                                **/
/*************************************************************/
#ifdef DEBUG
byte DebugZ80(register Z80 *R);
#endif

/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts. This    **/
/** function must return an address of the interrupt vector **/
/** (0x0038, 0x0066, etc.) or INT_NONE for no interrupt.    **/
/** Return INT_QUIT to exit the emulation loop.             **/
/************************************ TO BE WRITTEN BY USER **/
word16 LoopZ80(register Z80 *R);

/** JumpZ80() ************************************************/
/** Z80 emulation calls this function when it executes a    **/
/** JP, JR, CALL, RST, or RET. You can use JumpZ80() to     **/
/** trap these opcodes and switch memory layout.            **/
/************************************ TO BE WRITTEN BY USER **/


//used by the .z80 upload to set initial status
boolean SetZ80(Z80 *R, z80fileheader * header);
boolean GetZ80(Z80 *R, z80fileheader * header);

void RetnZ80(Z80 *R);


#ifndef JUMPZ80
#define JumpZ80(PC)
#else
void JumpZ80(word16 PC);
#endif

#ifdef __cplusplus
}
#endif
#endif /* Z80_H */
