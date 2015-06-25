/*************************************************** 
  This is an official distribution of WITH service.
  ----> http://www.hardcopyworld.com
  ----> http://blog.naver.com/365design

  HardCopyWorld and Design Studio 365 invests time 
  and resources providing this open source code.

  Written by Young Bae Suh for HardCopyWorld and Design Studio 365. 
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

///////////////////////////////////////////////////////
// CC3000 WiFi Shield

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "xxxxxxxx"           // cannot be longer than 32 characters!
#define WLAN_PASS       "xxxxxxxx"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

// What page to grab!
#define WEBSITE      "api.thingspeak.com"
// ThingTweet configurations
String thingspeakAPIKey = "xxxxxxxxx";
uint32_t ip;


///////////////////////////////////////////////////////
// Sensoring

unsigned long LastWarningTime = 0;
#define WARNING_TIME 5000

int PeakToPeak = 0;
long PeakToPeakAvg = 0;
int ShockDetect = 0;
int ShockCount = 0;
int NoisePeakCount = 0;
int NoiseAvgCount = 0;
int NoiseAndShockCount = 0;
int index = 0;

#define SHOCK_THRESHOLD 100
#define NOISE_AVERAGE_THRESHOLD 3
#define NOISE_PEAK_THRESHOLD 40
#define NOISE_PEAK_COUNT_THRESHOLD 3
#define NOISE_CHECK_INTERVAL_1M 60000
#define NOISE_CHECK_INTERVAL_5M 60000
#define NOISE_CHECK_INTERVAL_1H 3600000
#define NOISE_CHECK_INTERVAL_1D 86400000
#define TIME_MAX 4294897296
// Sample window width in mS (50 mS = 20Hz)
#define SAMPLE_WINDOW 50

unsigned long LastCheckMinute = 0;
unsigned long LastCheckHour = 0;
unsigned long LastCheckDay = 0;

float MaxNoiseMin = 0;
unsigned int MaxNoiseWarnHour = 0;
unsigned int MaxNoiseWarnDay = 0;
unsigned int MaxNoiseWarnMonth = 0;

float AvgNoiseMin = 0;
unsigned int AvgNoiseWarnHour = 0;
unsigned int AvgNoiseWarnDay = 0;
unsigned int AvgNoiseWarnMonth = 0;

#define LED_PIN 8
#define WAVE_PIN 7
#define SOUND_PIN A0

int LEDCount = 0;


void setup() {
  Serial.begin(9600);
  
  pinMode(WAVE_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin()) {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  // Optional SSID scan
  // listSSIDResults();
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(3000); // ToDo: Insert a DHCP timeout!
  }  

  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }

  ip = 0;
  // Try looking up the website's IP address
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(1500);
  }
  cc3000.printIPdotsRev(ip);
}

void loop() {
  // collect noise for 50 mS
  checkPeakToPeak();
  
  // check max noise
  if(NOISE_PEAK_THRESHOLD < PeakToPeak) {
    turnOnLed();
    NoisePeakCount++;
  }
  if(SHOCK_THRESHOLD < ShockDetect) {
    turnOnLed();
    ShockCount++;
  }
  
  Serial.print("Peak noise = ");
  Serial.print(NoisePeakCount);
  Serial.print(", Avg = ");
  Serial.print(PeakToPeakAvg);
  Serial.print(", ShockCount = ");
  Serial.println(ShockCount);
  
  checkLed();
  
  unsigned long current = millis();
  if(current - LastCheckMinute > NOISE_CHECK_INTERVAL_1M) {
    int isNoiseDetected = 0;
    MaxNoiseMin = NoisePeakCount;
    AvgNoiseMin = (float)PeakToPeakAvg / (float)(index + 1);
    
    if(NOISE_PEAK_COUNT_THRESHOLD <= MaxNoiseMin) {
      turnOnLed();
      AvgNoiseWarnHour++;
      isNoiseDetected = 1;
    }
    else if(NOISE_AVERAGE_THRESHOLD <= AvgNoiseMin) {
      turnOnLed();
      AvgNoiseWarnHour++;
      isNoiseDetected = 1;
    }
    else if(ShockCount > 0) {
      turnOnLed();
      AvgNoiseWarnHour++;
      isNoiseDetected = 1;
    }
    
    LastCheckMinute = current;
    
    if(current - LastCheckHour > NOISE_CHECK_INTERVAL_1H) {
      MaxNoiseWarnDay += MaxNoiseWarnHour;
      AvgNoiseWarnDay += AvgNoiseWarnHour;
      MaxNoiseWarnHour = 0;
      AvgNoiseWarnHour = 0;
      LastCheckHour = current;
    }
    if(current - LastCheckDay > NOISE_CHECK_INTERVAL_1D) {
      MaxNoiseWarnMonth += MaxNoiseWarnDay;
      AvgNoiseWarnMonth += AvgNoiseWarnDay;
      MaxNoiseWarnDay = 0;
      AvgNoiseWarnDay = 0;
      LastCheckDay = current;
    }
    
    // Send result to server
    sendStatusToServer(MaxNoiseMin, AvgNoiseMin, ShockCount, isNoiseDetected);

    // For debug
    Serial.print("Sampling count = ");
    Serial.println(index + 1);
    Serial.print("MaxNoiseMin = ");
    Serial.println(MaxNoiseMin);
    Serial.print("AvgNoiseMin = ");
    Serial.println(AvgNoiseMin);
    Serial.print("ShockCount = ");
    Serial.println(ShockCount);
    Serial.print("MaxNoiseWarnHour = ");
    Serial.println(MaxNoiseWarnHour);
    Serial.print("AvgNoiseWarnHour = ");
    Serial.println(AvgNoiseWarnHour);
    //Serial.print("MaxNoiseWarnDay = ");
    //Serial.println(MaxNoiseWarnDay);
    //Serial.print("AvgNoiseWarnDay = ");
    //Serial.println(AvgNoiseWarnDay);
    //Serial.print("MaxNoiseWarnMonth = ");
    //Serial.println(MaxNoiseWarnMonth);
    //Serial.print("AvgNoiseWarnMonth = ");
    //Serial.println(AvgNoiseWarnMonth);
    Serial.println();
    
    index = 0;
    PeakToPeakAvg = 0;
    ShockCount = 0;
    NoisePeakCount = 0;
    NoiseAvgCount = 0;
  } else {
    index++;
  }
  PeakToPeak = 0;
  ShockDetect = 0;
  
  if(millis() - LastWarningTime > WARNING_TIME) {
    //digitalWrite(LED_PIN, LOW);
    LastWarningTime = millis();
  }

  if(current > TIME_MAX) {
    resetAll();
  }
} // End of loop()


/**************************************************************************
    @brief  Check peak to peak distance
**************************************************************************/
void checkPeakToPeak() {
  unsigned long startMillis= millis();  // Start of sample window

  int sample;
  int signalMax = 0;
  int signalMin = 1024;
  
  while (millis() - startMillis < SAMPLE_WINDOW) {
    sample = analogRead(SOUND_PIN);
    // Check sound
    if (sample < 1024) {  // toss out spurious readings
      if (sample > signalMax) {
        signalMax = sample;  // save just the max levels
      } else if (sample < signalMin) {
        signalMin = sample;  // save just the min levels
      }
    }
    // Check shock wave
    if(digitalRead(WAVE_PIN) == HIGH) {
      ShockDetect++;
    }
  }
  PeakToPeak = signalMax - signalMin;    // peak-to-peak level
  PeakToPeakAvg += PeakToPeak;
}

