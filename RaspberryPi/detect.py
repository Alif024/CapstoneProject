from cv2 import VideoCapture
from ultralytics import YOLO
from time import time, sleep, ctime
from datetime import datetime
from threading import Thread
import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import socket

numberPeople = 0
statusAir = False
schedule = False

# เป็นฟังก์ชันการตรวจสอบว่า RPI มีการเชื่อมต่อกันอินเทอร์เน็ตหรือไม่
def check_wifi_connection(host="8.8.8.8", port=53, timeout=3):
    try:
        # ใช้ซ็อกเก็ตเพื่อพยายามเชื่อมต่อ
        socket.setdefaulttimeout(timeout)
        socket.socket(socket.AF_INET, socket.SOCK_STREAM).connect((host, port))
        return True
    except:
        return False

def task1():
    global numberPeople         
    cap = VideoCapture(0)               # ใช้เพื่อเริ่มการจับภาพวิดีโอจากกล้องแรกที่เชื่อมต่อกับคอมพิวเตอร์
    model = YOLO("yolov8n.pt")          # กำหนด model ที่ใช้
    startTime = time()
    
    while True:
        # ทำการ Process ภาพจากกล้อง Webcam ทุกๆ 3 วินาที
        if time() - startTime >= 3:
            ret, frame = cap.read()     # อ่านภาพจากวิดีโอ
            results = model.predict(frame, conf=0.25, classes=[0])[0]   # ตังค่าว่าให้ detect อะไร หากเป็น classes=[0] คือ detect แค่คน ส่วน conf=0.25 ค่าความแม่นยำอยู่ที่ 25%
            resultsOutput = results.boxes.cls.to('cpu').numpy()         # เพื่อย้ายข้อมูลเกี่ยวกับประเภทของวัตถุที่ถูกตรวจจับจาก GPU กลับไปยัง CPU และแปลงเป็นรูปแบบ array ของ NumPy
            numberPeople = len(resultsOutput)                           # นับจำนวนสมาชิกใน resultsOutput ซึ่งจะมีค่าเท่ากับจำนวนคนที่ detect เจอ
            startTime = time()
            
def task2():
    global statusAir
    global schedule
    peopleStatusPrevious = 0
    while True:
        if numberPeople <= 1 and statusAir:
            # เมื่อไม่เจอคน
            while True:
                # คนไม่อยู่นานตามเวลาไหม
                detectMillis = time()
                if peopleStatusPrevious == 0:
                    longDetectMillis = detectMillis
                    peopleStatusPrevious = 1
                detectDurations = detectMillis - longDetectMillis       # นับเวลาว่าคนไม่อยู่ในห้องนานเป็นเวลากี่วินาทีแล้ว

                # คนไม่อยู่นานตามเวลา >= 9 วินาที ให้ statusAir = False
                if numberPeople <= 1 and peopleStatusPrevious == 1 and detectDurations >= 9:
                    statusAir = False
                    peopleStatusPrevious = 0
                    break
                
                # หากคนอยู่ให้ออกจากลูปนี้ แล้วนับเวลาใหม่
                if numberPeople >= 3:
                    peopleStatusPrevious = 0
                    break

        elif numberPeople >= 3 and not(statusAir) and schedule:
            # เมื่อเจอคน
            while True:
                # คนอยู่นานตามเวลาไหม
                detectMillis = time()
                if peopleStatusPrevious == 0:
                    longDetectMillis = detectMillis
                    peopleStatusPrevious = 1
                detectDurations = detectMillis - longDetectMillis       # นับเวลาว่าคนอยู่ในห้องนานเป็นเวลากี่วินาทีแล้ว

                # คนอยู่นานตามเวลา >= 9 วินาที ให้ statusAir = True
                if numberPeople >= 3 and peopleStatusPrevious == 1 and detectDurations >= 9:
                    statusAir = True
                    peopleStatusPrevious = 0
                    break

                # หากคนไม่อยู่ให้ออกจากลูปนี้ แล้วนับเวลาใหม่
                if numberPeople <= 1 or schedule == False:
                    peopleStatusPrevious = 0
                    break

        # เมื่อสถานะเครื่องปรับอากาศเปิดอยู่แล้วหมดคาบเรียนให้เปลียนสถานะเป็นปิด
        if statusAir and schedule == False:
            statusAir = False

