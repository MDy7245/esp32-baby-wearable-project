#include <Arduino.h>
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

// --- THƯ VIỆN ---
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "baby_wearable_v2_inferencing.h"

// PINOUT & MAC ADDRESS
const int I2C_SDA_PIN = 4;
const int I2C_SCL_PIN = 3;
const int MPU_INT_PIN = 1;  
const int BAT_ADC_PIN = 0;
const int CHARGE_PIN = 5;   

// ĐỊA CHỈ MAC GATEWAY
uint8_t gatewayAddress[] = {0x6C, 0xC8, 0x40, 0x4E, 0x2E, 0x24}; 

// CẤU TRÚC GÓI TIN CHUẨN
typedef struct struct_message {
  uint8_t spo2;
  uint8_t bpm;
  uint8_t bat;
  bool charge;
  uint8_t state; 
  uint8_t mode;  
} struct_message;
struct_message myData;


typedef struct struct_receive {
  char cmd[10]; 
} struct_receive;
struct_receive incomingData;

// STATE MACHINE
enum DeviceMode { MODE_ACTIVE = 0, MODE_WAIT = 1, MODE_SLEEP = 2, MODE_DROPPED = 3 };

// Khởi tạo đối tượng
MAX30105 particleSensor;
Adafruit_MPU6050 mpu;
SemaphoreHandle_t i2cMutex;

// =============================
// CÁC HÀM CALLBACK VÀ TIỆN ÍCH
// =============================

// ---- Dò kênh WiFi ----
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i = 0; i < n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) return WiFi.channel(i);
    } 
  }
  return 0; 
}

// ---- Nhận lệnh SLEEP từ Gateway ----
volatile bool forceSleepCmd = false; // Cờ lệnh ngủ
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  if (strcmp(incomingData.cmd, "SLEEP") == 0) {
    Serial.println("\n[ESP-NOW] <<< NHẬN LỆNH [SLEEP] TỪ GATEWAY!");
    forceSleepCmd = true;
  }
}

// ---- Hàm callback mỗi khi có gói tin gửi đi, status: trạng thái gửi SUCCESS / FAIL ----
volatile bool txDone = false; // Cờ báo trạng thái bắn gói tin
volatile bool txSendSuccess = false;  // Cờ báo trạng thái bên nhận
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  txDone = true; // Đã bắn gói tin đi
  txSendSuccess = (status == ESP_NOW_SEND_SUCCESS); // Nếu bên nhận nhận thành công sẽ trả lại status 
}


// ==========================================
// CÁC HÀM TIỆN ÍCH GIAO TIẾP I2C RAW
// ==========================================
void writeRegister(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(0x68);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(0x68);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 1);
  return Wire.read();
}

