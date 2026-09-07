#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <windows.h>
#include <string>
#include <cmath>
#include "sqlite3.h"
#include "database.h"

void analyzeEyeBehavior(
    double distance,
    double pitch,
    double roll,
    double yaw,
    int continuousSeconds
)
{
    std::cout << "\n===== Behavior Analysis =====\n";

    bool normal = true;

    if (distance > 0 && distance < 40.0)
    {
        std::cout << "[WARNING] Viewing distance too close.\n";
        normal = false;
    }

    if (pitch < -15.0)
    {
        std::cout << "[WARNING] Head-down posture detected.\n";
        normal = false;
    }

    if (std::abs(roll) > 10.0)
    {
        std::cout << "[WARNING] Side-head posture detected.\n";
        normal = false;
    }

    if (std::abs(yaw) > 15.0)
    {
        std::cout << "[WARNING] Large horizontal head deviation detected.\n";
        normal = false;
    }

    if (continuousSeconds >= 20 * 60)
    {
        std::cout << "[REMINDER] Continuous viewing reached 20 minutes.\n";
        normal = false;
    }

    if (normal)
    {
        std::cout << "[OK] Current posture is normal.\n";
    }

    std::cout << "=============================\n";
}
static std::string getCurrentTime()
{
    std::time_t now = std::time(nullptr);

    std::tm* t = std::localtime(&now);

    if (t == nullptr)
    {
        return "UNKNOWN_TIME";
    }

    char buffer[32];

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        t
    );

    return buffer;
}

static std::string getEyeUsePeriod()
{
    std::time_t now = std::time(nullptr);

    std::tm* t = std::localtime(&now);

    if (t == nullptr)
    {
        return "UNKNOWN";
    }

    int hour = t->tm_hour;

    if (hour >= 23 || hour < 6)
    {
        return "LATE_NIGHT";
    }

    if (hour >= 18)
    {
        return "EVENING";
    }

    return "DAY";
}

static double calculateGazeAngleDeg(
    double gazeX,
    double gazeY,
    double centerX,
    double centerY,
    double pixelPitchMm,
    double distanceCm
)
{
    double dxMm = (gazeX - centerX) * pixelPitchMm;
    double dyMm = (gazeY - centerY) * pixelPitchMm;

    double offsetMm = std::sqrt(dxMm * dxMm + dyMm * dyMm);
    double distanceMm = distanceCm * 10.0;

    if (distanceMm <= 0.0)
        return 0.0;

    const double PI = 3.14159265358979323846;

    return std::atan2(offsetMm, distanceMm)
           * 180.0 / PI;
}

static void printRecord(const EyeRecord& r)
{
    std::cout << "\n----------------------------------\n";
    std::cout << "ID: " << r.id << '\n';
    std::cout << "Time: " << r.timestamp << '\n';

    std::cout << "Eye side: " << r.eyeSide << '\n';
    std::cout << "Eye image: " << r.eyeImagePath << '\n';
    std::cout << "Face image: " << r.faceImagePath << '\n';

    std::cout << "Eye position: ("
              << r.eyePosX << ", "
              << r.eyePosY << ")\n";

    std::cout << "Gaze point: ("
              << r.gazeX << ", "
              << r.gazeY << ")\n";

    std::cout << "Distance: "
              << r.eyeScreenDistanceCm
              << " cm\n";

    std::cout << "Gaze angle: "
              << r.gazeAngleDeg
              << " deg\n";

    std::cout << "Pitch: " << r.pitchDeg << " deg\n";
    std::cout << "Roll: " << r.rollDeg << " deg\n";
    std::cout << "Yaw: " << r.yawDeg << " deg\n";

    std::cout << "Eye-use period: "
              << r.eyeUsePeriod << '\n';

    std::cout << "Continuous use: "
              << r.continuousUseSeconds
              << " s\n";
}

