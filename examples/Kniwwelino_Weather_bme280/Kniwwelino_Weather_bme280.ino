/***************************************************

  KniwwelinoWeather

  Copyright (C) 2017 Luxembourg Institute of Science and Technology.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License.

  Example sketch for using an Bosch BME280 Sensor connected via I2C.

****************************************************/

#include <Kniwwelino.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; // I2C

void setup() {
  Kniwwelino.begin(rue, true); // Wifi=true, Fastboot=true
  Kniwwelino.MQTTsetGroup("KniwwelinoDemo");

  Serial.println(F("KniwwelinoWeather"));

    bool status;

    status = bme.begin(0x76);
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }
    delay(100); // let sensor boot up

    Kniwwelino.MATRIXwriteOnce("TEMP " + String(bme.readTemperature()) + "C");
    Kniwwelino.MQTTpublish(Kniwwelino.getName()+"/temp", String(bme.readTemperature()));
}

void loop() {

    uint8_t col = map(bme.readTemperature(),20, 30, 100, 255);
    //Serial.println(col);
    Kniwwelino.RGBsetColor(color_wheel(col));


    if (Kniwwelino.MATRIXtextDone()) {
          Kniwwelino.MATRIXwriteOnce(String(bme.readTemperature()) + "° C");
//          Kniwwelino.MQTTpublish(Kniwwelino.getName()+"/temp", String(bme.readTemperature()));
//    Kniwwelino.MATRIXwriteOnce("HUMID " + String(bme.readHumidity()) + "%");
    Kniwwelino.MQTTpublish(Kniwwelino.getName()+"/humid", String(bme.readHumidity()));
   }

    Kniwwelino.MQTTpublish(Kniwwelino.getName()+"/temp", String(bme.readTemperature()));

    Kniwwelino.sleep(1000);
    Kniwwelino.loop();
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
