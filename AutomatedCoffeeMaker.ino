#include "SPI.h"
#include "WiFiS3.h"
#include "RTC.h"
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include "NTPClient.h"
#include "DHT.h"
#include "EEPROM.h"
#include "secret.h"

#define relayPin 7
#define buttonPin 2
#define DHTPin 8

//STATE MACHINE
enum State {
  START,
  IDLE,
  BREWING,
  COOLING
};
State currentState = START;
unsigned long stateStartTime = 0;
//relaystate
int relayState = 1;
bool refreshPage = false;

//Router
const char ssid[] = WIFI_SSID;
const char pass[] = WIFI_PASS;
int wifiStatus = WL_IDLE_STATUS;
WiFiServer server(80);
bool clientReady = false;

//TIMER
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
bool timerStarted = false;
bool timerCanceled = false;
unsigned long duration = 10000;

//DAILY TIMER
bool dailyTimerStarted = false;
bool dailyTimerCanceled = false;
int dailyHours = 0;
int dailyMinutes = 0;
String dailyTimerOut = String(dailyHours) + ":" + String(dailyMinutes);

//Tempsensor
DHT dht(DHTPin, DHT11);
unsigned long tempUpdate = 1000;
float cTemp = 0;

//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

//RTC GetTime
WiFiUDP Udp;
NTPClient timeClient(Udp,"pool.ntp.org");
unsigned long timeUpdate = 0;

//statistics
int statWeek, statMonth, statYear = 0;
int cWeek, cMonth, cYear;
int cleaning=0;

