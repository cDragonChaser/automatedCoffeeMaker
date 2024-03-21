Introduction
-
The goal of project was to automate and add new functions to my old Szarvasi lever coffee maker. To achieve this I used an Arduino UNO R4 WiFi microcontroller, which had a built-in ESP32-S3 modul. 
The added features: 
 - Automated operation 
 - Remote access 
 - 2 separate timers, one working as a
   stopwatch timer and one working with a date
 - Temperature display    
 - Statistical data storage

The ESP modul was really handy as the coffee maker had to be accessed remotely from a browser. The webpage hosted by the controller is also the only way to configure the added timers. Most of the UI on the page is dynamic, as I wanted it to be in sync with the physical state.
Naturally, the Arduino turns the coffee maker on and off through a relay. Everything else is handled by the other peripherals or the software. I used a simple state machine to optimize and separate the functions within the software.

Note: I used a DHT11 temp sensor. I know it's a terrible choice but I was working with what I had. A good replacement would be a infrared temp sensor.
