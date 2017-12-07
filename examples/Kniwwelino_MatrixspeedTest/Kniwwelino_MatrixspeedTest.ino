/***************************************************

  KniwwelinoMatrixSpeed

  Copyright (C) 2017 Luxembourg Institute of Science and Technology.
  This program is free software: you can redistribute it and/or modify
  it under the terms of the Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License.

  Example sketch to test the text scroll speed on the matrix.

****************************************************/


#include <Kniwwelino.h>

int speed = 5;

void setup() {
  //Initialize the Kniwwelino Board
  Kniwwelino.begin(true, true); // Wifi=true, Fastboot=true

  Kniwwelino.MATRIXsetScrollSpeed(speed);
  Kniwwelino.MATRIXwriteAndWait(String(speed));
}

void loop() {

  if (Kniwwelino.BUTTONAclicked()) {
    speed--;
    Serial.println("A");
    Kniwwelino.MATRIXwriteAndWait(String(speed));
  }
  
  if (Kniwwelino.BUTTONBclicked()) {
    speed++;
    Serial.println("B");
    Kniwwelino.MATRIXwriteAndWait(String(speed));
  }
  
  speed = constrain(speed, 1, 10);
  Kniwwelino.MATRIXsetScrollSpeed(speed);
  Kniwwelino.MATRIXwrite("Kniwwelino");

  Kniwwelino.loop();
}