char webCSS[] = R"=====(
  <head>
  <style>
  body {
    font-family: Eina01,sans-serif;
    font-size: 18px;
  }
  div {
    paddig: 20px 20px;
    margin: 20px;
  }
  input[type=submit] {
    appearance: none;
    background-color: #6B6B6B;
    border: 1px solid rgba(27, 31, 35, 0.15);
    border-radius: 6px;
    box-shadow: rgba(27, 31, 35, 0.04) 0 1px 0, rgba(255, 255, 255, 0.25) 0 1px 0 inset;
    box-sizing: border-box;
    color: #24292E;
    cursor: pointer;
    display: inline-block;
    font-family: -apple-system, system-ui, "Segoe UI", Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji";
    font-size: 14px;
    font-weight: 500;
    line-height: 20px;
    list-style: none;
    padding: 6px 16px;
    position: relative;
    transition: background-color 0.2s cubic-bezier(0.3, 0, 0.5, 1);
    user-select: none;
    -webkit-user-select: none;
    touch-action: manipulation;
    vertical-align: middle;
    white-space: nowrap;
    word-wrap: break-word;
  }
  .button-78 {
    align-items: center;
    appearance: none;
    background-clip: padding-box;
    background-color: initial;
    background-image: none;
    border-style: none;
    box-sizing: border-box;
    color: #fff;
    cursor: pointer;
    display: inline-block;
    flex-direction: row;
    flex-shrink: 0;
    font-family: Eina01,sans-serif;
    font-size: 16px;
    font-weight: 800;
    justify-content: center;
    line-height: 24px;
    margin: 0;
    min-height: 64px;
    outline: none;
    overflow: visible;
    padding: 19px 26px;
    pointer-events: auto;
    position: relative;
    text-align: center;
    text-decoration: none;
    text-transform: none;
    user-select: none;
    -webkit-user-select: none;
    touch-action: manipulation;
    vertical-align: middle;
    width: auto;
    word-break: keep-all;
    z-index: 0;
  }

  @media (min-width: 768px) {
    .button-78 {
      padding: 19px 32px;
    }
  }

  .button-78:before,
  .button-78:after {
    border-radius: 80px;
  }

  .button-78:before {
    background-image: linear-gradient(92.83deg, #ff7426 0, #f93a13 100%);
    content: "";
    display: block;
    height: 100%;
    left: 0;
    overflow: hidden;
    position: absolute;
    top: 0;
    width: 100%;
    z-index: -2;
  }

  .button-78:after {
    background-color: initial;
    background-image: linear-gradient(#541a0f 0, #0c0d0d 100%);
    bottom: 4px;
    content: "";
    display: block;
    left: 4px;
    overflow: hidden;
    position: absolute;
    right: 4px;
    top: 4px;
    transition: all 100ms ease-out;
    z-index: -1;
  }

  .button-78:hover:not(:disabled):before {
    background: linear-gradient(92.83deg, rgb(255, 116, 38) 0%, rgb(249, 58, 19) 100%);
  }

  .button-78:hover:not(:disabled):after {
    bottom: 0;
    left: 0;
    right: 0;
    top: 0;
    transition-timing-function: ease-in;
    opacity: 0;
  }

  .button-78:active:not(:disabled) {
    color: #ccc;
  }

  .button-78:active:not(:disabled):before {
    background-image: linear-gradient(0deg, rgba(0, 0, 0, .2), rgba(0, 0, 0, .2)), linear-gradient(92.83deg, #ff7426 0, #f93a13 100%);
  }

  .button-78:active:not(:disabled):after {
    background-image: linear-gradient(#541a0f 0, #0c0d0d 100%);
    bottom: 4px;
    left: 4px;
    right: 4px;
    top: 4px;
  }

  .button-78:disabled {
    cursor: default;
    opacity: .24;
  }
  .float-container {
    border: 3px solid #fff;
    padding: 20px;
  }

  .float-child {
      width: 50%;
      float: left;
      padding: 20px;
      border: 2px solid red;
  }  
  </style>
  </head>
)=====";

//AJAXscript
char ajaxButton[] = R"=====(
  <br>
  <div id="relayStatus"></div>
  <p id="temperature"></p>
  <button class="button-78" id="toggleButton" onclick="sendToggleRequest()">Toggle Relay</button>
  <br>
  <script>
  //Update
    function updateStatus() {
      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        if (this.status === 200) {
          var response = this.responseText;
          var relayStatusDiv = document.getElementById('relayStatus');
          if(response.startsWith('1')){
            relayStatusDiv.innerHTML =  "Kávéfőző készen áll.";
            document.getElementById('temperature').style.display = 'none';
          } else if (response.startsWith('2')){
            relayStatusDiv.innerHTML = "Főzés megkezdődött";
            document.getElementById('temperature').style.display = '';
          } else if (response.startsWith('3')){
            relayStatusDiv.innerHTML = "Kávé elkészült. Kihülés...";
          } else {
              relayStatusDiv.innerHTML = 'Connecting...';
          }

          var toggleButton = document.getElementById('toggleButton');
          if (response.startsWith('2')) {
            toggleButton.innerHTML = 'Kikapcsolás';
          } else {
            toggleButton.innerHTML = 'Bekapcsolás';
          }
        }
      };
      xhr.open('GET', '/ajax/status', true);
      xhr.send();
    }
    //Gomb
    function sendToggleRequest() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          updateStatus();
        }
      };
      xhr.open('GET', '/ajax/toggle', true);
      xhr.send();
    }
    //Temp
    function getTemp() {
      var xhr = new XMLHttpRequest();
      xhr.onload = function () {
        if (xhr.status === 200) {
          var response = this.responseText;
          document.getElementById('temperature').textContent = 'Temperature: ' + response + ' °C';
        }
      };
      xhr.open('GET', '/ajax/temp', true);
      xhr.send();
    }
    // Initial status update
    updateStatus();
    setInterval(updateStatus, 5000);
    setInterval(getTemp, 2000);
  </script>
  <br>
)=====";

String httpreq = "";


void setup() {
  Serial.begin(9600);
  //Relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState);  // Ensure the relay is initially off
  pinMode(buttonPin, INPUT);
  //LCD
  lcd.init();
  lcd.backlight();
  // Start the server:
  connectToWiFi();
  Serial.println("Connected to WiFi");
  server.begin();
  Serial.println("Server started");
  // Print the IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  //RTC setup
  RTC.begin();
  Serial.println("\nStarting connection to server...");
  timeClient.begin();
  timeClient.update();

  auto timeZoneOffsetHours = 1;
  auto unixTime = timeClient.getEpochTime() + (timeZoneOffsetHours * 3600);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);
  RTCTime currentTime;
  RTC.getTime(currentTime);
  Serial.println("The RTC was just set to: " + String(currentTime));
  //Set Stats
  cMonth = Month2int(currentTime.getMonth());
  cWeek = currentTime.getDayOfMonth();
  cYear = currentTime.getYear();
  readData();
}

