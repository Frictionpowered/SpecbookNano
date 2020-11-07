/*
 * PCF8574 GPIO Port Expand
 * https://www.mischianti.org/2019/01/02/pcf8574-i2c-digital-i-o-expander-fast-easy-usage/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Renzo Mischianti www.mischianti.org All right reserved.
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

#include "PCF8574.h"
#include "Wire.h"

PCF8574::PCF8574(uint8_t address)
{
	_address = address;
};

PCF8574::PCF8574(uint8_t address, uint8_t interruptPin,  void (*interruptFunction)() )
{
	_address = address;
	_interruptPin = interruptPin;
	_interruptFunction = interruptFunction;
	_usingInterrupt = true;
};

#if !defined(__AVR) && !defined(__STM32F1__) && !defined(TEENSYDUINO)

	PCF8574::PCF8574(uint8_t address, uint8_t sda, uint8_t scl)
	{
		_address = address;
		_sda = sda;
		_scl = scl;
	};

	PCF8574::PCF8574(uint8_t address, uint8_t sda, uint8_t scl, uint8_t interruptPin,  void (*interruptFunction)() )
	{
		_address = address;
		_sda = sda;
		_scl = scl;

		_interruptPin = interruptPin;
		_interruptFunction = interruptFunction;

		_usingInterrupt = true;
	};
#endif


void PCF8574::attachInterrupt()
{
	// If using interrupt set interrupt value to pin
	if (_usingInterrupt)
	{
		::pinMode(_interruptPin, INPUT_PULLUP);
		::attachInterrupt(digitalPinToInterrupt(_interruptPin), (*_interruptFunction), FALLING );
	}
}

void PCF8574::detachInterrupt()
{
	// If using interrupt set interrupt value to pin
	if (_usingInterrupt){
		::detachInterrupt(digitalPinToInterrupt(_interruptPin));
	}
}

bool ICACHE_FLASH_ATTR PCF8574::begin()
{
	this->transmissionStatus = 4;
   
	Wire.begin(_sda, _scl);

	// Check if there are pins to set low
	if (writeMode>0 || readMode>0)
	{
		Wire.beginTransmission(_address);

#ifdef PCF8574_SOFT_INITIALIZATION
		resetInitial = writeModeUp | readModePullUp;
#else
		resetInitial = writeModeUp | readMode;
#endif

		Wire.beginTransmission(_address);
		Wire.write(resetInitial);

		initialBuffer = writeModeUp | readModePullUp;
		byteBuffered = initialBuffer;
		writeByteBuffered = writeModeUp;

		this->transmissionStatus = Wire.endTransmission();
	}

	PCF8574::attachInterrupt();

	return this->isLastTransmissionSuccess();
}

void ICACHE_FLASH_ATTR PCF8574::pinMode(uint8_t pin, uint8_t mode, uint8_t output_start)
{
	if (mode == OUTPUT)
	{
		writeMode = writeMode | bit(pin);
		if (output_start==HIGH) {
			writeModeUp = writeModeUp | bit(pin);
		}

		readMode =  readMode & ~bit(pin);
		readModePullDown 	=	readModePullDown 	& 	~bit(pin);
		readModePullUp 		=	readModePullUp 		& 	~bit(pin);

	} else if (mode == INPUT)
	{
		writeMode = writeMode & ~bit(pin);

		readMode 			=   readMode 			| bit(pin);
		readModePullDown 	=	readModePullDown 	| bit(pin);
		readModePullUp 		=	readModePullUp 		& ~bit(pin);

	} else if (mode == INPUT_PULLUP)
	{
		writeMode = writeMode & ~bit(pin);

		readMode 			=   readMode 			| bit(pin);
		readModePullDown 	=	readModePullDown 	& ~bit(pin);
		readModePullUp 		=	readModePullUp 		| bit(pin);
	} else
	{
	}
};


byte PCF8574::digitalReadAll(void)
{
	Wire.requestFrom(_address,(uint8_t)1);// Begin transmission to PCF8574 with the buttons
	//lastReadMillis = millis();
	if(Wire.available())   // If bytes are available to be recieved
	{
		  byte iInput = Wire.read();// Read a byte

		  if ((readModePullDown & iInput)>0 or (readModePullUp & ~iInput)>0){
			  byteBuffered = (byteBuffered & ~readMode) | (byte)iInput;

		  }
	}

	byte byteRead = byteBuffered | writeByteBuffered;

	byteBuffered = (initialBuffer & readMode) | (byteBuffered  & ~readMode); //~readMode & byteBuffered;
	return byteRead;
};


uint8_t PCF8574::digitalRead(uint8_t pin)//, bool forceReadNow)
{
	uint8_t value = (bit(pin) & readModePullUp)?HIGH:LOW;
	// Check if pin already HIGH than read and prevent reread of i2c

	if ((((bit(pin) & (readModePullDown & byteBuffered))>0) or (bit(pin) & (readModePullUp & ~byteBuffered))>0 ))
	{
		  if ((bit(pin) & byteBuffered)>0)
		  {
			  value = HIGH;
		  }else{
			  value = LOW;
		  }
	 } else //if (forceReadNow || (millis() > PCF8574::lastReadMillis))
	 {
		  Wire.requestFrom(_address,(uint8_t)1);// Begin transmission to PCF8574
		  if(Wire.available())   // If bytes are available to be recieved
		  {
			  byte iInput = Wire.read();// Read a byte

			  if ((readModePullDown & iInput)>0 or (readModePullUp & ~iInput)>0){
				  byteBuffered = (byteBuffered & ~readMode) | (byte)iInput;
				  if ((bit(pin) & byteBuffered)>0){
					  value = HIGH;
				  }else{
					  value = LOW;
				  }
//				  value = (bit(pin) & byteBuffered);
			  }
		  }
	}
   
	// If HIGH set to low to read buffer only one time
	if ((bit(pin) & readModePullDown) and value==HIGH)
	{
		byteBuffered = bit(pin) ^ byteBuffered;
	} else if ((bit(pin) & readModePullUp) and value==LOW)
	{
		byteBuffered = bit(pin) ^ byteBuffered;
	} else if(bit(pin) & writeByteBuffered)
	{
		value = HIGH;
	}
   
	return value;
};

bool PCF8574::digitalWrite(uint8_t pin, uint8_t value)
{
	Wire.beginTransmission(_address);     //Begin the transmission to PCF8574

	if (value==HIGH)
	{
		writeByteBuffered = writeByteBuffered | bit(pin);
		byteBuffered  = writeByteBuffered | bit(pin);
	} else
	{
		writeByteBuffered = writeByteBuffered & ~bit(pin);
		byteBuffered  = writeByteBuffered & ~bit(pin);
	}
   
	// writeByteBuffered = writeByteBuffered & (~writeMode & byteBuffered);
	byteBuffered = (writeByteBuffered & writeMode) | (resetInitial & readMode);

	Wire.write(byteBuffered);

	byteBuffered = (writeByteBuffered & writeMode) | (initialBuffer & readMode);

	this->transmissionStatus = Wire.endTransmission();

	return this->isLastTransmissionSuccess();
};
