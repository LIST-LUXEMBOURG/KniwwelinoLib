/***************************************************

  KniwwelinoDemo

  Copyright (C) 2017 Luxembourg Institute of Science and Technology.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License.

  Example sketch to do some fancy welcome messages and LED Stuff.

  Connects to the Broker and listens for Text/Icons on the Matrix
  RGB LED is connected to RGB/COLOR and does a random wait before changing color.
  Buttons push icons to the Matrix channel to be displayed by all boards in group.

****************************************************/

#include <Kniwwelino.h>

#define MAXBRIGHTNESS  150
#define MINBRIGHTNESS  5
#define STEPBRIGHTNESS  5

#define TEXTCOUNT  6
String texts[TEXTCOUNT] = {"", "Kniwwelino", "Willkommen!", "Welcome!", "Moien!", "Bienvenue!"};
long last = 0, wait = 0;

int brightness = MAXBRIGHTNESS, step = -STEPBRIGHTNESS;

void setup() {

  //Initialize the Kniwwelino Board
  Kniwwelino.begin("Kniwwelino_demo",true, true, false); // Wifi=true, Fastboot=true, MQTT logging false
  Kniwwelino.MQTTsetGroup("KniwwelinoDemo");
  Kniwwelino.MQTTonMessage(messageReceived);
  Kniwwelino.MQTTconnectMATRIX();
  Kniwwelino.MQTTsubscribe("RGB/COLOR");

  Kniwwelino.MATRIXdrawIcon(ICON_HEART);

  Serial.println("RED");
  Kniwwelino.RGBsetColor(255, 0, 0);
  delay(300);
  Serial.println("GREEN");
  Kniwwelino.RGBsetColor(0, 255, 0);
  delay(300);
  Serial.println("BLUE");
  Kniwwelino.RGBsetColor(0, 0, 255);
  delay(300);
  Kniwwelino.RGBclear();

  randomSeed(analogRead(A0));

  if (Kniwwelino.isConnected()) {
    Kniwwelino.RGBsetColor("00FF00");
  } else {
    Kniwwelino.RGBsetColorEffect("FF0000", RGB_FLASH , -1);
  }
}

void loop() {
  if (Kniwwelino.BUTTONAdown()) {
    // Show a smiley on the matrix
    // 01010
    // 00000
    // 10001
    // 01110
    // 00000
    Kniwwelino.MQTTpublish("MATRIX/ICON", String("B0101000000100010111000000"));

  }
  if (Kniwwelino.BUTTONBdown()) {
    // Show a heart on the matrix
    // 01010
    // 10101
    // 10001
    // 01010
    // 00100
    Kniwwelino.MQTTpublish("MATRIX/ICON", String("B0101010101100010101000100"));
  }

  if (last + wait < millis()) {
    int choose = random(1,TEXTCOUNT);
    Kniwwelino.MATRIXwrite(texts[choose]);
    Serial.println("\nText: " + texts[choose] + " " + choose);
    last = millis();
    wait = random(30000, 50000);
  }

  brightness = brightness + step;

  if (brightness > MAXBRIGHTNESS) {
    brightness = MAXBRIGHTNESS;
    step = - step;
  } else if (brightness < MINBRIGHTNESS) {
    brightness = MINBRIGHTNESS;
    step = - step;
  }

  //Serial.print(brightness); Serial.print(" ");
  Kniwwelino.RGBsetBrightness(brightness);

  Kniwwelino.sleep(200);
  Kniwwelino.loop(); // do background stuff...
}

static void messageReceived(String &topic, String &payload) {
  if (topic=="RGB/COLOR") {
    Kniwwelino.sleep(random(0,3000));
    if (payload == "random") {
      Serial.println("Received Color: random");
      Kniwwelino.RGBsetColor(color_wheel(random(256)));
    } else {
      Serial.println("Received Color: "+payload);
      Kniwwelino.RGBsetColor(payload);
    }

  }
}


/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t color_wheel(uint8_t pos) {
  pos = 255 - pos;
  if(pos < 85) {
    return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
  } else {
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
  }
}