static EyeRecord inputRecord()
{
    EyeRecord r;

    r.timestamp = getCurrentTime();
    r.eyeUsePeriod = getEyeUsePeriod();

    std::cout << "Eye side (LEFT/RIGHT): ";
    std::cin >> r.eyeSide;

    std::cout << "Eye image path: ";
    std::cin >> r.eyeImagePath;

    std::cout << "Face image path: ";
    std::cin >> r.faceImagePath;

    std::cout << "Eye position X: ";
    std::cin >> r.eyePosX;

    std::cout << "Eye position Y: ";
    std::cin >> r.eyePosY;

    std::cout << "Gaze point X: ";
    std::cin >> r.gazeX;

    std::cout << "Gaze point Y: ";
    std::cin >> r.gazeY;

    std::cout << "Eye-screen distance (cm): ";
    std::cin >> r.eyeScreenDistanceCm;

    std::cout << "Pitch angle (deg): ";
    std::cin >> r.pitchDeg;

    std::cout << "Roll angle (deg): ";
    std::cin >> r.rollDeg;

    std::cout << "Yaw angle (deg): ";
    std::cin >> r.yawDeg;

    std::cout << "Continuous eye-use time (seconds): ";
    std::cin >> r.continuousUseSeconds;

    // 临时按 1920x1080 屏幕演示。
    // 0.276 mm/pixel 只是示例，后面需要按真实屏幕标定。
    r.gazeAngleDeg = calculateGazeAngleDeg(
        r.gazeX,
        r.gazeY,
        960.0,
        540.0,
        0.276,
        r.eyeScreenDistanceCm
    );

    return r;
}


struct SerialSensorFrame {
    double eyeScr;   // (距离 cm)
    int pitch;       // 俯仰角 (低头/抬头)
    int roll;        // 翻滚角 (歪头)
    int yaw;         // 偏航角 (转头)
    bool isAlarm;    // 硬件预警状态
};

// 串口数据包解析逻辑 (8字节：帧头 0xA5 0x5A ... 帧尾 0x6B)
bool parseSerialBuffer(const std::vector<unsigned char>& buffer, SerialSensorFrame& frame) {
    for (size_t i = 0; i + 7 < buffer.size(); ++i) {
        if (buffer[i] == 0xA5 && buffer[i + 1] == 0x5A && buffer[i + 7] == 0x6B) {
            frame.eyeScr  = static_cast<double>(buffer[i + 2]); // 读取距离 cm
            frame.pitch   = static_cast<int8_t>(buffer[i + 3]);
            frame.roll    = static_cast<int8_t>(buffer[i + 4]);
            frame.yaw     = static_cast<int8_t>(buffer[i + 5]);
            frame.isAlarm = (buffer[i + 6] != 0);
            return true;
        }
    }
    return false;
}
// ------------------- 主程序-------------------

