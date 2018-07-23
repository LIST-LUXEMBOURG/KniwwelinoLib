
/***************************************************

  Kniwwelino Library by LIST.lu 2017

  Author Johannes Hermen

  Example sketch format the onboard filesystem in case of 
  Problems board resets after boot etc...

  WIFI Config will be LOST aftzer using this sketch!

****************************************************/
#include <Kniwwelino.h>

void setup() {
  //Initialize the Kniwwelino Board
  Kniwwelino.begin(false, true, false); // Wifi=false, Fastboot=true, MQTT Logging=false

  Serial.println("formating filesystem");
  Kniwwelino.RGBsetColorEffect(String("FF6600"), RGB_BLINK, -1);
  Kniwwelino.MATRIXwrite(String("Formating FS"));
  if (SPIFFS.format()) {
      Serial.println("filesystem format successful");
      Kniwwelino.RGBsetColorEffect(String("00FF00"), RGB_ON, -1);
      Kniwwelino.MATRIXdrawIcon(ICON_SMILE);
  } else {
    Serial.println("filesystem format successful");
    Kniwwelino.RGBsetColorEffect(String("FF0000"), RGB_ON, -1);
    Kniwwelino.MATRIXdrawIcon(ICON_SAD);
  }

}

void loop() {

  Kniwwelino.loop(); // do background stuff...
}