// ==========================================
// HÀM XỬ LÝ CHẾ ĐỘ NGỦ SÂU
// ==========================================
void enterDeepSleep() {
  Serial.println("\n[SYSTEM] === TIẾN HÀNH NGỦ SÂU (DEEP SLEEP) ===");
  myData.mode = MODE_SLEEP;
  myData.spo2 = 0; myData.bpm = 0; myData.state = 0;
  myData.charge = (digitalRead(CHARGE_PIN) == HIGH);
  txDone = false;
  txSendSuccess = false;
  esp_now_send(gatewayAddress, (uint8_t *) &myData, sizeof(myData));
  unsigned long txWait = millis();
  while (millis() - txWait < 200) { 
    if (txDone) {
      if (!txSendSuccess) { 
        vTaskDelay (pdMS_TO_TICKS (50));
        txDone = false;
        esp_now_send(gatewayAddress, (uint8_t *) &myData, sizeof(myData));
      }
      else break;
    }
    vTaskDelay (pdMS_TO_TICKS (1)); 
  }
  esp_wifi_stop(); 
  delay(50);
  // --- CẤU HÌNH MPU SANG CHẾ ĐỘ CANH GÁC ---
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    particleSensor.clearFIFO(); 
    particleSensor.shutDown(); 
    Serial.println("[SYSTEM] Dang dua MPU vao che do Ngu Dong canh gac...");
    // 0. KHÓA TỊT CÒI BÁO ĐỘNG TRƯỚC KHI LÀM GÌ ĐÓ (Tránh ngắt ảo lọt ra)
    writeRegister(0x38, 0x00); 
    // 1. Đánh thức hoàn toàn để Setup cho chuẩn
    writeRegister(0x6B, 0x00);
    delay(10);
    // 2. Tắt Gyro
    writeRegister(0x6C, 0x07);
    // 3. KÍCH HOẠT TẦN SỐ ĐO LÚC NGỦ (LP_ACCEL_ODR - 0x1E)
    // FIX LỖI KẸT HIGH: 0x05 = 7.81Hz (1 giây đo 8 lần). Không có lệnh này MPU sẽ ngủ chết!
    writeRegister(0x1E, 0x05);
    // 4. Bật AI phần cứng và Cài ngưỡng
    writeRegister(0x69, 0xC0); 
    writeRegister(0x1F, 20); 
    // 5. Open-Drain (0xC0), Active LOW, Nháy 50us
    writeRegister(0x37, 0xC0);
    // 6. Kích hoạt Cycle Mode (Sẽ sinh ra "Bóng ma" hẫng lò xo ở bước này)
    writeRegister(0x6B, 0x20);
    // ĐỢI BÓNG MA ĐI QUA RỒI MỚI BẬT CÒI BÁO ĐỘNG
    Serial.println("[SYSTEM] Đang chờ lò xo MEMS ổn định điện áp...");
    delay(500); // Ép ESP32 chờ 500ms cho con MPU xả hết ngắt ảo
    // 7. Súc ruột sạch sẽ cái ngắt ảo
    uint8_t ghostInt = readRegister(0x3A);
    Serial.printf("[SYSTEM] Đã dọn sạch rác ảo (Mã ngắt vừa súc: 0x%02X)\n", ghostInt);
    // 8. BÂY GIỜ MỚI BẬT NGẮT RUNG LẮC (Lên nòng súng thực sự)
    writeRegister(0x38, 0x40);
    xSemaphoreGive(i2cMutex);
  }
  // --- BỘ KIỂM SOÁT TỐI HẬU TRƯỚC KHI SẬP NGUỒN ---
  Serial.println("[SYSTEM] Đang kiểm tra an toàn chân INT...");
  int retry = 0;
  while (digitalRead(MPU_INT_PIN) == LOW && retry < 20) {
    Serial.println("   >> INT dang bi ket LOW! Dang ep xoa ngat lai...");
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
        readRegister(0x3A); 
        xSemaphoreGive(i2cMutex);
    }
    delay(50);
    retry++;
  }
  if (digitalRead(MPU_INT_PIN) == LOW) {
    Serial.println("❌ CẢNH BÁO: Chân INT vẫn kẹt LOW! (Phần cứng lỗi rò áp)");
  } else {
    Serial.println("✅ AN TOÀN TUYỆT ĐỐI! Chân INT đã ghim cứng HIGH (3.3V).");
  }
  // Kích hoạt đánh thức ESP32 bằng mức LOW
  esp_deep_sleep_enable_gpio_wakeup(1ULL << MPU_INT_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  Serial.println("[SYSTEM] ESP32-C3 chính thức sập nguồn...");
  delay(100); 
  esp_deep_sleep_start(); 
}

// ================================
// FUNCTION VÀ BIẾN TASK MAX30102
// ================================
// ---- Function đọc cảm biến MAX và check trạng thái tiếp xúc da ----
bool dataReady = false; // Cờ báo thu thập đủ 100 mẫu IR-RED
volatile bool isAttached = false; // Cờ đang đeo
unsigned long DropCountTime = 0; // Biến đếm tgian Dropped
// Bộ đệm IR và RED
const int32_t bufferLength = 100;
uint32_t irBuffer[bufferLength]; 
uint32_t redBuffer[bufferLength]; 
int sampleIndex = 0;

