#define NOMINMAX

#include <windows.h>

#include <opencv2/opencv.hpp>
#include <opencv2/xobjdetect.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cmath>

#include "database.h"


// ============================================================
// 串口数据
// ============================================================

struct SerialData
{
    bool valid = false;

    double distanceCm = 0;

    int pitch = 0;
    int roll = 0;
    int yaw = 0;

    int eyeStatus = 0;

    std::chrono::steady_clock::time_point lastUpdate;
};


// ============================================================
// OpenCV 数据
// ============================================================

struct VisionData
{
    bool valid = false;

    std::string eyeSide;

    int eyeX = 0;
    int eyeY = 0;

    std::string faceImagePath;
    std::string eyeImagePath;
};


// ============================================================
// 当前时间
// ============================================================

std::string getIntegratedCurrentTime()
{
    std::time_t now = std::time(nullptr);

    std::tm tmNow =
        *std::localtime(&now);

    std::ostringstream oss;

    oss << std::put_time(
        &tmNow,
        "%Y-%m-%d %H:%M:%S"
    );

    return oss.str();
}


// ============================================================
// 用眼时段
// ============================================================

std::string getIntegratedEyeUsePeriod()
{
    std::time_t now =
        std::time(nullptr);

    std::tm tmNow =
        *std::localtime(&now);

    int hour = tmNow.tm_hour;

    if (hour >= 6 && hour < 12)
        return "MORNING";

    if (hour >= 12 && hour < 18)
        return "AFTERNOON";

    if (hour >= 18 && hour < 24)
        return "EVENING";

    return "LATE_NIGHT";
}


// ============================================================
// 图片编号
// ============================================================

std::string makeIntegratedFileId()
{
    auto now =
        std::chrono::system_clock::now();

    auto ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            now.time_since_epoch()
        ).count();

    return std::to_string(ms);
}


// ============================================================
// 打开 COM21
// ============================================================