void turnOnLed() {
  digitalWrite(LED_PIN, HIGH);
  LEDCount = 20;
}

void checkLed() {
  if(LEDCount > 0) {
    LEDCount--;
  } else { 
    LEDCount = 0;
    digitalWrite(LED_PIN, LOW);
  }
}

void showWarning() {
  digitalWrite(LED_PIN, HIGH);
  LastWarningTime = millis();
}

void resetAll() {
  LastCheckMinute = 0;
  LastCheckHour = 0;
  LastCheckDay = 0;
  
  MaxNoiseWarnMonth = 0;
  AvgNoiseWarnMonth = 0;
}

/**************************************************************************
    @brief  Send request to ThingSpeak
**************************************************************************/
void sendStatusToServer(float data1, float data2, int data3, int summary) {
  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
  if (www.connected()) {
    // make query string, 140 chars max!
    String tsData = "api_key="+thingspeakAPIKey+"&field1="+data1+"&field2="+data2+"&field3="+data3+"&field4="+summary;
    char tempChar[156];
    for(int i=0; i<156; i++)
      tempChar[i] = 0x00;
    tsData.toCharArray(tempChar, 155);
    
    www.fastrprint(F("POST /update HTTP/1.1\r\n"));
    www.fastrprint(F("Host: api.thingspeak.com\r\n"));
    www.fastrprint(F("Connection: close\r\n"));
    www.fastrprint(F("Content-Type: application/x-www-form-urlencoded\r\n"));
    www.fastrprint(F("Content-Length: "));
    www.fastrprint(int2str(tsData.length()));
    www.fastrprint(F("\r\n\r\n"));
    www.fastrprint(tempChar);
    www.println();
    Serial.println(tsData);
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }

  Serial.println(F("-------------------------------------"));
  
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      //Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();
  Serial.println(F("-------------------------------------"));
}

