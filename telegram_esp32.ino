/*
albino98
https://github.com/albino98/telegram_esp32.git
*/
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
// Replace with your network credentials
const char* ssid = "NetworkName";
const char* password = "NetworkPW";
const int motionSensor = 13;

int armed = 1;

#define BOTtoken "xxxxxxx:xxxxxxxxxxxxxxxxxx"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
#define CHAT_ID "xxxxxxx"

//Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM  35
#define Y8_GPIO_NUM  34
#define Y7_GPIO_NUM  39
#define Y6_GPIO_NUM  36
#define Y5_GPIO_NUM  21
#define Y4_GPIO_NUM  19
#define Y3_GPIO_NUM  18
#define Y2_GPIO_NUM  5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

const int ledPin = 4;
bool ledState = LOW;
camera_fb_t * fb;
uint8_t* fb_buffer;
size_t fb_length;
int currentByte;
boolean startTimer = false;
// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/take_photo to take a picture \n";
      welcome += "/arm to arm the security system \n";
      welcome += "/disarm to disarm the security system \n";
      welcome += "/led_on to turn GPIO ON \n";
      welcome += "/led_off to turn GPIO OFF \n";
      welcome += "/state to request current GPIO state and security system state \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/led_on") {
      bot.sendMessage(chat_id, "LED state set to ON", "");
      ledState = HIGH;
      digitalWrite(ledPin, ledState);
    }
    
    if (text == "/led_off") {
      bot.sendMessage(chat_id, "LED state set to OFF", "");
      ledState = LOW;
      digitalWrite(ledPin, ledState);
    }

    if (text == "/take_photo") {
      take_send_photo(CHAT_ID);
    }

    if (text == "/security_on") {
      armed = 1;
      bot.sendMessage(chat_id, "Security System is ON", "");
    }

    if (text == "/security_off") {
      armed = 0;
      bot.sendMessage(chat_id, "Security System is OFF", "");
    }
    
    if (text == "/state") {
      if (digitalRead(ledPin)){
        bot.sendMessage(chat_id, "LED is ON", "");
      }
      else{
        bot.sendMessage(chat_id, "LED is OFF", "");
      }
      if(armed == 1){
        bot.sendMessage(chat_id, "The security system is ON", "");
      }
      else {
        bot.sendMessage(chat_id, "The security system is OFF", "");  
      }
    }
  }
}


bool isMoreDataAvailable() {
  return (fb_length - currentByte);
}

uint8_t photoNextByte() {
  currentByte++;
  return (fb_buffer[currentByte - 1]);
}

void take_send_photo(String chat_id)
{
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  currentByte = 0;
  fb_length = fb->len;
  fb_buffer = fb->buf;
  bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len, isMoreDataAvailable, photoNextByte, nullptr, nullptr);
  esp_camera_fb_return(fb);
  fb_length = NULL;
  fb_buffer = NULL;
}

void setup() {
  armed = 1;
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG,0);//disable brownout detector
  Serial.begin(115200);

  camera_config_t configg;
  configg.ledc_channel = LEDC_CHANNEL_0;
  configg.ledc_timer = LEDC_TIMER_0;
  configg.pin_d0 = Y2_GPIO_NUM;
  configg.pin_d1 = Y3_GPIO_NUM;
  configg.pin_d2 = Y4_GPIO_NUM;
  configg.pin_d3 = Y5_GPIO_NUM;
  configg.pin_d4 = Y6_GPIO_NUM;
  configg.pin_d5 = Y7_GPIO_NUM;
  configg.pin_d6 = Y8_GPIO_NUM;
  configg.pin_d7 = Y9_GPIO_NUM;
  configg.pin_xclk = XCLK_GPIO_NUM;
  configg.pin_pclk = PCLK_GPIO_NUM;
  configg.pin_vsync = VSYNC_GPIO_NUM;
  configg.pin_href = HREF_GPIO_NUM;
  configg.pin_sscb_sda = SIOD_GPIO_NUM;
  configg.pin_sscb_scl = SIOC_GPIO_NUM;
  configg.pin_pwdn = PWDN_GPIO_NUM;
  configg.pin_reset = RESET_GPIO_NUM;
  configg.xclk_freq_hz = 20000000;
  configg.pixel_format = PIXFORMAT_JPEG;

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  rtc_gpio_hold_dis(GPIO_NUM_4);

  pinMode(GPIO_NUM_13, INPUT_PULLUP);

  if(psramFound()){
    configg.frame_size = FRAMESIZE_UXGA;
    configg.jpeg_quality = 10;
    configg.fb_count = 2;
  }else{
    configg.frame_size = FRAMESIZE_SVGA;
    configg.jpeg_quality = 12;
    configg.fb_count = 1;
  }

  //Init Camera
  esp_err_t err = esp_camera_init(&configg);
  if(err != ESP_OK){
    Serial.printf("Camera init failed with error");  
    return;
  }
  
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // set the Telegram SSL cert

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());



}

void loop() {
  if(armed == 1){
    int isDetected = digitalRead(13);
    if(isDetected == 1){
      Serial.println("Presence detected");
      take_send_photo(CHAT_ID);
      delay(3000);
    }
  }
 //delay(1000);
 
  //if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    delay(1000);
    lastTimeBotRan = millis();
 // }
}
