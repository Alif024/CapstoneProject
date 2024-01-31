import { initializeApp } from "https://www.gstatic.com/firebasejs/10.7.0/firebase-app.js";
import { getDatabase, ref, child, get, set, update } from "https://www.gstatic.com/firebasejs/10.7.0/firebase-database.js";

// Your web app's Firebase configuration
const firebaseConfig = {
    apiKey: "AIzaSyCFcMdUr5eVTGJnEZfWD1YjRYXrV0Tyg_Y",
    authDomain: "esp32-aircontroller.firebaseapp.com",
    databaseURL: "https://esp32-aircontroller-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "esp32-aircontroller",
    storageBucket: "esp32-aircontroller.appspot.com",
    messagingSenderId: "257570449313",
    appId: "1:257570449313:web:62ba93010e1188df048de0"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);

var cellId_before = 'start';
var cell_before = document.getElementById(cellId_before);

function clearCellBefore(cellId_before, cellId) {
    if (cellId_before !== cellId) {
        if (cell_before !== null) {
            if (cell_before.classList[0] === 'selected') {
                cell_before.style.backgroundColor = 'lightblue';
                cell_before.textContent = '';
            } else {
                cell_before.style.removeProperty('background-color');
                cell_before.textContent = '';
            }
        }
        cellId_before = cellId;
        cell_before = document.getElementById(cellId_before);
    }
}

function highlightCurrentTimeSlot() {
    var now = new Date();
    var day = now.getDay(); // วันในสัปดาห์ (0 = วันอาทิตย์, 1 = วันจันทร์, ...)
    //var day = 5; // วันในสัปดาห์ (0 = วันอาทิตย์, 1 = วันจันทร์, ...)
    var hour = now.getHours();
    //var hour = 16;       // ทดสอบ
    var minute = now.getMinutes();

    var dayMap = ['sunday', 'monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday'];

    /* 8.15 - 9.15 */
    if ((hour == 8 && minute >= 15) || (hour == 9 && minute < 15)) {
        var cellId = dayMap[day] + '-815';
        if (cellId_before !== cellId) {
            cellId_before = cellId;
            cell_before = document.getElementById(cellId_before);
        }
    }

    /* 9.15 - 10.15 */
    else if ((hour == 9 && minute >= 15) || (hour == 10 && minute < 15)) {
        var cellId = dayMap[day] + '-915';
        clearCellBefore(cellId_before, cellId);
    }

    /* 10.15 - 11.15 */
    else if ((hour == 10 && minute >= 15) || (hour == 11 && minute < 15)) {
        var cellId = dayMap[day] + '-1015';
        clearCellBefore(cellId_before, cellId);
    }

    /* 11.15 - 12.15 */
    else if ((hour == 11 && minute >= 15) || (hour == 12 && minute < 15)) {
        var cellId = dayMap[day] + '-1115';
        clearCellBefore(cellId_before, cellId);
    }

    /* 12.15 - 13.15 */
    else if ((hour == 12 && minute >= 15) || (hour == 13 && minute < 15)) {
        var cellId = 'break';
        clearCellBefore(cellId_before, cellId);
    }

    /* 13.15 - 14.15 */
    else if ((hour == 13 && minute >= 15) || (hour == 14 && minute < 15)) {
        var cellId = dayMap[day] + '-1315';
        clearCellBefore(cellId_before, cellId);
    }

    /* 14.15 - 15.15 */
    else if ((hour == 14 && minute >= 15) || (hour == 15 && minute < 15)) {
        var cellId = dayMap[day] + '-1415';
        clearCellBefore(cellId_before, cellId);
    }

    /* 15.15 - 16.15 */
    else if ((hour == 15 && minute >= 15) || (hour == 16 && minute < 15)) {
        var cellId = dayMap[day] + '-1515';
        clearCellBefore(cellId_before, cellId);
    }

    /* 16.15 - 17.15 */
    else if ((hour == 16 && minute >= 15) || (hour == 17 && minute < 15)) {
        var cellId = dayMap[day] + '-1615';
        clearCellBefore(cellId_before, cellId);
    }

    /* 17.15 - 18.15 */
    else if ((hour == 17 && minute >= 15) || (hour == 18 && minute < 15)) {
        var cellId = dayMap[day] + '-1715';
        clearCellBefore(cellId_before, cellId);
    }

    /* 18.15 - 19.15 */
    else if ((hour == 18 && minute >= 15) || (hour == 19 && minute < 15)) {
        var cellId = dayMap[day] + '-1815';
        clearCellBefore(cellId_before, cellId);

    } else {
        var cellId = dayMap[day] + 'end';
        clearCellBefore(cellId_before, cellId);
    }

    var cell = document.getElementById(cellId);

    if (cell) {
        if (cell.classList[0] === 'selected') {
            cell.style.backgroundColor = 'lightgreen';
            cell.textContent = 'Now';

        } else {
            cell.style.backgroundColor = 'lightcoral';
            cell.textContent = 'Now';

        }
    }
}

