/***************************************************

  KniwwelinoLIB

  Copyright (C) 2017 Luxembourg Institute of Science and Technology.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License.

  Derived from Adafruit_LED_Backpack_Library library
  Written by Limor Fried/Ladyada for Adafruit Industries.
****************************************************/

#include "Kniwwelino.h"

#include <Wire.h>

#include <Adafruit_NeoPixel.h>
#include "Adafruit_GFX.h"
#include <Fonts/TomThumb.h>

//-- DEBUG Helpers -------------
#ifdef DEBUG
#define DEBUG_PRINT(x)         Serial.print (x)
#define DEBUG_PRINTDEC(x)      Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)       Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif

//-- CALLBACK Helpers -------------
typedef void (*MQTTClientCallbackSimple)(String &topic, String &payload);
MQTTClientCallbackSimple mqttCallback = nullptr;

/*
 * Lib Contructor. no need to call, as we provide a static Kniwwelino object instance
 *
 */
KniwwelinoLib::KniwwelinoLib() : Adafruit_GFX(5, 5) {
  RGB = Adafruit_NeoPixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);
}

/*
 * begin - Initializes the Board hardware, Wifi and MQTT.
 * MUST be called once in the setup routine to use the board.
 *
 * Wifi on, Fastboot false.
 *
 */
void KniwwelinoLib::begin() {
	begin(true, false);
}

/*
 * begin - Initializes the Board hardware, Wifi and MQTT.
 * MUST be called once in the setup routine to use the board.
 *
 * boolean enableWifi Enable Wifi and try to connecto to Wifi and MQTT
 * boolean fast  Startup faster without waiting for all scrolling texts.
 */
void KniwwelinoLib::begin(boolean enableWifi, boolean fast) {
	//if (enableWifi && fast) WiFi.begin();

	Serial.begin(115200);
	DEBUG_PRINT("\n");DEBUG_PRINT(getName());DEBUG_PRINT(F(" booting up..."));

	// init RGB LED
	RGB.begin();
	RGB.setBrightness(255);
	RGBsetColorEffect(255, 0, 0, RGB_FLASH, 5);
	DEBUG_PRINT(F(" RGB:On"));

	// init variables with defaults
	mqttTopicUpdate.replace(":","");
	mqttTopicReqPwd.replace(":","");
	mqttTopicSentPwd.replace(":","");
	mqttTopicStatus.replace(":","");
	strncpy(updateServer, DEF_UPDATESERVER, sizeof(DEF_UPDATESERVER));
	strncpy(mqttServer, DEF_MQTTSERVER, sizeof(DEF_MQTTSERVER));
	strncpy(mqttUser, 	DEF_MQTTUSER, sizeof(DEF_MQTTUSER));
	strncpy(mqttPW, 	DEF_MQTTPW, sizeof(DEF_MQTTPW));
	strncpy(fwVersion, 	FW_VERSION, sizeof(FW_VERSION));
	getName().toCharArray(nodename, getName().length());
	DynamicJsonBuffer jsonBuffer;
	myParameters = &jsonBuffer.createObject();
	mqttGroup = "";

	// init Variables from stored config
	DEBUG_PRINT(F(" Config:"));
	String conf = Kniwwelino.FILEread(FILE_CONF);
	if (conf.length() > 0) {
		Kniwwelino.PLATFORMupdateConf(conf);
		DEBUG_PRINT(F("OK "));
	}

	// initialize I2C and LED Matrix Driver
	Wire.begin();
	Wire.beginTransmission(HT16K33_ADDRESS);
	Wire.write(0x21);  // turn on oscillator
	Wire.endTransmission();

	// init LED MATRIX
	setTextWrap(false);
	setFont(&TomThumb);
	MATRIXsetBlinkRate(MATRIX_STATIC);
	MATRIXsetBrightness(MATRIX_DEFAULT_BRIGHTNESS);
	MATRIXclear();
	DEBUG_PRINT(F(" MATRIX:On"));

	// attach base ticker for display and buttons
	baseTicker.attach(TICK_FREQ, _baseTick);
	_baseTick();

	if (fast) {
		MATRIXwriteOnce(F("Kniwwelino"));
		Kniwwelino.sleep(1000);
	} else {
		MATRIXwriteAndWait(F("Kniwwelino"));
	}


	if (enableWifi) {
		DEBUG_PRINT(F(" WIFI:"));
		boolean wifiMgr = false;

		// if both buttons are clicked or pressed -> Wifi Manager
		if ( (Kniwwelino.BUTTONAdown() && Kniwwelino.BUTTONBdown())
			//	|| (Kniwwelino.BUTTONAclicked() && Kniwwelino.BUTTONBclicked())
			) {
					wifiMgr = true;
			DEBUG_PRINTLN(F("Starting Wifimanager."));
		} else if (Kniwwelino.BUTTONAdown()) {
			// if button A is pressed -> Display ID
			Kniwwelino.MATRIXshowID();
			idShowing = true;
		}

		Kniwwelino.WIFIsetup(wifiMgr, fast, false);
	} else {
		// save Energy
		WiFi.mode(WIFI_OFF);
	}

	if (!(WiFi.status() == WL_CONNECTED)) {
		wifiEnabled = false;
	}

	if (wifiEnabled) {
		// start mqtt
		Kniwwelino.MQTTsetup(mqttServer, mqttPort, mqttUser, mqttPW);
	}

	// wait here for button b pressed if ID is showning
	if (idShowing) {
		Kniwwelino.MATRIXshowID();
		Kniwwelino.RGBsetColorEffect(RGB_COLOR_ORANGE, RGB_BLINK, RGB_FOREVER);
		while (! Kniwwelino.BUTTONBdown())  {
			Kniwwelino.sleep(100);
		}
		idShowing = false;
	}

	RGBsetColor(RGB_COLOR_GREEN);
	Kniwwelino.sleep(1000);
	RGBclear();

	DEBUG_PRINTLN(F(" Done!"));
	DEBUG_PRINTLN(F("=== WELCOME ====================================="));
}