void ReadSensor_CheckAttach(unsigned long currentMillis) {
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    particleSensor.check(); 
    xSemaphoreGive(i2cMutex); 
  }
  while (particleSensor.available()) { 
    uint32_t irVal = particleSensor.getFIFOIR(); 
    uint32_t redVal = particleSensor.getFIFORed(); 
    particleSensor.nextSample(); 
    
    if (irVal < 50000) { 
      isAttached = false;
      if (DropCountTime == 0) DropCountTime = currentMillis; 
      sampleIndex = 0; 
    } 
    else { 
      irBuffer[sampleIndex] = irVal;
      redBuffer[sampleIndex] = redVal;
      sampleIndex++;
      
      if (sampleIndex == 100) {
        dataReady = true; 
        uint32_t ir_max = 0;
        uint32_t ir_min = 999999;
        
        // --- TÍNH TOÁN DC (TRUNG BÌNH) VÀ AC (DELTA) ---
        uint32_t ir_avg = 0;
        uint32_t red_avg = 0;
        for (uint8_t i = 0; i < bufferLength; i++) {
          ir_avg += irBuffer[i];
          red_avg += redBuffer[i];
          if (ir_max < irBuffer[i]) ir_max = irBuffer[i];
          if (ir_min > irBuffer[i]) ir_min = irBuffer[i]; 
        }
        ir_avg /= bufferLength;
        red_avg /= bufferLength;
        uint32_t ir_delta = ir_max - ir_min;

        // ==========================================================
        // XÉT DUYỆT TRẠNG THÁI BẰNG 3 ĐỊNH LUẬT VẬT LÝ DA NGƯỜI
        // ==========================================================
        
        if (ir_max > 255000) { 
          // Lớp 1: Mặt đen thui (hút sáng) hoặc lọt sáng bão hòa
          // Serial.printf("[LỖI] Vật thể hút sáng/Bão hòa (IR=%lu)\n", ir_max);
          isAttached = false;
          if (DropCountTime == 0) DropCountTime = currentMillis; 
        }
        else if (red_avg >= ir_avg * 0.90) { 
          // Lớp 2: Mặt phẳng vô cơ (Trắng/Sáng). Phản xạ IR và RED xấp xỉ nhau.
          // Da người thật thì máu sẽ hút RED, làm RED thấp hơn IR.
          Serial.println("[LỖI] Vật thể vô cơ (RED:IR gần 1:1). Ép rớt!");
          isAttached = false;
          if (DropCountTime == 0) DropCountTime = currentMillis; 
        }
        else if (ir_delta < 50) { 
          // Lớp 3: Bề mặt khác màu (lừa được tỉ lệ) nhưng LÀ VẬT CHẾT (Không có Delta mạch đập)
          Serial.printf("[LỖI] Vật thể tĩnh lặng (Delta=%lu < 50). Ép rớt!\n", ir_delta);
          isAttached = false;
          if (DropCountTime == 0) DropCountTime = currentMillis; 
        }
        else if (ir_delta > 5000) {
          // Vung tay mạnh lúc đang đeo -> Bỏ qua không chốt
          Serial.printf("[CẢNH BÁO] Nhiễu động học (Delta=%lu > 5000)\n", ir_delta);
        }
        else { 
          // ĐÃ VƯỢT QUA 3 LỚP LỌC VẬT LÝ! 100% LÀ TAY NGƯỜI!
          if (!isAttached) {
             Serial.printf(">> [THÀNH CÔNG] Đã xác nhận DA NGƯỜI! Delta=%lu\n", ir_delta);
          }
          isAttached = true;
          DropCountTime = 0; 
        }
        break;
      }
    }
  }
}

// ---- Function lọc nhiễu và chốt dữ liệu ----
volatile bool isMoving = false; // Cờ báo đang cử động
volatile bool isDataUnlocked = false; // Cờ khóa dữ liệu
// Kết quả tính sau 100 samples IR và RED
int32_t spo2; // Tính từ IR và RED
int32_t heartRate; // Tính từ IR
// Bản chất là cờ báo kết quả tính được là rác (0) hay chuẩn (1)
int8_t validSPO2; 
int8_t validHR; 
// Bộ lọc nhiễu dữ liệu 
volatile uint8_t currentBPM = 0;
volatile uint8_t currentSpO2 = 0;

// =======================================================
// BIẾN TOÀN CỤC CHO BỘ LỌC CỬA SỔ TRƯỢT (SLIDING WINDOW)
// =======================================================
const int WINDOW_SIZE = 4;
int raw_spo2_window[WINDOW_SIZE] = {0};
int raw_bpm_window[WINDOW_SIZE] = {0};
int window_idx = 0;
int valid_sample_count = 0; // Đếm số mẫu có ý nghĩa đã nạp vào cửa sổ
// BIẾN TOÀN CỤC CỦA BỘ LỌC
float pre_ema_spo2 = 0;
float pre_ema_bpm = 0;
int last_bpm_avg = 0;
bool isSpo2Locked = false;
bool isBPMLocked = false;