int main() {
    Database db;

    if (!db.open("eye_monitor.db"))
    {
        std::cout << "Database open failed.\n";
        return 1;
    }

    if (!db.createTable())
    {
        std::cout << "Create table failed.\n";
        return 1;
    }

    while (true)
    {
        std::cout << "\n========== Eye Monitor Database ==========\n";
        std::cout << "1. Add record\n";
        std::cout << "2. Show all records\n";
        std::cout << "3. Query by ID\n";
        std::cout << "4. Delete by ID\n";
        std::cout << "0. Exit\n";
        std::cout << "Choose: ";

        int choice;
        std::cin >> choice;

        if (choice == 0)
            break;

        if (choice == 1)
        {
            EyeRecord r = inputRecord();

            if (db.insertRecord(r))
                std::cout << "Insert success.\n";
            else
                std::cout << "Insert failed.\n";
        }
        else if (choice == 2)
        {
            auto records = db.getAllRecords();

            std::cout << "Total: "
                      << records.size()
                      << '\n';

            for (const auto& r : records)
                printRecord(r);
        }
        else if (choice == 3)
        {
            int id;
            std::cout << "Input ID: ";
            std::cin >> id;

            EyeRecord r;

            if (db.getRecordById(id, r))
                printRecord(r);
            else
                std::cout << "Record not found.\n";
        }
        else if (choice == 4)
        {
            int id;
            std::cout << "Input ID: ";
            std::cin >> id;

            if (db.deleteRecord(id))
                std::cout << "Delete success.\n";
            else
                std::cout << "Delete failed.\n";
        }
    }
    // 2. 初始化 Windows 原生串口 COM21
    HANDLE hSerial = CreateFileA("\\\\.\\COM21", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
   if (hSerial == INVALID_HANDLE_VALUE)
{
    DWORD errorCode = GetLastError();

    std::cerr << "COM21 open FAILED!" << std::endl;
    std::cerr << "Windows error code: "
              << errorCode
              << std::endl;

    return 1;
}
  DCB dcb = {};
dcb.DCBlength = sizeof(DCB);

GetCommState(hSerial, &dcb);

dcb.BaudRate = CBR_115200;
dcb.ByteSize = 8;
dcb.Parity = NOPARITY;
dcb.StopBits = ONESTOPBIT;
dcb.fBinary = TRUE;

SetCommState(hSerial, &dcb);

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutConstant = 50;
    SetCommTimeouts(hSerial, &timeouts);
    PurgeComm(
    hSerial,
    PURGE_RXCLEAR | PURGE_TXCLEAR
);

    std::cout << "COM21 opened successfully. Waiting for data..."
          << std::endl;
    // 3. 循环监听与解析
    unsigned char byteRead = 0;
DWORD bytesRead = 0;
std::vector<unsigned char> rxBuffer;

while (true)
{
    if (ReadFile(
            hSerial,
            &byteRead,
            1,
            &bytesRead,
            NULL
        ) && bytesRead > 0)
    {
        // ==============================
        // 1. 显示收到的原始字节
        // ==============================
        std::cout << "RX: 0x"
                  << std::hex
                  << std::uppercase
                  << static_cast<int>(byteRead)
                  << std::dec
                  << std::endl;

        rxBuffer.push_back(byteRead);

        // ==============================
        // 2. 寻找帧头 AA 55
        // ==============================
        while (rxBuffer.size() >= 2)
        {
            if (rxBuffer[0] == 0xAA &&
                rxBuffer[1] == 0x55)
            {
                break;
            }

            // 不是帧头就删掉第一个字节
            rxBuffer.erase(rxBuffer.begin());
        }

        // ==============================
        // 3. 等待完整10字节
        // ==============================
        if (rxBuffer.size() < 10)
        {
            continue;
        }

        // ==============================
        // 4. 计算校验和
        // 前9字节累加，保留低8位
        // ==============================
        unsigned char checksum = 0;

        for (int i = 0; i < 9; i++)
        {
            checksum += rxBuffer[i];
        }

        std::cout << "Calculated checksum: 0x"
                  << std::hex
                  << std::uppercase
                  << static_cast<int>(checksum)
                  << std::endl;

        std::cout << "Received checksum: 0x"
                  << static_cast<int>(rxBuffer[9])
                  << std::dec
                  << std::endl;

        // ==============================
        // 5. 校验正确
        // ==============================
        if (checksum == rxBuffer[9])
        {
            std::cout << "Frame checksum OK!"
                      << std::endl;

            // --------------------------
            // 解析10字节协议
            // --------------------------
            double distance =
                static_cast<double>(rxBuffer[2]);

            int pitch =
                static_cast<signed char>(rxBuffer[3]);

            int roll =
                static_cast<signed char>(rxBuffer[4]);

            int yaw =
                static_cast<signed char>(rxBuffer[5]);

            int eyeStatus =
                static_cast<int>(rxBuffer[6]);

            std::cout << "Distance = "
                      << distance
                      << " cm"
                      << std::endl;

            std::cout << "Pitch = "
                      << pitch
                      << " deg"
                      << std::endl;

            std::cout << "Roll = "
                      << roll
                      << " deg"
                      << std::endl;

            std::cout << "Yaw = "
                      << yaw
                      << " deg"
                      << std::endl;

            std::cout << "Eye status = "
                      << eyeStatus
                      << std::endl;
            
            analyzeEyeBehavior(
                 distance,
                 pitch,
                 roll,
                 yaw,
                 0
);

            // ==========================
            // 6. 构造数据库记录
            // ==========================
            EyeRecord record;

            record.timestamp = getCurrentTime();
            record.eyeUsePeriod = getEyeUsePeriod();

            // 串口传感器数据
            record.eyeScreenDistanceCm = distance;
            record.pitchDeg = pitch;
            record.rollDeg = roll;
            record.yawDeg = yaw;

            // 目前OpenCV数据还没有合并进来
            // 所以先设为空或0
            record.eyeSide = "";
            record.eyeImagePath = "";
            record.faceImagePath = "";

            record.eyePosX = 0;
            record.eyePosY = 0;

            record.gazeX = 0;
            record.gazeY = 0;

            record.gazeAngleDeg = 0;

            record.continuousUseSeconds = 0;

            // ==========================
            // 7. 写入SQLite数据库
            // ==========================
            if (db.insertRecord(record))
            {
                std::cout
                    << "DATABASE INSERT SUCCESS!"
                    << std::endl;
            }
            else
            {
                std::cout
                    << "DATABASE INSERT FAILED!"
                    << std::endl;
            }

            // 这一帧处理完成，删除10字节
            rxBuffer.erase(
                rxBuffer.begin(),
                rxBuffer.begin() + 10
            );
        }

        // ==============================
        // 8. 校验错误
        // ==============================
        else
        {
            std::cout
                << "CHECKSUM FAILED!"
                << std::endl;

            // 删除一个字节，然后重新寻找 AA 55
            rxBuffer.erase(rxBuffer.begin());
        }
    }
}

    CloseHandle(hSerial);
    //sqlite3_close(db);
    return 0;
}