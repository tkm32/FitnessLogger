/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Gsender.h>
extern "C" {
  #include "user_interface.h"
}

#define USER_INT 10

#define JST 3600*9
#define TRIP_PER_CYCLE 6.2
#define SEND_INTERVAL 10000
#define MAX_SABORI    15000
#define ADC_THRESH 500

const char* ssid     = "HG8045-C16B-bg";
const char* password = "3f6a9wmm";
const char* dst_dir = "http://takami.php.xdomain.jp/fl/";

unsigned long last_send = 0, last_rising = 0, now;
unsigned long fitness_start;
double trip = 0;
uint32_t adc_last = 0, adc;
uint8_t finish = 0;

volatile uint8_t user = 0, know_user_info = 0, req_user_info_cnt = 0;
String user_name;
String user_email;

String get_page(String url) {
  HTTPClient http;

  http.begin(url);
  int httpCode = http.GET();

  String result = "";

  if (httpCode < 0) {
    // result = http.errorToString(httpCode);
  } else {
    result = http.getString();
  }

  http.end();
  return result;
}


int get_user_info() {
  String url = String(dst_dir) + String("user_info.php?user=");
  url += user;
  Serial.println("Requesting: ");
  Serial.println(url);
  String json = get_page(url);
  // Serial.println(json);
  StaticJsonBuffer<180> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(json);
  int res = object.get<int>("result");
  if (res != 1) {
    Serial.println(" >>> Failed to get user info.");
    return -1;
  }
  user_name = object.get<String>("name");
  user_email = object.get<String>("email");
  Serial.println("Name:  " + user_name);
  Serial.println("Email: " + user_email);
  return 0;
}

int send_trip() {
  struct tm *tm;
  time_t t = time(NULL);
  tm = localtime(&t);
  String url = String(dst_dir) + "add.php?user=" + user;
  url += String("&hr=") + tm->tm_hour + "&min=" + tm->tm_min + "&sec=" + tm->tm_sec;
  url += String("&trip=") + (int)trip;
  Serial.println("Requesting: ");
  Serial.println(url);
  get_page(url);
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  configTime(JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  delay(2000);
}

int is_rising() {
  return adc_last < ADC_THRESH && adc > ADC_THRESH;
}

void loop() {
  if(finish) {
    return;
  }
  delay(6);
  now = millis();
  if (know_user_info && now - last_send > SEND_INTERVAL) {
    send_trip();
    last_send = now;
  }
  else if (!know_user_info && req_user_info_cnt == 3) {
    if(!get_user_info()) {
      Serial.println("User Information: ");
      Serial.println("----------");
      Serial.println(user_name);
      Serial.println(user_email);
      Serial.println("----------");
      know_user_info = 1;
      fitness_start = now;
      last_rising = now;
      last_send = now;
    }
  }

  if (know_user_info && now - last_rising > MAX_SABORI) {
    finish = 1;
    fitness_start = last_rising - fitness_start;
    Gsender *gsender = Gsender::Instance();    // Getting pointer to class instance
    String subject = "FitnessLogger";
    String body = user_name + "さん<br><br>お疲れ様でした。<br>今回の走行:<br>----------<br>  " + (int)trip + " m<br>  ";
    body += String("平均速度  ") + (int)(trip * 3600 / fitness_start) + " km/h<br>----------<br><br>";
    body += String("ユーザページ: ") + dst_dir + "index.php?user=" + user;
    if(gsender->Subject(subject)->Send(user_email, body)) {
        Serial.println("Message send.");
    } else {
        Serial.print("Error sending message: ");
        Serial.println(gsender->getError());
    }
  }
  adc = system_adc_read();
  if (is_rising() && millis() - last_rising > 250) {
    last_rising = millis();
    if (know_user_info) {
      trip += TRIP_PER_CYCLE;
    }
    else if (!know_user_info && req_user_info_cnt < 3){
      req_user_info_cnt++;
    }
  }
  adc_last = adc;
}