void Calculate_FilterData(unsigned long currentMillis) {
  if (dataReady) {
    if (isAttached) {
      maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHR);
      // BƯỚC 1: LỌC RÁC VẬT LÝ VÀ CHẶN CỬA MPU
      if (validHR && validSPO2 && !isMoving && heartRate >= 40 && heartRate <= 150 && spo2 >= 80 && spo2 <= 100) {
        if (spo2 >= 99) spo2 = spo2 - 2; // Bù trừ offset 2
        else spo2 = spo2 - 1;
        // =======================================================
        // TUYỆT CHIÊU 1: BỘ LỌC THÔ TIỀN TRẠM (Gọt bớt gai nhọn)
        // =======================================================
        if (pre_ema_spo2 == 0 || pre_ema_bpm == 0) { // Nếu là nhịp đầu tiên
          pre_ema_spo2 = spo2;
          pre_ema_bpm = heartRate;
        } 
        else {
        // San phẳng 50% nhiễu so với nhịp trước đó
          pre_ema_spo2 = (pre_ema_spo2 * 0.5) + (spo2 * 0.5);
          pre_ema_bpm = (pre_ema_bpm * 0.5) + (heartRate * 0.5);
        }
        // BƯỚC 2: NẠP DỮ LIỆU ĐÃ ĐƯỢC GỌT GAI VÀO CỬA SỔ
        raw_spo2_window[window_idx] = round(pre_ema_spo2);
        raw_bpm_window[window_idx] = round(pre_ema_bpm);
        window_idx = (window_idx + 1) % WINDOW_SIZE; // Quay vòng index từ 0 đến 3
        if (valid_sample_count < WINDOW_SIZE) valid_sample_count++;
        // BƯỚC 3: CHỈ XÉT DUYỆT KHI CỬA SỔ ĐÃ ĐẦY (Đủ 4 mẫu)
        if (valid_sample_count >= WINDOW_SIZE) {
          int spo2_max = 0, spo2_min = 200;
          int bpm_max = 0, bpm_min = 300;
          int spo2_sum = 0, bpm_sum = 0;
          // Quét qua 4 phần tử để tìm Max, Min và Tổng
          for (int i = 0; i < WINDOW_SIZE; i++) {
            // Cho SpO2
            if (raw_spo2_window[i] > spo2_max) spo2_max = raw_spo2_window[i];
            if (raw_spo2_window[i] < spo2_min) spo2_min = raw_spo2_window[i];
            spo2_sum += raw_spo2_window[i];
            // Cho BPM
            if (raw_bpm_window[i] > bpm_max) bpm_max = raw_bpm_window[i];
            if (raw_bpm_window[i] < bpm_min) bpm_min = raw_bpm_window[i];
            bpm_sum += raw_bpm_window[i];
          }
          // BƯỚC 4: PHÁN QUYẾT LỌT KHE (LÕI THUẬT TOÁN)
          int current_bpm_avg = round((float)bpm_sum / WINDOW_SIZE);
          int spo2_diff = spo2_max - spo2_min;
          int bpm_diff = abs (current_bpm_avg - last_bpm_avg);
          // =======================================================
          // TUYỆT CHIÊU 2: KHE CỬA ĐỘNG (ADAPTIVE THRESHOLD)
          // =======================================================
          // Chưa chốt -> Mở rộng (4/10) để bắt nhanh. Đã chốt -> Siết chặt (2/6) để chống nhiễu
          int threshold_spo2 = isDataUnlocked ? 2 : 4; 
          int threshold_bpm = isDataUnlocked ? 6 : 10;
          if (spo2_diff <= threshold_spo2) {
            isSpo2Locked = true;
            currentSpO2 = round((float)spo2_sum / WINDOW_SIZE);
            Serial.printf("[Filter] ✅ CẬP NHẬT -> SpO2: %d\n", currentSpO2);
          }
          else Serial.printf("[Filter] ❌ Từ chối cập nhật! (Lệch SpO2: %d)\n", spo2_diff);
          if (bpm_diff <= threshold_bpm && last_bpm_avg != 0) {
            isBPMLocked = true;
            currentBPM = current_bpm_avg;
            Serial.printf("[Filter] ✅ CẬP NHẬT ->BPM: %d\n", currentBPM);
          }
          else Serial.printf("[Filter] ❌ Từ chối cập nhật! (Lệch BPM: %d)\n", bpm_diff);
          last_bpm_avg = current_bpm_avg;
          if (isSpo2Locked && isBPMLocked) {
            // -> LỌT KHE! Dữ liệu đã chụm lại, chốt trung bình cộng.
            if (!isDataUnlocked) {
              isDataUnlocked = true;
              Serial.println("\n>>> [LOGIC] TÌM THẤY MỐC ỔN ĐỊNH! MỞ KHÓA DATA BUNG SANG MODE 0 <<<");
            }
          }
        } 
        else Serial.printf("[Filter] Đang nạp mẫu vào cửa sổ (%d/%d)...\n", valid_sample_count, WINDOW_SIZE);
      }
      else {
        if (isMoving) {
          Serial.println("[Filter] ⚠️ Cử động tay (MPU)! Đóng cửa sổ, từ chối nạp data để chống nhiễu.");
        } else if (!validHR || !validSPO2) {
          Serial.println("[Filter] ⚠️ Dữ liệu rác (Vòng lỏng / Hở sáng).");
        } else {
          Serial.printf("[Filter] ⚠️ Data ngoài vùng sinh lý an toàn (HR: %d, SpO2: %d)\n", heartRate, spo2);
        }
      }
      // Chốt giá trị nếu chưa unlock thì bằng 0
      if (!isDataUnlocked) {
        currentBPM = 0;
        currentSpO2 = 0;
      }
    }
    // Dịch mảng IR/RED cho MAX30102 (Logic cũ giữ nguyên)
    for (byte i = 50; i < 100; i++) {
      irBuffer[i - 50] = irBuffer[i];
      redBuffer[i - 50] = redBuffer[i];
    }
    sampleIndex = 50;
    dataReady = false;
  }
  // XỬ LÝ KHI RỚT TAY (ĐÁNH SẬP BỘ LỌC)
  if (!isAttached) {
    isDataUnlocked = false;
    isSpo2Locked = false;
    isBPMLocked = false;
    pre_ema_spo2 = 0;
    pre_ema_bpm = 0;
    last_bpm_avg = 0;
    currentBPM = 0;
    currentSpO2 = 0;
    // Đục lủng đáy cửa sổ trượt, dọn dẹp sạch sẽ
    window_idx = 0;
    valid_sample_count = 0;
    for(int i = 0; i < WINDOW_SIZE; i++) {
        raw_spo2_window[i] = 0;
        raw_bpm_window[i] = 0;
    }
  }
}

