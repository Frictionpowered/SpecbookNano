/*
 * PCF8574 GPIO Port Expand
 *
 * AUTHOR:  Renzo Mischianti
 * VERSION: 2.2.0
 *
 * https://www.mischianti.org/2019/01/02/pcf8574-i2c-digital-i-o-expander-fast-easy-usage/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Renzo Mischianti www.mischianti.org All right reserved.
 *
 * You may copy, alter and reuse this code in any way you like, but please leave
 * reference to www.mischianti.org in your comments if you redistribute this code.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef PCF8574_h
#define PCF8574_h

#include "Wire.h"

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define DEFAULT_SDA SDA;
#define DEFAULT_SCL SCL;

// Uncomment for low memory usage this prevent use of complex DigitalInput structure and free 7byte of memory
#define PCF8574_LOW_MEMORY

// Uncomment for low memory usage this prevent use of complex DigitalInput structure and free 7byte of memory
#define PCF8574_LOW_LATENCY


class PCF8574 {
public:

	PCF8574(uint8_t address);
	PCF8574(uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() );

#if !defined(__AVR) && !defined(__STM32F1__) && !defined(TEENSYDUINO)
	PCF8574(uint8_t address, uint8_t sda, uint8_t scl);
	PCF8574(uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)());
#endif

	bool begin();
	void pinMode(uint8_t pin, uint8_t mode, uint8_t output_start = HIGH);

	void attachInterrupt();
	void detachInterrupt();

	uint8_t digitalRead(uint8_t pin);
    
	byte digitalReadAll(void);
  
	bool digitalWrite(uint8_t pin, uint8_t value);

	uint8_t getTransmissionStatusCode() const 
	{
		return transmissionStatus;
	}

	bool isLastTransmissionSuccess()
	{
		return transmissionStatus==0;
	}
private:
	uint8_t _address;

	#ifdef __STM32F1__
	#ifndef SDA
	#define DEFAULT_SDA PB7
	#define DEFAULT_SCL PB6
	#endif
	#endif

	uint8_t _sda = DEFAULT_SDA;
	uint8_t _scl = DEFAULT_SCL;

	bool _usingInterrupt = false;
	uint8_t _interruptPin = 2;
	void (*_interruptFunction)(){};

	byte writeMode 			= 	B00000000;
	byte writeModeUp		= 	B00000000;
	byte readMode 			= 	B00000000;
	byte readModePullUp 	= 	B00000000;
	byte readModePullDown 	= 	B00000000;
	byte byteBuffered 		= 	B00000000;
	byte resetInitial		= 	B00000000;
	byte initialBuffer		= 	B00000000;

	byte writeByteBuffered = B00000000;

	uint8_t transmissionStatus = 0;
};

#endif