void loop() {

  WEBserver();

  switch (currentState) {
    case START:  //------------------------------------------------------------------------
      // Transition to the IDLE state once the client is connected
      if (clientReady || digitalRead(buttonPin) == HIGH) {
        delay(250);
        currentState = IDLE;
        stateStartTime = millis();
        LCDprint("Waiting to Start", "STATE IDLE");
      }
      break;

    case IDLE:  //------------------------------------------------------------------------
      if (digitalRead(buttonPin) == HIGH) {
        turnON();
        delay(250);
      }
      if (millis() - timeUpdate >= 10000) {
        RTCTime ctime;
        RTC.getTime(ctime);
        String t = String(ctime.getHour()) + ":" + String(ctime.getMinutes()) + " ";
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(t);
        timeUpdate = millis();
      }
      // Transition to the BREWING state on button press or when the client action is received
      if (timerStarted && elapsedTime % 1000 == 0) {
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print("Timer Ticking!");
      }
      if (relayState == 0) {
        currentState = BREWING;
        stateStartTime = millis();
        dht.begin();
        LCDprint("Brewing Coffee", "STATE BREWING");
      }
      connectToWiFi();
      break;

    case BREWING:  //------------------------------------------------------------------------
      if (digitalRead(buttonPin) == HIGH) {
        turnOFF();
        delay(200);
      }
      if (millis() - stateStartTime >= 5 * 60000) {
        turnOFF();
        currentState = COOLING;
        stateStartTime = millis();
        updateAndWriteData();
        LCDprint("Coffee Ready", "STATE COOLING");
        cleaning++;
      } else if (relayState == 1) {
        currentState = IDLE;
        stateStartTime = millis();
        LCDprint("Waiting to Start", "IDLE");
      }
      DHTRead();
      break;

    case COOLING:  //------------------------------------------------------------------------
      DHTRead();
      if (millis() - stateStartTime >= 15 * 60000 || cTemp <= 45) {
        currentState = IDLE;
        stateStartTime = millis();
        LCDprint("Waiting to Start", "IDLE");
      } else if (digitalRead(buttonPin) == HIGH) {
        delay(200);
        currentState = IDLE;
        stateStartTime = millis();
        LCDprint("Waiting to Start", "IDLE");
      }
      break;

    default:
      break;
  }
  if(cleaning >= 45){
    while(digitalRead(buttonPin) == LOW){
      LCDprint("Time to clean","the machine!");
      delay(1000);
    }
  }

  // Control the relay based on the timer and cancellation --------------------------------
  if (timerStarted && !timerCanceled) {
    elapsedTime = millis() - startTime;
    if (elapsedTime >= duration) {
      // Timer has expired, turn the relay on
      turnON();
      Serial.println("Timer finished");
      elapsedTime = 0;
      timerStarted = false;
    }

    if ((elapsedTime % 100) == 0) {
      Serial.println(elapsedTime);
    }
  } else if (timerCanceled){
    elapsedTime = 0;
    timerStarted = false;
    timerCanceled = false;
  }
  // Control the relay based on the daily timer and cancellation ---------------------------
  if (dailyTimerStarted && !dailyTimerCanceled) {
    RTCTime ctime;
    RTC.getTime(ctime);
    if (ctime.getHour() == dailyHours && ctime.getMinutes() == dailyMinutes && ctime.getSeconds() == 0 && currentState != 4) {
      // Execute the action for the daily timer
      turnON();
      Serial.println("Daily Timer finished");
    }
  }

}
//END LOOP-------------------------------------------------------------------------------
void LCDprint(String a, String b) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(a);
  lcd.setCursor(0, 1);
  lcd.print(b);
}
void DHTRead() {
  if (millis() - tempUpdate >= 1000) {
    cTemp = dht.readTemperature() * 2;
    tempUpdate = millis();
    //if(cTemp >= 36){
    String tempOut = "Temp: " + String(cTemp) + "C";
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(tempOut);

    //}
    Serial.print(cTemp);
  }
}

void connectToWiFi() {
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    wifiStatus = WiFi.begin(ssid, pass);
    delay(5000);
  }
}

String timeDisplay(unsigned long ms) {
  int seconds = (ms / 1000) % 60;
  int minutes = ((ms / (1000 * 60)) % 60);
  int hours = ((ms / (1000 * 60 * 60)) % 24);
  String ido = "H: " + String(hours) + " M: " + String(minutes) + " S: " + String(seconds);
  Serial.println(ido);
  return (ido);
}

