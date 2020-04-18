#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define thermometerPin D5
#define heaterPin D6
#define motorPin D7

#define resetTimer1s()   timer0_write(ESP.getCycleCount() + 80000000)

const char* ssid = "TDI!";
const char* password = "qazwsxedc2";

char packetBuffer[50];
byte szOfPacket;

IPAddress ip(192, 168, 0, 111);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0); 
IPAddress senderIP;

OneWire oneWire(thermometerPin);
DallasTemperature sensors(&oneWire);

int internalPort = 1111;
int senderPort;

void connectWifi();
void initTimer();
void timerCallback();
void checkForPacket();
void initEEPROM();
void temperatureRegulator();
void measureTemperature();
void parserCommand(String command);

float liveTemp = 0;
int nTimer = 0;
int nTimer2 = 0;
int delayH;
int requiredTemperature;
int motorW;
int motorB;
int timerMinC = 0;
byte addrT = 0;
byte addrD = 5;
byte addrMW = 10;
byte addrMB = 15;
bool debug = true;
bool flHeater = false;
bool flCheck = true;
bool flMotor = false;


WiFiUDP Udp;

void setup() {
  Serial.begin(115200);
  Serial.println("---------------- SETUP ----------------");
  connectWifi();
  sensors.begin();
  EEPROM.begin(512);
  initEEPROM();
  initTimer();
  pinMode(heaterPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  Serial.println("---------------- LOOP ----------------");
}

void connectWifi() {
  if(debug == true) {
     Serial.println();
     Serial.println();
     Serial.print("Searching for Wi-Fi with name: ");
     Serial.print(ssid);
     Serial.println();
     Serial.print("Trying to connect..."); 
  }
 
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(debug == true) {
       Serial.print(".");
    }
  }
  if(debug == true) {
     Serial.println("");
     Serial.println("Wi-Fi found and connected!");
     Serial.println("Server started!");
     Serial.println("Use this IP:");
     Serial.println(WiFi.localIP());
  }
  Udp.begin(internalPort);
}


void initTimer() {
  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timerCallback);
  resetTimer1s();
  interrupts();

}

void initEEPROM() {
  requiredTemperature = EEPROM.read(addrT);
  delayH = EEPROM.read(addrD);
  
  if (requiredTemperature >= 70) {
    requiredTemperature = 50;
    EEPROM.write(addrT,requiredTemperature);
    EEPROM.commit();
  }
  if (delayH >= 20 || delayH <= 0){
    delayH = 2;
    EEPROM.write(addrD,delayH);
    EEPROM.commit();
  }
  if (motorW >= 20 || motorW <= 0){
    motorW = 3;
    EEPROM.write(addrMW,motorW);
    EEPROM.commit();
  }
  if (motorB >= 60 || motorB <= 0){
    motorB = 15;
    EEPROM.write(addrMB,motorB);
    EEPROM.commit();
  }
  if (debug == true) Serial.printf("Required Temp: %d\n",requiredTemperature);
  if (debug == true) Serial.printf("Delay between temperature: %d\n", delayH);
  if (debug == true) Serial.printf("Motor working time: %d\n", motorW);
  if (debug == true) Serial.printf("Motor break time: %d\n", motorB);
}

void loop() {
  if (flMotor == false && timerMinC == 0) {
    digitalWrite(motorPin, HIGH);
    flMotor = true;
  } else if (flMotor == true && timerMinC == motorW) {
    digitalWrite(motorPin, LOW);
    digitalWrite(heaterPin, LOW);
    flMotor = false;
    flHeater = false;
  }
  if (flCheck == true) {
    measureTemperature();
    flCheck = false;
    if (flMotor == true) {
      temperatureRegulator();
    }
  }
  checkForPacket();
}

void checkForPacket() {
  szOfPacket = Udp.parsePacket();
  if ( szOfPacket ) {
      Udp.read(packetBuffer, szOfPacket);
      String command = packetBuffer;
      senderIP = Udp.remoteIP();
      senderPort = Udp.remotePort();
      if (debug == true) {
          Serial.print("Size of packet: ");
          Serial.print(szOfPacket);
          Serial.print('\t');
          Serial.print("Packet: ");
          Serial.println(packetBuffer);
          Serial.print("Remote IP: ");
          Serial.print(senderIP);
          Serial.print('\t');
          Serial.print("Remote Port: ");
          Serial.println(senderPort);
      } 
      parserCommand(command);
   }
  szOfPacket = 0;
}