//==== Kniwwelino functions===================================================

	/*
	 * returns the Kniwwelinos Device ID = last 6 digits of the WIFI MAC address
	 */
	String KniwwelinoLib::getID() {
		String mac = WiFi.macAddress();
		mac.replace(":","");
		mac = mac.substring(6);
		return mac;
	}

	/*
	 * returns the Kniwwelinos Device Name = Kniwwelino + last 6 digits of the WIFI MAC address
	 */
	String KniwwelinoLib::getName() {
		return String(NAME_PREFIX)+"_"+getID();
	}

	/*
	 * returns the Kniwwelinos WIFI MAC address
	 */
	String KniwwelinoLib::getMAC() {
		return WiFi.macAddress();
	}

	/*
	 * returns the Kniwwelinos WIFI IP address
	 * returns (0.0.0.0) if not connected
	 */
	String KniwwelinoLib::getIP() {
		return WiFi.localIP().toString();
	}

	/*
	 * Checks the Kniwwelinos Wifi and MQTT connection.
	 * returns true if connected, false if not connected
	 */
	boolean KniwwelinoLib::isConnected() {
		return ((WiFi.status() == WL_CONNECTED) && mqtt.connected());
	}


	/*
	 * Sleeps the current program for the given number of milli seconds.
	 * Use this one instead of arduino delay, as it handles Wifi and MQTT in the background.
	 */
	void KniwwelinoLib::sleep(uint16_t sleepMillis) {
		if (sleepMillis < 100) {
			delay(sleepMillis);
		} else {
			unsigned long till = millis() + sleepMillis;
			while (till > millis()) {
				if (mqttEnabled && mqtt.connected()) {
					mqtt.loop();
				}
				yield();
				sleepMillis = till - millis();
				if (sleepMillis > 10) {
					delay(10);
				} else {
					delay(sleepMillis);
				}
			}
		}
	}

	/*
	 * internal loop that handles the mqtt connection and message handling.
	 * needs to be called regularly -> end of the Arduino loop method.
	 */
	void KniwwelinoLib::loop() {
	    if (mqttEnabled) {
	    	if (!mqtt.connected()) {
	    		MQTTconnect();
	    	}

	    	if (mqtt.connected()) {
	    		mqtt.loop();
	    		_MQTTupdateStatus(false);
	    	}
	    }

	    // Reset button clicked state and read buttons again.
//	    buttonAClicked = false;
//	    buttonBClicked = false;
//	    _Buttonsread();
	}

	/*
	 * internal ticker function that is called in the background.
	 * handles the update of LED, MAtrix and reads the buttons.
	 */
	void KniwwelinoLib::_baseTick() {
		_tick++;

		Kniwwelino._PINblink();

		Kniwwelino._RGBblink();

		Kniwwelino._Buttonsread();

		Kniwwelino._MATRIXupdate();

		//DEBUG_PRINT(">");
	}

	/*
	 * internal function for PIN blink/flash effects
	 */
	void KniwwelinoLib::_PINblink() {

		for (int i = 0; i < sizeof(ioPinNumers); i++) {
			if (ioPinStatus[i] == RGB_UNUSED) {
				continue;
			}

			// if Effect Time has passed, turn off Pin
			if (ioPinStatus[i]==0) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINT(" LOW");
				digitalWrite(ioPinNumers[i], LOW);
				return;
			}
			// if Effect is static on, nothing to do
			if (ioPinStatus[i] == RGB_ON) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINT(" HIGH");
				digitalWrite(ioPinNumers[i], HIGH);
			}
			// handle effect
			if (rgbBlinkCount<=ioPinStatus[i]) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINT(" HIGH");
				digitalWrite(ioPinNumers[i], HIGH);
			} else {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINT(" LOW");
				digitalWrite(ioPinNumers[i], LOW);
			}
		}
	}

	/*
	 * Set the specified I/O Pin of the board to on/off/blink/flash.
	 * pin = D5/D6/D7
	 * effect = RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 */
	void KniwwelinoLib::PINsetEffect(uint8_t pin, int effect) {
		switch (pin) {
		    case D0:
		      ioPinStatus[0] = effect;
		      break;
		    case D5:
		      ioPinStatus[1] = effect;
		      break;
		    case D6:
		      ioPinStatus[2] = effect;
		      break;
		    case D7:
		      ioPinStatus[3] = effect;
		      break;
		    break;
		  }
	}

	/*
	 * Clear the specified I/O Pin of the board.
	 * (set to OFF and remove from ticker)
	 * pin = D5/D6/D7
	 */
	void KniwwelinoLib::PINclear(uint8_t pin) {
	    digitalWrite(pin, LOW);
		switch (pin) {
		    case D0:
		      ioPinStatus[0] = RGB_UNUSED;
		      break;
		    case D5:
		      ioPinStatus[1] = RGB_UNUSED;
		      break;
		    case D6:
		      ioPinStatus[2] = RGB_UNUSED;
		      break;
		    case D7:
		      ioPinStatus[3] = RGB_UNUSED;
		      break;
		    break;
		  }
	}

//==== RGB LED  functions ====================================================

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as String in format #FF00FF
	 * first 2 digits specify the RED, second the GREEN, third the BLUE component.
	 */
	void KniwwelinoLib::RGBsetColor(String col) {
		uint32_t color = _hex2int(col);
		RGBsetColorEffect(color, RGB_ON, RGB_FOREVER);
	}

	/*
	 * Set the RGB LED of the board to show the given color and effect.
	 * col = color to show as String in format #FF00FF
	 * first 2 digits specify the RED, second the GREEN, third the BLUE component.
	 * effect = on of RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 * count = how long shall the effect be shown. (10 = 1sec, -1 shows forever.)
	 */
	void KniwwelinoLib::RGBsetColorEffect(String col, uint8_t effect, int count) {
		uint32_t color = _hex2int(col);
		RGBsetColorEffect(color, effect, count);
	}

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as 32 bit int.
	 */
	void KniwwelinoLib::RGBsetColor(uint32 color) {
		RGBsetColorEffect(color, RGB_ON, RGB_FOREVER);
	}

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as 32 bit int.
	 * effect = on of RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 * count = how long shall the effect be shown. (10 = 1sec, -1 shows forever.)
	 */
	void KniwwelinoLib::RGBsetColorEffect(uint32 color, uint8_t effect, int count) {
	     RGBsetColorEffect((uint8_t)(color >> 16) ,(uint8_t)(color >>  8), (uint8_t)color, effect, count);
	}

	/*
	 * Set the RGB LED of the board to show the given color
	 * red = 	RED color component (0-255)
	 * green = 	GREEN color component (0-255)
	 * blue = 	BLUE color component (0-255)
	 */
	void KniwwelinoLib::RGBsetColor(uint8_t red ,uint8_t green, uint8_t blue) {
		RGBsetColorEffect(red ,green, blue, RGB_ON, RGB_FOREVER);
	}

	/*
	 * Set the RGB LED of the board to show the given color and effect.
	 * red = 	RED color component (0-255)
	 * green = 	GREEN color component (0-255)
	 * blue = 	BLUE color component (0-255)
	 * effect = on of RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 * count = how long shall the effect be shown. (10 = 1sec, -1 shows forever.)
	 */
	void KniwwelinoLib::RGBsetColorEffect(uint8_t red ,uint8_t green, uint8_t blue, uint8_t effect, int count) {
		rgbEffect =  effect;
		rgbEffectCount = count;
		RGB.setPixelColor(0, red, green, blue);
		RGB.show();
		rgbColor = RGB.getPixelColor(0);
	}

	/*
	 * Set the RGB LED of the board to be OFF
	 */
	void KniwwelinoLib::RGBclear() {
		RGB.setPixelColor(0, 0);
		RGB.show();
	}

	/*
	 * Set the Brightness of the RGB LED of the board.
	 * b = brightness from 0-255
	 */
	void KniwwelinoLib::RGBsetBrightness(uint8_t b) {
		b = constrain(b, 2, 255);
		RGB.setBrightness(b);
		RGB.show();
	}

	/*
	 * Internal ticker function to handle the LED effects.
	 */
	void KniwwelinoLib::_RGBblink() {
		// if Effect Time has passed, turn off LED
		if (rgbEffectCount==0) {
			Kniwwelino.RGBclear();
		} else if (rgbEffect == RGB_ON) {
			// if Effect is static on, nothing to do
		} else if (rgbBlinkCount<=rgbEffect) {
		// handle effect
			if (RGB.getPixelColor(0)==0) {
				RGB.setPixelColor(0, rgbColor);
				RGB.show();
			}
		} else {
			RGB.setPixelColor(0, 0);
			RGB.show();
		}

		rgbBlinkCount++;
		if (rgbBlinkCount>10) {
			rgbBlinkCount=1;
			if (rgbEffectCount > 0) rgbEffectCount--;
		}
	}

	/*
	 * Helper function to convert a given color in String format (#00FF00)
	 * to a 32bit int color.
	 */
	unsigned long KniwwelinoLib::_hex2int(String str) {
	   int i;
	   unsigned long val = 0;
	   int len = str.length();

	   // parse the first 6 chars for HEX
	   if (len > 6) len = 6;

	   for(i=0;i<len;i++)
	      if(str.charAt(i) <= 57)
	       val += (str.charAt(i)-48)*(1<<(4*(len-1-i)));
	      else
	       val += (str.charAt(i)-55)*(1<<(4*(len-1-i)));
	   return val;
	}