void turnON() {
  relayState = 0;
  digitalWrite(relayPin, relayState);
}
void turnOFF() {
  relayState = 1;
  digitalWrite(relayPin, relayState);
}
void AJAXbutton(WiFiClient client) {

  if (httpreq.indexOf("/status") > -1) {
    client.println(currentState);
  } else if (httpreq.indexOf("/toggle") > -1) {
    if (currentState == 4) {
      currentState = IDLE;
      LCDprint("Waiting to Start", "IDLE");
    } else if (relayState == 1) {
      turnON();
    } else if (relayState == 0) {
      turnOFF();
    }
  } else if(httpreq.indexOf("/temp")>-1){
    client.println(String(cTemp));
  }
}
void WEBserver() {
  WiFiClient client = server.available();  // Listen for incoming clients

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";      
    while (client.connected()) {  
      if (client.available()) {   
        clientReady = true;
        char c = client.read();  
        httpreq += c;
        if (c == '\n') {  
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=utf-8");
            client.println();

            if (httpreq.indexOf("/ajax") > -1) {
              AJAXbutton(client);
            } else {
              // Display the HTML web page ---------------------------------------------------------------------------------------
              client.println("<!DOCTYPE html><html lang=\"hu\">");
              client.println(webCSS);
              client.println("<body>");
              client.println("<h1>Kavéfőző vezérlés</h1>");
              client.println(ajaxButton);
              //Ki-be
              client.println("<div class=\"float-container\"><div class=\"float-child\">");
              if (!timerStarted) {
                client.println("<form action=\"/ontimer\"><input type=\"submit\" value=\"START TIMER\"></form>");
              } else {
                client.println("<h3>Időzítő bekapcsolva!</h3>");
                client.println("<form action=\"/offtimmer\"><input type=\"submit\" value=\"CANCEL TIMER\"></form>");
              }
              //display time
              client.println("Jelenlegi idozites:");
              client.println(timeDisplay(duration));
              //add time
              client.println("<br><div><form action=\"/submit\" method=\"get\">");
              client.println("<label for=\"hours\">Hours:</label>");
              client.println("<input type=\"number\" id=\"hours\" name=\"hours\" min=\"0\"><br><br>");
              client.println("<label for=\"minutes\">Minutes:</label>");
              client.println("<input type=\"number\" id=\"minutes\" name=\"minutes\" min=\"0\" max=\"59\"><br><br>");
              client.println("<label for=\"seconds\">Seconds:</label>");
              client.println("<input type=\"number\" id=\"seconds\" name=\"seconds\" min=\"0\" max=\"59\"><br><br>");
              client.println("<input type=\"submit\" value=\"Set Timer\">");
              client.println("</form></div></div>");
              client.println("<br><br>");
              client.println("<div class=\"float-child\">");
              //alarm clock 
              client.print("Jelenlegi Napi időzítés: ");
              client.println(dailyTimerOut);
              client.println("<br><div><form action=\"/setdaily\" method=\"get\">");
              client.println("<label for=\"hours\">Hours:</label>");
              client.println("<input type=\"number\" id=\"hours\" name=\"hours\" min=\"0\" max=\"23\" value=\"00\"><br><br>");
              client.println("<label for=\"minutes\">Minutes:</label>");
              client.println("<input type=\"number\" id=\"minutes\" name=\"minutes\" min=\"0\" max=\"59\" value=\"00\"><br><br>");
              client.println("<input type=\"submit\" value=\"Set Daily Timer\">");
              if (dailyTimerStarted) {
                client.println("</form><br><br>");
                client.println("<form action=\"/offdaily\" method=\"get\">");
                client.println("<input type=\"submit\" value=\"Cancel Daily Timer\">");
                client.println("</form></div>");
              }
              client.println("</div></div><div>Kávéfogyasztási statisztika:<br>");
              String s = "Heti:" + String(statWeek) + " <br>Havi:" + String(statMonth) + " <br>Éves:" + String(statYear);
              client.println(s);
              client.println("</div></body></html>");
              client.println("<tail>Boros Tamás 2023</tail>");
            }

            // End of the HTML webpage
            Serial.print(httpreq);
            httpreq = "";
            break;
          } else {  
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;      
        }
        // request -------------------------------------------------------------------------------------
        // Check for the ON and OFF requests
        if (currentLine.endsWith("GET /ontimer")) {
          // Check if the timer is not already started
          if (!timerStarted) {
            startTime = millis();
            timerStarted = true;
            timerCanceled = false;
            Serial.println("Timer started");
          }
        }
        if (currentLine.endsWith("GET /offtimer")) {
          // Cancel the timer
          timerCanceled = true;
          timerStarted = false;
          elapsedTime = 0;
          turnOFF();
          Serial.println("Timer canceled");
        }
        if (currentLine.endsWith("GET /offdaily")) {
          // Cancel the timer 2
          dailyTimerCanceled = true;
          dailyTimerStarted = false;
          dailyHours = 0;
          dailyMinutes = 0;
          Serial.println("Daily Timer canceled");
          dailyTimerOut = String(dailyHours) + ":" + String(dailyMinutes);
        }


        //Timer handling -----------------------------------------------------------
        if (currentLine.startsWith("GET /submit")) {
          // Extract the timer duration value from the client request
          int hoursIndex = currentLine.indexOf("hours=");
          int minutesIndex = currentLine.indexOf("minutes=");
          int secondsIndex = currentLine.indexOf("seconds=");
          if (hoursIndex != -1 && minutesIndex != -1 && secondsIndex != -1) {
            String hoursString = currentLine.substring(hoursIndex + 6);
            int hoursEndIndex = hoursString.indexOf("&");
            hoursString = hoursString.substring(0, hoursEndIndex);

            String minutesString = currentLine.substring(minutesIndex + 8);
            int minutesEndIndex = minutesString.indexOf("&");
            minutesString = minutesString.substring(0, minutesEndIndex);

            String secondsString = currentLine.substring(secondsIndex + 8);
            int secondsEndIndex = secondsString.indexOf(" ");
            secondsString = secondsString.substring(0, secondsEndIndex);

            int hours = hoursString.toInt();
            int minutes = minutesString.toInt();
            int seconds = secondsString.toInt();

            duration = (hours * 3600 + minutes * 60 + seconds) * 1000;
            Serial.print("New Timer Duration: ");
            Serial.println(duration);
          }
        }


        //DAILY TIMER ------------------------------------------------------------------
        if (currentLine.startsWith("GET /setdaily")) {
          // Extract the time value for the daily timer from the client request
          int hoursIndex = currentLine.indexOf("hours=");
          int minutesIndex = currentLine.indexOf("minutes=");
          if (hoursIndex != -1 && minutesIndex != -1) {
            String hoursString = currentLine.substring(hoursIndex + 6);
            int separatorIndex = hoursString.indexOf("&");
            if (separatorIndex != -1) {
              hoursString = hoursString.substring(0, separatorIndex);
            }

            String minutesString = currentLine.substring(minutesIndex + 8);
            separatorIndex = minutesString.indexOf(" ");
            if (separatorIndex != -1) {
              minutesString = minutesString.substring(0, separatorIndex);
            }
            dailyHours = hoursString.toInt();
            dailyMinutes = minutesString.toInt();
            dailyTimerStarted = true;
            dailyTimerCanceled = false;
            dailyTimerOut = String(dailyHours) + ":" + String(dailyMinutes);
          }
        }
      }
    }
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void updateAndWriteData() {
  RTCTime ctime;
  RTC.getTime(ctime);

  if (ctime.getDayOfMonth() / 7 + 1 == cWeek / 7 + 1) {
    statWeek++;
  } else {
    cWeek = ctime.getDayOfMonth();
    statWeek = 0;
  }
  if (Month2int(ctime.getMonth()) == cMonth) {
    statMonth++;
  } else {
    cYear = Month2int(ctime.getMonth());
    statMonth = 0;
  }
  if (ctime.getYear() == cYear) {
    statYear++;
  } else {
    cYear = ctime.getYear();
    statYear = 0;
  }
  EEPROM.write(0, statWeek);
  EEPROM.write(sizeof(int), statMonth);
  EEPROM.write(2 * sizeof(int), statYear);
}
void readData() {
  statWeek = EEPROM.read(0);
  statMonth = EEPROM.read(sizeof(int));
  statYear = EEPROM.read(2 * sizeof(int));
  String s = "heti:" + String(statWeek) + " havi:" + String(statMonth) + " éves:" + String(statYear);
  Serial.println("Jelenlegi statisztika: ");
  Serial.println(s);
}
