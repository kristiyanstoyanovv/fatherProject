#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define thermometerPin D5
#define heaterPin D6
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

int nTimer = 0;
float liveTemp = 0;
int delayH;
int requiredTemperature;
byte addrT = 0;
byte addrD = 5;
bool debug = true;
bool flHeater = false;
bool flCheck = true;


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
  if (debug == true) Serial.printf("Required Temp: %d\n",requiredTemperature);
  if (debug == true) Serial.printf("Delay between temperature: %d\n", delayH);
}

void loop() {
  if (flCheck == true) {
    measureTemperature();
    temperatureRegulator();
    flCheck = false;
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
  if (nTimer >= 5) {
    flCheck = true;
    nTimer = 0;
  }
  resetTimer1s();
}


void measureTemperature() {
  sensors.requestTemperatures();
  if (debug == true) Serial.println(sensors.getTempCByIndex(0));
  liveTemp = sensors.getTempCByIndex(0);
}

void temperatureRegulator() {
  if (liveTemp <= requiredTemperature-delayH && flHeater == false) {
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
    if (command.charAt(1) == 'T') {
      switch(command.charAt(2)) {
        case 'N': {
          char temp[10];
          sprintf(temp,"!TN:%f", liveTemp);
          Udp.beginPacket(senderIP, senderPort);
          Udp.write(temp);
          Udp.endPacket();
          if (debug == true) Serial.printf("Temperature at that moment is: %d\n", requiredTemperature);
        } break;
        
        case 'R': {
          char temp[10];
          sprintf(temp,"!T:%d", requiredTemperature);
          Udp.beginPacket(senderIP, senderPort);
          Udp.write(temp);
          Udp.endPacket();
          if (debug == true) Serial.printf("Required temperature is: %d\n", requiredTemperature);
        } break;
        
        case 'S': {
          command.remove(0,4);
          requiredTemperature = command.toInt();
          EEPROM.write(addrT,requiredTemperature);
          EEPROM.commit();
          if (debug == true) Serial.printf("Temperature is set to: %d\n", requiredTemperature);
        } break;
      }
      
    } else if (command.charAt(1) == 'D') {
        switch(command.charAt(2)) {
        case 'R': {
          char temp[10];
          sprintf(temp,"!D:%d", delayH);
          Udp.beginPacket(senderIP, senderPort);
          Udp.write(temp);
          Udp.endPacket();
          if (debug == true) Serial.printf("Delay is: %d\n", delayH);
        } break;
        
        case 'S': {
          command.remove(0,4);
          delayH = command.toInt();
          EEPROM.write(addrD,delayH);
          EEPROM.commit();
          if (debug == true) Serial.printf("Delay is set to: %d\n", delayH);
        } break;
      }
    }
  }
}
