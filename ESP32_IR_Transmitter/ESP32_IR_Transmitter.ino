#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "COC123"
#define WIFI_PASSWORD "ooooo123"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "AIzaSyCFcMdUr5eVTGJnEZfWD1YjRYXrV0Tyg_Y"

/* 3. Define the RTDB URL */
#define DATABASE_URL "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "aleefrock12345@gmail.com"
#define USER_PASSWORD "!12345"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool statusAirFirebase;
bool statusAir = false;

#include "PinDefinitionsAndMore.h"  // Define macros for input and output pin etc.
#if !defined(IR_SEND_PIN)
#define IR_SEND_PIN 4
#endif

/*
 * Specify DistanceWidthProtocol for decoding. This must be done before the #include <IRremote.hpp>
 */
#define DECODE_DISTANCE_WIDTH  // Universal decoder for pulse distance width protocols
//
#if !defined(RAW_BUFFER_LENGTH)
#if RAMEND <= 0x4FF || RAMSIZE < 0x4FF
#define RAW_BUFFER_LENGTH 120
#elif RAMEND <= 0xAFF || RAMSIZE < 0xAFF  // 0xAFF for LEONARDO
#define RAW_BUFFER_LENGTH 400             // 600 is too much here, because we have additional uint8_t rawCode[RAW_BUFFER_LENGTH];
#else
#define RAW_BUFFER_LENGTH 750
#endif
#endif

//#define NO_LED_FEEDBACK_CODE      // saves 92 bytes program memory
//#define RECORD_GAP_MICROS 12000   // Default is 5000. Activate it for some LG air conditioner protocols
//#define SEND_PWM_BY_TIMER         // Disable carrier PWM generation in software and use (restricted) hardware PWM.
//#define USE_NO_SEND_PWM           // Use no carrier PWM, just simulate an active low receiver signal. Overrides SEND_PWM_BY_TIMER definition

//#define DEBUG // Activate this for lots of lovely debug output from the decoders.

#include <IRremote.hpp>

#define SEND_BUTTON_PIN 13

#define DELAY_BETWEEN_REPEATS_MILLIS 70

// Storage for the recorded code, pre-filled with NEC data
IRRawDataType sDecodedRawOpen[RAW_DATA_ARRAY_SIZE] = { 0x6000000521C };                        // address 0x12 command 0x34
IRRawDataType sDecodedRawClose[RAW_DATA_ARRAY_SIZE] = { 0x7000000520C };                       // address 0x12 command 0x34
DistanceWidthTimingInfoStruct sDistanceWidthTimingInfo = { 9050, 4550, 600, 1650, 600, 500 };  // NEC timing
uint8_t sNumberOfBits = 44;

unsigned long repeatSendIR;

void setup() {
  Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) /*stm32duino*/ || defined(USBCON) /*STM32_stm32*/ || defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
  delay(4000);  // To be able to connect Serial monitor after reset or power up and before first print out. Do not wait for an attached Serial Monitor!
#endif
  IrSender.begin();  // Start with IR_SEND_PIN as send pin and enable feedback LED at default feedback LED pin
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  // Or use legacy authenticate method
  // config.database_url = DATABASE_URL;
  // config.signer.tokens.legacy_token = "<database secret>";

  // To connect without auth in Test Mode, see Authentications/TestMode/TestMode.ino

  //////////////////////////////////////////////////////////////////////////////////////////////
  // Please make sure the device free Heap is not lower than 80 k for ESP32 and 10 k for ESP8266,
  // otherwise the SSL connection will fail.
  //////////////////////////////////////////////////////////////////////////////////////////////

  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println("Connected to WiFi");
    Firebase.reconnectWiFi(true);
  } else {
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
      sendDataPrevMillis = millis();
      if (Firebase.RTDB.getBool(&fbdo, F("/DeviceStatus/AirConditioner"), &statusAirFirebase)) {
        Serial.print("Update status success: ");
        Serial.println(statusAirFirebase);
      } else {
        Serial.println(fbdo.errorReason().c_str());
      }
    }

    if (!statusAir && statusAirFirebase != statusAir) {
      statusAir = statusAirFirebase;
      repeatSendIR = millis();
      while (millis() - repeatSendIR < 1000) {
        Serial.println();
        Serial.flush();  // To avoid disturbing the software PWM generation by serial output interrupts

        IrSender.sendPulseDistanceWidthFromArray(38, &sDistanceWidthTimingInfo, &sDecodedRawOpen[0], sNumberOfBits,
#if defined(USE_MSB_DECODING_FOR_DISTANCE_DECODER)
                                                 PROTOCOL_IS_MSB_FIRST
#else
                                                 PROTOCOL_IS_LSB_FIRST
#endif
                                                 ,
                                                 100, 0);

        delay(DELAY_BETWEEN_REPEATS_MILLIS);  // Wait a bit between retransmissions
      }
    } else if (statusAir && statusAirFirebase != statusAir) {
      statusAir = statusAirFirebase;
      repeatSendIR = millis();
      while (millis() - repeatSendIR < 1000) {
        Serial.println();
        Serial.flush();  // To avoid disturbing the software PWM generation by serial output interrupts

        IrSender.sendPulseDistanceWidthFromArray(38, &sDistanceWidthTimingInfo, &sDecodedRawClose[0], sNumberOfBits,
#if defined(USE_MSB_DECODING_FOR_DISTANCE_DECODER)
                                                 PROTOCOL_IS_MSB_FIRST
#else
                                                 PROTOCOL_IS_LSB_FIRST
#endif
                                                 ,
                                                 100, 0);

        delay(DELAY_BETWEEN_REPEATS_MILLIS);  // Wait a bit between retransmissions
      }
    }
  }
  delay(100);
}