// ---- Task MAX30102 ----
void Task_MAX30102(void *pvParameters) {
  for(;;) {
    unsigned long currentMillis = millis ();
    ReadSensor_CheckAttach (currentMillis);
    Calculate_FilterData (currentMillis);
    vTaskDelay(pdMS_TO_TICKS (20)); 
  }
}

// ===============================
// FUNCTION VÀ BIẾN TASK TINY ML
// ===============================

// ---- Function đọc cảm biến MPU và xử lý dữ liệu đầu vào ----
bool isFirstBoot = true; // Cờ báo lần chạy đầu tiên của hệ thống
const TickType_t xPeriod = pdMS_TO_TICKS(EI_CLASSIFIER_INTERVAL_MS); // Chu kỳ lấy mẫu 
const int bufferMPULength = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; // FRAMESIZE là số mẫu AI cần cho 1 lần ra phán đoán
float bufferMPU [bufferMPULength] = { 0 }; // Buffer chứa cặp dữ liệu MPU (amag - gmag) đưa vào AI
unsigned long lastMotionTime = 0; // Biến nhớ thời gian cử động cuối cùng
const unsigned long MOTION_COOLDOWN_MS = 2000; // Trễ 2 giây sau khi dừng tay mới cho đo lại

void Read_Process_MPU (void) {
  TickType_t xLastWakeTime = xTaskGetTickCount(); // Lấy mốc tgian của RTOS mỗi khi được gọi để bấm giờ cho việc lấy mẫu
  int start_index = 0; // Phần tử bắt đầu
  int elements_to_read = bufferMPULength; // Số lượng phần tử cần đọc
  // 1. Dịch mảng buffer (nếu kh phải lần đầu)
  if (!isFirstBoot) { // Đưa nửa cuối buffer lên đầu
    int half_buffer = bufferMPULength / 2; 
    for (int i = 0; i < half_buffer; i++) bufferMPU [i] = bufferMPU [i + half_buffer];
    start_index = half_buffer; 
    elements_to_read = half_buffer;
  }
  // 2. Đọc dữ liệu từ MPU gán vào buffer
  for (int i = 0; i < elements_to_read; i += 2) {
    // 2.1. Đợi tới đúng thời điểm thì đọc dữ liệu của MPU
    vTaskDelayUntil(&xLastWakeTime, xPeriod); // Tương tự vTaskDelay nhưng canh chính xác thời điểm, đảm bảo chu kỳ lấy mẫu kh bị sai lệch
    sensors_event_t a, g, temp; // a (acceleration): Gia tốc, bao gồm x,y,z
                                // g (gyroscope): Tốc độ góc, bao gồm x,y,z
                                // temp: Nhiệt độ, dùng để tính toán bù trừ sai số khi nóng lên, kh sdung đến nhưng cần phải có đủ cấu trúc lệnh      
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) { 
      mpu.getEvent(&a, &g, &temp); 
      xSemaphoreGive(i2cMutex); 
    }
    // 2.2. Tính amag - gmag để đưa vào mảng buffer theo cặp
    float amag = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)) / 9.80665; // Chia cho gia tốc trọng trg 9.8m/s^2 để chuẩn hóa về mốc 1
    float gmag = sqrt(sq(g.gyro.x) + sq(g.gyro.y) + sq(g.gyro.z)); // Kh có chuyển động thì ~= 0 rad/s
    // Cặp dữ liệu (amag-gmag) -> i tăng 2 mỗi vòng (i+=2)
    bufferMPU [start_index + i + 0] = amag; 
    bufferMPU [start_index + i + 1] = gmag; 
    // 2.3. Dựa vào amag - gmag tính được để suy ra trạng thái chuyển động để set cờ
    if (abs (amag - 1.0) > 0.2 || gmag > 0.5) { // Có chuyển động
      isMoving = true;
      lastMotionTime = millis (); // Reset time liên tục
    }
  }
  isFirstBoot = false; // Đổi cờ sau khi đọc dữ liệu dù là lần đầu hay kh thì sau bước này đều kh còn là lần đầu nữa
  if (isMoving && (millis () - lastMotionTime >= MOTION_COOLDOWN_MS)) isMoving = false; // Đã nằm yên được 2s thì reset cờ chuyển động
}

