#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ระบุข้อมูลกระบวนการสร้างโทเค็น.
#include <addons/TokenHelper.h>

// ระบุข้อมูลการพิมพ์ real-time database payload และฟังก์ชันตัวช่วยอื่นๆ.
#include <addons/RTDBHelper.h>

/* 1. กำหนดข้อมูลของการรับรองความถูกต้องและการเข้ารหัสที่ router ใช้ */
#define WIFI_SSID "COC123"          // ชื่อ WIFI
#define WIFI_PASSWORD "ooooo123"    // รหัส WIFI
// สำหรับข้อมูลประจำตัวต่อไปนี้ สามารถดูตัวอย่างได้ที่ examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. กำหนดคีย์ API */
#define API_KEY "AIzaSyCFcMdUr5eVTGJnEZfWD1YjRYXrV0Tyg_Y"

/* 3. กำหนด URL ของ real-time database */
#define DATABASE_URL "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. กำหนดอีเมลและรหัสผ่านผู้ใช้ที่ลงทะเบียนหรือเพิ่มใน project แล้ว */
#define USER_EMAIL "aleefrock12345@gmail.com"   // อีเมลที่เพิ่มใน Authentication บนแพลตฟอร์ม firebase
#define USER_PASSWORD "!12345"                  // รหัสของอีเมลที่เพิ่มไว้ข้างต้น

// กำหนด object ข้อมูล Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;

// กำหนดขา GPIO 
#define peoplePin 13
#define schedulePin 25

// กำหนดตัวแปร global
unsigned long detectDurations;
unsigned long longDetectMillis;
int peopleStatusPrevious = LOW;
unsigned long detectMillis;
bool statusAirFirebase;
bool statusAir = false;
int people;

void setup() {
  Serial.begin(115200);                     // การกําหนดอัตราความเร็วการสื่อสารพอร์ตอนุกรมด้วยความเร็ว 115200 บิตต่อวินาที (bps)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);     // เป็นฟังก์ชั่นเพื่อเตรียมการ WiFi Library และตั้งค่าเครือข่าย
  Serial.print("Connecting to Wi-Fi");

  // เชื่อมต่อกับ Wifi ให้สำเร็จก่อนถึงจะดำเนินขั้นตอนถัดไป  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());           // แสดง IP address ของ esp32
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
  config.timeout.serverResponse = 10 * 1000;    // หมดเวลา response read ของเซิร์ฟเวอร์ในหน่วยมิลลิวินาที (1 วินาที - 1 นาที)

  // ควรเรียก Firebase.ready() ซ้ำๆ เพื่อจัดการงานการตรวจสอบสิทธิ์
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0 /* Update ข้อมูลลง firebase real-time database ทุกๆ 15 วินาที */)) {
    sendDataPrevMillis = millis();

    // Update สถานะของเครื่องปรับอากาศ
    if (Firebase.RTDB.setBool(&fbdo, F("/DeviceStatus/AirConditioner"), statusAir)) {
      Serial.print("Update status success: ");
      Serial.println(statusAir);
    } else {
      Serial.println(fbdo.errorReason().c_str());
    }
  }

  // กำหนดการทำงานของ GPIO
  pinMode(peoplePin, INPUT_PULLUP);
  pinMode(schedulePin, INPUT_PULLUP);
}

// เป็นฟังก์ชันการ Update ข้อมูลลงใน firebase real-time database โดยจะทำงานทุกๆ 15 วินาที
void UpdateData() {
  if (WiFi.status() != WL_CONNECTED   /* ตรวจสอบว่า Wifi ขาดการเชื่อมต่อหรือไม่ */) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();   // ทำการเชื่อมต่อ Wifi ใหม่เมื่อขาดการเชื่อมต่อ
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println("Connected to WiFi");
    Firebase.reconnectWiFi(true);
  } else {
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0) /* Update ข้อมูลลง firebase real-time database ทุกๆ 15 วินาที */) {
      sendDataPrevMillis = millis();

      // Update สถานะของเครื่องปรับอากาศ
      if (Firebase.RTDB.getBool(&fbdo, F("/DeviceStatus/AirConditioner"), &statusAirFirebase)) {
        if (statusAirFirebase != statusAir) {
          if (Firebase.RTDB.setBool(&fbdo, F("/DeviceStatus/AirConditioner"), statusAir)) {
            Serial.print("Update status success: ");
            Serial.println(statusAir);
          } else {
            Serial.println(fbdo.errorReason().c_str());
          }
        }
      } else {
        Serial.println(fbdo.errorReason().c_str());
      }

      // Update สถานะการตรวจจับผู้คน
      if (Firebase.RTDB.setBool(&fbdo, F("/Human/Detection"), !people)) {
        Serial.print("Detection : ");
        Serial.println(!people);
      } else {
        Serial.println(fbdo.errorReason().c_str());
      }
    }
  }
}

void loop() {
  /* ตรวจคาบ */
  int schedule = digitalRead(schedulePin);

  /* ตรวจคน */
  people = digitalRead(peoplePin);

  if (people == 1 && statusAir) {
    /* เมื่อไม่เจอคน */
    while (true) {
      /* คนไม่อยู่นานตามเวลาไหม */
      detectMillis = millis();
      if (peopleStatusPrevious == LOW) {
        longDetectMillis = detectMillis;
        peopleStatusPrevious = HIGH;
      }
      detectDurations = detectMillis - longDetectMillis;
      if (people == 1 && peopleStatusPrevious == HIGH && detectDurations >= 60 * 1000) {
        statusAir = false;
        peopleStatusPrevious = LOW;
        break;
      }
      /* ตรวจคน */
      people = digitalRead(peoplePin);
      if (people == 0) {
        peopleStatusPrevious = LOW;
        break;
      }
      UpdateData();   // Update ข้อมูลลงใน firebase real-time database
    }
  } else if (people == 0 && !statusAir && schedule == LOW) {
    /* เมื่อเจอคน */
    while (true) {
      /* คนอยู่นานตามเวลาไหม */
      detectMillis = millis();
      if (peopleStatusPrevious == LOW) {
        longDetectMillis = detectMillis;
        peopleStatusPrevious = HIGH;
      }
      detectDurations = detectMillis - longDetectMillis;
      if (people == 0 && peopleStatusPrevious == HIGH && detectDurations >= 60 * 1000) {
        statusAir = true;
        peopleStatusPrevious = LOW;
        break;
      }
      int schedule = digitalRead(schedulePin);
      /* ตรวจคน */
      people = digitalRead(peoplePin);
      if (people == 1 || schedule == HIGH) {
        peopleStatusPrevious = LOW;
        break;
      }
      UpdateData();   // Update ข้อมูลลงใน firebase real-time database
    }
  }

  // เมื่อสถานะเครื่องปรับอากาศเปิดอยู่แล้วหมดคาบเรียนให้เปลียนสถานะเป็นปิด
  if (statusAir && schedule == HIGH) {
    statusAir = false;
  }

  UpdateData();   // Update ข้อมูลลงใน firebase real-time database
}
