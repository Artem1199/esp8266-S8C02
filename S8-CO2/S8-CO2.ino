#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFiGeneric.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>
#include "iot_iconset_16x16.h"

// ****** LCD Setup ****** //
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include "arduino_secrets.h"
// #include "arduino_secrets_vav.h"

// ****** TELEMETRY ****** //
const char* ssid          = SECRET_SMARTHOME_WIFI_SSID;
const char* password      = SECRET_SMARTHOME_WIFI_PASSWORD;

WiFiClient client;
bool wifi_on = false;

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
int co2_ths_int = 1000;
String co2_ths;

// SoftwareSerial s8Serial(Rx, Tx);

int s8_co2;
int s8_co2_mean;
int s8_co2_mean2;

float smoothing_factor = 0.5;

byte cmd_s8[]       = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};
byte abc_s8[]       = {0xFE, 0x03, 0x00, 0x1F, 0x00, 0x01, 0xA1, 0xC3};
byte response_s8[7] = {0, 0, 0, 0, 0, 0, 0};

const int r_len = 7;
const int c_len = 8;

// ****** Functions ****** //

void wifi_reconnect() {
  // Serial.printf("\nConnecting to %s ", ssid);
  scrolltextwifi();
  
  WiFi.hostname("TestUnit");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int conn_retry = 0;
 
  while (WiFi.status() != WL_CONNECTED) {
    // Serial.print(".");
    delay(500);
    conn_retry += 1;
    if (conn_retry > 20){
      wifi_on = false;
      display.stopscroll();
      delay(1000);
      display.clearDisplay();
      display.setCursor(10, 0);
      display.println(F("Connection to WiFi/nfailed"));
      display.display();
      delay(1000);
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED){
      wifi_on = true;
      display.stopscroll();
      delay(1000);
      display.clearDisplay();
      display.setCursor(10, 0);
      display.println(F("Connection to WiFi/nsuccessful"));
      display.display();
      delay(1000);
  }
  return;
}

void scrolltextwifi(void){
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  String ssid_String = "Connecting to \n" + String(ssid) + "...";
  display.println(ssid_String);
  display.display();
  display.startscrollleft(0x00, 0x0F);
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

  // I2C setup for LCD
  Wire.begin(2,0);
  
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  
  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display(); // .display() must be called if you want to display the image
  delay(2000); 
  
  co2_ths = String(co2_ths_int);  // co2 convert to string, not sure if needed

  
  if(WiFi.status() != WL_CONNECTED) {
    wifi_reconnect();
  }
    PrimePushOver("Connected to Wifi...");
    UpdatePushServer();
  // Serial.print("Connecting to s8 sensor...");
    get_abc();
  // Serial.print("Finished setup");
}

void displayreadings(String co2_mean, int co2_mean_int){
  display.clearDisplay();
  display.setTextSize(2.5);
  display.setCursor(0,10);
  display.println(co2_mean + " ppm");
  if (wifi_on){
      display.drawBitmap(110, 10, wifiOn, 16, 16, 1);
  } else {
    display.drawBitmap(110, 10, wifiOff, 16, 16, 1);
  }
  display.display();
}
int read_counter = 0;  // read more often than checking sensor
void loop() {
  
  // pushover();
  
  int co2_mean_int = 0;

  read_counter += 1;
  co2_mean_int = co2_measure_smooth();
  String co2_mean = String(co2_mean_int);
  displayreadings(co2_mean, co2_mean_int);

  if (read_counter > 1){
    read_counter = 0;
    if ((co2_mean_int > co2_ths_int) && wifi_on){
      String po_co2_msg = String("Warning: CO2 value at " + co2_mean + " ppm (Threshold: " + co2_ths + " ppm).");
      PrimePushOver(po_co2_msg);
      UpdatePushServer();
     }
  }
  
  delay(2 * 1000L);
}
