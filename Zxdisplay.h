#ifndef SCREEN_H
#define SCREEN_H

void zxDisplaySetup(unsigned char *RAM);

#ifdef __cplusplus
extern "C" {
#endif

void  zxDisplayBorderSet(int i);
int zxDisplayBorderGet(void);

//void  zxDisplayWriteSerial(int i);

#ifdef __cplusplus
}
#endif

void  zxDisplayStartWrite(void);
//void  zxDisplayContinueWrite(char*buffer, int counter);
void  zxDisplayStopWrite(void);

void zxDisplayScan(void);

void zxDisplayReset(void);
void zxDisplayDisableInterrupt(void);
void zxDisplayEnableInterrupt(void);
void zxDisplaySetIntFrequency(int);

void zxDisplayStart();
void zxDisplayStop();

#endif
