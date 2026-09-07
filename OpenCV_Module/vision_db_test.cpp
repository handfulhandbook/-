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

#include "database.h"

// ================================
// 获取当前时间：写入数据库
// ================================
std::string getVisionCurrentTime()
{
    std::time_t now = std::time(nullptr);

    std::tm tmNow = *std::localtime(&now);

    std::ostringstream oss;

    oss << std::put_time(
        &tmNow,
        "%Y-%m-%d %H:%M:%S"
    );

    return oss.str();
}


// ================================
// 判断当前用眼时段
// ================================
std::string getVisionEyeUsePeriod()
{
    std::time_t now = std::time(nullptr);

    std::tm tmNow = *std::localtime(&now);

    int hour = tmNow.tm_hour;

    if (hour >= 6 && hour < 12)
        return "MORNING";

    if (hour >= 12 && hour < 18)
        return "AFTERNOON";

    if (hour >= 18 && hour < 24)
        return "EVENING";

    return "LATE_NIGHT";
}


// ================================
// 生成图片文件名
// ================================
std::string makeFileId()
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


int main()
{
    std::cout
        << "====================================\n"
        << " OpenCV + SQLite Vision Test\n"
        << "====================================\n";

    // ==========================================
    // 1. 初始化数据库
    // ==========================================

    Database db;

    if (!db.open("eye_monitor.db"))
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


    // ==========================================
    // 2. Haar 模型
    //
    // 暂时继续使用你原来 D:\opencv 中的 XML
    // 这和 MSYS2 OpenCV 不冲突
    // ==========================================

    std::string faceCascadePath =
        "D:/opencv/build/etc/haarcascades/"
        "haarcascade_frontalface_default.xml";

    std::string eyeCascadePath =
        "D:/opencv/build/etc/haarcascades/"
        "haarcascade_eye_tree_eyeglasses.xml";


    cv::CascadeClassifier faceCascade;
    cv::CascadeClassifier eyeCascade;


    if (!faceCascade.load(faceCascadePath))
    {
        std::cerr
            << "Face cascade load FAILED!"
            << std::endl;

        return 1;
    }


    if (!eyeCascade.load(eyeCascadePath))
    {
        std::cerr
            << "Eye cascade load FAILED!"
            << std::endl;

        return 1;
    }

    std::cout
        << "Cascade models loaded."
        << std::endl;


    // ==========================================
    // 3. 创建图片目录
    // ==========================================

    std::filesystem::create_directories(
        "captures/faces"
    );

    std::filesystem::create_directories(
        "captures/eyes"
    );


    // ==========================================
    // 4. 打开摄像头
    // ==========================================

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


    // ==========================================
    // 限制数据库写入速度
    //
    // 如果不限制，摄像头每秒30帧，
    // 数据库会疯狂写入。
    // ==========================================

    auto lastSave =
        std::chrono::steady_clock::now();

    const int saveIntervalSeconds = 3;


    // ==========================================
    // 5. 主循环
    // ==========================================

    while (true)
    {
        cv::Mat frame;

        camera >> frame;

        if (frame.empty())
        {
            std::cerr
                << "Empty camera frame."
                << std::endl;

            break;
        }


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


        // ======================================
        // 6. 人脸检测
        // ======================================

        std::vector<cv::Rect> faces;

        faceCascade.detectMultiScale(
            gray,
            faces,
            1.1,
            4,
            0,
            cv::Size(100, 100)
        );


        for (const auto& face : faces)
        {
            // 人脸框
            cv::rectangle(
                frame,
                face,
                cv::Scalar(0, 255, 0),
                2
            );


            cv::putText(
                frame,
                "FACE",
                cv::Point(
                    face.x,
                    std::max(20, face.y - 10)
                ),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(0, 255, 0),
                2
            );


            // ==================================
            // 7. 在脸部区域检测眼睛
            // ==================================

            cv::Mat faceGray =
                gray(face);

            std::vector<cv::Rect> detectedEyes;

            eyeCascade.detectMultiScale(
                faceGray,
                detectedEyes,
                1.1,
                5,
                0,
                cv::Size(20, 20)
            );


            // ----------------------------------
            // 过滤脸部下方误检测
            // ----------------------------------

            std::vector<cv::Rect> eyes;

            for (const auto& eye : detectedEyes)
            {
                int eyeCenterY =
                    eye.y + eye.height / 2;

                if (
                    eyeCenterY <
                    static_cast<int>(
                        face.height * 0.60
                    )
                )
                {
                    eyes.push_back(eye);
                }
            }


            // 按 X 坐标排序
            std::sort(
                eyes.begin(),
                eyes.end(),
                [](
                    const cv::Rect& a,
                    const cv::Rect& b
                )
                {
                    return a.x < b.x;
                }
            );


            // 最多处理两只眼睛
            if (eyes.size() > 2)
            {
                eyes.resize(2);
            }


            for (
                size_t i = 0;
                i < eyes.size();
                ++i
            )
            {
                const cv::Rect& eye = eyes[i];


                // ==================================
                // 转换为整个摄像头画面的坐标
                // ==================================

                cv::Rect eyeBox(
                    face.x + eye.x,
                    face.y + eye.y,
                    eye.width,
                    eye.height
                );


                int eyeX =
                    eyeBox.x +
                    eyeBox.width / 2;

                int eyeY =
                    eyeBox.y +
                    eyeBox.height / 2;


                // 左右仅代表当前图像中的左右位置
                std::string eyeSide;

                if (eyeX <
                    face.x + face.width / 2)
                {
                    eyeSide = "LEFT";
                }
                else
                {
                    eyeSide = "RIGHT";
                }


                // ==================================
                // 显示眼睛框
                // ==================================

                cv::rectangle(
                    frame,
                    eyeBox,
                    cv::Scalar(255, 0, 0),
                    2
                );


                cv::circle(
                    frame,
                    cv::Point(eyeX, eyeY),
                    5,
                    cv::Scalar(0, 0, 255),
                    -1
                );


                cv::putText(
                    frame,
                    eyeSide,
                    cv::Point(
                        eyeBox.x,
                        std::max(
                            20,
                            eyeBox.y - 5
                        )
                    ),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(255, 255, 0),
                    2
                );


                // ==================================
                // 8. 每3秒保存一次
                // ==================================

                auto now =
                    std::chrono::steady_clock::now();

                auto elapsed =
                    std::chrono::duration_cast<
                        std::chrono::seconds
                    >(
                        now - lastSave
                    ).count();


                if (
                    elapsed >=
                    saveIntervalSeconds
                )
                {
                    std::string fileId =
                        makeFileId();


                    std::string facePath =
                        "captures/faces/face_" +
                        fileId +
                        ".jpg";


                    std::string eyePath =
                        "captures/eyes/eye_" +
                        eyeSide +
                        "_" +
                        fileId +
                        ".jpg";


                    // --------------------------
                    // 保存脸部图像
                    // --------------------------

                    cv::Mat faceImage =
                        frame(face).clone();

                    cv::imwrite(
                        facePath,
                        faceImage
                    );


                    // --------------------------
                    // 保存眼睛图像
                    // --------------------------

                    cv::Mat eyeImage =
                        frame(eyeBox).clone();

                    cv::imwrite(
                        eyePath,
                        eyeImage
                    );


                    // ==================================
                    // 9. 构造数据库 EyeRecord
                    // ==================================

                    EyeRecord record;


                    record.timestamp =
                        getVisionCurrentTime();


                    record.eyeUsePeriod =
                        getVisionEyeUsePeriod();


                    record.continuousUseSeconds =
                        0;


                    // OpenCV 数据
                    record.eyeSide =
                        eyeSide;

                    record.eyeImagePath =
                        eyePath;

                    record.faceImagePath =
                        facePath;

                    record.eyePosX =
                        eyeX;

                    record.eyePosY =
                        eyeY;


                    // ==================================
                    // 注视点暂时不伪造
                    //
                    // 眼睛位置 ≠ 屏幕注视点
                    // 后面做瞳孔/标定后再填
                    // ==================================

                    record.gazeX = 0;
                    record.gazeY = 0;


                    // ==================================
                    // 串口传感器数据暂时为0
                    // 后面与 main.cpp 合并后由 COM21 填入
                    // ==================================

                    record.eyeScreenDistanceCm =
                        0;

                    record.gazeAngleDeg =
                        0;

                    record.pitchDeg =
                        0;

                    record.rollDeg =
                        0;

                    record.yawDeg =
                        0;


                    // ==================================
                    // 10. 写入 SQLite
                    // ==================================

                    if (
                        db.insertRecord(record)
                    )
                    {
                        std::cout
                            << "\nDATABASE INSERT SUCCESS!"
                            << std::endl;

                        std::cout
                            << "Eye side: "
                            << eyeSide
                            << std::endl;

                        std::cout
                            << "Eye position: ("
                            << eyeX
                            << ", "
                            << eyeY
                            << ")"
                            << std::endl;

                        std::cout
                            << "Face image: "
                            << facePath
                            << std::endl;

                        std::cout
                            << "Eye image: "
                            << eyePath
                            << std::endl;
                    }
                    else
                    {
                        std::cout
                            << "DATABASE INSERT FAILED!"
                            << std::endl;
                    }


                    lastSave = now;

                    // 一次只保存一个眼睛，
                    // 防止同一帧写入两条数据
                    break;
                }
            }


            // 暂时只处理第一张脸
            break;
        }


        // ======================================
        // 11. 屏幕显示状态
        // ======================================

        std::string info =
            "Faces: " +
            std::to_string(
                faces.size()
            );


        cv::putText(
            frame,
            info,
            cv::Point(20, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.7,
            cv::Scalar(0, 255, 255),
            2
        );


        cv::imshow(
            "Eye Monitor Vision + Database",
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


    camera.release();

    cv::destroyAllWindows();


    std::cout
        << "Program stopped."
        << std::endl;


    return 0;
}