// ---- Function chẩn đoán chuyển động của TinyML ----
int abnormal_count = 0;
uint8_t lastMotionState = 0; // Biến lưu phán đoán AI ở vòng lặp trước đó
volatile uint8_t currentMotionState = 0; // Biến chốt kqua phán đoán AI hiện tại

void Predict_Movement_TinyML (void) {
  // 1. Chạy chẩn đoán dữ liệu từ mảng bufferMPU
  signal_t signal; // Tạo một cái hộp theo chuẩn Edge Impulse
  numpy::signal_from_buffer(bufferMPU, bufferMPULength, &signal); // Ép kiểu từ mảng dữ liệu buffer vào cái hộp vừa tạo
  ei_impulse_result_t result = { 0 }; // Tạo khay hứng kqua AI
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false); // Gửi đi hộp tín hiệu, lấy khay hứng kqua, false: kh in log quá trình phân tích
  if (res != EI_IMPULSE_OK) {
      Serial.printf("[TinyML LỖI] Mã lỗi: %d\n", res);
  }
  // ========================================================================================
  // THÊM ĐOẠN NÀY ĐỂ IN ĐỘ TRỄ (DELAY) CỦA AI VÀ BỘ LỌC FFT
  // ========================================================================================
  Serial.print("\n[AI TIMING] DSP: ");
  Serial.print(result.timing.dsp); // Thời gian chạy khối Spectral Analysis (DSP Block)
  Serial.print(" ms | Classification: ");
  Serial.print(result.timing.classification); // Thời gian mạng Neural Network suy luận
  Serial.print(" ms | Tong Delay: ");
  Serial.print(result.timing.dsp + result.timing.classification);
  Serial.println(" ms");
  // ========================================================================================
  // 2. Tìm nhãn có độ tin cậy cao nhất (Kqua trả về là label kèm theo xác suất, phải tự dò xem label nào có xác suất trùng khớp cao nhất)
  float max_val = 0.0; 
  int max_idx = 0;
  Serial.print("[TinyML Result] ");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.printf("%s: %.2f | ", result.classification[ix].label, result.classification[ix].value);
    if (result.classification[ix].value > max_val) { // Xét từng vtri phần tử trong mảng, vtri lớn hơn thì lưu lại, nhỏ hơn bỏ qua
      max_val = result.classification[ix].value; // Tìm được xác suất max
      max_idx = ix; // Vtri có xác suất max
    }
  }
  const char* predicted_label = result.classification[max_idx].label; // Sdung vtri tìm được ở trên để lấy chuỗi label chính xác
  if ((strcmp(predicted_label, "clonic") == 0 || strcmp(predicted_label, "tonic") == 0) && max_val >= 0.75) { 
    abnormal_count += 2;
  } 
  else { 
    abnormal_count -= 1; 
  }
  if (abnormal_count > 10) abnormal_count = 10;
  if (abnormal_count < 0) abnormal_count = 0;
  // Chốt trạng thái báo động
  if (abnormal_count >= 8) {
    currentMotionState = 1;
  }
  else if (abnormal_count <= 2) {
    currentMotionState = 0;
  }
  // Nếu kh tiếp xúc da hoặc dữ liệu chưa ổn định (Locked) thì kh xét AI
  if (!isAttached || !isDataUnlocked) {
    currentMotionState = 0;
    abnormal_count = 0;
  }
  if (currentMotionState == 1 && lastMotionState == 0) {
    Serial.println("[TinyML] ⚠️ CẢNH BÁO: KÍCH HOẠT BÁO ĐỘNG (AI State = 1)!");
  }
  lastMotionState = currentMotionState;
}

