#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ระบุข้อมูลกระบวนการสร้างโทเค็น
#include <addons/TokenHelper.h>

// ระบุข้อมูลการพิมพ์ real-time database payload และฟังก์ชันตัวช่วยอื่นๆ
#include <addons/RTDBHelper.h>

/* 1. กำหนดข้อมูลของการรับรองความถูกต้องและการเข้ารหัสที่ router ใช้ */
#define WIFI_SSID "COC123"          // ชื่อ WIFI
#define WIFI_PASSWORD "ooooo123"    // รหัส WIFI

// สำหรับข้อมูลประจำตัวต่อไปนี้ สามารถดูตัวอย่างได้ที่ examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. กำหนดคีย์ API */
#define API_KEY "AIzaSyCFcMdUr5eVTGJnEZfWD1YjRYXrV0Tyg_Y"

/* 3. กำหนด URL ของ real-time database */
#define DATABASE_URL "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/" 

/* 4. กำหนดอีเมลและรหัสผ่านผู้ใช้ที่ลงทะเบียนหรือเพิ่มใน project แล้ว */
#define USER_EMAIL "aleefrock12345@gmail.com"   // อีเมลที่เพิ่มใน Authentication บนแพลตฟอร์ม firebase
#define USER_PASSWORD "!12345"                  // รหัสของอีเมลที่เพิ่มไว้ข้างต้น

// กำหนด object ข้อมูล Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool statusAirFirebase;
bool statusAir = false;

#include "PinDefinitionsAndMore.h"  // กำหนดมาโครสำหรับขาขาเข้าและขาขาออก ฯลฯ.
#if !defined(IR_SEND_PIN)
#define IR_SEND_PIN 4               // กำหนดให้ขา 4 สำหรับส่งสัญญาณอินฟราเรด
#endif

/*
 * ระบุ DistanceWidthProtocol สำหรับการถอดรหัส สิ่งนี้ต้องทำก่อน #include <IRremote.hpp>
 */
#define DECODE_DISTANCE_WIDTH  // ตัวถอดรหัสสากลสำหรับโปรโตคอลความกว้างระยะของพัลส์
#if !defined(RAW_BUFFER_LENGTH)
#if RAMEND <= 0x4FF || RAMSIZE < 0x4FF
#define RAW_BUFFER_LENGTH 120
#elif RAMEND <= 0xAFF || RAMSIZE < 0xAFF  // 0xAFF สำหรับ LEONARDO
#define RAW_BUFFER_LENGTH 400             // 600 มากเกินไปที่นี่ เนื่องจากมี uint8_t rawCode[RAW_BUFFER_LENGTH]; เพิ่มเติม
#else
#define RAW_BUFFER_LENGTH 750
#endif
#endif

#include <IRremote.hpp>

#define DELAY_BETWEEN_REPEATS_MILLIS 70

// พื้นที่จัดเก็บสำหรับรหัสที่บันทึกไว้ ซึ่งได้ถูกเติมล่วงหน้าด้วยข้อมูล NEC
IRRawDataType sDecodedRawOpen[RAW_DATA_ARRAY_SIZE] = { 0x6000000521C };                        // command 0x6000000521C ซึ่งเป็นคำสั่งเปิดแอร์
IRRawDataType sDecodedRawClose[RAW_DATA_ARRAY_SIZE] = { 0x7000000520C };                       // command 0x7000000520C ซึ่งเป็นคำสั่งปิดแอร์
DistanceWidthTimingInfoStruct sDistanceWidthTimingInfo = { 9050, 4550, 600, 1650, 600, 500 };  // NEC timing
uint8_t sNumberOfBits = 44;

unsigned long repeatSendIR;

void setup() {
  Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) || defined(USBCON) || defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
  delay(4000); 
