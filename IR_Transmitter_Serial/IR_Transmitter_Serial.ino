#include <Arduino.h>

#include "PinDefinitionsAndMore.h"  // กำหนดมาโครสำหรับขาขาเข้าและขาขาออก ฯลฯ.
#define IR_SEND_PIN 26  // กำหนดให้ขา 4 สำหรับส่งสัญญาณอินฟราเรด

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
String command;

void setup() {
  Serial.begin(9600);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) || defined(USBCON) || defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
  delay(4000);
#endif
  IrSender.begin();
}

void loop() {
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equals("on")) {
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
    } else if (command.equals("off")) {
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
    } else {
      Serial.println("bad command");
    }
  }
}