# Deprecated  #

This repository is deprecated.
The official repository is : https://github.com/kniwwelino/KniwwelinoLib

# KniwwelinoLib  #

Copyright&copy; 2018 [Luxembourg Institute of Science and Technology](http://www.list.lu).
This program is free software: you can redistribute it and/or modify
it under the terms of the Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License.

The [Kniwwelino&reg;](http://kniwwelino.lu/en/) hardware is the first micro-controller development platform entirely designed in Luxembourg for primary school children.

The name Kniwwelino&reg; is a composition of the luxembourgish word "kniwweln", that means to craft something, "ino" should show the deep affinity to the Arduino ecosystem and finally "Lino" as a name has a relation to lion, which is the heraldic animal of the Grand Duchy of Luxembourg.

![Kniwwelino&trade;](http://www.kniwwelino.lu/fileadmin/_processed_/csm_components_4654b3da98.jpg)

The Kniwwelinoo&reg; hardware consists of a 5x5 LEDs matrix, a RGB LED and two push buttons. There are additional ports that can be used to extend the board by additional sensors and other peripherals. The underlaying micro-controller platform is also embedding a Wi-Fi stack. That enables the Kniwwelinoo&reg; to connect itself to other Kniwwelinoso&reg; over the Internet. By implementing standard IoT message protocols, like MQTT it could be easily integrated in existing IoT installations. The small size of the printed circuit board makes it possible to embed the Kniwwelino&reg; in nearly every crafted enclosure or object to not limit the development of the child's creativity.

For using the KniwwelinoLib you need to install additional Libraries and the esp8266 core 2.4.0.

Select the Wemos D1 mini with the Flash Size set to "4M (3M SPIFFS)" (online important if you like to keep your Kniwwelino compatible to the online programming on https://code.kniwwelino.lu)

Please add the additional Boards Manager URL to the Arduino Preferences :
  * http://arduino.esp8266.com/stable/package_esp8266com_index.json

The following libraries are required to use the Kniwwelino Arduino Library:
  * Adafruit GFX Library v1.2.2 https://github.com/adafruit/Adafruit-GFX-Library
  * Adafruit NeoPixel v1.1.6 https://github.com/adafruit/Adafruit_NeoPixel
  * ArduinoJson v5.11.1 https://github.com/bblanchon/ArduinoJson
  * MQTT v2.3.1 https://github.com/256dpi/arduino-mqtt
  * WiFiManager v0.12 https://github.com/tzapu/WiFiManager.git
  * Time 1.5 https://github.com/PaulStoffregen/Time
  * TimeAlarms 1.5  https://github.com/PaulStoffregen/TimeAlarms
  * Timezone 1.1.1 https://github.com/JChristensen/Timezone