def task3():
    global schedule
    cred = credentials.Certificate('RaspberryPi\credentials.json')      # ดึงข้อมูลเนื้อหาไฟล์ JSON ของคีย์บัญชีบริการ

    # ตรวจสอบว่ามีการเชื่อมต่อกับอิเทอร์เน็ตไหม
    while not(check_wifi_connection()):
        sleep(3)

    # เริ่มต้นแอปพลิเคชันด้วยบัญชีบริการ โดยมอบสิทธิ์ผู้ดูแลระบบ
    firebase_admin.initialize_app(cred, {
        'databaseURL': "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/"
    })
    startTime = time()
    while True:
        # อัพข้อมูลลง database ทุกๆ 5 วินาที
        if time() - startTime >= 5:
            if check_wifi_connection():
                if numberPeople >= 3:
                    db.reference('Human').child('Detection').set(True)
                elif numberPeople <= 1:
                    db.reference('Human').child('Detection').set(False)
                db.reference('Human').child('Number').set(int(numberPeople))
                db.reference('DeviceStatus').child('AirConditioner').set(statusAir)
            startTime = time()

        day = ctime().split()[0]            # รับค่าเฉพาะชื่อของวัน
        hour = datetime.now().hour          # รับค่าชั่วโมงในปัจจุบัน
        minute = datetime.now().minute      # รับค่านาทีในปัจจุบัน
        
        # คาบของวันจันทร์
        if day == 'Mon' and ((hour == 8 and minute >= 15 and minute <= 59) or ((hour == 9 or hour == 10) and minute >= 0 and minute <= 59) or (hour == 11 and minute >= 0 and minute <= 15)):
            schedule = True
        elif day == 'Mon' and ((hour == 13 and minute >= 15 and minute <= 59) or (hour >= 14 and hour <= 16 and minute >= 0 and minute <= 59) or (hour == 17 and minute >= 0 and minute <= 15)):
            schedule = True

        # คาบของวันอังคาร
        elif day == 'Tue' and ((hour == 8 and minute >= 15 and minute <= 59) or (hour >= 9 and hour <= 11 and minute >= 0 and minute <= 59) or (hour == 12 and minute >= 0 and minute <= 15)):
            schedule = True
        elif day == 'Tue' and ((hour == 13 and minute >= 15 and minute <= 59) or (hour >= 14 and hour <= 17 and minute >= 0 and minute <= 59) or (hour == 18 and minute >= 0 and minute <= 15)):
            schedule = True

        # คาบของวันพุธ
        elif day == 'Wed' and ((hour == 9 and minute >= 15 and minute <= 59) or ((hour == 10 or hour == 11) and minute >= 0 and minute <= 59) or (hour == 12 and minute >= 0 and minute <= 15)):
            schedule = True
        elif day == 'Wed' and ((hour == 14 and minute >= 15 and minute <= 59) or (hour >= 15 and hour <= 17 and minute >= 0 and minute <= 59) or (hour == 18 and minute >= 0 and minute <= 15)):
            schedule = True

        # คาบของพฤหัสบดี
        elif day == 'Thu' and ((hour == 9 and minute >= 15 and minute <= 59) or ((hour == 10 or hour == 11) and minute >= 0 and minute <= 59) or (hour == 12 and minute >= 0 and minute <= 15)):
            schedule = True
        elif day == 'Thu' and ((hour == 13 and minute >= 15 and minute <= 59) or (hour >= 14 and hour <= 16 and minute >= 0 and minute <= 59) or (hour == 17 and minute >= 0 and minute <= 15)):
            schedule = True

        # คาบของศุกร์
        elif day == 'Fri' and ((hour == 8 and minute >= 15 and minute <= 59) or ((hour == 9 or hour == 10) and minute >= 0 and minute <= 59) or (hour == 11 and minute >= 0 and minute <= 15)):
            schedule = True
        elif day == 'Fri' and ((hour == 13 and minute >= 15 and minute <= 59) or (hour >= 14 and hour <= 16 and minute >= 0 and minute <= 59) or (hour == 17 and minute >= 0 and minute <= 15)):
            schedule = True
        
        # นอกจากวันจันทร์ถึงวันศุกร์ให้เป็นไม่มีคาบ
        else:
            schedule = False
        
if __name__ == "__main__":
    Thread(target=task1).start()      # เริ่มต้นเธรด 1
    Thread(target=task2).start()      # เริ่มต้นเธรด 2
    Thread(target=task3).start()      # เริ่มต้นเธรด 3