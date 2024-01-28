/*
NOTES

used ESP32 WROOM 30-pin

DISPLAY
#include "SSD1306Wire.h" //library can be found in library loader: ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays
                         //ESP32 wire connections: https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/
                         //works eg with example SSD1306ScrollVerticalDemo
SSD1306Wire display(0x3c, SDA, SCL); //default address for SSD1306 display and default ESP32 pins
SDA     -> D21
SCL/SCK -> D22
VCC     -> 3.3V ESP32 output pin
GND     -> GND

ASYNCHRONOUS WEB SERVER (AsyncWebServer)
- implemented as asynchronous web server -> the page has not to be reloaded as it is performed by using a submit() command.
  Instead only a little piece of information is send to the server by using JavaScript.
- testing a 'normal / standard' server and using submit() did not work on smartphone browsers. Because it was not possible
  to set the slider to the last input value. Instead the default 'middle' position was always set by the browser when the page was reloaded.
  By using browsers on a PC, it was possible to set the correct / last values by using a piece of JavaScript at the very end of the HTML code
  (eg: <script>setValues('160', '40');</script>).
  But for smartphone browsers this JS code was not executed at all when loading the page.
libraries:
AsyncTCP
ESPAsyncWebServer

SPEED AND DIRECTION VALUES
- speed and direction ranges from 0 to 200; center value 'stop' or 'straight' respectively
  mapping of those values to the PWM values 0 to maxSpeed: int pwmValue = map(value,0,200,0,maxSpeed);  
- spinner function: car rotates around its own axis - left or right

BROWSER
- on Android smartphone, Google Chrome did not handle the CSS for the vertical slider correctly
 -> Firefox and Opera worked as expected 
- on PC, all browsers tested worked correctly

INTERNAL BLUE LED
- high if connected to WiFi
- blink if not connected
*/

#include <WiFi.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Wire.h>  
#include "SSD1306Wire.h" 

SSD1306Wire display(0x3c, SDA, SCL); 
 
const char* ssid     = "esp32";
const char* password = "cbsHeidelberg!";
 
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Auto Fernsteuerung</title>
  
    <style>
      body {
        font-size: 250%;
      }
      input[id='range2'] {
        width:80%;
      }
      input[id='range1'] {
        width:80%;
      }
      .button {
      font-size: 100%;
      }  
    </style>

    <script>
      function setValue(rangeObject){
        if(rangeObject.id == "range2"){
          const rangeValue2Int = parseInt(rangeObject.value);
          if(rangeValue2Int == 100){
            rangeValue2.innerText = "stop";
          }else if(rangeValue2Int > 100){
            rangeValue2.innerText = "vorwärts "+(rangeValue2Int-100)+" %";
          }else if(rangeValue2Int < 100){
            rangeValue2.innerText = "rückwärts "+(100-rangeValue2Int)+" %";
          }
          sendData('speedValue', rangeObject.value);
        }else{
          const rangeValue2Int = parseInt(rangeObject.value);
          if(rangeValue2Int == 100){
            rangeValue1.innerText = "geradeaus";
          }else if(rangeValue2Int > 100){
            rangeValue1.innerText = "rechts "+(rangeValue2Int-100)+" %";
          }else if(rangeValue2Int < 100){
            rangeValue1.innerText = "links "+(100-rangeValue2Int)+" %";
          }
          sendData('directionValue', rangeObject.value);
        }        
      };

      function setValues(valueRange1, valueRange2){
        const range2 = document.querySelector("#range2");
        range2.value = valueRange2;
        setValue(range2);
        const range1 = document.querySelector("#range1");
        range1.value = valueRange1;
        setValue(range1);
      }

      function setSpinner(directions){
        setValues('100', '100')
        if(directions == true){
          sendData('spinnerLeft', 'true');
        }else{
          sendData('spinnerRight', 'false');
        }
      }

      //there is no special reason for using JSON, could be also text/plain or text/html; the response is not evaluated anyway
      function sendData(dataId, dataValue){
        fetch('/setData?' + dataId + '=' + dataValue)
        .then(
              response => response.json()
              ).then(jsonResponse => {
        });
      }           

    </script>
  </head>

  <body>

<datalist id="markers1">
  <option value="0"></option>
  <option value="20"></option>
  <option value="40"></option>
  <option value="60"></option>
  <option value="80"></option>
  <option value="100"></option>
  <option value="120"></option>
  <option value="140"></option>
  <option value="160"></option>
  <option value="180"></option>
  <option value="200"></option>
</datalist>

