from cv2 import VideoCapture
from ultralytics import YOLO
from time import time, sleep
from threading import Thread
import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import socket

numberPeople = 0
statusAir = False
schedule = True

def check_wifi_connection(host="8.8.8.8", port=53, timeout=3):
    try:
        # Use socket to attempt a connection
        socket.setdefaulttimeout(timeout)
        socket.socket(socket.AF_INET, socket.SOCK_STREAM).connect((host, port))
        return True
    except:
        return False

def task1():
    global numberPeople
    cap = VideoCapture(0)
    model = YOLO("yolov8n.pt")
    startTime = time()
    
    while True:
        if time() - startTime >= 3:
            ret, frame = cap.read()
            results = model.predict(frame, conf=0.25, classes=[0])[0]
            resultsOutput = results.boxes.cls.to('cpu').numpy()
            # print("number of people : ", len(resultsOutput))
            numberPeople = len(resultsOutput)
            startTime = time()
            
def task2():
    global statusAir
    global schedule
    peopleStatusPrevious = 0
    while True:
        #int schedule = digitalRead(schedulePin);
        # ตรวจคน 
        if numberPeople <= 1 and statusAir:
            # เมื่อไม่เจอคน
            while True:
                # คนไม่อยู่นานตามเวลาไหม
                detectMillis = time()
                if peopleStatusPrevious == 0:
                    longDetectMillis = detectMillis
                    peopleStatusPrevious = 1
                detectDurations = detectMillis - longDetectMillis
                if numberPeople <= 1 and peopleStatusPrevious == 1 and detectDurations >= 9:
                    statusAir = False
                    peopleStatusPrevious = 0
                    break

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
                detectDurations = detectMillis - longDetectMillis
                if numberPeople >= 3 and peopleStatusPrevious == 1 and detectDurations >= 9:
                    statusAir = True
                    peopleStatusPrevious = 0
                    break
                if numberPeople <= 1 or schedule == False:
                    peopleStatusPrevious = 0
                    break

        # เมื่อสถานะเครื่องปรับอากาศเปิดอยู่แล้วหมดคาบเรียนให้เปลียนสถานะเป็นปิด
        if statusAir and schedule == False:
            statusAir = False

def task3():
    # Fetch the service account key JSON file contents
    cred = credentials.Certificate('credentials.json')
    while not(check_wifi_connection()):
        sleep(3)
    # Initialize the app with a service account, granting admin privileges
    firebase_admin.initialize_app(cred, {
        'databaseURL': "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/"
    })
    startTime = time()
    while True:
        if time() - startTime >= 5:
            if check_wifi_connection():
                if numberPeople >= 3:
                    db.reference('Human').child('Detection').set(True)
                elif numberPeople <= 1:
                    db.reference('Human').child('Detection').set(False)
                db.reference('Human').child('Number').set(int(numberPeople))
                db.reference('DeviceStatus').child('AirConditioner').set(statusAir)
            startTime = time()
        
if __name__ == "__main__":
    t1 = Thread(target=task1)
    t2 = Thread(target=task2)
    t3 = Thread(target=task3)
    # starting thread 1
    t1.start()
    # starting thread 2
    t2.start()
    # starting thread 2
    t3.start()
