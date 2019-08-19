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

#include <Adafruit_NeoPixel.h>
#include "Adafruit_GFX.h"
#include <Fonts/TomThumb.h>

//-- DEBUG Helpers -------------
#ifdef DEBUG
	#define DEBUG_PRINT(x)         Kniwwelino.log (String(x))
	#define DEBUG_PRINTLN(x)       Kniwwelino.logln (String(x))
#else
	#define DEBUG_PRINT(x)
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
 * Wifi enabled
 * Fastboot false
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
 * MQTT logging disabled
 */
void KniwwelinoLib::begin(boolean enableWifi, boolean fast) {
	begin(enableWifi, fast, false);
}


/*
 * begin - Initializes the Board hardware, Wifi and MQTT.
 * MUST be called once in the setup routine to use the board.
 *
 * boolean enableWifi Enable Wifi and try to connecto to Wifi and MQTT
 * boolean fast  Startup faster without waiting for all scrolling texts.
 * boolean mqttLog log debug messages to mqtt server
 */
void KniwwelinoLib::begin(boolean enableWifi, boolean fast, boolean mqttLog) {
	begin(FW_VERSION, enableWifi, fast, mqttLog);
}

/*
 * begin - Initializes the Board hardware, Wifi and MQTT.
 * MUST be called once in the setup routine to use the board.
 *
 * const char nameStr[] name of the running program
 * boolean enableWifi Enable Wifi and try to connecto to Wifi and MQTT
 * boolean fast  Startup faster without waiting for all scrolling texts.
 * boolean mqttLog log debug messages to mqtt server
 */
void KniwwelinoLib::begin(const char nameStr[], boolean enableWifi, boolean fast, boolean mqttLog) {

	unsigned long startTime = millis();

	EEPROM.begin(512);

	mqttLogEnabled = mqttLog;

	Serial.begin(115200);
	DEBUG_PRINTLN("=====================================================");
	DEBUG_PRINT("\n");DEBUG_PRINT(getName());DEBUG_PRINT(" Reset:\"");DEBUG_PRINTLN(ESP.getResetReason());
	DEBUG_PRINT(F("\" booting Sketch: "));DEBUG_PRINTLN(nameStr);

	// init RGB LED
	RGB.begin();
	if (silent) {
		RGB.setBrightness(1);
	} else {
		RGB.setBrightness(RGB_BRIGHTNESS);
		RGBsetColorEffect(255, 0, 0, RGB_FLASH, 5);
	}
	DEBUG_PRINT(F(" RGB:On"));

	// initialize I2C and LED Matrix Driver
	Wire.begin();
	Wire.beginTransmission(HT16K33_ADDRESS);
	Wire.write(0x21);  // turn on oscillator
	Wire.endTransmission();

	// init LED MATRIX
	setTextWrap(false);
	setFont(&TomThumb);
	MATRIXsetBlinkRate(MATRIX_STATIC);

	if (silent) {
		MATRIXsetBrightness(0);
	} else {
		MATRIXsetBrightness(MATRIX_DEFAULT_BRIGHTNESS);
	}

	MATRIXclear();
	// all on to start.
	fillRect(0,0, 5, 5, 1);
	DEBUG_PRINT(F(" MATRIX:On"));

	// init variables with defaults
	mqttTopicUpdate.replace(":","");
	mqttTopicReqPwd.replace(":","");
	mqttTopicLogEnabled.replace(":","");
	mqttTopicSentPwd.replace(":","");
	mqttTopicStatus.replace(":","");
	strncpy(updateServer, DEF_UPDATESERVER, sizeof(DEF_UPDATESERVER));
	strncpy(mqttServer, DEF_MQTTSERVER, sizeof(DEF_MQTTSERVER));
	strncpy(mqttUser, 	DEF_MQTTUSER, sizeof(DEF_MQTTUSER));
	strncpy(mqttPW, 	DEF_MQTTPW, sizeof(DEF_MQTTPW));
	strncpy(fwVersion, 	nameStr, strlen(nameStr));

	getName().toCharArray(nodename, getName().length());
	DynamicJsonBuffer jsonBuffer;
	myParameters = &jsonBuffer.createObject();
	mqttGroup = DEF_MQTTBASETOPIC;

	// BOOT: vars initialized
	MATRIXsetStatus(0);

	// attach base ticker for display and buttons
	baseTicker.attach(TICK_FREQ, _baseTick);
	_baseTick();
	// BOOT: ticker running
	MATRIXsetStatus(1);

	Kniwwelino.RGBsetColorEffect(RGB_COLOR_CYAN, RGB_FLASH, RGB_FOREVER);
	SPIFFS.begin();

	// BOOT: filesystem mounted
	MATRIXsetStatus(2);

	// init Variables from stored config
	DEBUG_PRINT(F(" Config:"));
	String conf = Kniwwelino.FILEread(FILE_CONF);
	yield();
	if (conf.length() > 10) {
		Kniwwelino.PLATFORMupdateConf(conf);
		yield();
		DEBUG_PRINT(F("OK "));
	}
	// BOOT: plattform config read
	MATRIXsetStatus(3);

	Kniwwelino.RGBclear();

	// BOOT: All done but Wifi -> line 2
	MATRIXsetStatus(4);

	DEBUG_PRINT(F("MAC: "));DEBUG_PRINTLN(WiFi.macAddress());

	unsigned long wifiStart = millis();
	if (enableWifi) {
		DEBUG_PRINT(F(" WIFI:"));
		boolean wifiMgr = false;

		// if button B is pressed -> Wifi Manager
		if (Kniwwelino.BUTTONBdown()){
			wifiMgr = true;
			DEBUG_PRINTLN(F("Starting Wifimanager."));
		} else if (Kniwwelino.BUTTONAdown()) {
			// if button A is pressed -> Display ID
			Kniwwelino.MATRIXshowID();
			idShowing = true;
		}

		Kniwwelino.WIFIsetup(wifiMgr, fast, false);
		DEBUG_PRINT(F("Wifi Logon took: "));DEBUG_PRINT((millis()-wifiStart));DEBUG_PRINTLN(F("ms"));

	} else {
		// save Energy
		WiFi.mode(WIFI_OFF);
	}

	if (!(WiFi.status() == WL_CONNECTED)) {
		wifiEnabled = false;
	}

	if (wifiEnabled) {
		// BOOT: Wifi Established
		MATRIXsetStatus(15);

		// start mqtt
		Kniwwelino.MQTTsetup(mqttServer, mqttPort, mqttUser, mqttPW);
		// BOOT: MQTT Established
		MATRIXsetStatus(16);

		_initNTP();
		// BOOT: MQTT Established
		MATRIXsetStatus(17);
		DEBUG_PRINT(F("Network Logon took: "));DEBUG_PRINT((millis()-wifiStart));DEBUG_PRINTLN(F("ms"));
	}

	// wait here for button b pressed if ID is showing
	if (wifiEnabled && idShowing) {

		// Store that we were in update Mode
		updateMode = true;
		EEPROM.write(EEPROM_ADR_UPDATE, updateMode);
		EEPROM.commit();

		// BOOT: Update Mode stored
		MATRIXsetStatus(18);

		_MQTTupdateStatus(true);

		// BOOT: status updated
		MATRIXsetStatus(19);

		DEBUG_PRINT("Time: ");DEBUG_PRINTLN(getTime());
		Kniwwelino.MATRIXshowID();
		Kniwwelino.RGBsetColorEffect(RGB_COLOR_ORANGE, RGB_BLINK, RGB_FOREVER);
		while (idShowing && !Kniwwelino.BUTTONBdown())  {
			Kniwwelino.sleep(1000);
			DEBUG_PRINTLN(F("Waiting for firmware download..."));

		}
		idShowing = false;
	} else if (!updateMode){
		// if we were in update Mode before the last Reset, stay in update mode.
		updateMode = EEPROM.read(EEPROM_ADR_UPDATE);
		if (ESP.getResetReason() == "Software/System restart" && updateMode) {
			updateMode = true;
		} else if (updateMode) {
			updateMode = false;
			EEPROM.write(EEPROM_ADR_UPDATE, updateMode);
			EEPROM.commit();
		}
	}

	// BOOT: nearly done
	MATRIXsetStatus(20);

	if (updateMode) {
		DEBUG_PRINTLN(F("FWUpdate Mode: Active"));
	}

	MATRIXclear();
	if (!silent) {
		MATRIXwriteOnce("Kniwwelino");
	}

	DEBUG_PRINT("Boot took: ");DEBUG_PRINT((millis()-startTime));DEBUG_PRINT("ms Time: ");DEBUG_PRINTLN(getTime());
	DEBUG_PRINTLN(F("=== WELCOME ====================================="));

	if (enableWifi && !wifiEnabled) {
		RGBsetColorEffect(STATE_ERR, RGB_BLINK, RGB_FOREVER);
	} else if (!silent) {
		RGBsetColorEffect(RGB_COLOR_GREEN, RGB_ON, 10);
	}

	RGB.setBrightness(RGB_BRIGHTNESS);
	MATRIXsetBrightness(MATRIX_DEFAULT_BRIGHTNESS);

}

