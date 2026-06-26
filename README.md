# 🏥 Wearable Health Monitoring & Seizure Detection System

## 📖 Project Overview
This repository contains the complete firmware source code for an IoT-based wearable health monitoring system. Designed for continuous real-time tracking, the device captures vital signs and motion data to detect abnormal physical behaviors (such as clonic and tonic seizures) directly at the edge. 

The system architecture consists of two main hardware components:
1. **Wearable Sensor Node:** Responsible for biological data acquisition (SpO2, BPM) and edge AI inference.
2. **Companion Gateway:** Receives data via low-latency ESP-NOW protocol and securely routes the information to a Firebase Real-time Database for remote monitoring and mobile alerts.

## ✨ Key Algorithmic Features
* **Real-time Edge AI Inference:** Integrates TinyML (via Edge Impulse) to run a neural network directly on the microcontroller. It classifies motion states (Normal, Clonic, Tonic) with minimal processing delay utilizing a 15-frame sliding window approach.
* **Adaptive Signal Processing:** Continuously measures Heart Rate and Blood Oxygen Saturation using an adaptive sliding window filter and Exponential Moving Average (EMA) algorithm to eliminate physical noise, ambient light interference, and motion artifacts.
* **High-Speed Wireless Communication:** Employs the ESP-NOW protocol for connectionless, robust data transmission between the wearable node and the gateway.
* **Intelligent Power Management:** Features a deep-sleep mechanism with hardware-interrupt wake-up triggers (via MPU6050) to optimize battery consumption for continuous long-term operation.

## 🛠️ Hardware & Tech Stack
* **Microcontrollers:** ESP32-C3 (Wearable Node) & ESP32 (Gateway)
* **Sensors:** MAX30102 (Pulse Oximeter), MPU6050 (6-axis IMU)
* **Software Frameworks:** C/C++, Arduino Core, FreeRTOS
* **Machine Learning:** Edge Impulse (TinyML C++ Library)
* **Cloud & Database:** Firebase Real-time Database (`baby-wearable` instance)

## 👨‍💻 Author
* Nguyen Minh Duy
* Electronics and Telecommunication Engineering
* Passionate about Embedded Systems, IoT, and Firmware Development.
* This project is developed as part of an academic engineering thesis.

## 📁 Repository Structure
```text
📦 esp32-baby-wearable-project
 ┣ 📂 Sensor         # Firmware for the ESP32-C3 Wearable Node (Vital signs + TinyML)
 ┣ 📂 Gateway        # Firmware for the ESP32 Gateway (ESP-NOW to Firebase routing)
 ┗ 📜 README.md
(Note: TinyML library files are compressed within the lib folder of the Sensor directory to optimize repository size. Please extract before compiling).