/**************************************************************************
    @brief  Begins an SSID scan and prints out all the visible networks
**************************************************************************/
void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33]; 

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
    
    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  }
  Serial.println(F("================================================"));

  cc3000.stopSSIDscan();
}

/**************************************************************************
    @brief  Tries to read the IP address and other connection details
**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

/**************************************************************************
    Utilities
**************************************************************************/
char _int2str[7];
char* int2str( register int i ) {
  register unsigned char L = 1;
  register char c;
  register boolean m = false;
  register char b;  // lower-byte of i
  // negative
  if ( i < 0 ) {
    _int2str[ 0 ] = '-';
    i = -i;
  }
  else L = 0;
  // ten-thousands
  if( i > 9999 ) {
    c = i < 20000 ? 1
      : i < 30000 ? 2
      : 3;
    _int2str[ L++ ] = c + 48;
    i -= c * 10000;
    m = true;
  }
  // thousands
  if( i > 999 ) {
    c = i < 5000
      ? ( i < 3000
          ? ( i < 2000 ? 1 : 2 )
          :   i < 4000 ? 3 : 4
        )
      : i < 8000
        ? ( i < 6000
            ? 5
            : i < 7000 ? 6 : 7
          )
        : i < 9000 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    i -= c * 1000;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // hundreds
  if( i > 99 ) {
    c = i < 500
      ? ( i < 300
          ? ( i < 200 ? 1 : 2 )
          :   i < 400 ? 3 : 4
        )
      : i < 800
        ? ( i < 600
            ? 5
            : i < 700 ? 6 : 7
          )
        : i < 900 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    i -= c * 100;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // decades (check on lower byte to optimize code)
  b = char( i );
  if( b > 9 ) {
    c = b < 50
      ? ( b < 30
          ? ( b < 20 ? 1 : 2 )
          :   b < 40 ? 3 : 4
        )
      : b < 80
        ? ( i < 60
            ? 5
            : i < 70 ? 6 : 7
          )
        : i < 90 ? 8 : 9;
    _int2str[ L++ ] = c + 48;
    b -= c * 10;
    m = true;
  }
  else if( m ) _int2str[ L++ ] = '0';
  // last digit
  _int2str[ L++ ] = b + 48;
  // null terminator
  _int2str[ L ] = 0;  
  return _int2str;
}

