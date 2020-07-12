#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define thermometerPin D7
#define heaterPin D6
#define motorPin D8

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
int startTemperature;
int stopTemperature;
int motorW;
int motorB;
int timerMinC = 0;
byte addrT = 0;
byte addrD = 5;
byte addrMW = 10;
byte addrMB = 15;
bool debug = false;
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
  stopTemperature = EEPROM.read(addrT);
  startTemperature = EEPROM.read(addrD);
  motorW = EEPROM.read(addrMW);
  motorB = EEPROM.read(addrMB);
  if (stopTemperature >= 70) {
    stopTemperature = 55;
    EEPROM.write(addrT,stopTemperature);
    EEPROM.commit();
  }
  if (startTemperature >= 70){
    startTemperature = 53;
    EEPROM.write(addrD,startTemperature);
    EEPROM.commit();
  }
  if (motorW >= 20 || motorW <= 0){
    motorW = 3;
    EEPROM.write(addrMW,motorW);
    EEPROM.commit();
  }
  if (motorB >= 120 || motorB <= 0){
    motorB = 15;
    EEPROM.write(addrMB,motorB);
    EEPROM.commit();
  }
  if (debug == true) Serial.printf("Stop temperature: %d\n",stopTemperature);
  if (debug == true) Serial.printf("Start temperature: %d\n", startTemperature);
  if (debug == true) Serial.printf("Motor working time: %d\n", motorW);
  if (debug == true) Serial.printf("Motor break time: %d\n", motorB);
}

void loop() {
  if (flMotor == false && timerMinC == 0) {
    digitalWrite(motorPin, HIGH);
    flMotor = true;
    if (debug == true) Serial.println("Motor is turned ON!");
  } else if (flMotor == true && timerMinC == motorW) {
    digitalWrite(motorPin, LOW);
    digitalWrite(heaterPin, LOW);
    flMotor = false;
    flHeater = false;
    if (debug == true) Serial.println("Motor is turned OFF!\nHeater is turned OFF!");
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
      for (int i = 0; i < sizeof(packetBuffer); i++) {
        packetBuffer[i] = '\0';
      }
      szOfPacket = 0;
   }
}

void timerCallback() {
  nTimer++;
  nTimer2++;
  if (nTimer >= 60) {
    nTimer = 0;
    timerMinC++;
    Serial.printf("Timer min: %d\n", timerMinC);
  }
  if (timerMinC == motorW + motorB) {
    Serial.println("Timer min reseted!");
    timerMinC = 0;
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
  if (liveTemp <= startTemperature && flHeater == false && flMotor == true) {
    if (debug == true) Serial.println("Heater is ON!");
    digitalWrite(heaterPin, HIGH);
    flHeater = true;
  } else if (liveTemp >= stopTemperature && flHeater == true) {
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
        
        if (command.charAt(2) == 'S') { //RS
          char temp[10];
          sprintf(temp,"!D:%d", startTemperature);
          Udp.beginPacket(senderIP, senderPort);
          Udp.write(temp);
          Udp.endPacket();
          if (debug == true) Serial.printf("Start temperature is: %d\n", startTemperature);
          break;
        }
        
        switch (command.charAt(2)) {
          case 'T': {
            if (command.charAt(3) == 'R') { //RTR
              char temp[10];
              sprintf(temp,"!TR:%d", stopTemperature);
              Udp.beginPacket(senderIP, senderPort);
              Udp.write(temp);
              Udp.endPacket();
              if (debug == true) Serial.printf("Required temperature is: %d\n", stopTemperature); 
              
            } else if (command.charAt(3) == 'N') { //RTN
              char temp[10];
              sprintf(temp,"!TN:%.2f", liveTemp);
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
            
            if (command.charAt(3) == 'W') { //WMW
              command.remove(0,5);
              motorW = command.toInt();
              EEPROM.write(addrMW,motorW);
              EEPROM.commit();
            if (debug == true) Serial.printf("Motor working time is set to: %d\n", motorW);
              
            } else if (command.charAt(3) == 'B') { // WMB
              command.remove(0,5);
              motorB = command.toInt();
              EEPROM.write(addrMB,motorB);
              EEPROM.commit();
              if (debug == true) Serial.printf("Motor break time is set to: %d\n", motorB);
            }
            
          } break;
          
          case 'R': { //WR
            command.remove(0,4);
            stopTemperature = command.toInt();
            EEPROM.write(addrT,stopTemperature);
            EEPROM.commit();
            if (debug == true) Serial.printf("Stop temperature is set to: %d\n", stopTemperature);
          } break;
          
          case 'S': { //WS
            command.remove(0,4);
            startTemperature = command.toInt();
            EEPROM.write(addrD,startTemperature);
            EEPROM.commit();
            if (debug == true) Serial.printf("Start temperature is set to: %d\n", startTemperature);
          } break;
          
        }
        
      } break;
      
    }
  }
}
