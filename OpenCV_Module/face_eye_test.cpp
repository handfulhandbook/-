#include <opencv2/opencv.hpp>
#include <opencv2/xobjdetect.hpp>

#include <iostream>
#include <vector>

int main()
{
    std::cout << "PROGRAM START!" << std::endl;
    // OpenCV 自带的人脸和眼睛检测模型
    std::string faceCascadePath =
        "D:/opencv/build/etc/haarcascades/haarcascade_frontalface_default.xml";

    std::string eyeCascadePath =
        "D:/opencv/build/etc/haarcascades/haarcascade_eye_tree_eyeglasses.xml";

    cv::CascadeClassifier faceCascade;
    cv::CascadeClassifier eyeCascade;

    if (!faceCascade.load(faceCascadePath))
    {
        std::cout << "Face cascade load failed!" << std::endl;
        return -1;
    }

    if (!eyeCascade.load(eyeCascadePath))
    {
        std::cout << "Eye cascade load failed!" << std::endl;
        return -1;
    }

    cv::VideoCapture camera(0);

    if (!camera.isOpened())
    {
        std::cout << "Camera open failed!" << std::endl;
        return -1;
    }

    std::cout << "Camera opened successfully!" << std::endl;
    std::cout << "Press Q or ESC to exit." << std::endl;

    while (true)
    {
        cv::Mat frame;
        camera >> frame;

        if (frame.empty())
        {
            break;
        }

        cv::Mat gray;

        cv::cvtColor(
            frame,
            gray,
            cv::COLOR_BGR2GRAY
        );

        cv::equalizeHist(gray, gray);

        // =========================
        // 1. 检测人脸
        // =========================

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
            // 画人脸框
            cv::rectangle(
                frame,
                face,
                cv::Scalar(0, 255, 0),
                2
            );

            cv::putText(
                frame,
                "FACE",
                cv::Point(face.x, face.y - 10),
                cv::FONT_HERSHEY_SIMPLEX,
                0.7,
                cv::Scalar(0, 255, 0),
                2
            );

            // =========================
            // 2. 在人脸区域内找眼睛
            // =========================

            cv::Mat faceGray = gray(face);

            std::vector<cv::Rect> eyes;

            eyeCascade.detectMultiScale(
                faceGray,
                eyes,
                1.1,
                5,
                0,
                cv::Size(20, 20)
            );

            int eyeNumber = 0;

            for (const auto& eye : eyes)
            {
                if (eyeNumber >= 2)
                    break;

                // 转换到整个画面的坐标
                int eyeX =
                    face.x + eye.x + eye.width / 2;

                int eyeY =
                    face.y + eye.y + eye.height / 2;

                cv::Point eyeCenter(
                    eyeX,
                    eyeY
                );

                // 判断左/右位置
                std::string eyeSide;

                int faceCenterX =
                    face.x + face.width / 2;

                if (eyeX < faceCenterX)
                {
                    eyeSide = "LEFT";
                }
                else
                {
                    eyeSide = "RIGHT";
                }

                // 画眼睛中心
                cv::circle(
                    frame,
                    eyeCenter,
                    8,
                    cv::Scalar(0, 0, 255),
                    2
                );

                // 画眼睛框
                cv::Rect eyeBox(
                    face.x + eye.x,
                    face.y + eye.y,
                    eye.width,
                    eye.height
                );

                cv::rectangle(
                    frame,
                    eyeBox,
                    cv::Scalar(255, 0, 0),
                    2
                );

                // 显示左右眼
                cv::putText(
                    frame,
                    eyeSide,
                    cv::Point(
                        eyeBox.x,
                        eyeBox.y - 5
                    ),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(255, 255, 0),
                    2
                );

                // 在终端打印眼睛坐标
                std::cout
                    << eyeSide
                    << " eye position: X="
                    << eyeX
                    << " Y="
                    << eyeY
                    << std::endl;

                eyeNumber++;
            }
        }

        // 显示检测到的人脸数量
        std::string info =
            "Faces: " +
            std::to_string(faces.size());

        cv::putText(
            frame,
            info,
            cv::Point(20, 30),
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,
            cv::Scalar(0, 255, 255),
            2
        );

        cv::imshow(
            "Face and Eye Detection",
            frame
        );

        int key = cv::waitKey(1);

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

    return 0;
}