void Task_TinyML(void *pvParameters) {
  for(;;) {
    Read_Process_MPU ();
    Predict_Movement_TinyML ();
  }
}

//==============
// TASK LOGIC
//==============
const unsigned long WAIT_TIMEOUT = 30000; // Tgian chờ sau khi thức dậy
const unsigned long DROP_TIMEOUT = 20000;
volatile DeviceMode currentMode = MODE_WAIT; // Mode khi mới khởi động lần đầu hoặc khi mới tỉnh dậy là chờ xem có đeo kh
volatile uint8_t currentBattery = 0;
volatile bool currentChargingState = false; 
volatile bool isFakeData = false;
unsigned long sleepTime = millis (); 
unsigned long fakeDataTime = millis ();

void Task_LogicAndSend(void *pvParameters) {
  bool wasAttached = false; // FIX: Biến theo dõi khoảnh khắc VỪA MỚI chạm da
  for(;;) {
    int rawADC = analogRead(BAT_ADC_PIN);
    currentBattery = constrain(map((rawADC * 2.0 * 3.3 / 4095.0) * 100, 330, 400, 0, 100), 0, 100); 
    
    // 1. Logic trạng thái
    currentChargingState = (digitalRead(CHARGE_PIN) == HIGH);
    if (currentChargingState || forceSleepCmd) {
      Serial.println("[Logic] Yêu cầu chuyển sang MODE_SLEEP...");
      currentMode = MODE_SLEEP;
      enterDeepSleep(); 
    }

    switch(currentMode) {
      case MODE_WAIT:
        if (isDataUnlocked) {
          currentMode = MODE_ACTIVE;
          isFakeData = false; 
        } 
        else {
          if (isAttached) {
            // ĐANG CÓ DA: Giữ đồng hồ ngủ ở mốc hiện tại (Reset liên tục)
            sleepTime = millis(); 
            // Đếm thời gian Fake Data (30s)
            if (millis() - fakeDataTime > 60000 && fakeDataTime != 0) {
              Serial.println("[Logic] Đã qua tgian ép chốt mà data = 0 -> DA FAKE! Chuyển DROP!");
              isFakeData = true; 
              currentMode = MODE_DROPPED;
            }
          }
          else {
            // KHÔNG CÓ DA: Giữ đồng hồ Fake Data ở mốc hiện tại (Reset liên tục)
            fakeDataTime = millis(); 
            // Đếm thời gian Ngủ (30s)
            if (millis() - sleepTime > WAIT_TIMEOUT && sleepTime != 0) {
              Serial.println("[Logic] Quá tgian chờ (Không có da). Buộc ngủ...");
              forceSleepCmd = true; 
            }
          }
        }
        break;

      case MODE_ACTIVE:
        isFakeData = false; 
        sleepTime = millis();    // Neo đồng hồ ngủ
        fakeDataTime = millis(); // Neo đồng hồ Fake Data

        if (!isAttached) {
          if (DropCountTime != 0 && (millis() - DropCountTime > DROP_TIMEOUT)) {
            Serial.println("[Logic] Thiết bị rớt thật. Chuyển MODE_DROPPED!");
            currentMode = MODE_DROPPED; 
          }
        } 
        else {
          if (!isDataUnlocked) {
             Serial.println("[Logic] Data nát/Da ảo! Đưa về WAIT chờ bộ lọc ép chốt...");
             currentMode = MODE_WAIT;
          }
        }
        break;

      case MODE_DROPPED:
        if (!isAttached) {
           isFakeData = false; // Nhấc lên khỏi bàn là xóa án Fake
        }
        if (isAttached && !isFakeData) { 
          Serial.println("[Logic] Đeo lại đàng hoàng. Về WAIT leo dốc lại!");
          currentMode = MODE_WAIT; 
          sleepTime = millis();    // Reset sạch sẽ khi quay lại
          fakeDataTime = millis(); // Reset sạch sẽ khi quay lại
          DropCountTime = 0; 
        }
        break;
    }

    // Chốt trạng thái chạm da của vòng lặp này để vòng sau so sánh
    wasAttached = isAttached; 
    // 2. Chốt dữ liệu gửi đi
    myData.mode = (uint8_t)currentMode;
    myData.bat = currentBattery;
    myData.charge = currentChargingState;
    if (currentMode == MODE_WAIT || currentMode == MODE_DROPPED) {
      myData.spo2 = 0; 
      myData.bpm = 0; 
      myData.state = 0;
    } 
    else {
      myData.spo2 = currentSpO2; 
      myData.bpm = currentBPM; 
      myData.state = currentMotionState;
    }
    Serial.printf("[Logic] Gửi: Mode %d | Pin %d%% | SpO2 %d | BPM %d | AI %d\n", myData.mode, myData.bat, myData.spo2, myData.bpm, myData.state);
    esp_now_send(gatewayAddress, (uint8_t *) &myData, sizeof(myData));
    vTaskDelay(1200 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// HÀM KHỞI TẠO MPU CHO CHẾ ĐỘ THỨC (AI)
// ==========================================
void setupMPU() {
  Serial.println("\n=======================================");
  Serial.println("   KHỞI TẠO MPU6500 (CHẾ ĐỘ THỨC)");
  Serial.println("=======================================\n");
  // 1. Ép tốc độ bus I2C về lại mức an toàn (100kHz) để gọi hồn con MPU
  Wire.setClock(100000); 
  delay(10);
  // 2. Gửi lệnh Reset Phần Cứng cực mạnh (Đánh thức Zombie)
  Serial.print("[Setup] Dang gui lenh ep Reset MPU... ");
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    Wire.beginTransmission(0x68);
    Wire.write(0x6B); // Thanh ghi PWR_MGMT_1
    Wire.write(0x80); // Set bit 7 (Device Reset) lên 1
    Wire.endTransmission();
    xSemaphoreGive(i2cMutex);
  }
  delay(100); // Chờ 100ms cho con MPU xả điện và khởi động lại lõi silicon
  Serial.println("Xong!");
  Serial.print("[Setup] Cho Adafruit nhan dien chip... ");
  bool mpu_ok = false;
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    mpu_ok = mpu.begin(0x68, &Wire); // Lúc này MPU đã hoàn toàn tỉnh táo
    xSemaphoreGive(i2cMutex);
  }
  if (!mpu_ok) {
    Serial.println("❌ THẤT BẠI! Kiem tra day SDA/SCL.");
    return; 
  }
  Serial.println("✅ OK.");
  if (xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    // Cài đặt ngắt
    writeRegister(0x37, 0xC0);
    writeRegister(0x38, 0x00);
    readRegister(0x3A);
    xSemaphoreGive(i2cMutex);
  }
  // 3. Trả lại tốc độ 400kHz cho bus I2C để mạch chạy AI cho lẹ
  Wire.setClock(400000);
}

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.println("\n==========================");
  Serial.println("   KHỞI ĐỘNG VÒNG TAY C3");
  Serial.println("==========================\n");
  pinMode(MPU_INT_PIN, INPUT_PULLUP);
  pinMode(CHARGE_PIN, INPUT_PULLDOWN); 
  analogReadResolution(12); // Set bộ ADC 12 bit
  i2cMutex = xSemaphoreCreateMutex(); // Tạo key Semaphore cho I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  setupMPU ();
  Serial.print("[Setup] Khởi tạo MAX30102... ");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ THẤT BẠI!");
  } else {
    Serial.println("✅ Thành công.");
    particleSensor.setup(63, 8, 2, 200, 411, 4096); // (Độ sáng LED, Lấy mẫu trung bình, Mode sáng 1 hay 2 đèn (IR và RED), Tần số lấy mẫu, Độ rộng xung, ADC Range - Thang đo của cảm biến)
  }
  WiFi.mode(WIFI_STA);
  Serial.println("\n[Setup] Đang dò Kênh Wi-Fi của Gateway (U.S.P.C.C)...");
  int gatewayChannel = getWiFiChannel("U.S.P.C.C");
  if (gatewayChannel != 0) {
    Serial.printf("[Setup] ✅ Tuyệt vời! Tìm thấy Wi-Fi ở Kênh %d.\n", gatewayChannel);
    esp_wifi_set_channel(gatewayChannel, WIFI_SECOND_CHAN_NONE);
  } 
  else {
    Serial.println("[Setup] ⚠️ Không tìm thấy mạng U.S.P.C.C! Dùng Kênh 1.");
  }
  if (esp_now_init() != ESP_OK) ESP.restart();
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayAddress, 6);
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  Serial.println("\n[Setup] Kích hoạt các luồng xử lý...\n");
  xTaskCreate(Task_MAX30102, "MAX", 4096, NULL, 3, NULL);
  xTaskCreate(Task_TinyML, "AI", 16384, NULL, 2, NULL); 
  xTaskCreate(Task_LogicAndSend, "SEND", 4096, NULL, 4, NULL);
  vTaskDelete(NULL);
}

void loop() {}