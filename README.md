# -
基于眼动追踪的视屏终端使用行为实时监测与自适应干预系统
## 1. 项目简介

本项目面向视觉终端使用场景，设计一套基于计算机视觉、深度学习和嵌入式控制的眼部状态与使用行为监测系统。

系统通过摄像头采集用户面部及眼部图像，利用 OpenCV 完成人脸检测、眼部区域定位和图像预处理，并采用 CNN 模型实现眼睛睁闭状态识别。

同时结合距离传感器、IMU、RTC 等模块，对用户的观看距离、头部姿态、连续使用时间等信息进行采集，并将检测结果存入 SQLite 数据库。

当检测到闭眼时间过长、观看距离过近、头部姿态异常或连续使用时间过长时，由 STM32 下位机控制蜂鸣器、LED 等设备进行提醒。
---

## 2. 系统总体结构

系统主要包括三个模块：

### OpenCV 视觉检测模块

主要负责：

- 摄像头图像采集
- 人脸检测
- 左右眼区域定位
- 眼部 ROI 提取
- 眼睛状态初步判断
- SQLite 数据库存储

### CNN 深度学习模块

主要负责：

- 眼睛睁闭状态分类
- 公开眼部数据集训练
- CNN 特征提取
- 模型测试与优化
- 后续导出 ONNX 模型供 C++ 调用

### STM32 下位机模块

计划采用 STM32F103C8T6，主要负责：

- 与上位机串口通信
- 接收监测及报警状态
- 蜂鸣器提醒
- LED 状态提示
- 后续扩展 OLED、RTC、距离传感器及 IMU

---

## 3. 项目目录

```text
.
├── OpenCV_Module
│   ├── main.cpp
│   ├── eye_tracking.cpp
│   ├── database.cpp
│   ├── database.h
│   ├── face_eye_test.cpp
│   ├── integrated_test.cpp
│   ├── vision_db_test.cpp
│   ├── sqlite3.c
│   ├── sqlite3.h
│   └── schema.sql
│
├── CNN_Module
│   ├── model.py
│   ├── train.py
│   ├── test.py
│   └── test_model.py
│
├── STM32_Module
│
├── docs
│
├── .gitignore
└── README.md

4. 开发环境
C++ / OpenCV
Visual Studio Code
C++17
MSYS2 UCRT64
GCC / G++
OpenCV 5.0
SQLite3

OpenCV 编译示例：

g++ -std=c++17 eye_tracking.cpp -o eye_tracking.exe $(pkg-config --cflags --libs opencv5)

运行：

./eye_tracking.exe
CNN
Python 3.11
PyTorch
Torchvision
NumPy
Pillow
Matplotlib

当前 CNN 环境使用 Conda：

conda activate cnn
STM32
STM32F103C8T6
Keil5
C语言
ST-Link

硬件模块正在开发中。

5. 当前开发进度
已完成
 SQLite 数据库建立
 C++ 数据库读写
 PC 串口数据解析测试
 OpenCV 摄像头读取
 人脸检测
 眼睛区域检测
 眼睛 ROI 提取
 OPEN / CLOSE 初步判断
 连续闭眼疲劳判断测试
 CNN Python 环境配置
 PyTorch 环境配置
 基础 CNN 网络前向传播测试
正在进行
 收集并整理公开眼部数据集
 CNN 睁闭眼分类训练
 CNN 模型准确率优化
 模型导出 ONNX
 C++ 调用 CNN 模型
后续计划
 STM32F103C8T6 硬件调试
 蜂鸣器报警
 LED / OLED 显示
 RTC 使用时间统计
 距离传感器接入
 IMU 姿态检测
 PC 与 STM32 串口联调
 系统整体测试
6. 数据库监测信息

系统数据库计划记录：

时间戳
使用时间段
连续使用时长
左右眼信息
眼部图像路径
人脸图像路径
眼睛坐标
注视位置
眼睛与屏幕距离
注视角度
Pitch
Roll
Yaw
7. 项目说明

当前项目仍处于开发阶段。

OpenCV 模块目前采用传统图像处理方法完成眼部状态初步检测，后续将由 CNN 模型替代传统阈值方法，以提高不同光照、姿态和个体条件下的识别准确率。


保存之后在仓库终端：

```powershell
git add README.md
git commit -m "Update project README"
git push