<center>
    <h2>Auto Fernsteuerung</h2>

     <label for="rangeLabel2">vorwärts / rückwärts</label><br/>
     <input id="range2" name="range2" type="range" min="0" max="200" orient="horizontal" value="100" step="20" list="markers1" oninput="setValue(this);">
     <br>
     <div id="rangeValue2">stop</div>
     <div id="dataValues">dataValues</div>
     <br><br>

     <label for="rangeLabel1">links / rechts</label><br/>
     <input id="range1" name="range1" type="range" min="0" max="200" orient="horizontal" value="100" step="20" list="markers1" oninput="setValue(this);">
     <br>
     <div id="rangeValue1">geradeaus</div>
     <br/><br/>
     <input type="button" id="stopButton" name="stopButton" value="stop" style="background-color:blue; border-color:blue; color:white" 
            class="button" onclick="setValues('100', '100')"/>
     <br/><br/><br/>
     <input type="button" name="spinnerLeft" value="links kreiseln" style="background-color:blue; border-color:blue; color:white" 
            class="button" onclick="setSpinner(true);"/>
     <input type="button" name="spinnerRight" value="rechts kreiseln" style="background-color:blue; border-color:blue; color:white" 
            class="button" onclick="setSpinner(false);"/>
     <br><br>
  
    </center>
  </body>
</html>
)rawliteral";


String ipAdress = "none";
int speedValue = 100;     // = off
int directionValue = 100; // = middle / straight
int maxSpeed = 180;
int maxCurveSpeed = 54;
int spinnerSpeed = maxSpeed * 0.8; // car rotates around its axis - left or right
int minValuePWM = 100; //at a PWM of 20% or 40% of 255, the car does not start to move (just some motor sound) => correction as minimal value
int correctionLeftMotorPWM = 14; //the rigth motor is somewhat faster, thus the speed_ is slightly reduced for driving straight ahead

int internalBlueLED = 2;

int m1_PWM = 14;  //D14
int m2_1 = 27;    //D27
int m2_0 = 33;    //D33
int m1_1 = 32;    //D32
int m1_0 = 23;    //D23
int m2_PWM = 26;  //D26

/*
RF Pins on H-bridge input as tested:
B4 B3 B2 B1 B4 B3 (level shifter output)
14 27 33 32 23 26
*/

void setup(){
  Serial.begin(115200);

  pinMode(internalBlueLED, OUTPUT);
  digitalWrite(internalBlueLED, LOW);
  
  pinMode(m1_PWM, OUTPUT);
  pinMode(m2_PWM, OUTPUT);
  pinMode(m1_0, OUTPUT);
  pinMode(m1_1, OUTPUT);
  pinMode(m2_0, OUTPUT);
  pinMode(m2_1, OUTPUT);  

  digitalWrite(m1_0, LOW); //left motor 
  digitalWrite(m1_1, LOW);
  digitalWrite(m2_0, LOW); //right motor 
  digitalWrite(m2_1, LOW);      

  analogWrite(m1_PWM, 0);
  analogWrite(m2_PWM, 0);

  delay(10);

  display.init();
  display.setContrast(255);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.display();
 
  WiFi.begin(ssid, password);

  checkWifiConnection();

 Serial.print("connected to: ");
 Serial.println(WiFi.localIP());

 //receive data from client
 server.on("/setData", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("speedValue")) {
      String speedValueIn = request->getParam("speedValue")->value();    
      speedValue = speedValueIn.toInt(); 
      setNewSpeed();
    }else if(request->hasParam("directionValue")){
      String directionValueIn = request->getParam("directionValue")->value();  
      directionValue = directionValueIn.toInt();  
      setNewSpeed();
    }else if(request->hasParam("spinnerLeft")){
      setSpinner(true);
    }else if(request->hasParam("spinnerRight")){
      setSpinner(false);
    }
    request->send(200, "application/json", "everything ok");
    Serial.print("request new data ");
    Serial.print("speedValue: ");
    Serial.print(speedValue);
    Serial.print(" directionValue: ");
    Serial.println(directionValue);
  }); 


  //send GUI / HTML code to client browser
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });


  server.begin();

  String textIP = "IP: "+WiFi.localIP().toString();
  showTextOnDisplay(textIP);
  
}// end setup
 
void loop(){

checkWifiConnection();
  
//testLedSetup();
}

void checkWifiConnection(){

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to WiFi.....");
    Serial.println("");

    digitalWrite(internalBlueLED, HIGH); 
    delay(500);  
    digitalWrite(internalBlueLED, LOW); 
    delay(500);
    setValuesMotors(LOW, LOW, LOW, LOW);
    

    String ssidString = ssid;
    String pwString = password;
    String text [4] = {ssidString, pwString, "Connecting to WiFi......","Reboot ESP32?"}; 
    showTextLinesOnDisplay(text);
  }

  String textIP = "IP: "+WiFi.localIP().toString();
  if(ipAdress != textIP){
   Serial.print("connected to: ");
   Serial.println(WiFi.localIP());
   ipAdress = textIP;
  }
   //String textIP = "IP: "+WiFi.localIP().toString();
   //showTextOnDisplay(textIP);
  
   digitalWrite(internalBlueLED, HIGH);
}


