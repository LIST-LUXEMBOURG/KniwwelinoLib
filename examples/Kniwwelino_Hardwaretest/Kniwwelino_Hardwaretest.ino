/***************************************************

  Kniwwelino Library by LIST.lu 2017

  Author Johannes Hermen

  Example sketch to basic test the hardware.
  Wifi is disabled in this Sketch.

****************************************************/

#include <Kniwwelino.h>

int pos = 0;
boolean icon = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n-----------------------------------------------");
  Serial.println("Kniwwelino Hardware Test Sketch");
  Serial.println("-----------------------------------------------");

  Kniwwelino.begin( false, false); // Wifi=off, Fastboot=off

  pinMode(D0, INPUT);
  pinMode(D5, INPUT);
  pinMode(D6, INPUT);
  pinMode(D7, INPUT);
  
  // all pixels on
  Kniwwelino.MATRIXclear();
  Kniwwelino.fillRect(0, 0, 5, 5, 1);

  Kniwwelino.RGBsetColor(255, 0, 0);
  delay(300);
  Kniwwelino.RGBsetColor(0, 255, 0);
  delay(300);
  Kniwwelino.RGBsetColor(0, 0, 255);
  delay(300);
  Kniwwelino.RGBclear();
  delay(200);

  Kniwwelino.MATRIXwrite("Kniwwelino");
}

void loop() {  
    Serial.print(" BT_A:");Serial.print(Kniwwelino.BUTTONAdown());Serial.print("  CL: ");Serial.print(Kniwwelino.BUTTONAclicked());
    Serial.print(" BT_B:");Serial.print(Kniwwelino.BUTTONBdown());Serial.print("  CL: ");Serial.print(Kniwwelino.BUTTONBclicked());
    Serial.print("  AB CL: ");Serial.print(Kniwwelino.BUTTONABclicked());
    Serial.print("\tD0: ");Serial.print(digitalRead(D0));
    Serial.print("\tD5: ");Serial.print(digitalRead(D5));
    Serial.print("\tD6: ");Serial.print(digitalRead(D6));
    Serial.print("\tD7: ");Serial.print(digitalRead(D7));
    Serial.print("\tAD: ");Serial.print(analogRead(A0));
    Serial.println();


    if (Kniwwelino.BUTTONAdown() && Kniwwelino.BUTTONBdown()) {
      Kniwwelino.MATRIXdrawIcon(ICON_ARROW_DOWN);
      icon = true;
    } else if (Kniwwelino.BUTTONAdown()) {
      Kniwwelino.MATRIXdrawIcon(ICON_ARROW_LEFT);
      icon = true;
    } else if (Kniwwelino.BUTTONBdown()) {
      Kniwwelino.MATRIXdrawIcon(ICON_ARROW_RIGHT);
      icon = true;
    } else if (icon == true) {
      Kniwwelino.MATRIXwrite("Kniwwelino");
      icon = false;
    }



    if (pos % 3 ==0) {
      Kniwwelino.RGBsetColor(100, 0, 0);
    } else if (pos % 2 ==0) {
      Kniwwelino.RGBsetColor(0, 100, 0);
    } else  {
      Kniwwelino.RGBsetColor(0, 0, 100);
    }  
    
    pos++;
    if (pos > 3) pos = 0;
    
    delay(150);

  Kniwwelino.loop();
}

