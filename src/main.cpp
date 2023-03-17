#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// #defines WIFI_SSID, WIFI_PASSWORD, API_KEY, USER_EMAIL, USER_PASSWORD, DATABASE_URL
#include "secrets.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String uid;
String databasePath;
String todaysDate;
String timePath = "/timestamp";
unsigned long timestamp;
struct tm *pTimeStruct;

// Parent Node (to be updated in every loop)
String parentPath;
FirebaseJson json;
DynamicJsonDocument jsonObject(1024);
String serialInputBuffer;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

bool sendJsonStringtoFirebase(String jsonString){

  Serial.print(F("Recieved jsonString : "));
  Serial.println(jsonString);

  DeserializationError jsonError = deserializeJson(jsonObject, jsonString);
  if (jsonError) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(jsonError.f_str());
    return false;
  }

  timestamp = getTime();


  time_t epochTime = timeClient.getEpochTime();
  Serial.print("Epoch Time: ");
  Serial.println(epochTime);
  
  pTimeStruct = gmtime ((time_t *)&epochTime); 

  todaysDate = String(pTimeStruct->tm_year+1900) + "-" + String(pTimeStruct->tm_mon+1) + "-" + String(pTimeStruct->tm_mday);
  Serial.print(F("todaysDate : "));
  Serial.println(todaysDate);

  parentPath= databasePath + "/" + todaysDate + "/" + String(timestamp);
  Serial.print(F("parentPath : "));
  Serial.println(parentPath);

  json.clear();
  json.setJsonData(jsonString);
  json.set(timePath, String(timestamp));

  // output final json object
  json.toString(Serial, true);

  // save to RTDB
  if(Firebase.ready()){
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    return true;
  }else{
    Serial.println(F("Firebase not ready."));
    return false;
  }

}

void setup(){
  Serial.begin(9600);
  serialInputBuffer.reserve(255);
  pinMode(BUILTIN_LED, OUTPUT);

  delay(5000);

  initWiFi();
  timeClient.begin();

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task 
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // timestamp test
  timestamp = getTime();
  Serial.print("timestamp: ");
  Serial.println(timestamp);

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('-');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/pvTracks";
  
}

void loop(){

  while(Serial.available()) {
    digitalWrite(BUILTIN_LED, HIGH); 

    serialInputBuffer = Serial.readStringUntil('\n');
    if(serialInputBuffer.length() > 0) {
      sendJsonStringtoFirebase(serialInputBuffer);
    }
  }
  digitalWrite(BUILTIN_LED, LOW); 

}
