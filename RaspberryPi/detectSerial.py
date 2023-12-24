from cv2 import VideoCapture
from ultralytics import YOLO
from time import sleep, time
from threading import Thread
from firebase_admin import credentials, db, initialize_app
from socket import setdefaulttimeout, socket, AF_INET, SOCK_STREAM
from requests import post
from datetime import datetime
import RPi.GPIO as gpio
from serial import Serial

gpio.setmode(gpio.BCM)
gpio.setup(16, gpio.OUT)

numberPeople = 0
statusAir = False
schedule = False
DefaultDataSchedule = [
    'monday-815',
    'monday-915',
    'monday-1015',
    'monday-1115',
    'monday-1315',
    'monday-1415',
    'monday-1515',
    'monday-1615',
    'monday-1715',
    'tuesday-815',
    'tuesday-915',
    'tuesday-1015',
    'tuesday-1115',
    'tuesday-1315',
    'tuesday-1415',
    'tuesday-1515',
    'tuesday-1615',
    'tuesday-1715',
    'wednesday-815',
    'wednesday-915',
    'wednesday-1015',
    'wednesday-1115',
    'wednesday-1315',
    'wednesday-1415',
    'wednesday-1515',
    'wednesday-1615',
    'wednesday-1715',
    'thursday-815',
    'thursday-915',
    'thursday-1015',
    'thursday-1115',
    'thursday-1315',
    'thursday-1415',
    'thursday-1515',
    'thursday-1615',
    'thursday-1715',
    'friday-815',
    'friday-915',
    'friday-1015',
    'friday-1115',
    'friday-1315',
    'friday-1415',
    'friday-1515',
    'friday-1615',
    'friday-1715']


def checkSchedule(hour, minute):
    cellId = ""
    if minute > 59:
        hour += 1
        minute -= 60
        if hour > 23:
            hour = 0
    # 8.15 - 9.15
    if ((hour == 8 and minute >= 15) or (hour == 9 and minute < 15)):
        cellId = '-815'
    # 9.15 - 10.15
    elif ((hour == 9 and minute >= 15) or (hour == 10 and minute < 15)):
        cellId = '-915'
    # 10.15 - 11.15
    elif ((hour == 10 and minute >= 15) or (hour == 11 and minute < 15)):
        cellId = '-1015'
    # 11.15 - 12.15
    elif ((hour == 11 and minute >= 15) or (hour == 12 and minute < 15)):
        cellId = '-1115'
    # 12.15 - 13.15
    elif ((hour == 12 and minute >= 15) or (hour == 13 and minute < 15)):
        cellId = 'break'
    # 13.15 - 14.15
    elif ((hour == 13 and minute >= 15) or (hour == 14 and minute < 15)):
        cellId = '-1315'
    # 14.15 - 15.15
    elif ((hour == 14 and minute >= 15) or (hour == 15 and minute < 15)):
        cellId = '-1415'
    # 15.15 - 16.15
    elif ((hour == 15 and minute >= 15) or (hour == 16 and minute < 15)):
        cellId = '-1515'
    # 16.15 - 17.15
    elif ((hour == 16 and minute >= 15) or (hour == 17 and minute < 15)):
        cellId = '-1615'
    # 17.15 - 18.15
    elif ((hour == 17 and minute >= 15) or (hour == 18 and minute < 15)):
        cellId = '-1715'
    # 18.15 - 19.15
    elif (hour == 18 and minute >= 15) or (hour == 19 and minute < 15):
        cellId = '-1815'

    if cellId == 'break':
        return cellId
    else:
        x = datetime.now().strftime("%A").lower()
        return x + cellId


def check_wifi_connection(host="8.8.8.8", port=53, timeout=3):
    try:
        # Use socket to attempt a connection
        setdefaulttimeout(timeout)
        socket(AF_INET, SOCK_STREAM).connect((host, port))
        return True
    except BaseException:
        return False


