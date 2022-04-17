#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFiGeneric.h>

// #include "arduino_secrets.h"
#include "arduino_secrets_vav.h"

// ****** TELEMETRY ****** //
const char* ssid          = SECRET_SMARTHOME_WIFI_SSID;
const char* password      = SECRET_SMARTHOME_WIFI_PASSWORD;

WiFiClient client;

// Pushover //
String Token  = "affsqwrh74kwryx2yxv6rz4uxmpxs4";
String User   = "umv3pw6khbwnswgv54bneo3o6wnmpz";
const String MSG_ALARMTRIGGERED = "TEST ALARM TRIGGERED!";
bool isSendPush = false;
String pushParameters;
bool TRIG_PIN = true;

int length;

// ****** CO2 Sensor ****** //

// CO2 Sensor defines //
//#define Rx ()
//#define Tx ()
int co2_ths_int = 400;
String co2_ths;

// SoftwareSerial s8Serial(Rx, Tx);

int s8_co2;
int s8_co2_mean;
int s8_co2_mean2;

float smoothing_factor = 0.5;
float smoothing_factor2 = 0.15;

byte cmd_s8[]       = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};
byte abc_s8[]       = {0xFE, 0x03, 0x00, 0x1F, 0x00, 0x01, 0xA1, 0xC3};
byte response_s8[7] = {0, 0, 0, 0, 0, 0, 0};

const int r_len = 7;
const int c_len = 8;

// ****** Functions ****** //

boolean wifi_reconnect() {
  // Serial.printf("\nConnecting to %s ", ssid);
  WiFi.hostname("TestUnit");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    // Serial.print(".");
    delay(500);
  }
  // Serial.printf("\nConnected to the WiFi network: %s\n", ssid);

  return true;
}


void s8Request(byte cmd[]) {
  // PrimePushOver("Starting s8Request...");
  // UpdatePushServer();
  Serial.begin(9600);
  while(!Serial.available()) { 
    Serial.write(cmd, c_len);  
    delay(50); 
  }
  int timeout=0;
  while(Serial.available() < r_len ) { 
    timeout++; 
    if(timeout > 10) {
      while(Serial.available()) { 
        Serial.read(); 
        break;
      }
    } 
    delay(50); 
  } 
  for (int i=0; i < r_len; i++) { 
    response_s8[i] = Serial.read();  
  }
  
  Serial.end(); 

 // PrimePushOver("Finished s8Request.");
 // UpdatePushServer();
}                     

unsigned long s8Replay(byte rc_data[]) { 
  int high = rc_data[3];
  int low = rc_data[4];
  unsigned long val = high*256 + low;
  return val; 
}

int co2_measure_smooth() {
  s8Request(cmd_s8);
  s8_co2 = s8Replay(response_s8);
  
  if (!s8_co2_mean) s8_co2_mean = s8_co2;
  s8_co2_mean = s8_co2_mean - smoothing_factor*(s8_co2_mean - s8_co2);
  
   return s8_co2_mean;
}

void get_abc() {
  PrimePushOver("Starting get_abc()...");
  UpdatePushServer();
  int abc_s8_time;
  s8Request(abc_s8);
  abc_s8_time = s8Replay(response_s8);
  // Serial.printf("ABC Time is: %d hours\n", abc_s8_time);
  return;
}

void PrimePushOver(String Msg){
  if (isSendPush == false)
  // if (isSendPush == false && digitalRead(TRIG_PIN) == HIGH)
  {
      StartPushNotification(Msg);
  }
}

void StartPushNotification(String message) {
  // Form the string
  pushParameters = "token="+Token+"&user="+User+"&message="+message;
  isSendPush = true;
  client.connect("api.pushover.net", 80);
  //Serial.println("Connect to push server");
}


// Keep track of the push server connection status without holding 
// up the code execution
void UpdatePushServer(){
    if(isSendPush == true && client.connected()) {
      int length = pushParameters.length();
      //Serial.println("Posting push notification: " + pushParameters);
      client.println("POST /1/messages.json HTTP/1.1");
      client.println("Host: api.pushover.net");
      client.println("Connection: close\r\nContent-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.print(length);
      client.println("\r\n");
      client.print(pushParameters);

      client.stop();
      isSendPush = false;
    }
}

void setup() {

  co2_ths = String(co2_ths_int);
  
  if(WiFi.status() != WL_CONNECTED) {
    wifi_reconnect();
  }
    PrimePushOver("Connected to Wifi...");
    UpdatePushServer();
  // Serial.print("Connecting to s8 sensor...");
    get_abc();
  // Serial.print("Finished setup");
}

int read_counter = 0;  // read more often than checking sensor
void loop() {
  
  // pushover();
  
  int co2_mean_int = 0;

  read_counter += 1;
  co2_mean_int = co2_measure_smooth();

  if (read_counter > 1){
    read_counter = 0;
    if (co2_mean_int > co2_ths_int){
      String co2_mean = String(co2_mean_int);
      String po_co2_msg = String("Warning: CO2 value at " + co2_mean + " ppm (Threshold: " + co2_ths + " ppm).");
      PrimePushOver(po_co2_msg);
      UpdatePushServer();
     }
  }
  
  delay(2 * 1000L);
}