void KniwwelinoLib::setSilent() {
	silent = true;
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

	void KniwwelinoLib::bgI2CStop() {
		bgI2C=false;
	}
	void KniwwelinoLib::bgI2CStart() {
		bgI2C=true;
	}

	/*
	 * Sleeps the current program for the given number of milli seconds.
	 * Use this one instead of arduino delay, as it handles Wifi and MQTT in the background.
	 */
	void KniwwelinoLib::sleep(unsigned long sleepMillis) {
		yield();
		if (sleepMillis < 100) {
			delay(sleepMillis);
		} else {
			unsigned long till = millis() + sleepMillis;
			while (till > millis()) {
				yield();

				if (mqttEnabled && mqtt.connected()) {
					mqtt.loop();
				}

				sleepMillis = till - millis();
				if (sleepMillis > 100) {
					delay(100);
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
		yield();
	    if (mqttEnabled) {
	    	if (!mqtt.connected()) {
	    		MQTTconnect();
	    	}

	    	if (mqtt.connected()) {
	    		mqtt.loop();
	    		_MQTTupdateStatus(false);
	    	}
	    }
	}

	/*
	 * internal ticker function that is called in the background.
	 * handles the update of LED, MAtrix and reads the buttons.
	 */
	void KniwwelinoLib::_baseTick() {
		_tick++;
		Kniwwelino._PINhandle();
		Kniwwelino._RGBblink();
		Kniwwelino._Buttonsread();
		Kniwwelino._MATRIXupdate();
	}

	//====  logging  =============================================================

	  void KniwwelinoLib::log (const String s) {
		  Serial.print (s);
		  if (mqttLogEnabled && mqttEnabled && mqtt.connected()) {
			  mqtt.publish(mqttTopicStatus + "/log", s);
		  }
	  }

	  void KniwwelinoLib::logln	(const String s) {
		  Serial.println (s);
		  if (mqttLogEnabled && mqttEnabled && mqtt.connected()) {
			  mqtt.publish(mqttTopicStatus + "/log", s + "\n");
		  }
	  }

	  void KniwwelinoLib::log (const char  s[]) {
		  Serial.print (s);
		  if (mqttLogEnabled && mqttEnabled && mqtt.connected()) {
			  mqtt.publish(mqttTopicStatus + "/log", s);
		  }
	  }

	  void KniwwelinoLib::logln	(const char s[]) {
		  Serial.println (s);
		  if (mqttLogEnabled && mqttEnabled && mqtt.connected()) {
			  mqtt.publish(mqttTopicStatus + "/log", s);
		  }
	  }

	/*
	 * internal function for EXTERNAL PIN button or LED blink/flash effects
	 */
	void KniwwelinoLib::_PINhandle() {
		pinBlinkCount++;
		if (pinBlinkCount > 10) {
			pinBlinkCount = 1;
		}
		
		for (int i = 0; i < sizeof(ioPinNumers); i++) {
			if (ioPinStatus[i] == PIN_UNUSED) {
				continue;
			}

			// read external Button
			if (ioPinStatus[i] == PIN_INPUT) {
				if (!digitalRead(ioPinNumers[i])) ioPinclicked[i] = true;
				continue;
			}

			// if Effect Time has passed, turn off Pin
			if (ioPinStatus[i] == PIN_OFF) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINTLN(" LOW");
				digitalWrite(ioPinNumers[i], LOW);
				continue;
			}
			// if Effect is static on, nothing to do
			if (ioPinStatus[i] == PIN_ON) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINTLN(" HIGH");
				digitalWrite(ioPinNumers[i], HIGH);
				continue;
			}
			// handle effect
			if (pinBlinkCount <= ioPinStatus[i]) {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINTLN(" HIGH");
				digitalWrite(ioPinNumers[i], HIGH);
			} else {
				//DEBUG_PRINT("_PINblink ");DEBUG_PRINT(ioPinNumers[i]);DEBUG_PRINTLN(" LOW");
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
		      ioPinStatus[0] = PIN_UNUSED;
		      break;
		    case D5:
		      ioPinStatus[1] = PIN_UNUSED;
		      break;
		    case D6:
		      ioPinStatus[2] = PIN_UNUSED;
		      break;
		    case D7:
		      ioPinStatus[3] = PIN_UNUSED;
		      break;
		    break;
		  }
	}

	void KniwwelinoLib::PINenableButton(uint8_t pin) {
		pinMode(pin, INPUT_PULLUP);
		switch (pin) {
		case D0:
			ioPinStatus[0] = PIN_INPUT;
			break;
		case D5:
			ioPinStatus[1] = PIN_INPUT;
			break;
		case D6:
			ioPinStatus[2] = PIN_INPUT;
			break;
		case D7:
			ioPinStatus[3] = PIN_INPUT;
			break;
		}
	}

	boolean KniwwelinoLib::PINbuttonClicked(uint8_t pin) {
    	if (!digitalRead(pin)) return false; // still pressed

		boolean clicked = false;
		switch (pin) {
		case D0:
			clicked = ioPinclicked[0];
			ioPinclicked[0] = false;
			break;
		case D5:
			clicked = ioPinclicked[1];
			ioPinclicked[1] = false;
			break;
		case D6:
			clicked = ioPinclicked[2];
			ioPinclicked[2] = false;
			break;
		case D7:
			clicked = ioPinclicked[3];
			ioPinclicked[3] = false;
			break;
		}

    	return (clicked);
	}

	boolean KniwwelinoLib::PINbuttonDown(uint8_t pin) {
		return !digitalRead(pin);
	}

//==== RGB LED  functions ====================================================

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as String in format #FF00FF
	 * first 2 digits specify the RED, second the GREEN, third the BLUE component.
	 */
	void KniwwelinoLib::RGBsetColor(String col) {
		uint32_t color = RGBhex2int(col);
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
		unsigned long color = RGBhex2int(col);
		RGBsetColorEffect(color, effect, count);
	}

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as 32 bit int.
	 */
	void KniwwelinoLib::RGBsetColor(unsigned long color) {
		RGBsetColorEffect(color, RGB_ON, RGB_FOREVER);
	}

	/*
	 * Set the RGB LED of the board to show the given color.
	 * col = color to show as 32 bit int.
	 * effect = on of RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 * count = how long shall the effect be shown. (10 = 1sec, -1 shows forever.)
	 */
	void KniwwelinoLib::RGBsetColorEffect(unsigned long color, uint8_t effect, int count) {
	     RGBsetColorEffect((uint8_t)(color >> 16) ,(uint8_t)(color >>  8), (uint8_t)color, effect, count);
	}

	void KniwwelinoLib::RGBsetColorEffect(String colorEffect) {
		int pos = 0, lastpos = 0;
		String color = "";
		int effect = RGB_ON;
		int duration = RGB_FOREVER;

		pos = colorEffect.indexOf(":", pos);
		if (pos == -1) {
			RGBsetColor(colorEffect);
			return;
		} else if (pos == 6) {
			color = colorEffect.substring(0,6);
			lastpos = pos+1;
			pos = colorEffect.indexOf(":", lastpos);
			if (pos != -1) {
				effect = colorEffect.substring(lastpos,pos).toInt();
				lastpos = pos+1;
				if (colorEffect.length() >= lastpos) {
					duration = colorEffect.substring(pos+1).toFloat();
				}
			}
		}
		RGBsetColorEffect(color, effect, duration);
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
		RGBsetEffect(effect, count);
		RGB.setPixelColor(0, red, green, blue);

		if (rgbColor != RGB.getPixelColor(0)) {
			RGB.show();
		}
		rgbColor = RGB.getPixelColor(0);
	}

	/*
	 * Set the RGB LED of the board to show the given effect.
	 * effect = on of RGB_ON/RGB_BLINK/RGB_FLASH/RGB_OFF
	 * count = how long shall the effect be shown. (10 = 1sec, -1 shows forever.)
	 */
	void KniwwelinoLib::RGBsetEffect(uint8_t effect, int count) {
		bool changed = (rgbEffect != effect);

		rgbEffect = effect;
		rgbEffectCount = count;

		if (rgbEffect==RGB_ON) {
			RGB.setPixelColor(0, rgbColor);
			changed = true;
		} else if (rgbEffect > 10) {
			changed = true;
		}

		if (changed) {
			RGB.setBrightness(rgbBrightness);
		}
	}

	/*
	 * Set the RGB LED of the board to be OFF
	 */
	void KniwwelinoLib::RGBclear() {
		rgbEffectCount=0;
		if (rgbColor!=0) {
			RGB.setPixelColor(0, 0);
			RGB.show();
		}
		rgbColor = 0;
	}

	/*
	 * Set the Brightness of the RGB LED of the board.
	 * b = brightness from 0-255
	 */
	void KniwwelinoLib::RGBsetBrightness(uint8_t b) {
		b = constrain(b, 1, 255);
		rgbBrightness = b;
		RGB.setBrightness(b);
		RGB.show();
	}

	/*
	 * returns the current LED Color as int
	 */
	uint32_t KniwwelinoLib::RGBgetColor() {
		return rgbColor;
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
			rgbBlinkCount++;
			if (rgbBlinkCount > 10) {
				rgbBlinkCount = 1;
				if (rgbEffectCount > 0) rgbEffectCount--;
			}
		} else if (rgbEffect <= 10) {
			// blink/flash onoff Effects
			if (rgbBlinkCount <= rgbEffect) {
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
			if (rgbBlinkCount > 10) {
				rgbBlinkCount = 1;
				if (rgbEffectCount > 0) rgbEffectCount--;
			}
		} else if (rgbEffect > 10) {
			// other brightness effects
			if (rgbEffect == RGB_SPARK) {
				RGB.setPixelColor(0, rgbColor);
				RGB.setBrightness(rgbEffectBrightness);
				RGB.show();
				rgbEffectBrightness /=2;
				if (rgbEffectBrightness <= 0) {
					rgbEffectBrightness = rgbBrightness;
					if (rgbEffectCount > 0) rgbEffectCount--;
				}
			} else if (rgbEffect == RGB_GLOW) {
				RGB.setPixelColor(0, rgbColor);
				RGB.setBrightness(rgbEffectBrightness/10);
				RGB.show();
				if (rgbEffectModifier>0) {
					rgbEffectBrightness *= 1.2;
				} else {
					rgbEffectBrightness /= 1.2;
				}
				if (rgbEffectBrightness <= 10) {
					rgbEffectBrightness = 10;
					rgbEffectModifier = 1;
				} else if (rgbEffectBrightness >= (rgbBrightness*10)) {
					rgbEffectModifier = -1;
					if (rgbEffectCount > 0) rgbEffectCount--;
				}
			}

		} else {
			RGB.setPixelColor(0, 0);
			RGB.show();
		}
	}

	/*
	 * Helper function to convert a given color in String format (#00FF00)
	 * to a 32bit int color.
	 */
	unsigned long KniwwelinoLib::RGBhex2int(String str) {
	   int i;
	   unsigned long val = 0;
	   int len = str.length();

	   // parse the first 6 chars for HEX
	   if (len > 6) len = 6;

	   for(i=0;i<len;i++) {
		  char c = str.charAt(i);
	      if(c <= 57)
	       val += (c-48)*(1<<(4*(len-1-i)));
	      else
	       val += (c-55)*(1<<(4*(len-1-i)));
	   }
	   return val;
	}
	
	/*
	 * Helper function to convert a given color hue (0-255)
	 * to a 32bit int color.
     * The colours are a transition r -> g -> b -> back to r
     * Inspired by the Adafruit examples and WS2812FX lib.
	 */
	unsigned long KniwwelinoLib::RGBhue2int(uint8_t hue) {
	  hue = 255 - hue;
	  if(hue < 85) {
	    return ((uint32_t)(255 - hue * 3) << 16) | ((uint32_t)(0) << 8) | (hue * 3);
	  } else if(hue < 170) {
	    hue -= 85;
	    return ((uint32_t)(0) << 16) | ((uint32_t)(hue * 3) << 8) | (255 - hue * 3);
	  } else {
	    hue -= 170;
	    return ((uint32_t)(hue * 3) << 16) | ((uint32_t)(255 - hue * 3) << 8) | (0);
	  }
	}

	String KniwwelinoLib::RGB82Hex(uint8_t c) {
		String s =  String(String(c<16?"0":"") + String(c, HEX));
		s.toUpperCase();
		return s;
	}

	String KniwwelinoLib::RGBcolor2Hex(unsigned long color) {
		return String(RGB82Hex((uint8_t)(color >> 16)) + RGB82Hex((uint8_t)(color >>  8)) + RGB82Hex((uint8_t)color));
	}
	
	String KniwwelinoLib::RGBhue2Hex(uint8_t hue) {
		return Kniwwelino.RGBcolor2Hex(Kniwwelino.RGBhue2int(hue));
	}

	String KniwwelinoLib::RGBcolor2Hex(uint8_t r, uint8_t g, uint8_t b) {
	  return String(RGB82Hex(r) + RGB82Hex(g) + RGB82Hex(b));
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
		MATRIXsetBlinkRate(MATRIX_STATIC);
    	if (text.length() == 0) {
    		Kniwwelino.MATRIXclear();
    	}
    	// if wait or different Text, reset position, else keep scrolling.
    	if (wait || text != matrixText) {
        	matrixPos = 4;
    	}
    	matrixText = text;
    	matrixCount = count;
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
		MATRIXclear();
    	matrixText = "";
    	matrixCount = -1;
		MATRIXsetBlinkRate(MATRIX_STATIC);

    	if (iconString.startsWith("B") || iconString.startsWith("b")) {
    		int pos = 0, lastpos = 0;
    		// count icons
    		iconcount = 1;
    		if (iconString.indexOf("\n") <= 0) {
				while (pos != -1 && lastpos < iconString.length()) {
					pos = iconString.indexOf("\n",lastpos);
					if (pos != -1) {
						iconcount++;
						lastpos = pos+1;
					}
				}
    		}

    		// String must be like "B1111100000111110000011111" 25 pixels one after the other
    		if (iconString.length() >25) {
				for(int i=0; i < 25; i++) {
					if (iconString.charAt(i+1) != '0') {
						drawPixel(i%5, i/5, 1);
					}
				}

				if (iconString.length()>26) {
					pos = 0;
					lastpos = 0;
					pos = iconString.indexOf(":", lastpos);
					if (pos >= 26) {
						lastpos = pos+1;
						pos = iconString.indexOf(":", lastpos);
						int effect = iconString.substring(lastpos,pos).toInt();
						MATRIXsetBlinkRate(effect);
						lastpos = pos+1;

						if (iconString.length() >= lastpos) {
							pos = iconString.indexOf("\n", lastpos);
							float duration = iconString.substring(lastpos, pos).toFloat();
							matrixCount = duration*10;
						}
					}
				}
    		} else {
    			DEBUG_PRINTLN(F("MATRIXdrawIcon:Binay: Wrong String length (not 26)"));
    			return;
    		}
    		redrawMatrix = true;
    	} else if (iconString.startsWith("0x")) {
    		// String must be like "0x7008E828A0" one byte for each 5px row
    		if (iconString.length() != 12) {
    			DEBUG_PRINTLN(F("MATRIXdrawIcon:Hex: Wrong String length (not 12)"));
    			return;
    		} else {
    		}
    		for(int i=0; i < 5; i++) {
    			String sub = iconString.substring((i*2)+2,(i*2)+4);
    			int number = (int) strtol( &sub[0], NULL, 16);
    			for(int j=0; j < 5; j++) {
    				MATRIXsetPixel(j, i, bitRead(number, 7-j));
    			}
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
			MATRIXsetBlinkRate(MATRIX_STATIC);
			matrixCount = -1;
			matrixText = "";

        	MATRIXclear();
        	for(int i=0; i < 25; i++) {
        		MATRIXsetPixel(i%5, i/5, bitRead(iconLong, 24-i));
        	}
    }

	/*
	 * Sets the given pixel of the matrix to be on or off
	 *
	 * x = pixel column
	 * y = pixel row
	 * on = true-> Pixel on, false->Pixel off.
	 */
    void KniwwelinoLib::MATRIXsetPixel(uint8_t x, uint8_t y, uint8_t on) {
    	matrixText = "";
    	matrixCount = -1;
    	drawPixel(x, y, on);
    }

	/*
	 * returns the status of the given pixel of the matrix
	 *
	 */
    boolean KniwwelinoLib::MATRIXgetPixel(uint8_t x, uint8_t y) {
    	// kniwwelino hardware specific: mirror cols
    	int x1 = 4 - x;
    	int y1 = y;
    	// swap x/y
//    	int a = x;
//    	x = y;
//    	y = a;

  	  if (rotation == 0) {
  		  x = y1;
  		  y = x1;
  	  } else if (rotation == 1) {
  		  x = 4-x1;
  		  y = y1;
  	  } else if (rotation == 2) {
  		  x = 4-y1;
  		  y = 4-x1;
  	  } else if (rotation == 3) {
  		  x = x1;
  		  y = 4-y1;
  	  }
    	return bitRead((uint8_t) displaybuffer[y], x);
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
		matrixCount = 0;
		for (uint8_t i = 0; i < 8; i++) {
			displaybuffer[i] = 0;
		}
		redrawMatrix = true;
	}


    void KniwwelinoLib::MATRIXsetStatus(uint8_t s) {
    	if (idShowing) return;

    	for(int i=0; i < 25; i++) {
    		MATRIXsetPixel(i%5, i/5, s<i);
    	}
    }

	/*
	 * Show the internal device ID on the matrix as on/off digital pattern.
	 * the first 5 digits of the ID on the 5 lines (first 4 cols)
	 * the last digits of the ID on the 5th column
	 */
	void KniwwelinoLib::MATRIXshowID() {
		String id = Kniwwelino.getID();
		DEBUG_PRINT("ID:");DEBUG_PRINT(id);DEBUG_PRINT(" MAC:");DEBUG_PRINTLN(getMAC());
		// write the first 5 digits of the ID to the 5 lines (first 4 cols)
		for(int i=0; i < 5; i++) {
			String sub = id.substring((i),(i)+1);
			int number = (int) strtol( &sub[0], 0, 16);
			for(int j=0; j < 4; j++) {
				Kniwwelino.MATRIXsetPixel(j, i, bitRead(number, 3-j));
			}
		}
		// write the last digits of the ID to the 5th column
		String sub = id.substring(5);
		int number = (int) strtol( &sub[0], 0, 16);
		for(int j=0; j < 4; j++) {
			Kniwwelino.MATRIXsetPixel(4, j, bitRead(number, 3-j));
		}
		Kniwwelino.MATRIXsetPixel(4, 4, 0);
		redrawMatrix = true;
	}

	/*
	 * returns true if the matrix text scrolling has been finished.
	 */
	boolean KniwwelinoLib::MATRIXtextDone() {
		return (matrixCount == 0);
	}

	void KniwwelinoLib::setRotation(uint8_t rot) {
		rotation = rot;
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
    	  int x1 = 4 - x;
    	  int y1 = y;
    	  // swap x/y
    	  //x = y1;
    	  //y = x1;

    	  if (rotation == 0) {
    		  x = y1;
    		  y = x1;
    	  } else if (rotation == 1) {
    		  x = 4-x1;
    		  y = y1;
    	  } else if (rotation == 2) {
    		  x = 4-y1;
    		  y = 4-x1;
    	  } else if (rotation == 3) {
    		  x = x1;
    		  y = 4-y1;
    	  }

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
    	if (matrixText.length()>0) {
			if (matrixCount != 0 && (_tick%matrixScrollDiv) == 0) {
				for (uint8_t i = 0; i < 8; i++) {
					displaybuffer[i] = 0;
				}
				if (matrixText.length() == 1) {
					setCursor(1,5);
					print(matrixText);
					redrawMatrix = true;
					matrixCount = 0;
				} else {
					setCursor(matrixPos,5);
					print(matrixText);
					redrawMatrix = true;
					matrixPos--;
					if (matrixPos < -(((int)matrixText.length())*4)) {
						matrixPos = 4;
						if (matrixCount > 0) {
							matrixCount--;
						}
					}
				}
			}
		// else handle icon
    	} else {
    		// every sec.
    		if ((_tick%2) == 0) {
    			if (matrixCount > 0) matrixCount--;
    		}

    		if (matrixCount == 0) {
    			MATRIXclear();
    		}
    	}

    	if (!redrawMatrix) return;

    	if (!bgI2C) return;

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

		  if (!bgI2C) return;

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

	  // Delete stored networks
#ifdef CLEARCONF
	  SPIFFS.begin();
	  SPIFFS.remove(FILE_WIFI);
#endif

	  // read wifi conf file
	  String wifiConf = FILEread(FILE_WIFI);
	  DEBUG_PRINTLN("read wifi conf");//DEBUG_PRINT(wifiConf);
	  DEBUG_PRINTLN("-----------------");

	  // BOOT: wifi config read
	  if (!reconnecting) {
		  MATRIXsetStatus(5);
	  }

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

		  // BOOT: wifi manager ended
		  MATRIXsetStatus(5);
	  }

	  // show connecting to Wifi
	  if (!silent) Kniwwelino.RGBsetColorEffect(STATE_WIFI, RGB_BLINK, RGB_FOREVER);
	  if (!fast) Kniwwelino.MATRIXdrawIcon(ICON_WIFI);

	  // AP no longer needed, saves Energy
	  WiFi.mode(WIFI_STA); //  Force the ESP into client-only mode
	  WiFi.hostname(getName());

	  String wifiSSID = WiFi.SSID();
	  String wifiPWD 	= WiFi.psk();

	  if (wifiSSID.length() > 0) {
		  // BOOT: trying last used wifi
		  if (!reconnecting) {
			  MATRIXsetStatus(6);
		  }
		  DEBUG_PRINT(F("Connecting to Last Used Wifi: "));DEBUG_PRINTLN(wifiSSID);
		  WiFi.begin();
		  uint8_t retries = 0;
		  while (WiFi.status() != WL_CONNECTED && retries < 20) {
			  DEBUG_PRINT(".");
			  if (! reconnecting) {
				  if (retries%2 == 0) {
					  MATRIXsetStatus(6);
				  } else {
					  MATRIXsetStatus(5);
				  }
			  }
			  retries++;
			  delay(500);
		  }
		  DEBUG_PRINT("IP: ");DEBUG_PRINTLN(getIP());

	  } else {
		  DEBUG_PRINTLN("No Last Used Wifi stored");
	  }

		//forced Wifi Configuration Modus if not connected anyway
		//read file
		boolean forcedMode = false;
		String forcedWifiConf = FILEread(FILE_FORCED_WIFI);
		DEBUG_PRINTLN("read forced wifi conf");//DEBUG_PRINT(forcedWifiConf);
	  DEBUG_PRINTLN("-----------------");
		if (forcedWifiConf.length() > 0) {

			//parce file content.
			int pos = forcedWifiConf.indexOf("=");

			String ssID = forcedWifiConf.substring(0, pos);
			String pwd = forcedWifiConf.substring(pos+1, forcedWifiConf.indexOf("\n", pos));
			DEBUG_PRINT("read forced wifi conf: SSID=");DEBUG_PRINTLN(ssID);
			DEBUG_PRINT("read forced wifi conf: PWD=");DEBUG_PRINTLN(pwd);

			//skip connect if already CONNECTED
			if (WiFi.status() == WL_CONNECTED) {
				if (WiFi.SSID() == ssID) {
					forcedMode = true;
				}
			} else {
				char cSSID[32];char cPWD[63];
				ssID.toCharArray(cSSID, 32);
				pwd.toCharArray(cPWD, 63);
				WiFi.begin(cSSID, cPWD);
				uint8_t retries = 0;
				while (WiFi.status() != WL_CONNECTED && retries < 10) { //only 5 retries
					DEBUG_PRINT(F("."));
					if (!reconnecting) {
						 if (retries%2 == 0) {
							 MATRIXsetStatus(8);
						 } else {
							 MATRIXsetStatus(7);
						 }
					}
					retries++;
					delay(1000);
				}
				if (WiFi.status() == WL_CONNECTED) {
					DEBUG_PRINTLN("Connected.");
					forcedMode = true;
				}
			}
		}


	  // if we have a connection -> store it
	  if (WiFi.status() == WL_CONNECTED) {
		  // add current wifi if not in file.
		  String newLine = wifiSSID + "=" + wifiPWD + "\n";
		  if (!forcedMode && wifiConf.indexOf(newLine) == -1) {
			  DEBUG_PRINT(F("Storing Wifi to /wifi.conf: "));DEBUG_PRINTLN(wifiSSID);
			  wifiConf = newLine + wifiConf;
			  //DEBUG_PRINTLN(wifiConf);
			  FILEwrite(FILE_WIFI, wifiConf);
		  }
	  } else if (wifiConf.length() > 0){

		  // BOOT: scanning available wifis
		  if (! reconnecting) {
			  MATRIXsetStatus(7);
		  }

		  // check stored credentials against available networks
		  int networks = WiFi.scanNetworks();
		  DEBUG_PRINT(F("Available Wifi networks: "));DEBUG_PRINTLN(networks);

		  // if Wifi Networks are available
		  if (networks > 0) {

			  // sort by RSSI
			  int indices[networks];
			  for (int i = 0; i < networks; i++) {
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

					  // BOOT: connecting to saved WIFI
					  if (! reconnecting) {
						  MATRIXsetStatus(8);
					  }

					 String pwd = wifiConf.substring(wifiConf.indexOf("=", pos)+1, wifiConf.indexOf("\n", pos));
					 DEBUG_PRINT(F(" PW is: ")); DEBUG_PRINT("***");//DEBUG_PRINT(pwd);
					 DEBUG_PRINT(F(" connecting"));

					 char cSSID[32];char cPWD[63];
					 ssID.toCharArray(cSSID, 32);
					 pwd.toCharArray(cPWD, 63);
					 WiFi.begin(cSSID, cPWD);
					 uint8_t retries = 0;
					 while (WiFi.status() != WL_CONNECTED && retries < 20) {
						 DEBUG_PRINT(F("."));
						 if (! reconnecting) {
							  if (retries%2 == 0) {
								  MATRIXsetStatus(8);
							  } else {
								  MATRIXsetStatus(7);
							  }
						 }
						 retries++;
						 delay(1000);
				     }
					 if (WiFi.status() == WL_CONNECTED) {
						 DEBUG_PRINTLN("Connected.");
						 break;
					 }
				  } else {
					  DEBUG_PRINTLN(F(" is unknown."));
				  }
			  }
		  }
	  } else {
		  DEBUG_PRINTLN(F("No valid Wifi Config found"));
	  }

	  // set connected status.
	  if (WiFi.status() == WL_CONNECTED) {
		  String wifiSSID = WiFi.SSID();
		  String wifiPWD 	= WiFi.psk();
		  DEBUG_PRINT(F("Wifi is connected to "));DEBUG_PRINT(wifiSSID); DEBUG_PRINT(F(" IP: "));DEBUG_PRINTLN(getIP());
		  DEBUG_PRINT(F("Gateway: "));DEBUG_PRINT(WiFi.gatewayIP().toString().c_str());DEBUG_PRINT(F(" DNS: "));DEBUG_PRINTLN(WiFi.dnsIP(0).toString().c_str());

		  if (! silent) Kniwwelino.RGBsetColor(STATE_WIFI);
		  if (! reconnecting) {
			  // BOOT: WIFI CONNECTED
			  MATRIXsetStatus(9);
		  }
		  wifiEnabled = true;
		  return true;
	  } else {
		  DEBUG_PRINTLN(F("Error connecting to Wifi!"));
		  Kniwwelino.RGBsetColorEffect(STATE_ERR, RGB_BLINK, RGB_FOREVER);

		  if (! reconnecting) {
			  MATRIXwriteAndWait(F("NO WIFI!"));
		  }
	  }
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
		IPAddress brokerIP;
		WiFi.hostByName(broker, brokerIP);
		DEBUG_PRINT(F("Setting up MQTT Broker: "));DEBUG_PRINT(broker);DEBUG_PRINT(F(" "));DEBUG_PRINTLN(brokerIP.toString().c_str());
		mqtt.begin(broker, port, wifi);
		mqtt.onMessage(Kniwwelino._MQTTmessageReceived);
		strcpy(Kniwwelino.mqttUser, user);
		strcpy(Kniwwelino.mqttPW, password);
		mqttEnabled = MQTTconnect(false);
	}


	/*
	 * internal function to connect the Kniwwelino to the mqtt broker.
	 * function is called during setup, or if the connection is lost
	 *
	 * NO NEED TO CALL THIS FUNCTION MANUALLY
	 *
	 */
	boolean KniwwelinoLib::MQTTconnect() {
		return MQTTconnect(true);
	}

	boolean KniwwelinoLib::MQTTconnect(boolean silent) {
		// do nothing if connected
		if (mqtt.connected()) {
			return true;
		}

		// stop if no Wifi is available.
		if (!WIFIsetup(false, true, true)) {
			return false;
		}

		if (! silent) Kniwwelino.RGBsetColorEffect(STATE_MQTT, RGB_BLINK, RGB_FOREVER);

		char c_clientID[getName().length()+1];
		getName().toCharArray(c_clientID, sizeof(c_clientID)+1);


		uint8_t retries = 0;
		DEBUG_PRINT(F(" Connecting to MQTT "));
		while (!mqtt.connect(c_clientID, Kniwwelino.mqttUser, Kniwwelino.mqttPW)&& retries < 20) {
			DEBUG_PRINT(".");
			if (!silent) {
				if (retries%2 == 0) {
					MATRIXsetStatus(16);
				} else {
					MATRIXsetStatus(15);
				}
			}
			retries++;
			delay(1000);
		}

		if (mqtt.connected())  {
			mqttEnabled = true;
			DEBUG_PRINTLN(" CONNECTED");

			DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(Kniwwelino.mqttTopicUpdate);
			mqtt.subscribe(Kniwwelino.mqttTopicUpdate);
			DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(Kniwwelino.mqttTopicReqPwd);
			mqtt.subscribe(Kniwwelino.mqttTopicReqPwd);
			DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(Kniwwelino.mqttTopicLogEnabled);
			mqtt.subscribe(Kniwwelino.mqttTopicLogEnabled);

			_MQTTupdateStatus(true);

			// resubscribe to topics
			for (int j=9; j>-1; j--) {
			      if(Kniwwelino.mqttSubscriptions[j] != "") {
			    	  mqtt.subscribe(mqttSubscriptions[j]);
			      }
			}

			if (! silent) Kniwwelino.RGBsetColor(STATE_MQTT);
			return true;
		} else {
			DEBUG_PRINTLN("\nUnable to connect to MQTT!");
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
		Kniwwelino.mqttGroup = DEF_MQTTBASETOPIC + group + "/";
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
	 * subscribes to the specified public MQTT topic.
	 *
	 * see https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices
	 * for more information on topic subscriptions.
	 *
	 * once a message is received for a subscribed topic, the MQTTonMessage callback is called.
	 *
	 * topic - topic to subscribe to
	 *
	 */
	boolean KniwwelinoLib::MQTTsubscribepublic(const char topic[]) {
		if (!mqttEnabled || !MQTTconnect()) return false;

		String s_topic = DEF_MQTTBASETOPIC + String(topic);

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
	 *
	 * see https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices
	 * for more information on topic subscriptions.
	 *
	 * once a message is received for a subscribed topic, the MQTTonMessage callback is called.
	 *
	 * topic - topic to subscribe to
	 *
	 */
	boolean KniwwelinoLib::MQTTsubscribepublic( String s_topic) {
		if (!mqttEnabled || !MQTTconnect()) return false;

		DEBUG_PRINT(F("MQTTsubscribe: "));DEBUG_PRINTLN(DEF_MQTTBASETOPIC + s_topic);
		boolean ok = mqtt.subscribe(DEF_MQTTBASETOPIC + s_topic);

		// stored new subscription
		for (int j=9; j>0; j--) {
			Kniwwelino.mqttSubscriptions[j] = Kniwwelino.mqttSubscriptions[(j-1)];
		}
		Kniwwelino.mqttSubscriptions[0] = mqttGroup + s_topic;
		return ok;
	}

	/*
	 * unsubscribes from the specified MQTT topic.
	 *
	 * topic - topic to unsubscribe from
	 *
	 */
	boolean KniwwelinoLib::MQTTunsubscribepublic(const char topic[]) {
		if (!mqttEnabled || ! MQTTconnect()) return false;

		String s_topic = DEF_MQTTBASETOPIC + String(topic);

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
    	if (payload) DEBUG_PRINTLN(""+payload);
    	else DEBUG_PRINTLN("");
    	// For the Platform update, login etc...
    	if (topic == Kniwwelino.mqttTopicReqPwd) {
    		DEBUG_PRINTLN("MQTT->PLATTFORM PW Request");
        	DEBUG_PRINT(F("MQTTpublish: "));DEBUG_PRINT(Kniwwelino.mqttTopicSentPwd);DEBUG_PRINT(F(" : "));DEBUG_PRINTLN(String(Kniwwelino.platformPW));
        	Kniwwelino.mqtt.publish(Kniwwelino.mqttTopicSentPwd, String(Kniwwelino.platformPW));
    	} else if (topic == Kniwwelino.mqttTopicUpdate) {
    		DEBUG_PRINTLN("MQTT->PLATTFORM UPDATE Request");
			if (payload && payload.equals("configuration")) {
				Kniwwelino.PLATFORMcheckConfUpdate();
			} else if (payload && payload.equals("firmware")) {
				if (! Kniwwelino.PLATFORMcheckFWUpdate()) {
					Kniwwelino.MATRIXwriteAndWait("Update Failed! ");
				}
			}
    	} else if (topic == Kniwwelino.mqttTopicLogEnabled) {
			DEBUG_PRINTLN("MQTT->PLATTFORM MQTT LOG");
			if (payload && payload.equals("on")) {
				mqttLogEnabled = true;
			} else {
				mqttLogEnabled = false;
			}

		// for simple LED and MAtrix functionalities
    	} else if (Kniwwelino.mqttRGB && topic.startsWith(Kniwwelino.mqttGroup + MQTT_RGBCOLOR)) {
    		Kniwwelino.RGBsetColorEffect(payload);
    	} else if (Kniwwelino.mqttMATRIX && topic.startsWith(Kniwwelino.mqttGroup + MQTT_MATRIXICON)) {
    		Kniwwelino.MATRIXdrawIcon(payload);
    	} else if (Kniwwelino.mqttMATRIX && topic.startsWith(Kniwwelino.mqttGroup + MQTT_MATRIXTEXT)) {
    		if (!payload || payload.length() == 0) {
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
    		mqtt.publish(mqttTopicStatus + "/firmware", fwVersion);
    		mqtt.publish(mqttTopicStatus + "/resetReason", ESP.getResetReason());
    		mqtt.publish(mqttTopicStatus + "/number", String(EEPROM.read(EEPROM_ADR_NUM)));
			mqttLastPublished = millis();
    	}
    }

	//==== IOT: Platform functions ==============================================

	/*
	 * internal function to handle over the air firmware updates
	 *
	 */
    boolean KniwwelinoLib::PLATFORMcheckFWUpdate() {
    	if (!updateMode) return true;
		Kniwwelino.RGBsetColorEffect(STATE_UPDATE, RGB_BLINK, RGB_FOREVER);
		//Kniwwelino.MATRIXdrawIcon(ICON_ARROW_DOWN);
		Kniwwelino.MATRIXdrawIcon("B0010000100101010111000100:1:-1");


		DEBUG_PRINT(getTime());
		DEBUG_PRINT(F(" UPDATE: Checking for FW Update at: "));
		DEBUG_PRINT(DEF_UPDATESERVER);
		DEBUG_PRINT(DEF_FWUPDATEURL);
		IPAddress updateIP;
		WiFi.hostByName(DEF_UPDATESERVER, updateIP);
		DEBUG_PRINT(F(" "));DEBUG_PRINTLN(updateIP.toString().c_str());

		t_httpUpdate_return ret = ESPhttpUpdate.update(DEF_UPDATESERVER, 80, DEF_FWUPDATEURL, fwVersion);
		switch (ret) {
		case HTTP_UPDATE_FAILED:
#ifdef DEBUG
			Serial.printf("\tHTTP_UPDATE_FAILD Error (%d): %s \n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
#endif
			Kniwwelino.RGBsetColorEffect(STATE_ERR, RGB_BLINK, RGB_FOREVER);
			idShowing = false;
			return false;

		case HTTP_UPDATE_NO_UPDATES:
			DEBUG_PRINTLN("\tHTTP_UPDATE_NO_UPDATES");
			Kniwwelino.RGBclear();
			idShowing = false;
			return false;

		case HTTP_UPDATE_OK:
			DEBUG_PRINTLN("\tHTTP_UPDATE_OK");
			Kniwwelino.RGBsetColor(STATE_UPDATE);
			Kniwwelino.MATRIXdrawIcon("B0111000000011100000011111:1:-1");
			idShowing = false;
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
		DEBUG_PRINTLN(DEF_FWUPDATEURL);

		HTTPClient http;
		http.begin(updateServer, 80, DEF_CONFUPDATEURL);
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
		http.addHeader(F("x-ESP8266-version"), fwVersion);
		http.addHeader(F("x-ESP8266-type"), DEF_TYPE);
		http.addHeader(F("x-ESP8266-conf"), Kniwwelino.FILEread(FILE_CONF));

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
#ifdef DEBUG
    	    Serial.print("CONFIG: JSON: ");
    	    json.printTo(Serial);
#endif

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
#ifdef DEBUG
    	          Serial.print("key-value pair ");
    	          Serial.print(key);
    	          Serial.println(jValue);
#endif
    	        myParameters->set(key, jValue);
    	      }
    	    }

    	    Kniwwelino.PLATFORMprintConf();

    	    return true;
    	  } else {
    		DEBUG_PRINTLN("\t failed to parse json config");
    	    return false;
    	  }
    }

	/*
	 * internal function to print the current config.
	 *
	 */
    void KniwwelinoLib::PLATFORMprintConf() {
      // print config
#ifdef DEBUG
        Serial.println("CONFIG: ");
        Serial.print("\t nodename: ");            	Serial.println(nodename);
        Serial.print("\t updateServer: ");  		Serial.println(updateServer);
        Serial.print("\t mqttServer: ");     		Serial.println(mqttServer);
        Serial.print("\t mqttPort: ");    			Serial.println(mqttPort);
        Serial.print("\t mqttUser: ");    			Serial.println(mqttUser);
        Serial.print("\t mqttPW: "); 				Serial.println(mqttPW);
        Serial.print("\t mqttPublishDelay: ");  	Serial.println(mqttPublishDelay);
        Serial.print("\t PersonalParameters: ");  	Serial.println(confPersonalParameters);
#endif
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

			if (!SPIFFS.exists(fileName)) {
				DEBUG_PRINT(F("FILEread: file not found: "));DEBUG_PRINTLN(fileName);
				return String();
			}
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

//==== Tone functions ==============================================

	/*
	 * creates a short sound/tone on the given pin with the given note/frequency/duration
	 * pin - pin to output the sound
	 * note - frequency of played sound
	 * noteDuration - duration - type of note 4,8,16
	 */
	void KniwwelinoLib::playNote(uint8_t pin, unsigned int note, uint8_t noteDuration) {

		if (note < 1) note = 1;

		int duration = 1000 / noteDuration;
		tone(pin, note, duration);
		yield();

		// to distinguish the notes, set a minimum time between them.
		// the note's duration + 30% seems to work well:
		int pauseBetweenNotes = duration * 1.30;
		delay(pauseBetweenNotes);
		yield();

		// stop the tone playing:
		noTone (pin);
		yield();
	}

	/*
	 * creates a permanent sound/tone on the given pin with the given note/frequency
	 * pin - pin to output the sound
	 * note - frequency of played sound
	 */
	void KniwwelinoLib::playTone(uint8_t pin, unsigned int note) {
		tone(pin, note);
	}

	/*
	 * turns off the sound/tone on the given pin
	 * pin - pin to turn off the sound
	 */
	void KniwwelinoLib::toneOff(uint8_t pin) {
		noTone(pin);
	}

	KniwwelinoLib* KniwwelinoLib::getNTPTimeObject;

	void KniwwelinoLib::_initNTP() {
		ntpUdp.begin(NTP_PORT);
		Kniwwelino.getNTPTimeObject = this;
		setSyncProvider(_globalGetNTPTime);
		setSyncInterval(300);
	}
	time_t KniwwelinoLib::_getNtpTime()	{
	  IPAddress ntpServerIP; // NTP server's ip address
	  while (ntpUdp.parsePacket() > 0) ; // discard any previously received packets
	  DEBUG_PRINT("Transmit NTP Request ");
	  // get a random server from the pool
	  WiFi.hostByName(NTP_SERVER, ntpServerIP);
	  DEBUG_PRINTLN(NTP_SERVER);
	  // set all bytes in the buffer to 0
	  memset(packetBuffer, 0, NTP_PACKET_SIZE);
	  // Initialize values needed to form NTP request
	  // (see URL above for details on the packets)
	  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	  packetBuffer[1] = 0;     // Stratum, or type of clock
	  packetBuffer[2] = 6;     // Polling Interval
	  packetBuffer[3] = 0xEC;  // Peer Clock Precision
	  // 8 bytes of zero for Root Delay & Root Dispersion
	  packetBuffer[12] = 49;
	  packetBuffer[13] = 0x4E;
	  packetBuffer[14] = 49;
	  packetBuffer[15] = 52;
	  // all NTP fields have been given values, now
	  // you can send a packet requesting a timestamp:
	  ntpUdp.beginPacket(ntpServerIP, 123); //NTP requests are to port 123
	  ntpUdp.write(packetBuffer, NTP_PACKET_SIZE);
	  ntpUdp.endPacket();
	  uint32_t beginWait = millis();
	  while (millis() - beginWait < 1500) {
	    int size = ntpUdp.parsePacket();
	    if (size >= NTP_PACKET_SIZE) {
	      DEBUG_PRINTLN("Receive NTP Response");
	      ntpUdp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
	      unsigned long secsSince1900;
	      // convert four bytes starting at location 40 to a long integer
	      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
	      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
	      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
	      secsSince1900 |= (unsigned long)packetBuffer[43];
	      return timeZone.toLocal(secsSince1900 - 2208988800UL);
	    }
	  }
	  logln("No NTP Response :-(");
	  return 0; // return 0 if unable to get the time
	}

	String KniwwelinoLib::getTime()	{
	  // digital clock display of the time
	  String timeStr = String("") + (hour()<10?"0":"") + hour() + ":" + (minute()<10?"0":"") + minute() + ":" + (second()<10?"0":"") + second() +
			  " " + (day()<10?"0":"") + day() + "." + (month()<10?"0":"") + month() + "." + year();
	  return timeStr;
	}

// pre-instantiate Objects //////////////////////////////////////////////////////
KniwwelinoLib Kniwwelino = KniwwelinoLib();