#endif
  IrSender.begin();  // Start with IR_SEND_PIN as send pin and enable feedback LED at default feedback LED pin
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   // เป็นฟังก์ชั่นเพื่อเตรียมการ WiFi Library และตั้งค่าเครือข่าย
  Serial.print("Connecting to Wi-Fi");    
  unsigned long ms = millis();

  // เชื่อมต่อกับ Wifi ให้สำเร็จก่อนถึงจะดำเนินขั้นตอนถัดไป 
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());        // แสดง IP address ของ esp32
  Serial.println();
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);    // แสดง version ของ Firebase Client

  /* กำหนดคีย์ API (จำเป็น) */
  config.api_key = API_KEY;

  /* กำหนดข้อมูลรับรองการลงชื่อเข้าใช้ของผู้ใช้ */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* กำหนด URL ของ real-time database (จำเป็น) */
  config.database_url = DATABASE_URL;

  /* กำหนดฟังก์ชัน callback สำหรับงานสร้างโทเค็นที่รันระยะยาว */
  config.token_status_callback = tokenStatusCallback;  // สามารถดูตัวอย่างได้ที่ addons/TokenHelper.h

  // Comment หรือส่งค่าเท็จเมื่อการเชื่อมต่อ WiFi ใหม่จะควบคุมโดย code หรือ third party library เช่น WiFiManager
  Firebase.reconnectNetwork(true);

  // ตั้งแต่เวอร์ชัน 4.4.x มีการใช้กลไก BearSSL จึงจำเป็นต้องตั้งค่าบัฟเฟอร์ SSL
  // การส่งข้อมูลขนาดใหญ่อาจต้องใช้บัฟเฟอร์ RX ที่ใหญ่กว่า ไม่เช่นนั้นปัญหาการเชื่อมต่อหรือการอ่านข้อมูลอาจ time out ได้
  fbdo.setBSSLBufferSize(4096 /* ขนาดบัฟเฟอร์ Rx เป็นไบต์ตั้งแต่ 512 - 16384 */, 1024 /* ขนาดบัฟเฟอร์ Tx เป็นไบต์ตั้งแต่ 512 - 16384 */);

  // จำกัดขนาดของ response payload ที่จะรวบรวมใน FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.setDoubleDigits(5);

  config.timeout.serverResponse = 10 * 1000;        // หมดเวลา response read ของเซิร์ฟเวอร์ในหน่วยมิลลิวินาที (1 วินาที - 1 นาที)
}

void loop() {
  // ตรวจสอบว่า Wifi ขาดการเชื่อมต่อหรือไม่
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
    // รับค่ามาจาก firebase real-time database
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
      // เมื่อแอร์ปิดอยู่แล้วข้อมูลที่รับมาเป็น True ให้ทำการส่งสัญญาณเปิดแอร์
      statusAir = statusAirFirebase;
      repeatSendIR = millis();
      while (millis() - repeatSendIR < 1000) {
        Serial.println();
        Serial.flush();  // เพื่อหลีกเลี่ยงการรบกวนการสร้าง PWM โดยซอฟต์แวร์จากการขัดจังหวะของสัญญาณออกแบบอนุกรม

        IrSender.sendPulseDistanceWidthFromArray(38, &sDistanceWidthTimingInfo, &sDecodedRawOpen[0], sNumberOfBits,
#if defined(USE_MSB_DECODING_FOR_DISTANCE_DECODER)
                                                 PROTOCOL_IS_MSB_FIRST
#else
                                                 PROTOCOL_IS_LSB_FIRST
#endif
                                                 ,
                                                 100, 0);

        delay(DELAY_BETWEEN_REPEATS_MILLIS);  // รอสักครู่ระหว่างการส่งข้อมูลซ้ำ
      }
    } else if (statusAir && statusAirFirebase != statusAir) {
      // เมื่อแอร์เปิดอยู่แล้วข้อมูลที่รับมาเป็น False ให้ทำการส่งสัญญาณปิดแอร์
      statusAir = statusAirFirebase;
      repeatSendIR = millis();
      while (millis() - repeatSendIR < 1000) {
        Serial.println();
        Serial.flush();  // เพื่อหลีกเลี่ยงการรบกวนการสร้าง PWM โดยซอฟต์แวร์จากการขัดจังหวะของสัญญาณออกแบบอนุกรม

        IrSender.sendPulseDistanceWidthFromArray(38, &sDistanceWidthTimingInfo, &sDecodedRawClose[0], sNumberOfBits,
#if defined(USE_MSB_DECODING_FOR_DISTANCE_DECODER)
                                                 PROTOCOL_IS_MSB_FIRST
#else
                                                 PROTOCOL_IS_LSB_FIRST
#endif
                                                 ,
                                                 100, 0);

        delay(DELAY_BETWEEN_REPEATS_MILLIS);  // รอสักครู่ระหว่างการส่งข้อมูลซ้ำ
      }
    }
  }
  delay(100);
}