void setNewSpeed(){

    //values shown on the small SSD1306 display:
    String displaySpeed = "stoppt";
    String displayDirection = "geradeaus";
    
    if(speedValue == 100){ //stop
         analogWrite(m1_PWM, 0);
         analogWrite(m2_PWM, 0);
         setValuesMotors(LOW, LOW, LOW, LOW);
         displaySpeed = {"stoppt"};
         
    }else if(speedValue > 100){ //forward
        
        int totalSpeed_m1 = getPWMValueSpeed(speedValue - 100, maxSpeed);
        int totalSpeed_m2 = totalSpeed_m1;

        //add here motor speed correction
        //correctionLeftMotorPWM                 
        totalSpeed_m2 = totalSpeed_m2 - correctionLeftMotorPWM;
        
        displayDirection = setDirectionPWM(totalSpeed_m1, totalSpeed_m2);
        
        setValuesMotors(HIGH, LOW, HIGH, LOW);
        displaySpeed = "vorwärts "+String(speedValue - 100)+" %";
             
    }else if(speedValue < 100){ //backwards

        int totalSpeed_m1 = getPWMValueSpeed(100 - speedValue, maxSpeed);
        int totalSpeed_m2 = totalSpeed_m1;

        //add here motor speed correction
        //correctionLeftMotorPWM                 
        totalSpeed_m2 = totalSpeed_m2 - correctionLeftMotorPWM;
        
        displayDirection = setDirectionPWM(totalSpeed_m1, totalSpeed_m2);

        setValuesMotors(LOW, HIGH, LOW, HIGH);
        displaySpeed = "rückwärts "+String(100 - speedValue)+" %";
        
    }
    
    String localIP_ = "IP: "+WiFi.localIP().toString();
    String displayText [3] = {localIP_, displaySpeed, displayDirection};
    showTextLinesOnDisplay(displayText);
}


String setDirectionPWM(int totalSpeed_m1, int totalSpeed_m2){
        String displayDirection = "geradeaus";
  
        if(directionValue > 100){  //move to the right
          int pwmSpeedAdditionRight = getPWMValueDirection(directionValue - 100, maxCurveSpeed);      
          totalSpeed_m2 = totalSpeed_m2 + pwmSpeedAdditionRight; 
          displayDirection = "rechts "+String(directionValue - 100)+" %";
        }else if(directionValue < 100){ //move to the left 
          int pwmSpeedAdditionLeft = getPWMValueDirection(100 - directionValue, maxCurveSpeed);
          totalSpeed_m1 = totalSpeed_m1 + pwmSpeedAdditionLeft;
          displayDirection = "links "+String(directionValue - 100)+" %";
        }
        analogWrite(m1_PWM, totalSpeed_m1);
        analogWrite(m2_PWM, totalSpeed_m2 - correctionLeftMotorPWM);

        Serial.print("new speed m1_PWM: ");
        Serial.print(totalSpeed_m1);
        Serial.print(" m2_PWM: ");
        Serial.print(totalSpeed_m2);
        Serial.println("");
             
     return displayDirection;         
}

void setSpinner(boolean left){
         analogWrite(m1_PWM, spinnerSpeed);
         analogWrite(m2_PWM, spinnerSpeed);
    if(left){
         setValuesMotors(HIGH, LOW, LOW, HIGH);
    }else{
         setValuesMotors(LOW, HIGH, HIGH, LOW);
    }
}


int getPWMValueSpeed(int value, int maxSpeed){//mapping to the scale - here from 0 to 200 and max speed
  int pwmValue = map(value,0,200,0,maxSpeed);  
  pwmValue = minValuePWM + pwmValue;
  return pwmValue;
}

int getPWMValueDirection(int value, int maxCurveSpeed){//mapping to the scale - here from 0 to 200 and max direction
  int pwmValue = map(value,0,200,0,maxCurveSpeed);  
  pwmValue = pwmValue * 2;   //because a single scale is just from 0 to 100 => doubling
  return pwmValue;
}

void setValuesMotors(boolean m1_0_s, boolean m1_1_s, boolean m2_0_s, boolean m2_1_s){
         digitalWrite(m1_0, m1_0_s); //left motor
         digitalWrite(m1_1, m1_1_s);
         digitalWrite(m2_0, m2_0_s); //right motor
         digitalWrite(m2_1, m2_1_s);    
}

void showTextLinesOnDisplay(String text []){
  display.clear();
  for(int i=0; i<sizeof(text)-1;i++){
    display.drawString(0, i*10, text[i]);
  }
  display.display();
  delay(100);
}

void showTextOnDisplay(String text){
  display.clear();
  display.drawString(0, 0, text);
  display.display();
  delay(100);
}

void printData(String id, String value){
      Serial.print(id);
      Serial.print(": ");
      Serial.print(value);
      Serial.println("");   
}

void testLedSetup(){ //instead of 2 motors, LEDs can be simply used for testing 
                     // => for this, no external power source is required for wiring, the power delivered by USB and ESP32 is sufficient
             analogWrite(m1_PWM, 255);
             analogWrite(m2_PWM, 255);
          
             digitalWrite(m1_0, LOW); //motor left
             digitalWrite(m1_1, HIGH);
             digitalWrite(m2_0, LOW); //motor right
             digitalWrite(m2_1, HIGH);
             delay(1000);
             digitalWrite(m1_0, HIGH); //motor left
             digitalWrite(m1_1, LOW);
             digitalWrite(m2_0, HIGH); //motor right
             digitalWrite(m2_1, LOW);    
             delay(1000);            
}