//==== LED MATRIX functions ==================================================

	/*
	 * Write the given text to the matrix and scroll it for a number of times.
	 *
	 * text = text to be shown.
	 * count = number of times the text shall be scrolled before disappearing (-1 = forever)
	 * wait = if true, wait before text has been shown before returning.
	 *
	 */
    void KniwwelinoLib::MATRIXwrite(String text, int count, boolean wait) {
    	if (text.length() == 0) {
    		Kniwwelino.MATRIXclear();
    	}

    	// if wait or different Text, reset position, else keep scrolling.
    	if (wait || text != matrixText) {
        	matrixPos = 4;
    	}
    	matrixText = text;
    	matrixTextCount = count;
    	if (wait) {
    		if (matrixText.length() == 1) {
    			Kniwwelino.sleep(1000);
    		} else {
    			//Kniwwelino.sleep((matrixText.length()+2)*4*count*(TICK_FREQ*2000.0));
				while (!Kniwwelino.MATRIXtextDone()) {
					Kniwwelino.sleep(100);
				}
    		}
    	}
    }

	/*
	 * Write the given text to the matrix and scroll it infinitely.
	 *
	 * text = text to be shown.
	 */
    void KniwwelinoLib::MATRIXwrite(String text) {
    	MATRIXwrite(text, MATRIX_FOREVER, false);

    }

	/*
	 * Write the given text to the matrix and scroll it,
	 * wait before text has been shown before returning.
	 *
	 * text = text to be shown.
	 */
    void KniwwelinoLib::MATRIXwriteAndWait(String text) {
    	MATRIXwrite(text, 1, true);
    }

	/*
	 * Write the given text to the matrix and scroll it once.
	 *
	 * text = text to be shown.
	 */
    void KniwwelinoLib::MATRIXwriteOnce(String text) {
    	MATRIXwrite(text, 1, false);
    }


	/*
	 * Draw the given icon on the matrix.
	 * Icon is given as string and accepted in the following formats:
	 * "B1111100000111110000011111" 25 pixels binary one after the other, prefix B
	 * "0x7008E828A0" one byte for each 5px row, prefix 0x
	 *
	 * iconString = icon to be shown.
	 */
    void KniwwelinoLib::MATRIXdrawIcon(String iconString) {
    	matrixTextCount = 0;
		MATRIXclear();

    	if (iconString.startsWith("B") || iconString.startsWith("b")) {
    		// String must be like "B1111100000111110000011111" 25 pixels one after the other
    		if (iconString.length() != 26) {
    			DEBUG_PRINTLN(F("MATRIXdrawIcon:Binay: Wrong String length (not 26)"));
    			return;
    		} else {
    			//DEBUG_PRINT(F("MATRIXdrawIcon:Binay: "));DEBUG_PRINTLN(iconString);
    		}
    		for(int i=0; i < iconString.length()-1; i++) {
    			if (iconString.charAt(i+1) != '0') {
    				MATRIXsetPixel(i%5, i/5, 1);
    			}
    		}
    		redrawMatrix = true;
    	} else if (iconString.startsWith("0x")) {
    		// String must be like "0x7008E828A0" one byte for each 5px row
    		if (iconString.length() != 12) {
    			DEBUG_PRINTLN(F("MATRIXdrawIcon:Hex: Wrong String length (not 12)"));
    			return;
    		} else {
    			//DEBUG_PRINT(F("MATRIXdrawIcon:Hex: "));DEBUG_PRINTLN(iconString);
    		}
    		for(int i=0; i < 5; i++) {
    			String sub = iconString.substring((i*2)+2,(i*2)+4);
    			//DEBUG_PRINT("\t"+sub + " ");
    			int number = (int) strtol( &sub[0], NULL, 16);
    			//DEBUG_PRINT(number);
    			for(int j=0; j < 5; j++) {
    				//DEBUG_PRINT(bitRead(number, 7-j));
    				MATRIXsetPixel(j, i, bitRead(number, 7-j));
    			}
    			//DEBUG_PRINTLN();
    		}
    		redrawMatrix = true;
    	}
    }

	/*
	 * Draw the given icon on the matrix.
	 * Icon is given as 32bit long.
	 * bits correspond to the pixels starting at top left.
	 *
	 * iconLong = icon to be shown.
	 */
    void KniwwelinoLib::MATRIXdrawIcon(uint32_t iconLong) {
        	matrixTextCount = 0;
        	MATRIXclear();
        	//DEBUG_PRINT(F("MATRIXdrawIcon:Long "));DEBUG_PRINTLN(iconLong);
        	for(int i=0; i < 25; i++) {
        		//DEBUG_PRINT(bitRead(iconLong, 24-i));
        		MATRIXsetPixel(i%5, i/5, bitRead(iconLong, 24-i));
        	}
        	//DEBUG_PRINTLN();
    }

	/*
	 * Sets the given pixel of the matrix to be on or off
	 *
	 * x = pixel column
	 * y = pixel row
	 * on = true-> Pixel on, false->Pixel off.
	 */
    void KniwwelinoLib::MATRIXsetPixel(uint8_t x, uint8_t y, uint8_t on) {
    	matrixTextCount = 0;
    	drawPixel(x, y, on);
    }

	/*
	 * Sets the blink rate of the matrix.
	 *
	 * b = Blink Rate (one of: MATRIX_STATIC/MATRIX_BLINK_2HZ/MATRIX_BLINK_1HZ/MATRIX_BLINK_HALFHZ)
	 */
	void KniwwelinoLib::MATRIXsetBlinkRate(uint8_t b) {
	  Wire.beginTransmission(HT16K33_ADDRESS);
	  if (b > 3) b = 0; // turn static on if not sure;
	  Wire.write(HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (b << 1));
	  Wire.endTransmission();
	}

	/*
	 * Sets the brightness of the matrix.
	 *
	 * b = Brightness (range 1-15)
	 */
	void KniwwelinoLib::MATRIXsetBrightness(uint8_t b) {
	  b = constrain(b, MATRIX_MIN_BRIGHTNESS, MATRIX_MAX_BRIGHTNESS);
	  Wire.beginTransmission(HT16K33_ADDRESS);
	  Wire.write(HT16K33_CMD_BRIGHTNESS | b);
	  Wire.endTransmission();
	}

	/*
	 * Sets the scrolling speed of the Matrix.
	 *
	 * speed = Speed (range 1-10)
	 */
	void KniwwelinoLib::MATRIXsetScrollSpeed(uint8_t speed) {
		speed = constrain(speed, 1, 10);
		matrixScrollDiv = (11-speed);
	}

	/*
	 * Clears the Matrix.
	 */
	void KniwwelinoLib::MATRIXclear() {
		matrixTextCount = 0;
		for (uint8_t i = 0; i < 8; i++) {
			displaybuffer[i] = 0;
		}
		redrawMatrix = true;
	}

	/*
	 * Show the internal device ID on the matrix as on/off digital pattern.
	 * the first 5 digits of the ID on the 5 lines (first 4 cols)
	 * the last digits of the ID on the 5th column
	 */
	void KniwwelinoLib::MATRIXshowID() {
		String id = Kniwwelino.getID();
		DEBUG_PRINTLN(id);
		// write the first 5 digits of the ID to the 5 lines (first 4 cols)
		for(int i=0; i < 5; i++) {
			String sub = id.substring((i),(i)+1);
//			DEBUG_PRINT("\t"+sub + " ");
			int number = (int) strtol( &sub[0], 0, 16);
//			DEBUG_PRINT(number);Serial.print("\t");
			for(int j=0; j < 4; j++) {
//				DEBUG_PRINT(bitRead(number, 3-j));
				Kniwwelino.MATRIXsetPixel(j, i, bitRead(number, 3-j));
			}
//			DEBUG_PRINTLN();
		}
		// write the last digits of the ID to the 5th column
		String sub = id.substring(5);
//		DEBUG_PRINT("\t"+sub + " ");
		int number = (int) strtol( &sub[0], 0, 16);
//		DEBUG_PRINT(number);Serial.print("\t");
		for(int j=0; j < 4; j++) {
//			DEBUG_PRINT(bitRead(number, 3-j));
			Kniwwelino.MATRIXsetPixel(4, j, bitRead(number, 3-j));
		}
//		DEBUG_PRINTLN();
		redrawMatrix = true;
	}

	/*
	 * returns true if the matrix text scrolling has been finished.
	 */
	boolean KniwwelinoLib::MATRIXtextDone() {
		return (matrixTextCount == 0);
	}

	/*
	 * set the Pixel at pos x,y of the matrix to on/off
	 *
	 * x = X Position (0-4)
	 * y = Y Position (0-4)
	 * on = true/false
	 *
	 */
    void KniwwelinoLib::drawPixel(int16_t x, int16_t y, uint16_t on) {
    	if ((y < 0) || (y >= 5)) return;
    	  if ((x < 0) || (x >= 5)) return;
    	  // kniwwelino hardware specific: mirror cols
    	  x = 4 - x;
    	  // swap x/y
    	  int a = x;
    	  x = y;
    	  y = a;

    	  if (on) {
    	    displaybuffer[y] |= 1 << x;
    	  } else {
    	    displaybuffer[y] &= ~(1 << x);
    	  }
    	  redrawMatrix = true;
    }


	/*
	 * internal function that handles all matrix updates and text scrolling.
	 * called by the ticker.
	 */
    void KniwwelinoLib::_MATRIXupdate() {
    	// move Matrix Text if active.
    	if (matrixTextCount != 0 && (_tick%matrixScrollDiv) == 0) {
    		for (uint8_t i = 0; i < 8; i++) {
    			displaybuffer[i] = 0;
    		}
    		if (matrixText.length() == 1) {
    			setCursor(1,5);
    			print(matrixText);
    			redrawMatrix = true;
    			matrixTextCount = 0;
    		} else {
				setCursor(matrixPos,5);
				print(matrixText);
				redrawMatrix = true;
				matrixPos--;
				if (matrixPos < -(((int)matrixText.length())*4)) {
					matrixPos = 4;
					if (matrixTextCount > 0) {
						matrixTextCount--;
					}
				}
    		}
    	} else {

    	}

    	if (!redrawMatrix) return;

    	Wire.beginTransmission(HT16K33_ADDRESS);
    	Wire.write(HT16K33_DISP_REGISTER); // start at address $00
    	for (uint8_t i = 0; i < 8; i++) {
    	  Wire.write(displaybuffer[i] & 0xFF);
    	  Wire.write(displaybuffer[i] >> 8);
    	}
    	Wire.endTransmission();
    	redrawMatrix = false;
    }

