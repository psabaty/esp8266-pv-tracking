#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <ArduinoJson.h>
//#include <NTPClient.h>
//#include <WiFiUdp.h>
#include "addons/TokenHelper.h"
//#include "addons/RTDBHelper.h"

// #defines WIFI_SSID, WIFI_PASSWORD, API_KEY, USER_EMAIL, USER_PASSWORD, DATABASE_URL
#include "secrets.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
String uid;
String documentPath;

// Parent Node (to be updated in every loop)
String parentPath;
FirebaseJson fbRequestContent;
DynamicJsonDocument jsonObject(1024);
String serialInputBuffer;

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

bool sendJsonStringtoFirestore(String jsonString){

  Serial.print(F("Recieved jsonString : "));
  Serial.println(jsonString);

  DeserializationError jsonError = deserializeJson(jsonObject, jsonString);
  if (jsonError) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(jsonError.f_str());
    return false;
  }

  //FirebaseJson content;
  fbRequestContent.clear();
  for (JsonPair kv : jsonObject.as<JsonObject>()) {
      String fieldPath = "fields/";
      fieldPath += kv.key().c_str();
      fieldPath += "/doubleValue";
      fbRequestContent.set(fieldPath,  kv.value().as<double>());
  }
  
  fbRequestContent.toString(Serial, true);

  // save to Firestore
  if(Firebase.ready() && (millis() - sendDataPrevMillis > 3000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

        Serial.print("Create a document... ");

        if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), fbRequestContent.raw(), "", false)){
            
            Serial.println("request sent.");
            Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
            return true;
        }else{
            Serial.println("Error.");
            Serial.println(fbdo.errorReason());
            return false;
        }
        
  }else{
    Serial.println(F("Firebase not ready."));
    return false;
  }

}

void setup(){
  Serial.begin(9600);
  serialInputBuffer.reserve(512);
  pinMode(BUILTIN_LED, OUTPUT);

  //delay(5000);

  initWiFi();
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

  documentPath = "UsersData/" + uid + "/pvRecords/latest";
  Serial.print(F("documentPath : "));
  Serial.println(documentPath);
}

void loop(){

  while(Serial.available()) {
    digitalWrite(BUILTIN_LED, HIGH); 

    serialInputBuffer = Serial.readStringUntil('\n');
    if(serialInputBuffer.length() > 0) {
      sendJsonStringtoFirestore(serialInputBuffer);
    }
  }
  digitalWrite(BUILTIN_LED, LOW); 

}