void timerCallback() {
  nTimer++;
  nTimer2++;
  if (nTimer >= 60) {
    nTimer = 0;
    timerMinC++;
  }
  if (timerMinC == motorB) {
    timerMinC == 0;
  }
  if (nTimer2 >= 5) {
    flCheck = true;
    nTimer2 = 0;
  }
  resetTimer1s();
}


void measureTemperature() {
  sensors.requestTemperatures();
  if (debug == true) Serial.println(sensors.getTempCByIndex(0));
  liveTemp = sensors.getTempCByIndex(0);
}

void temperatureRegulator() {
  if (liveTemp <= requiredTemperature-delayH && flHeater == false && flMotor == true) {
    if (debug == true) Serial.println("Heater is ON!");
    digitalWrite(heaterPin, HIGH);
    flHeater = true;
  } else if (liveTemp >= requiredTemperature && flHeater == true) {
    if (debug == true) Serial.println("Heater is OFF!");
    digitalWrite(heaterPin, LOW);
    flHeater = false;
  }
}

void parserCommand(String command) {
  Serial.println(command);
  if (command.charAt(0) == '#') {
    switch (command.charAt(1)) {
      
      case 'R': {
        
        if (command.charAt(2) == 'D') { //RD
          char temp[10];
          sprintf(temp,"!D:%d", delayH);
          Udp.beginPacket(senderIP, senderPort);
          Udp.write(temp);
          Udp.endPacket();
          if (debug == true) Serial.printf("Delay is: %d\n", delayH);
          break;
        }
        
        switch (command.charAt(2)) {
          case 'T': {
            if (command.charAt(3) == 'R') { //RTR
              char temp[10];
              sprintf(temp,"!TR:%d", requiredTemperature);
              Udp.beginPacket(senderIP, senderPort);
              Udp.write(temp);
              Udp.endPacket();
              if (debug == true) Serial.printf("Required temperature is: %d\n", requiredTemperature); 
              
            } else if (command.charAt(3) == 'N') { //RTN
              char temp[10];
              sprintf(temp,"!TN:%f", liveTemp);
              Udp.beginPacket(senderIP, senderPort);
              Udp.write(temp);
              Udp.endPacket();
              if (debug == true) Serial.printf("Temperature at that moment is: %f\n", liveTemp);
            }
          } break;
          
          case 'M': {
            
            if (command.charAt(3) == 'W') { //RMW
              char temp[10];
              sprintf(temp,"!MW:%d", motorW);
              Udp.beginPacket(senderIP, senderPort);
              Udp.write(temp);
              Udp.endPacket();
              if (debug == true) Serial.printf("Motor working time: %d\n", motorW);
              
            } else if (command.charAt(3) == 'B') { //RMB
              char temp[10];
              sprintf(temp,"!MB:%d", motorB);
              Udp.beginPacket(senderIP, senderPort);
              Udp.write(temp);
              Udp.endPacket();
              if (debug == true) Serial.printf("Motor break time: %d\n", motorB);
            }
            
          } break;
          
        }
      } break;
      
      case 'W': {
        
        switch (command.charAt(2)) {
          
          case 'M': {
            
            if (command.charAt(3) == 'W') {
              command.remove(0,5);
              motorW = command.toInt();
              EEPROM.write(addrMW,motorW);
              EEPROM.commit();
            if (debug == true) Serial.printf("Motor working time is set to: %d\n", motorW);
              
            } else if (command.charAt(3) == 'B') {
              command.remove(0,5);
              motorB = command.toInt();
              EEPROM.write(addrMB,motorB);
              EEPROM.commit();
              if (debug == true) Serial.printf("Motor break time is set to: %d\n", motorB);
            }
            
          } break;
          
          case 'T': {
            command.remove(0,4);
            requiredTemperature = command.toInt();
            EEPROM.write(addrT,requiredTemperature);
            EEPROM.commit();
            if (debug == true) Serial.printf("Temperature is set to: %d\n", requiredTemperature);
          } break;
          
          case 'D': {
            command.remove(0,4);
            delayH = command.toInt();
            EEPROM.write(addrD,delayH);
            EEPROM.commit();
            if (debug == true) Serial.printf("Delay is set to: %d\n", delayH);
          } break;
          
        }
        
      } break;
      
    }
  }
}
