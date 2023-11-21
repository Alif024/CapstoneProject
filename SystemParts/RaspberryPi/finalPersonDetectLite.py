from cv2 import VideoCapture
from ultralytics import YOLO
from time import time, ctime, sleep
from threading import Thread
from datetime import datetime
import RPi.GPIO as GPIO

detectPin = 17
schedulePin = 27
finishSchedulePin = 22
schedulePinTest = 16
finishSchedulePinTest = 26
breakPin = 23

GPIO.setmode(GPIO.BCM)
GPIO.setup(detectPin, GPIO.OUT)
GPIO.setup(schedulePin, GPIO.OUT)
GPIO.setup(finishSchedulePin, GPIO.OUT)
GPIO.setup(breakPin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(schedulePinTest, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(finishSchedulePinTest, GPIO.IN, pull_up_down=GPIO.PUD_UP)

def task1():
    cap = VideoCapture(0)
    model = YOLO("yolov8n.pt")
    startTime = time()
    
    while True:
        ret, frame = cap.read()
        results = model.predict(frame, conf=0.25, classes=[0])[0]
        if time() - startTime >= 3:
            resultsOutput = results.boxes.cls.to('cpu').numpy()
            print("number of person : ", len(resultsOutput))
            if len(resultsOutput) >= 3:
                #print('fine')
                GPIO.output(detectPin, 1)
            else:
                #print('not fine')
                GPIO.output(detectPin, 0) 
            startTime = time()
            
def task2():
    while GPIO.input(breakPin) == 1:
        day = ctime().split()[0]
        hour = datetime.now().hour
        minute = datetime.now().minute
        if day == 'Wed' and hour >= 13 and minute >= 15 and minute <= 20:
            #print("Open air")
            GPIO.output(schedulePin, 1)
        if day == 'Wed' and hour >= 17 and minute >= 15:
            GPIO.output(finishSchedulePin, 1)
        if GPIO.input(schedulePinTest) == 0:
            GPIO.output(schedulePin, 1)
            print('test schedule')
        else:
            GPIO.output(schedulePin, 0)
        if GPIO.input(finishSchedulePinTest) == 0:
            GPIO.output(finishSchedulePin, 1)
            print('test finish schedule')
        else:
            GPIO.output(finishSchedulePin, 0)
        sleep(3)
        
if __name__ == "__main__":
    t1 = Thread(target=task1)
    t2 = Thread(target=task2)
    # starting thread 1
    t1.start()
    # starting thread 2
    t2.start()
    # wait until thread 1 is completely executed
    # t1.join()
    # wait until thread 2 is completely executed
    # t2.join()