let isEditing = false;
const correctPassword = "1234"; // ตั้งรหัสที่ต้องการ

const toggleClickEvent = function (e) {
    const target = e.target;
    if (target.tagName === 'TD' && !target.classList.contains('day-column') && !target.classList.contains('footer-schedule-column')) {
        target.classList.toggle('selected');
    }
};

function saveSelectedCells() {
    const selectedCells = document.querySelectorAll('.selected');
    const selectedIds = Array.from(selectedCells).map(cell => cell.id);

    // บันทึกข้อมูลไปยัง Firebase
    var db = getDatabase(app);
    var dbRef = ref(db, 'selectedCells');
    set(dbRef, selectedIds)
        .then(() => {
            console.log("Data saved successfully.");
        })
        .catch((error) => {
            console.error("Failed to save data: ", error);
        });

    // Update ข้อมูลลง database ดังนี้
    const toggleButton = document.getElementById('toggleButton');
    if (toggleButton.textContent != 'Save') {
        var db = getDatabase(app);
        update(ref(db, 'EditTable'), {
            Status: true
        });
    }
}

function toggleEditMode() {
    const toggleButton = document.getElementById('toggleButton');
    if (!isEditing) {
        const enteredPassword = prompt("กรุณากรอกรหัส:");
        if (enteredPassword === correctPassword) {
            isEditing = true;
            const table = document.querySelector('table');
            table.addEventListener('click', toggleClickEvent);
            toggleButton.textContent = 'Save';
            console.log("เข้าสู่โหมดแก้ไข");
        } else {
            alert("รหัสผิด");
        }
    } else {
        const confirmSave = confirm("คุณต้องการบันทึกและปิดการแก้ไขหรือไม่?");
        if (confirmSave) {
            isEditing = false;
            const table = document.querySelector('table');
            table.removeEventListener('click', toggleClickEvent);
            toggleButton.textContent = 'Edit';
            console.log("การแก้ไขถูกบันทึกและถูกปิดใช้งาน");
            saveSelectedCells();
            location.reload();
        } else {
            console.log("ยังคงอยู่ในโหมดแก้ไข");
        }
    }
}

function restoreSelectedCells(id) {
    return new Promise((resolve, reject) => {
        var dbRef = ref(getDatabase(app));
        get(child(dbRef, `selectedCells/${id}`)).then((snapshot) => {
            if (snapshot.exists()) {
                resolve(snapshot.val());
            } else {
                resolve("No data available");
            }
        }).catch((error) => {
            reject(error);
        });
    });
}

async function checkDataAndBreak() {
    var id = 0;
    while (true) {
        try {
            const data = await restoreSelectedCells(id);
            if (data === "No data available") {
                break;
            }
            document.getElementById(data).classList.toggle('selected');
            id = id + 1;
        } catch (error) {
            console.error(error);
            break; // หรืออาจจะใช้ continue แทน ขึ้นอยู่กับว่าคุณต้องการจัดการกับข้อผิดพลาดอย่างไร
        }
    }
}

function updateStatusFromFirebase() {
    get(child(ref(getDatabase(app)), "/DeviceStatus")).then((snapshot) => {
        if (snapshot.exists()) {
            const DeviceStatus = snapshot.val();

            const statusElement = document.getElementById(`AirConditioner`);
            if (statusElement) {
                statusElement.innerHTML = DeviceStatus.AirConditioner ? '<b style="color: #4F7934;">ON</b>' : '<b style="color: #DD4400;">OFF</b>';
            }
        } else {
            console.log("No data available");
        }
    }).catch((error) => {
        console.error(error);
    });

    get(child(ref(getDatabase(app)), "/Human/Number")).then((snapshot) => {
        if (snapshot.exists()) {
            const DetectionNumber = snapshot.val();
            const numberElement = document.getElementById(`Human`);
            if (numberElement) {
                numberElement.innerHTML = '<b style="color: black;">~ ' + DetectionNumber + ' คน</b>';
            }
        } else {
            console.log("No data available");
        }
    }).catch((error) => {
        console.error(error);
    });

    get(child(ref(getDatabase(app)), "/DeviceStatus")).then((snapshot) => {
        if (snapshot.exists()) {
            const DeviceStatus = snapshot.val();
            const statusElement = document.getElementById(`RaspberryPi`);
            if (statusElement) {
                statusElement.innerHTML = DeviceStatus.RaspberryPi ? '<b style="color: #4F7934;">ON</b>' : '<b style="color: #DD4400;">OFF</b>';
            }
        } else {
            console.log("No data available");
        }
    }).catch((error) => {
        console.error(error);
    });
}

highlightCurrentTimeSlot();
updateStatusFromFirebase();
setInterval(highlightCurrentTimeSlot, 500);
setInterval(updateStatusFromFirebase, 15000);

window.onresize = updateDayNames;

document.getElementById('toggleButton').addEventListener('click', toggleEditMode);
// โหลดตอนเริ่มต้น
window.onload = function () {
    checkDataAndBreak();
    init(); // หรือฟังก์ชันเริ่มต้นอื่นๆ
};