def task1():
    global numberPeople
    cap = VideoCapture(0)
    model = YOLO("yolov8n.pt")
    # startTime = time()

    while True:
        # if time() - startTime >= 5:
        _, frame = cap.read()
        results = model.predict(frame, conf=0.25, classes=[0])[0]
        resultsOutput = results.boxes.cls.to('cpu').numpy()
        # print("number of people : ", len(resultsOutput))
        numberPeople = len(resultsOutput)
        # startTime = time()
        sleep(5)


def task2():
    global statusAir

    ser = Serial('/dev/ttyACM0',9600, timeout=1)
    ser.flush()
    gpio.output(16, gpio.HIGH)
    sleep(1)
    gpio.output(16, gpio.LOW)
    sleep(1)
    gpio.cleanup()
    ser.write(b"off\n")
    sleep(3)

    peopleStatusPrevious = 0
    longDetectMillis = 0
    while True:
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
                    ser.write(b"off\n")
                    sleep(3)
                    break
                if numberPeople >= 3:
                    peopleStatusPrevious = 0
                    break
                sleep(1)
        elif numberPeople >= 3 and not (statusAir) and schedule:
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
                    ser.write(b"on\n")
                    sleep(3)
                    break
                if numberPeople <= 1 or schedule == False:
                    peopleStatusPrevious = 0
                    break
                sleep(1)

        # เมื่อสถานะเครื่องปรับอากาศเปิดอยู่แล้วหมดคาบเรียนให้เปลียนสถานะเป็นปิด
        if statusAir and schedule == False:
            statusAir = False
            ser.write(b"off\n")
            sleep(3)
        sleep(1)


def task3():
    global schedule
    updateSchedule = False
    defaultStatusAir = False
    # Fetch the service account key JSON file contents
    cred = credentials.Certificate(
        '/home/aleef/Desktop/Project/credentials.json')
    # Initialize the app with a service account, granting admin privileges
    initialize_app(
        cred, {
            'databaseURL': "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app/"})
    url = 'https://notify-api.line.me/api/notify'
    token = 'K7TYEiwnKtOscbCjaeyR4Z92iobzVcz7VAYLuVKaZLE'
    headers = {
        'content-type': 'application/x-www-form-urlencoded',
        'Authorization': 'Bearer ' + token}
    if check_wifi_connection():
        DataSchedule = db.reference('/selectedCells/').get()
    else:
        DataSchedule = DefaultDataSchedule
        updateSchedule = True
    # startTime = time()
    while True:
        # if time() - startTime >= 10:
        # startTime = time()
        if check_wifi_connection():
            if numberPeople >= 3:
                db.reference('Human').child('Detection').set(True)
            elif numberPeople <= 1:
                db.reference('Human').child('Detection').set(False)
            db.reference('Human').child('Number').set(int(numberPeople))
            db.reference('DeviceStatus').child('AirConditioner').set(statusAir)
            editSchedule = db.reference('/EditTable').child('Status').get()
            if editSchedule or updateSchedule:
                # ดึงข้อมูลทุกชนิดจาก root
                DataSchedule = db.reference('/selectedCells/').get()
                db.reference('/EditTable').child('Status').set(False)
                updateSchedule = False
            if defaultStatusAir != statusAir:
                if statusAir:
                    post(
                        url, headers=headers, data={
                            'message': 'แอร์กำลังทำงาน'})
                else:
                    post(
                        url, headers=headers, data={
                            'message': 'แอร์หยุดทำงานแล้ว'})
                defaultStatusAir = statusAir
        else:
            if not updateSchedule:
                DataSchedule = DefaultDataSchedule
                updateSchedule = True

        hour = datetime.now().hour
        minute = datetime.now().minute
        id = checkSchedule(hour, minute)
        idAfter = checkSchedule(hour, minute + 15)

        if id in DataSchedule and idAfter in DataSchedule:
            schedule = True
        else:
            schedule = False
        sleep(3)


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