bool openSerialPort(
    HANDLE& hSerial
)
{
    hSerial = CreateFileA(
        "\\\\.\\COM21",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (
        hSerial ==
        INVALID_HANDLE_VALUE
    )
    {
        std::cerr
            << "COM21 OPEN FAILED!"
            << std::endl;

        std::cerr
            << "Windows error code: "
            << GetLastError()
            << std::endl;

        return false;
    }


    DCB dcb = {};

    dcb.DCBlength =
        sizeof(DCB);


    if (
        !GetCommState(
            hSerial,
            &dcb
        )
    )
    {
        std::cerr
            << "GetCommState FAILED!"
            << std::endl;

        CloseHandle(hSerial);

        return false;
    }


    dcb.BaudRate =
        CBR_115200;

    dcb.ByteSize = 8;

    dcb.Parity =
        NOPARITY;

    dcb.StopBits =
        ONESTOPBIT;

    dcb.fBinary =
        TRUE;


    if (
        !SetCommState(
            hSerial,
            &dcb
        )
    )
    {
        std::cerr
            << "SetCommState FAILED!"
            << std::endl;

        CloseHandle(hSerial);

        return false;
    }


    COMMTIMEOUTS timeouts = {};

    timeouts.ReadIntervalTimeout = 20;

    timeouts.ReadTotalTimeoutConstant = 20;

    timeouts.ReadTotalTimeoutMultiplier = 0;


    SetCommTimeouts(
        hSerial,
        &timeouts
    );


    PurgeComm(
        hSerial,
        PURGE_RXCLEAR |
        PURGE_TXCLEAR
    );


    std::cout
        << "COM21 opened successfully."
        << std::endl;


    return true;
}


// ============================================================
// 解析10字节串口协议
//
// AA 55 distance pitch roll yaw eyeStatus 00 00 checksum
// ============================================================

bool parseSerialBuffer(
    std::vector<unsigned char>& buffer,
    SerialData& data
)
{
    while (buffer.size() >= 2)
    {
        // 找帧头
        if (
            buffer[0] != 0xAA ||
            buffer[1] != 0x55
        )
        {
            buffer.erase(
                buffer.begin()
            );

            continue;
        }


        // 等待完整10字节
        if (buffer.size() < 10)
        {
            return false;
        }


        unsigned char checksum = 0;

        for (int i = 0; i < 9; ++i)
        {
            checksum +=
                buffer[i];
        }


        // 校验失败
        if (
            checksum !=
            buffer[9]
        )
        {
            std::cout
                << "Serial checksum FAILED!"
                << std::endl;

            buffer.erase(
                buffer.begin()
            );

            continue;
        }


        // -------------------------
        // 解析数据
        // -------------------------

        data.distanceCm =
            static_cast<double>(
                buffer[2]
            );


        data.pitch =
            static_cast<int8_t>(
                buffer[3]
            );


        data.roll =
            static_cast<int8_t>(
                buffer[4]
            );


        data.yaw =
            static_cast<int8_t>(
                buffer[5]
            );


        data.eyeStatus =
            static_cast<int>(
                buffer[6]
            );


        data.valid = true;

        data.lastUpdate =
            std::chrono::steady_clock::now();


        // 删除当前帧
        buffer.erase(
            buffer.begin(),
            buffer.begin() + 10
        );


        std::cout
            << "\n[SERIAL FRAME OK]"
            << std::endl;

        std::cout
            << "Distance = "
            << data.distanceCm
            << " cm"
            << std::endl;

        std::cout
            << "Pitch = "
            << data.pitch
            << " deg"
            << std::endl;

        std::cout
            << "Roll = "
            << data.roll
            << " deg"
            << std::endl;

        std::cout
            << "Yaw = "
            << data.yaw
            << " deg"
            << std::endl;

        std::cout
            << "Eye status = "
            << data.eyeStatus
            << std::endl;


        return true;
    }


    return false;
}


// ============================================================
// 非阻塞读取串口
// ============================================================

void pollSerial(
    HANDLE hSerial,
    std::vector<unsigned char>& rxBuffer,
    SerialData& latestData
)
{
    if (
        hSerial ==
        INVALID_HANDLE_VALUE
    )
    {
        return;
    }


    DWORD errors = 0;

    COMSTAT status = {};


    if (
        !ClearCommError(
            hSerial,
            &errors,
            &status
        )
    )
    {
        return;
    }


    if (status.cbInQue == 0)
    {
        return;
    }


    DWORD toRead =
        status.cbInQue;

    if (toRead > 64)
    {
        toRead = 64;
    }


    unsigned char temp[64];

    DWORD bytesRead = 0;


    if (
        ReadFile(
            hSerial,
            temp,
            toRead,
            &bytesRead,
            NULL
        )
    )
    {
        for (
            DWORD i = 0;
            i < bytesRead;
            ++i
        )
        {
            rxBuffer.push_back(
                temp[i]
            );
        }


        // 可能连续收到多个数据帧
        while (
            parseSerialBuffer(
                rxBuffer,
                latestData
            )
        )
        {
        }
    }
}


// ============================================================
// 不良用眼行为分析
//
// 注意：这里是项目演示阈值，后续可根据实验调整。
// ============================================================

void analyzeBehavior(
    double distance,
    double pitch,
    double roll,
    double yaw,
    int continuousSeconds
)
{
    std::cout
        << "\n===== Behavior Analysis ====="
        << std::endl;


    bool normal = true;


    if (
        distance > 0 &&
        distance < 40
    )
    {
        std::cout
            << "[WARNING] Viewing distance too close."
            << std::endl;

        normal = false;
    }


    if (pitch < -15)
    {
        std::cout
            << "[WARNING] Head-down posture."
            << std::endl;

        normal = false;
    }


    if (
        std::abs(roll) > 10
    )
    {
        std::cout
            << "[WARNING] Side-head posture."
            << std::endl;

        normal = false;
    }


    if (
        std::abs(yaw) > 15
    )
    {
        std::cout
            << "[WARNING] Large yaw deviation."
            << std::endl;

        normal = false;
    }


    if (
        continuousSeconds >=
        20 * 60
    )
    {
        std::cout
            << "[REMINDER] 20-minute eye-use reminder."
            << std::endl;

        normal = false;
    }


    if (normal)
    {
        std::cout
            << "[OK] Current behavior normal."
            << std::endl;
    }


    std::cout
        << "============================="
        << std::endl;
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "======================================\n"
        << " Eye Monitor Integrated Test\n"
        << " OpenCV + Serial + SQLite\n"
        << "======================================\n";


    // ========================================================
    // 1. 数据库
    // ========================================================

    Database db;


    if (
        !db.open(
            "eye_monitor.db"
        )
    )
    {
        std::cerr
            << "Database open FAILED!"
            << std::endl;

        return 1;
    }


    db.createTable();


    std::cout
        << "Database opened successfully."
        << std::endl;


    // ========================================================
    // 2. 串口
    // ========================================================

    HANDLE hSerial =
        INVALID_HANDLE_VALUE;


    bool serialOpened =
        openSerialPort(
            hSerial
        );


    if (!serialOpened)
    {
        std::cout
            << "WARNING: Serial unavailable."
            << std::endl;

        std::cout
            << "Vision mode will continue."
            << std::endl;
    }


    std::vector<unsigned char>
        rxBuffer;


    SerialData latestSerial;


    // ========================================================
    // 3. Haar 模型
    // ========================================================

    std::string faceCascadePath =
        "D:/opencv/build/etc/haarcascades/"
        "haarcascade_frontalface_default.xml";


    std::string eyeCascadePath =
        "D:/opencv/build/etc/haarcascades/"
        "haarcascade_eye_tree_eyeglasses.xml";


    cv::CascadeClassifier
        faceCascade;


    cv::CascadeClassifier
        eyeCascade;


    if (
        !faceCascade.load(
            faceCascadePath
        )
    )
    {
        std::cerr
            << "Face cascade load FAILED!"
            << std::endl;

        return 1;
    }


    if (
        !eyeCascade.load(
            eyeCascadePath
        )
    )
    {
        std::cerr
            << "Eye cascade load FAILED!"
            << std::endl;

        return 1;
    }


    // ========================================================
    // 4. 图片文件夹
    // ========================================================

    std::filesystem::create_directories(
        "captures/faces"
    );


    std::filesystem::create_directories(
        "captures/eyes"
    );


    // ========================================================
    // 5. 摄像头
    // ========================================================

    cv::VideoCapture camera(0);


    if (!camera.isOpened())
    {
        std::cerr
            << "Camera open FAILED!"
            << std::endl;

        return 1;
    }


    std::cout
        << "Camera opened successfully."
        << std::endl;

    std::cout
        << "Press Q or ESC to exit."
        << std::endl;


    // ========================================================
    // 连续用眼计时
    // ========================================================

    bool continuousUsing =
        false;


    auto continuousStart =
        std::chrono::steady_clock::now();


    int continuousSeconds = 0;


    // 每3秒写一次数据库
    auto lastSave =
        std::chrono::steady_clock::now();


    // ========================================================
    // 6. 主循环
    // ========================================================

    while (true)
    {
        // ----------------------------------------------------
        // 读取最新串口数据
        // ----------------------------------------------------

        if (serialOpened)
        {
            pollSerial(
                hSerial,
                rxBuffer,
                latestSerial
            );
        }


        // ----------------------------------------------------
        // 获取摄像头
        // ----------------------------------------------------

        cv::Mat frame;


        camera >> frame;


        if (frame.empty())
        {
            break;
        }


        cv::Mat original =
            frame.clone();


        cv::Mat gray;


        cv::cvtColor(
            frame,
            gray,
            cv::COLOR_BGR2GRAY
        );


        cv::equalizeHist(
            gray,
            gray
        );


        // ----------------------------------------------------
        // 人脸检测
        // ----------------------------------------------------

        std::vector<cv::Rect>
            faces;


        faceCascade.detectMultiScale(
            gray,
            faces,
            1.1,
            4,
            0,
            cv::Size(100, 100)
        );


        VisionData vision;


        if (!faces.empty())
        {
            cv::Rect face =
                faces[0];


            cv::rectangle(
                frame,
                face,
                cv::Scalar(
                    0,
                    255,
                    0
                ),
                2
            );


            cv::Mat faceGray =
                gray(face);


            std::vector<cv::Rect>
                detectedEyes;


            eyeCascade.detectMultiScale(
                faceGray,
                detectedEyes,
                1.1,
                5,
                0,
                cv::Size(20, 20)
            );


            // -----------------------------------------------
            // 过滤掉脸下半部分的误识别
            // -----------------------------------------------

            std::vector<cv::Rect>
                eyes;


            for (
                const auto& eye :
                detectedEyes
            )
            {
                int centerY =
                    eye.y +
                    eye.height / 2;


                if (
                    centerY <
                    static_cast<int>(
                        face.height *
                        0.60
                    )
                )
                {
                    eyes.push_back(
                        eye
                    );
                }
            }


            std::sort(
                eyes.begin(),
                eyes.end(),
                [](
                    const cv::Rect& a,
                    const cv::Rect& b
                )
                {
                    return
                        a.x < b.x;
                }
            );


            if (!eyes.empty())
            {
                cv::Rect eye =
                    eyes[0];


                cv::Rect eyeBox(
                    face.x + eye.x,
                    face.y + eye.y,
                    eye.width,
                    eye.height
                );


                vision.eyeX =
                    eyeBox.x +
                    eyeBox.width / 2;


                vision.eyeY =
                    eyeBox.y +
                    eyeBox.height / 2;


                if (
                    vision.eyeX <
                    face.x +
                    face.width / 2
                )
                {
                    vision.eyeSide =
                        "LEFT";
                }
                else
                {
                    vision.eyeSide =
                        "RIGHT";
                }


                vision.valid = true;


                cv::rectangle(
                    frame,
                    eyeBox,
                    cv::Scalar(
                        255,
                        0,
                        0
                    ),
                    2
                );


                cv::circle(
                    frame,
                    cv::Point(
                        vision.eyeX,
                        vision.eyeY
                    ),
                    5,
                    cv::Scalar(
                        0,
                        0,
                        255
                    ),
                    -1
                );


                cv::putText(
                    frame,
                    vision.eyeSide,
                    cv::Point(
                        eyeBox.x,
                        eyeBox.y - 5
                    ),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(
                        255,
                        255,
                        0
                    ),
                    2
                );


                // -------------------------------------------
                // 连续用眼计时
                // -------------------------------------------

                bool eyeActive =
                    true;


                if (latestSerial.valid)
                {
                    eyeActive =
                        latestSerial.eyeStatus
                        != 0;
                }


                if (eyeActive)
                {
                    if (!continuousUsing)
                    {
                        continuousUsing =
                            true;


                        continuousStart =
                            std::chrono::
                            steady_clock::
                            now();
                    }


                    continuousSeconds =
                        static_cast<int>(
                            std::chrono::
                            duration_cast<
                                std::chrono::
                                seconds
                            >(
                                std::chrono::
                                steady_clock::
                                now()
                                -
                                continuousStart
                            ).count()
                        );
                }
                else
                {
                    continuousUsing =
                        false;

                    continuousSeconds =
                        0;
                }


                // -------------------------------------------
                // 每3秒存一次
                // -------------------------------------------

                auto now =
                    std::chrono::
                    steady_clock::
                    now();


                auto elapsed =
                    std::chrono::
                    duration_cast<
                        std::chrono::
                        seconds
                    >(
                        now -
                        lastSave
                    ).count();


                if (elapsed >= 3)
                {
                    std::string id =
                        makeIntegratedFileId();


                    vision.faceImagePath =
                        "captures/faces/face_"
                        +
                        id
                        +
                        ".jpg";


                    vision.eyeImagePath =
                        "captures/eyes/eye_"
                        +
                        vision.eyeSide
                        +
                        "_"
                        +
                        id
                        +
                        ".jpg";


                    cv::imwrite(
                        vision.faceImagePath,
                        original(face)
                    );


                    cv::imwrite(
                        vision.eyeImagePath,
                        original(eyeBox)
                    );


                    // =======================================
                    // 构造最终数据库记录
                    // =======================================

                    EyeRecord record;


                    record.timestamp =
                        getIntegratedCurrentTime();


                    record.eyeUsePeriod =
                        getIntegratedEyeUsePeriod();


                    record.continuousUseSeconds =
                        continuousSeconds;


                    // OpenCV
                    record.eyeSide =
                        vision.eyeSide;


                    record.eyeImagePath =
                        vision.eyeImagePath;


                    record.faceImagePath =
                        vision.faceImagePath;


                    record.eyePosX =
                        vision.eyeX;


                    record.eyePosY =
                        vision.eyeY;


                    // ---------------------------------------
                    // 目前还没有真正完成屏幕注视点标定
                    // 所以不能把 eyeX/eyeY 冒充 gazeX/gazeY
                    // ---------------------------------------

                    record.gazeX = 0;
                    record.gazeY = 0;


                    // ---------------------------------------
                    // 串口传感器
                    // ---------------------------------------

                    if (latestSerial.valid)
                    {
                        record.eyeScreenDistanceCm =
                            latestSerial.distanceCm;


                        record.pitchDeg =
                            latestSerial.pitch;


                        record.rollDeg =
                            latestSerial.roll;


                        record.yawDeg =
                            latestSerial.yaw;
                    }
                    else
                    {
                        record.eyeScreenDistanceCm =
                            0;

                        record.pitchDeg = 0;
                        record.rollDeg = 0;
                        record.yawDeg = 0;
                    }


                    // 真正 gazeAngle 等注视点标定后计算
                    record.gazeAngleDeg = 0;


                    // =======================================
                    // 数据库写入
                    // =======================================

                    if (
                        db.insertRecord(
                            record
                        )
                    )
                    {
                        std::cout
                            << "\n================================"
                            << std::endl;

                        std::cout
                            << "DATABASE INSERT SUCCESS!"
                            << std::endl;


                        std::cout
                            << "Eye = "
                            << record.eyeSide
                            << std::endl;


                        std::cout
                            << "Eye position = ("
                            << record.eyePosX
                            << ", "
                            << record.eyePosY
                            << ")"
                            << std::endl;


                        std::cout
                            << "Distance = "
                            << record.eyeScreenDistanceCm
                            << " cm"
                            << std::endl;


                        std::cout
                            << "Pitch/Roll/Yaw = "
                            << record.pitchDeg
                            << " / "
                            << record.rollDeg
                            << " / "
                            << record.yawDeg
                            << std::endl;


                        std::cout
                            << "Continuous use = "
                            << record.continuousUseSeconds
                            << " sec"
                            << std::endl;


                        analyzeBehavior(
                            record.eyeScreenDistanceCm,
                            record.pitchDeg,
                            record.rollDeg,
                            record.yawDeg,
                            record.continuousUseSeconds
                        );
                    }
                    else
                    {
                        std::cout
                            << "DATABASE INSERT FAILED!"
                            << std::endl;
                    }


                    lastSave = now;
                }
            }
        }


        // ====================================================
        // 7. 在摄像头画面显示传感器数据
        // ====================================================

        if (latestSerial.valid)
        {
            std::string text =
                "D:"
                +
                std::to_string(
                    static_cast<int>(
                        latestSerial.distanceCm
                    )
                )
                +
                "cm P:"
                +
                std::to_string(
                    latestSerial.pitch
                )
                +
                " R:"
                +
                std::to_string(
                    latestSerial.roll
                )
                +
                " Y:"
                +
                std::to_string(
                    latestSerial.yaw
                );


            cv::putText(
                frame,
                text,
                cv::Point(20, 30),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(
                    0,
                    255,
                    255
                ),
                2
            );
        }
        else
        {
            cv::putText(
                frame,
                "SERIAL: WAITING",
                cv::Point(20, 30),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(
                    0,
                    0,
                    255
                ),
                2
            );
        }


        std::string useText =
            "Use time: "
            +
            std::to_string(
                continuousSeconds
            )
            +
            " s";


        cv::putText(
            frame,
            useText,
            cv::Point(20, 60),
            cv::FONT_HERSHEY_SIMPLEX,
            0.6,
            cv::Scalar(
                255,
                255,
                0
            ),
            2
        );


        cv::imshow(
            "Integrated Eye Monitor",
            frame
        );


        int key =
            cv::waitKey(1);


        if (
            key == 'q' ||
            key == 'Q' ||
            key == 27
        )
        {
            break;
        }
    }


    // ========================================================
    // 8. 退出
    // ========================================================

    camera.release();

    cv::destroyAllWindows();


    if (
        hSerial !=
        INVALID_HANDLE_VALUE
    )
    {
        CloseHandle(
            hSerial
        );
    }


    std::cout
        << "Program stopped."
        << std::endl;


    return 0;
}