//==== Onboard Button functions ==============================================

	/*
	 * Check if Button A has been clicked (pressed-released) since the last check.
	 *
	 * click state will be reset after checking,
	 * means calling this method twice it will report true, false.
	 *
	 * return true if yes, false if no.
	 */
    boolean KniwwelinoLib::BUTTONAclicked() {
    	if (buttonA) return false; // still pressed

    	boolean clicked = buttonAClicked;
    	buttonAClicked = false;
    	return (clicked);
    }

	/*
	 * Check if Button B has been clicked (pressed-released) since the last check.
	 *
	 * click state will be reset after checking,
	 * means calling this method twice it will report true, false.
	 *
	 * return true if yes, false if no.
	 */
    boolean KniwwelinoLib::BUTTONBclicked() {
    	if (buttonB) return false; // still pressed

    	boolean clicked = buttonBClicked;
    	buttonBClicked = false;
    	return (clicked);
    }

	/*
	 * Check if Button A and B have been clicked (pressed-released) since the last check.
	 *
	 * click state will be reset after checking,
	 * means calling this method twice it will report true, false.
	 *
	 * return true if yes, false if no.
	 */
    boolean KniwwelinoLib::BUTTONABclicked() {
    	if (buttonA || buttonB) return false; // still pressed

    	boolean clicked = buttonABClicked;
    	buttonABClicked = false;
    	if (clicked) {
    		buttonAClicked = false;
    		buttonBClicked = false;
    	}
    	return (clicked);
    }

	/*
	 * Check if Button A is pressed right now.
	 *
	 * return true if yes, false if no.
	 */
    boolean KniwwelinoLib::BUTTONAdown() {
    	return buttonA;
    }

	/*
	 * Check if Button B is pressed right now.
	 *
	 * return true if yes, false if no.
	 */
    boolean KniwwelinoLib::BUTTONBdown() {
    	return buttonB;
    }

	/*
	 * Internal function to check the buttons states.
	 * called by the ticker.
	 *
	 */
	void KniwwelinoLib::_Buttonsread() {
		  // read Buttons
		  Wire.beginTransmission(HT16K33_ADDRESS);
		  Wire.write(HT16K33_KEYINT_REGISTER);
		  Wire.endTransmission();
		  Wire.requestFrom(HT16K33_ADDRESS, 1);
		  buttonsPressed = (Wire.read() != 0);

		  Wire.beginTransmission(HT16K33_ADDRESS);
		  Wire.write(HT16K33_KEYS_REGISTER);
		  Wire.endTransmission();

		  Wire.requestFrom(HT16K33_ADDRESS, 2);
		  buttonA = (Wire.read() != 0);
		  buttonB = (Wire.read() != 0);

		  if (buttonA) buttonAClicked = true;
		  if (buttonB) buttonBClicked = true;
		  if (buttonA && buttonB) buttonABClicked = true;
	}


	//==== IOT: WIFI functions ==============================================

	/*
	 * internal function to connect the Kniwwelino to the Wifi.
	 * function is called during setup, or if the connection is lost
	 *
	 * NO NEED TO CALL THIS FUNCTION MANUALLY
	 *
	 * wifiMgr = if true, the Board will act as an access point allowing you to connect to it with your phone/tablet/pc
	 * and manage to configure which wifi to use on the next bootup.
	 *
	 * fast = if true, it will skip displaying the wifi name after connecting.
	 *
	 * wifi credentials for all wifis that have been entered via the wifi manager will be stored
	 * on the board to be able to connect to the wifi again next time.
	 * The Kniwwelino board will try to autoconnect to the last used wifi.
	 * If that is not working, it will try check all available wifi networks and connect to the
	 * one with the best reception rate and available credentials.
	 *
	 */
	boolean KniwwelinoLib::WIFIsetup(boolean wifiMgr, boolean fast, boolean reconnecting) {
      if (!wifiEnabled) return false;

	  // do nothing if connected
	  if (!wifiMgr && fast && WiFi.status() == WL_CONNECTED) {
		  return true;
	  }

	  //DEBUG_PRINTLN(getIP());
	  //WiFi.persistent(false);

	  // Delete stored networks
#ifdef CLEARCONF
	  SPIFFS.begin();
	  SPIFFS.remove(FILE_WIFI);
#endif

	  // read wifi conf file
	  String wifiConf = FILEread(FILE_WIFI);
	  //DEBUG_PRINTLN(wifiConf);

	  // No Wifis Saved -> Wifi MAnager.
//	  if (wifiConf.length() == 0) {
//		  wifiMgr = true;
//	  }

	  // if both buttons clicked on startup -> Wifi Manager
	  if (wifiMgr) {
		  Kniwwelino.RGBsetColorEffect(STATE_WIFIMGR, RGB_FLASH, -1);
		  MATRIXwrite("WIFI AP: "+ Kniwwelino.getID(), MATRIX_FOREVER, false);
		  char apID[getName().length()+1];
		  getName().toCharArray(apID, sizeof(apID)+1);

		  DEBUG_PRINT(F("Starting WIFI Manager Access Point with SSID: "));DEBUG_PRINTLN(apID);
		  WiFiManager wifiManager;
		  wifiManager.resetSettings();
		  wifiManager.setTimeout(300);
		  wifiManager.autoConnect(apID);
		  DEBUG_PRINT(F("Wifi Manager Ended "));
	  }

	  // show connecting to Wifi
	  Kniwwelino.RGBsetColorEffect(STATE_WIFI, RGB_BLINK, RGB_FOREVER);
	  if (!fast) Kniwwelino.MATRIXdrawIcon(ICON_WIFI);

	  // AP no longer needed, saves Energy
	  WiFi.mode(WIFI_STA); //  Force the ESP into client-only mode
	  wifi_set_sleep_type(LIGHT_SLEEP_T); //  Enable light sleep

	  //DEBUG_PRINTLN(getIP());

	  // if we have a connection -> store it
	  if (WiFi.status() == WL_CONNECTED) {
		  String wifiSSID = WiFi.SSID();
		  String wifiPWD 	= WiFi.psk();
		  //DEBUG_PRINT(F("Wifi is connected to "));DEBUG_PRINTLN(wifiSSID + " " + wifiPWD);

		  // add current wifi if not in file.
		  String newLine = wifiSSID + "=" + wifiPWD + "\n";
		  if (wifiConf.indexOf(newLine) == -1) {
			  DEBUG_PRINT(F("Storing Wifi to /wifi.conf: "));DEBUG_PRINTLN(wifiSSID);
			  wifiConf = newLine + wifiConf;
			  //DEBUG_PRINTLN(wifiConf);
			  FILEwrite(FILE_WIFI, wifiConf);
		  }
	  } else if (wifiConf.length() > 0){
		  // check stored credentials against available networks
		  int networks = WiFi.scanNetworks();
		  DEBUG_PRINT(F("Available Wifi networks: "));DEBUG_PRINTLN(networks);

		  // if Wifi Networks are available
		  if (networks > 0) {

			  // sort by RSSI
			  int indices[networks];
			  for (int i = 0; i < networks; i++) {
				 //DEBUG_PRINT(WiFi.SSID(i));DEBUG_PRINT(" RSSI: ");DEBUG_PRINTLN(WiFi.RSSI(i));
			     indices[i] = i;
			  }
			  for (int i = 0; i < networks; i++) {
				  for (int j = i + 1; j < networks; j++) {
					  if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
			              std::swap(indices[i], indices[j]);
					  }
				  }
			  }

			  // try to connect to the available networks
			  for (int i = 0; i<networks;i++) {
				  int nw = indices[i];
				  String ssID = WiFi.SSID(nw);
				  DEBUG_PRINT("\t");DEBUG_PRINT(ssID); DEBUG_PRINT(" RSSI: ");DEBUG_PRINT(WiFi.RSSI(nw));
				  int pos = wifiConf.indexOf(ssID+"=");
				  if (pos >= 0) {
				  // we have credentials for this network...
					 String pwd = wifiConf.substring(wifiConf.indexOf("=", pos)+1, wifiConf.indexOf("\n", pos));
					 DEBUG_PRINT(F(" PW is: ")); DEBUG_PRINT("***");//DEBUG_PRINT(pwd);
					 DEBUG_PRINT(F(" connecting"));

					 char cSSID[20];char cPWD[20];
					 ssID.toCharArray(cSSID, 20);
					 pwd.toCharArray(cPWD, 20);
					 //DEBUG_PRINT(F("|"));DEBUG_PRINT(cSSID);DEBUG_PRINT("|");DEBUG_PRINT(pwd);DEBUG_PRINT(F("| connecting"));
//					 WiFi.mode(WIFI_OFF);
//					 WiFi.mode(WIFI_STA);
					 WiFi.begin(cSSID, cPWD);
					 uint8_t retries = 0;
					 while (WiFi.status() != WL_CONNECTED && retries < 20) {
						 delay(2000);
						 DEBUG_PRINT(F("."));
						 retries++;
				     }
					 if (WiFi.status() == WL_CONNECTED) {
						 DEBUG_PRINTLN();
						 break;
					 }
				  } else {
					  DEBUG_PRINTLN(F(" is unknown."));
				  }
			  }
		  }
	  }

	  // set connected status.
	  if (WiFi.status() == WL_CONNECTED) {
		  String wifiSSID = WiFi.SSID();
		  String wifiPWD 	= WiFi.psk();
		  DEBUG_PRINT(F("Wifi is connected to "));DEBUG_PRINT(wifiSSID); DEBUG_PRINT(F(" IP: "));DEBUG_PRINTLN(getIP());
		  Kniwwelino.RGBsetColor(STATE_WIFI);

		  if (! reconnecting) {
			  if (fast) {
				  MATRIXwriteOnce("WIFI: "+ wifiSSID);
			  }	 else {
				  MATRIXwriteAndWait("WIFI: "+ wifiSSID);
			  }
		  }
		  wifiEnabled = true;
		  return true;
	  } else {
		  DEBUG_PRINTLN(F("Error connecting to Wifi!"));
		  Kniwwelino.RGBsetColorEffect(STATE_ERR, RGB_BLINK, RGB_FOREVER);

		  if (! reconnecting) {
			  if (fast) {
				  MATRIXwriteOnce(F("NO WIFI!"));
			  } else {
				  MATRIXwriteAndWait(F("NO WIFI!"));
			  }
		  }
	  }

	  //wifiEnabled = false;
	  return false;

	}

	//==== IOT: MQTT functions ==============================================

	/*
	 * internal function to setup the mqtt connection
	 * function is called during setup
	 *
	 * NO NEED TO CALL THIS FUNCTION MANUALLY
	 *
	 */
	boolean KniwwelinoLib::MQTTsetup(const char broker[], int port, const char user[], const char  password[]) {
		DEBUG_PRINT(F("Setting up MQTT Broker: "));DEBUG_PRINT(broker);


		mqtt.begin(broker, port, wifi);
		mqtt.onMessage(Kniwwelino._MQTTmessageReceived);

		strcpy(Kniwwelino.mqttUser, user);
		strcpy(Kniwwelino.mqttPW, password);


		mqttEnabled = MQTTconnect();
	}


	/*
	 * internal function to connect the Kniwwelino to the mqtt broker.
	 * function is called during setup, or if the connection is lost
	 *
	 * NO NEED TO CALL THIS FUNCTION MANUALLY
	 *
	 */
	boolean KniwwelinoLib::MQTTconnect() {
		// do nothing if connected
		if (mqtt.connected()) {
			return true;
		}

		// stop if no Wifi is available.
		if (!WIFIsetup(false, true, true)) {
//			mqttEnabled = false;
			return false;
		}

		Kniwwelino.RGBsetColorEffect(STATE_MQTT, RGB_BLINK, RGB_FOREVER);
		//Kniwwelino.MATRIXdrawIcon(ICON_WIFI);

		char c_clientID[getName().length()+1];
		getName().toCharArray(c_clientID, sizeof(c_clientID)+1);


		uint8_t retries = 0;
		DEBUG_PRINT(F(" Connecting to MQTT "));
		while (!mqtt.connect(c_clientID, Kniwwelino.mqttUser, Kniwwelino.mqttPW)&& retries < 20) {
			DEBUG_PRINT(".");
			delay(1000);
			retries++;
		}

		if (mqtt.connected())  {
			mqttEnabled = true;
			DEBUG_PRINTLN(" CONNECTED");

			DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(Kniwwelino.mqttTopicUpdate);
			mqtt.subscribe(Kniwwelino.mqttTopicUpdate);
			DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(Kniwwelino.mqttTopicReqPwd);
			mqtt.subscribe(Kniwwelino.mqttTopicReqPwd);

			_MQTTupdateStatus(true);

			// resubscribe to topics
			for (int j=9; j>-1; j--) {
			      if(Kniwwelino.mqttSubscriptions[j] != "") {
			    	  mqtt.subscribe(mqttSubscriptions[j]);
			      }
			}

			Kniwwelino.RGBsetColor(STATE_MQTT);
			return true;
		} else {
			DEBUG_PRINTLN("\nUnable to connect to MQTT!");
//			mqttEnabled = false;
			Kniwwelino.RGBsetColor(STATE_ERR);
			return false;
		}
	}

	/*
	 * sets the MQTT group where all messages are sent and all subscriptions are done.
	 * can be used to easily separate different applications/users/groups
	 * e.g. specify one group per game or one group per boards playing in the same game.
	 *
	 */
	void KniwwelinoLib::MQTTsetGroup(String group) {
		Kniwwelino.mqttGroup = group + "/";
	}

	/*
	 * sets the callback function that is called one a MQTT message is received for one of the
	 * subscribed topics.
	 *
	 * needs a callback function with signature:
	 * 		topic - String topic on which the message was received
	 * 		message - String content of the received message.
	 *
	 */
    void KniwwelinoLib::MQTTonMessage(void (cb)(String &topic, String &message)) {
    	mqttCallback = cb;
    }

	/*
	 * publishes/sents a message to the specified MQTT topic.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 *
	 * topic - topic to which the message will be sent
	 * message - content of the message.
	 *
	 */
    boolean KniwwelinoLib::MQTTpublish(const char topic[], String message) {
    	if (!mqttEnabled || ! MQTTconnect()) return false;
    	DEBUG_PRINT(F("MQTTpublish: "));DEBUG_PRINT(mqttGroup + topic);DEBUG_PRINT(F(" : "));DEBUG_PRINTLN(message);
    	boolean retVal =  mqtt.publish(mqttGroup + topic, message);
    	return retVal;
    }

	/*
	 * publishes/sents a message to the specified MQTT topic.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 *
	 * topic - topic to which the message will be sent
	 * message - content of the message.
	 *
	 */
    boolean KniwwelinoLib::MQTTpublish(String topic, String message) {
    	if (!mqttEnabled || ! MQTTconnect()) return false;
    	DEBUG_PRINT(F("MQTTpublish: "));DEBUG_PRINT(mqttGroup + topic);DEBUG_PRINT(F(" : "));DEBUG_PRINTLN(message);
    	boolean retVal =  mqtt.publish(mqttGroup + topic, message);
    	return retVal;
    }

	/*
	 * subscribes to the specified MQTT topic.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 * see https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices
	 * for more information on topic subscriptions.
	 *
	 * once a message is received for a subscribed topic, the MQTTonMessage callback is called.
	 *
	 * topic - topic to subscribe to
	 *
	 */
    boolean KniwwelinoLib::MQTTsubscribe(const char topic[]) {
    	if (!mqttEnabled || !MQTTconnect()) return false;

    	String s_topic = mqttGroup + String(topic);

    	DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(s_topic);
    	boolean ok = mqtt.subscribe(s_topic);

    	// stored new subscription
    	for (int j=9; j>0; j--) {
    		Kniwwelino.mqttSubscriptions[j] = Kniwwelino.mqttSubscriptions[(j-1)];
    	}
    	Kniwwelino.mqttSubscriptions[0] = s_topic;
    	return ok;
    }

	/*
	 * subscribes to the specified MQTT topic.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 * see https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices
	 * for more information on topic subscriptions.
	 *
	 * once a message is received for a subscribed topic, the MQTTonMessage callback is called.
	 *
	 * topic - topic to subscribe to
	 *
	 */
    boolean KniwwelinoLib::MQTTsubscribe( String s_topic) {
    	if (!mqttEnabled || !MQTTconnect()) return false;

    	DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(mqttGroup + s_topic);
    	boolean ok = mqtt.subscribe(mqttGroup + s_topic);

    	// stored new subscription
    	for (int j=9; j>0; j--) {
    		Kniwwelino.mqttSubscriptions[j] = Kniwwelino.mqttSubscriptions[(j-1)];
    	}
    	Kniwwelino.mqttSubscriptions[0] = mqttGroup + s_topic;
    	return ok;
    }


	/*
	 * unsubscribes from the specified MQTT topic.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 *
	 * topic - topic to unsubscribe from
	 *
	 */
    boolean KniwwelinoLib::MQTTunsubscribe(const char topic[]) {
    	if (!mqttEnabled || ! MQTTconnect()) return false;

    	String s_topic = mqttGroup + String(topic);

    	DEBUG_PRINT(F("MQTTunsubscribe: "));DEBUG_PRINTLN(s_topic);
    	boolean ok = mqtt.unsubscribe(s_topic);

    	// delete stored subscription
    	for (int j=9; j>-1; j--) {
    		if(Kniwwelino.mqttSubscriptions[j] == s_topic)  Kniwwelino.mqttSubscriptions[j]= "";
    	}

    	return ok;
    }

	/*
	 * subscribes the Kniwwelinos RGB LED to specific MQTT topics.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 *
	 *	the board will listen on group/RGB/COLOR for messages.
	 *	Message format: "FF00FF", first 2 digits specify the RED, second the GREEN, third the BLUE component.
	 *	If a message is received, the LED color will be changed.
	 *
	 */
    void KniwwelinoLib::MQTTconnectRGB() {
    	KniwwelinoLib::MQTTsubscribe(String(MQTT_RGB) + "/#");
    	mqttRGB = true;
    }

	/*
	 * subscribes the Kniwwelinos MATRIX to specific MQTT topics.
	 * if an MQTT group is set, the topic is automatically preceded with this group string.
	 *
	 *	the board will listen on group/MATRIX/ICON for icons.
	 *	Message format: "B1111100000111110000011111" 25 pixels binary one after the other, prefix B OR "0x7008E828A0" one byte for each 5px row, prefix 0x
	 *	If a message is received, the ICON will be displayed.
	 *
	 *	the board will listen on group/MATRIX/TEXT for icons.
	 *	Message format: "Hello World!"
	 *	If a message is received, the Text will be displayed.
	 *
	 */
    void KniwwelinoLib::MQTTconnectMATRIX() {
    	KniwwelinoLib::MQTTsubscribe(String(MQTT_MATRIX) + "/#");
    	mqttMATRIX = true;
    }

	/*
	 * internal function to handle incoming messages on subscribed topics.
	 *
	 */
    void KniwwelinoLib::_MQTTmessageReceived(String &topic, String &payload) {
    	DEBUG_PRINT("MQTT messageReceived: ");
    	DEBUG_PRINT(topic);
    	DEBUG_PRINT(": ");
    	DEBUG_PRINTLN(""+payload);

    	// For the Platform update, login etc...
    	if (topic == Kniwwelino.mqttTopicReqPwd) {
    		DEBUG_PRINTLN("MQTT->PLATTFORM PW Request");
    		Kniwwelino.MQTTpublish(Kniwwelino.mqttTopicSentPwd, String(Kniwwelino.platformPW));
    	} else if (topic == Kniwwelino.mqttTopicUpdate) {
    		DEBUG_PRINTLN("MQTT->PLATTFORM UPDATE Request");
			if (payload.equals("configuration")) {
				Kniwwelino.PLATFORMcheckConfUpdate();
			} else if (payload.equals("firmware")) {
				if (! Kniwwelino.PLATFORMcheckFWUpdate()) {
					Kniwwelino.MATRIXwriteAndWait("Update Failed! ");
//					ESP.restart();
				}
			}

		// for simple LED and MAtrix functionalities
    	} else if (Kniwwelino.mqttRGB && topic.startsWith(Kniwwelino.mqttGroup + MQTT_RGBCOLOR)) {
    		Kniwwelino.RGBsetColor(payload);
    	} else if (Kniwwelino.mqttMATRIX && topic.startsWith(Kniwwelino.mqttGroup + MQTT_MATRIXICON)) {
    		Kniwwelino.MATRIXdrawIcon(payload);
    	} else if (Kniwwelino.mqttMATRIX && topic.startsWith(Kniwwelino.mqttGroup + MQTT_MATRIXTEXT)) {
    		if (payload.length() == 0) {
    			Kniwwelino.MATRIXclear();
    		} else {
    			Kniwwelino.MATRIXwrite(payload, MATRIX_FOREVER, false);
    		}
    	}

    	// for everything else -> call external callback function.
    	if (mqttCallback != nullptr) {
        	topic.replace(Kniwwelino.mqttGroup, "");
    		mqttCallback(topic, payload);
    	}
    }

    void KniwwelinoLib::_MQTTupdateStatus(boolean force) {
    	if (force || ((millis()-mqttLastPublished)/1000 > mqttPublishDelay)) {
    		if (!mqttEnabled || ! MQTTconnect()) return;
    		DEBUG_PRINTLN(F("MQTTpublish Status"));
    		mqtt.publish(mqttTopicStatus + "/libversion", LIB_VERSION);
    		mqtt.publish(mqttTopicStatus + "/firmware", FW_VERSION);
			mqttLastPublished = millis();
    	}
    }


	//==== IOT: Platform functions ==============================================

	/*
	 * internal function to handle over the air firmware updates
	 *
	 */
    boolean KniwwelinoLib::PLATFORMcheckFWUpdate() {
    	if (! idShowing) return true;

		Kniwwelino.RGBsetColorEffect(STATE_UPDATE, RGB_BLINK, RGB_FOREVER);
		Kniwwelino.MATRIXdrawIcon(ICON_ARROW_DOWN);

		DEBUG_PRINT(F("UPDATE: Checking for FW Update at: "));
		DEBUG_PRINTLN(DEF_UPDATESERVER);
		DEBUG_PRINTLN(DEF_FWUPDATEURL);

		t_httpUpdate_return ret = ESPhttpUpdate.update(DEF_UPDATESERVER, 80, DEF_FWUPDATEURL, fwVersion);
		switch (ret) {
		case HTTP_UPDATE_FAILED:
			if (DEBUG) Serial.printf("\tHTTP_UPDATE_FAILD Error (%d): %s \n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
			Kniwwelino.RGBsetColorEffect(STATE_ERR, RGB_BLINK, RGB_FOREVER);
			return false;

		case HTTP_UPDATE_NO_UPDATES:
			if (DEBUG) Serial.println("\tHTTP_UPDATE_NO_UPDATES");
			Kniwwelino.RGBclear();
			return false;

		case HTTP_UPDATE_OK:
			if (DEBUG) Serial.println("\tHTTP_UPDATE_OK");
			Kniwwelino.RGBsetColor(STATE_UPDATE);
			return true;
		}
    }

	/*
	 * internal function to handle over the air config updates
	 *
	 */
    boolean KniwwelinoLib::PLATFORMcheckConfUpdate() {

		Kniwwelino.RGBsetColorEffect(STATE_UPDATE, RGB_BLINK, RGB_FOREVER);
		Kniwwelino.MATRIXdrawIcon(ICON_ARROW_DOWN);

		DEBUG_PRINT(F("UPDATE: Checking for Conf Update at: "));
//		DEBUG_PRINT(updateServer);
		DEBUG_PRINTLN(DEF_FWUPDATEURL);

		HTTPClient http;
		http.begin(updateServer, 3001, DEF_CONFUPDATEURL);
		http.useHTTP10(true);
		http.setTimeout(8000);
		http.setUserAgent(F("ESP8266-http-Update"));
		http.addHeader(F("x-ESP8266-STA-MAC"), WiFi.macAddress());
		http.addHeader(F("x-ESP8266-AP-MAC"), WiFi.softAPmacAddress());
		http.addHeader(F("x-ESP8266-free-space"), String(ESP.getFreeSketchSpace()));
		http.addHeader(F("x-ESP8266-sketch-size"), String(ESP.getSketchSize()));
		http.addHeader(F("x-ESP8266-chip-size"),
				String(ESP.getFlashChipRealSize()));
		http.addHeader(F("x-ESP8266-sdk-version"), ESP.getSdkVersion());
		http.addHeader(F("x-ESP8266-version"), FW_VERSION);
		http.addHeader(F("x-ESP8266-type"), DEF_TYPE);

		//if (shouldSaveConfig) {
		http.addHeader(F("x-ESP8266-conf"), Kniwwelino.FILEread(FILE_CONF));
		//  shouldSaveConfig = false;
		//}

		DEBUG_PRINT("Sending conf : ");
		DEBUG_PRINTLN(Kniwwelino.FILEread(FILE_CONF));

		int httpCode = http.POST("");

		String payload = http.getString();

		DEBUG_PRINT("\tReceived HTTP Code: ");
		DEBUG_PRINTLN(httpCode + "");
		DEBUG_PRINT("\tPayload: ");
		DEBUG_PRINTLN(payload);
		DEBUG_PRINTLN();

		http.end();

		if (httpCode == 200) {
			// parse and update conf
			if (PLATFORMupdateConf(payload)) {
				// save conf to flash
				Kniwwelino.FILEwrite(FILE_CONF, payload);

				//reset after saving config.
				DEBUG_PRINTLN("Received and Saved new Config -> Rebooting....");

				WiFi.disconnect();
				ESP.restart();
				delay(1000);
			}
		}

		return true;
    }

	/*
	 * internal function to update the config from a given JSON object.
	 *
	 */
    boolean KniwwelinoLib::PLATFORMupdateConf(String confJSON) {
    	DynamicJsonBuffer jsonBuffer;
    	  JsonObject& json = jsonBuffer.parseObject(confJSON);
    	  String keyValueArray[8];
    	  if (DEBUG) {
    	    Serial.print("CONFIG: JSON: ");
    	    json.printTo(Serial);
    	  }

    	  if (json.success()) {
    	    DEBUG_PRINTLN("\t parsed json");

    	    if (json.containsKey("nodename"))             strcpy(nodename,            	json["nodename"]);
    	    if (json.containsKey("configupdateserver"))   strcpy(updateServer,  		json["configupdateserver"]);
    	    if (json.containsKey("configbrokerurl"))      strcpy(mqttServer,     		json["configbrokerurl"]);
    	    if (json.containsKey("configbrokerport"))     mqttPort =	    			json["configbrokerport"];
    	    if (json.containsKey("configbrokeruser"))     strcpy(mqttUser,    			json["configbrokeruser"]);
    	    if (json.containsKey("configbrokerpassword")) strcpy(mqttPW, 				json["configbrokerpassword"]);
    	    if (json.containsKey("configpublishdelay"))   mqttPublishDelay= 			json["configpublishdelay"];
    	    if (json.containsKey("fw_version"))           strcpy(fwVersion,          	json["fw_version"]);
    	    if (json.containsKey("personalParameters"))   strcpy(confPersonalParameters,json["personalParameters"]);

    	    int i = 0;
    	    char *value, bruteValue[32];
    	    char tmpValues[256];
    	    String jValue, key;
    	    strncpy(tmpValues, confPersonalParameters, 256);
    	    value = strtok(tmpValues, ",");
    	    while (value) {
    	      keyValueArray[i] = value;
    	      i++;
    	      value = strtok(0, ",");
    	    }
    	    for (i = 0; i < 8; i++) {
    	      if (keyValueArray[i] != NULL) {
    	        keyValueArray[i].toCharArray(bruteValue, 32);
    	        key = strtok(bruteValue, ":");
    	        jValue = strtok(0, ":");
    	        if (DEBUG) {
    	          Serial.print("key-value pair ");
    	          Serial.print(key);
    	          Serial.println(jValue);
    	        }
    	        myParameters->set(key, jValue);
    	      }
    	    }

    	    Kniwwelino.PLATFORMprintConf();

    	    return true;
    	  } else {
    	    if (DEBUG) Serial.println("\t failed to parse json config");
    	    return false;
    	  }
    }

	/*
	 * internal function to print the current config.
	 *
	 */
    void KniwwelinoLib::PLATFORMprintConf() {
      // print config
      if (DEBUG) {
        Serial.println("CONFIG: ");
        Serial.print("\t nodename: ");            	Serial.println(nodename);
        Serial.print("\t updateServer: ");  		Serial.println(updateServer);
        Serial.print("\t mqttServer: ");     		Serial.println(mqttServer);
        Serial.print("\t mqttPort: ");    			Serial.println(mqttPort);
        Serial.print("\t mqttUser: ");    			Serial.println(mqttUser);
        Serial.print("\t mqttPW: "); 				Serial.println(mqttPW);
        Serial.print("\t mqttPublishDelay: ");  	Serial.println(mqttPublishDelay);
        Serial.print("\t PersonalParameters: ");  	Serial.println(confPersonalParameters);
      }
    }


	//==== File System functions ==============================================

	/*
	 * reads the given filename as textfile from the internal flash memory
	 *
	 * fileName - filename to save the file, should start with /
	 *
	 * returns the content of the file as String object.
	 */
    String KniwwelinoLib::FILEread(String fileName) {
    	SPIFFS.begin();
    	File file = SPIFFS.open(fileName, "r");
    	if (!file) {
    		DEBUG_PRINT(F("FILEread: failed to read file: "));DEBUG_PRINTLN(fileName);
    		return String();
    	} else {
           // Allocate a buffer to store contents of the file.
    	   size_t size = file.size();
           std::unique_ptr<char[]> buf(new char[size]);
           file.readBytes(buf.get(), size);
    	   file.close();
    	   DEBUG_PRINT(F("FILEread: "));DEBUG_PRINTLN(fileName);
    	   return String(buf.get());
    	}
    }

	/*
	 * save the given string as textfile on the internal flash memory
	 *
	 * fileName - filename to save the file, should start with /
	 * content - String object containing the data to be saved.
	 */
    void KniwwelinoLib::FILEwrite(String fileName, String content) {
      SPIFFS.begin();
      File file = SPIFFS.open(fileName, "w");
      if (!file) {
    	  DEBUG_PRINT(F("FILEwrite: failed to write file: "));DEBUG_PRINTLN(fileName);
      } else {
        file.print(content);
        file.close();
        DEBUG_PRINT(F("FILEwrite: "));DEBUG_PRINTLN(fileName);
      }
      return;
    }

//// returns the current config variables
//// encoded as as Json String
//String BaseKitLib::getJson() {
//
//  DynamicJsonBuffer jsonBuffer;
//  JsonObject& json = jsonBuffer.createObject();
//
//  json["nodename"]              = nodename;
//  json["configupdateserver"]    = configupdateserver;
//  json["configbrokerurl"]       = configbrokerurl;
//  json["configbrokerport"]      = configbrokerport;
//  json["configbrokeruser"]      = configbrokeruser;
//  json["configbrokerpassword"]  = configbrokerpassword;
//  json["configpublishdelay"]    = configpublishdelay;
//  json["personalParameters"]    = personalParameters;
//
//  if (DEBUG) {
//    Serial.print("\tgetting this json ");
//    json.printTo(Serial);
//  }
//  String jString = String();
//  json.printTo(jString);
//
//  if (DEBUG) {
//    Serial.print("CONFIG: Created JSON: ");
//    Serial.println(jString);
//  }
//
//  return jString;
//}


//void BaseKitLib::connectToPlatform(){
//
//  Kniwwelino.readConfFile();
//  Kniwwelino.checkFWUpdate();
//  Kniwwelino.checkConfigUpdate();
//  Kniwwelino.int_configbrokerport= atoi(Kniwwelino.configbrokerport);
//  if (DEBUG)  Serial.println("DEBUG MODE: Parameters");
//  if(DEBUG) Serial.print("*DEBUG  configbrokerurl "), Serial.println(Kniwwelino.configbrokerurl);
//  if(DEBUG) Serial.print("*DEBUG  configbrokerport  "), Serial.println(Kniwwelino.configbrokerport);
//  if(DEBUG) Serial.print("*DEBUG  int_configbrokerport  "), Serial.println(int_configbrokerport);
//  if(DEBUG) Serial.print("*DEBUG  configbrokeruser  "), Serial.println(Kniwwelino.configbrokeruser);
//  if(DEBUG) Serial.print("*DEBUG  configbrokerpassword  "), Serial.println(Kniwwelino.configbrokerpassword);
//
//  Kniwwelino.mqtt_setting(Kniwwelino.configbrokerurl, Kniwwelino.int_configbrokerport, Kniwwelino.configbrokeruser, Kniwwelino.configbrokerpassword);
//
//
//  reqPwd = "/" + String(configbrokeruser) + "/reqBrokerPwd";
//  notif = "/" + String(configbrokeruser) + "/notification/" + WiFi.macAddress();
//  resPwd = "/" + String(configbrokeruser) + "/resBrokerPwd";
//
//
//  if(DEBUG) Serial.print("*DEBUG 1  notif   "), Serial.println(Kniwwelino.notif);
//  if(DEBUG) Serial.print("*DEBUG 1  reqPwd  "), Serial.println(Kniwwelino.reqPwd);
//  if(DEBUG) Serial.print("*DEBUG 1  resPwd  "), Serial.println(Kniwwelino.resPwd);
//  Serial.println("");
//
//  delay(100);
//
//
//  reqPwd.toCharArray(c_reqPwd, 100);
//  delay(500);
//  if(DEBUG) Serial.print("*DEBUG 2  c_notif   "), Serial.println(Kniwwelino.c_notif);
//  if(DEBUG) Serial.print("*DEBUG 2  c_reqPwd  "), Serial.println(Kniwwelino.c_reqPwd);
//  if(DEBUG) Serial.print("*DEBUG 2  c_resPwd  "), Serial.println(Kniwwelino.c_resPwd);
//  Serial.println("");
//
//  resPwd.toCharArray(c_resPwd, 100);
//  delay(500);
//  if(DEBUG) Serial.print("*DEBUG 3  c_notif   "), Serial.println(Kniwwelino.c_notif);
//  if(DEBUG) Serial.print("*DEBUG 3  c_reqPwd  "), Serial.println(Kniwwelino.c_reqPwd);
//  if(DEBUG) Serial.print("*DEBUG 3  c_resPwd  "), Serial.println(Kniwwelino.c_resPwd);
//  Serial.println("");
//
//  notif.toCharArray(c_notif, 100);
//  delay(500);
//  if(DEBUG) Serial.print("*DEBUG 4  c_notif   "), Serial.println(Kniwwelino.c_notif);
//  if(DEBUG) Serial.print("*DEBUG 4  c_reqPwd  "), Serial.println(Kniwwelino.c_reqPwd);
//  if(DEBUG) Serial.print("*DEBUG 4  c_resPwd  "), Serial.println(Kniwwelino.c_resPwd);
//
//  Serial.println("");
//  Serial.println("---------------------------------------------------------------------------");
//  Serial.println("");
//
//
//  if(DEBUG) Serial.print("*DEBUG  notif   "), Serial.println(Kniwwelino.notif);
//  if(DEBUG) Serial.print("*DEBUG  reqPwd  "), Serial.println(Kniwwelino.reqPwd);
//  if(DEBUG) Serial.print("*DEBUG  c_notif   "), Serial.println(Kniwwelino.c_notif);
//  delay(100);
//  if(DEBUG) Serial.print("*DEBUG  c_reqPwd  "), Serial.println(Kniwwelino.c_reqPwd);
//
//  Kniwwelino.subscribe(c_reqPwd);
//  Kniwwelino.subscribe(c_notif);
//
//}
//
//void BaseKitLib::new_fwversion(String newname )
//{
//
//  Kniwwelino.fw_version[100] = ' ';
//  newname.toCharArray(Kniwwelino.fw_version, 40);
//  if(DEBUG) Serial.print("*DEBUG  New fw_version"), Serial.println(Kniwwelino.fw_version);
//
//
//}


// pre-instantiate Objects //////////////////////////////////////////////////////
KniwwelinoLib Kniwwelino = KniwwelinoLib();
