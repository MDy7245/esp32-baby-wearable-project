#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h> 
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <String.h>

// ==========================================
// 1. CHÂN PHẦN CỨNG & CẤU HÌNH CƠ BẢN
// ==========================================
const int PIN_BTN = 14;     
const int PIN_BUZZER = 13;  
const int PIN_LED = 4;      
const int NUM_LEDS = 4;     

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800); // Cấu hình LED

// Cấu hình SIM A7680C (Serial2)
const int RXD2 = 16;
const int TXD2 = 17;
String alertPhoneNumber = "";

uint8_t c3Address[] = {0x08, 0x92, 0x72, 0x8F, 0x76, 0x84}; // Địa chỉ MAC C3

// ID và PASS WiFi
const char* ssid = "LAPTOP-Yii";
const char* password = "12345678990";

// ==========================================
// 2. THÔNG TIN FIREBASE & BỘ NHỚ LƯU TRỮ
// ==========================================
#define FIREBASE_DATABASE_URL "https://baby-wearable-9a2f6-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_PROJECT_ID "baby-wearable-9a2f6"
#define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-fbsvc@baby-wearable-9a2f6.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDrw4UFiL7Hy+X6\npuOJ7bX6G6ocYHPfD7hnk5ombDifyZIiGza8PVOrNTmiUqys3Laem8qP0hG9SvuM\nvL2ZtBf7EsMbRgv45sWaDqh/+lhE3GbKdOr3taYEaCnxwRWEQRkZA1OeEQ5pnngw\naVi7Bz0GF0BpboSmIfolg7LI3wlMSPxYeF8L/MfP4lcrPEAR42JE2ZvB7YX7KXQW\nA5J7H3gZSGCkR7Zqp3G+unos1RrUWDQLm1aNEgmlxyGPGxRMxvI8/5IIiHSBl5Sm\ncZy4nRptiq8oFwKFDBxzb5wDGT7ICUbr7HafMmRT0rPO1AWf7cJ5a8qf2aCu+PQr\nLMu1ONYJAgMBAAECggEABYPMT8XUGJ5XYdaXiflqgVA2WXWUIEqg2KNmPrHXXT18\n0plqXVrd4ypaj8Z5TY3YI7unMFgMnC3t1mvvz3GcAf59W9z2mLrlMmZXYK79c5bo\nPKIhgOvZ106MdKA8nZxcNu6R/SeO+79+NjuwnKddsQ/ILO8t4Sj4TfpXq0U0VXlw\nFw8vzhko688MSOIBmzqvL5D8xNUvVgD8Y+r2S4jZzJf8IXUbfBoHXjF5uqYQ+ElQ\n2ua5DM/CbBw+Dv04yfFY4PInmmk/C+wNAO5a+8KY3cpgAasBKcHB4ZL4mY3jaWGY\nT/3Fk9SYE/dTE6xfncMbOarw2YzqMXlaeFOk1dAURQKBgQD5K61TYnl2F+RIexq7\ngaRVxJGbozEAzMCIt6XEYY/VAyMpskIhxdmYsim6OV3OQ4QwlhSQM1Ex9uikeTDx\nMSs3XjA+8ANEk66RTHmMWHHIKQsGzzXryypX7I0VeH0ZL6LBGybkNjFgctzXswEN\n2VEuyW1HYNnzqehCLopD2i6ZLwKBgQDyOcW5dD+EUqIHWhS+ozwkAVZ5DYs6q8SI\nW6K80Q/U8XFozWkeNgCJBb6vkKPzKmnzDrq3u6SU6F7gvdzQlf3I2UskPkkpDfTh\nyfYTBztqP/MVYuWgM3dJZdhPWJj6zNXKhMalWMbqvH0d53rqD0zBpfJnG/g0OBLg\nIVqyXM7GRwKBgQCATjhjTrsCz4yysgly8m//5kegYCk0ozqlbAPFGwPoiUQLDYq2\nP14sHdoU1cNzGhswtaeDFZnC48SqJOnJ2SrKyQqI7iiQoIdstHeGiGiWzGOsLvky\nIRz9x4ZivveUB46EZ4ngS/OjGaKUw81QJ06BjCFkdv/kU+KQacyy4d/K6wKBgGAq\npaBqUalUMTXgAqppHkBhM5ad1O/3L/C/CM4T5kgxj0f/fUNcRzwfRsRnFWA+L5Ar\nAeBeJApmcvyhBGDfm8E428dI3zzoaJeX2hVV4rXdK8IK4IWIyUwfnhBqaVnxJtPf\ngQo2Q8sACGBt/XOdnWioXKOpDBKXTI8lTt/PTtyTAoGAO3DFGryZNQcByZxsg8lJ\n6aWFZjobg0usk3+xEQ7lfcYzjqCZ986v2Rd8O4QYTBONw2qf/wEWUl3BfVOHEohD\npR040xChBQGLFFp4WhB2+C3sVtPQoOQmCDPKh/Peg1KSbJqNSN4uwo4pkz9/mhfv\nhirSM5g1kBNUx2e/IjBVB8U=\n-----END PRIVATE KEY-----\n";
FirebaseData fbdo_rtdb; // Giống xe tải vận chuyển gói hàng
FirebaseData fbdo_read; 
FirebaseAuth auth;
FirebaseConfig config;

Preferences preferences;

// ===================================================================
// 3. CẤU TRÚC DỮ LIỆU GỬI - NHẬN (ESP NOW) VÀ CÁC TRẠNG THÁI GATEWAY
// ===================================================================
typedef struct struct_message { 
  uint8_t spo2; 
  uint8_t bpm; 
  uint8_t bat; 
  bool charge; 
  uint8_t state; // AI = 0: Normal, AI = 1: Co giật
  uint8_t mode;  // 0: Normal, 1: Waiting, 2: Sleep, 3: Dropped (Mode C3)
} struct_message; // Cấu trúc gói tin nhận từ C3

typedef struct struct_send { 
  char cmd[10]; 
} struct_send; // Cấu trúc gói tin gửi cho C3

enum SystemState { WAITING_FIRST_DATA, NORMAL_STATE, ALARM_LV1, ALARM_LV2, ALARM_LV3, STANDBY };  // Các state của hệ thống
SystemState currentState = WAITING_FIRST_DATA;  // Trạng thái chính thức của Gateway (mới khởi động mặc định là WAITING_FIRST_DATA)

// =====================================
// 4. BIẾN VÀ CÁC FUNCTION HỖ TRỢ TASK
// =====================================
// ---- Function set màu LED WS2812B ----
void setLEDColor(int index, uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(index, strip.Color(r, g, b));
}

// ---- Function xử lý gói tin nhận từ C3 ----
volatile uint8_t currentSpO2 = 0, currentBPM = 0;
volatile uint8_t currentBat = 0;
volatile uint8_t currentAIState = 0;  // Kết quả AI (0: Normal, 1: Co giật)
volatile uint8_t currentMode = 1; // Mode cua C3 (0: Normal, 1: Waiting, 2: Sleep, 3: Dropped)
volatile bool currentCharge = false;  // Cờ báo sạc
volatile unsigned long lastDataReceivedTime = 0;  // Biến tgian lần cuối nhận gói tin từ C3

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  struct_message incomingData;
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  currentSpO2 = incomingData.spo2; 
  currentBPM = incomingData.bpm;
  currentBat = incomingData.bat; 
  currentCharge = incomingData.charge;
  currentAIState = incomingData.state; 
  currentMode = incomingData.mode;
  lastDataReceivedTime = millis(); 
  Serial.printf("\n>>> [ESP-NOW] SpO2: %d | BPM: %d | Pin: %d%% | AI: %d | Mode: %d\n", currentSpO2, currentBPM, currentBat, currentAIState, currentMode);
}

// ---- Function xử lý thao tác nhấn ----
int clickCount = 0;
int currentBtnState = HIGH, lastBtnState = HIGH;  
bool isBtnHolding = false;
bool isMuted = false;
unsigned long btnPressTime = 0;
unsigned long lastDebounceTime = 0;
volatile uint8_t simulationMode = 0;  // Mode giả lập SPO2, BPM (0: Normal, 1: LV2, 2: LV3)

void handleButton (unsigned long currentMillis) {
  int reading = digitalRead(PIN_BTN);
  // Lọc nhiễu debounce nút nhấn
  if (reading != lastBtnState) lastDebounceTime = currentMillis;  // Nếu có debounce tgian sẽ làm mới liên tục ở đây
  if ((currentMillis - lastDebounceTime) > 50) { // Sau 50ms kh còn debounce nữa tức là nút nhấn đã ổn định
    if (reading != currentBtnState) { // Kiếm tra trạng thái nút có thay đổi
      currentBtnState = reading;  
      if (currentBtnState == LOW) {
        clickCount++;
        btnPressTime = currentMillis;
        isBtnHolding = true; 
      } 
      else {
        btnPressTime = currentMillis;
        isBtnHolding = false; 
      }
    }
  }
  lastBtnState = reading; // Lưu trạng thái cuối cùng của nút nhấn sau mỗi vòng lặp xử lý
  if (isBtnHolding && currentBtnState == LOW && (currentMillis - btnPressTime >= 3000)) { 
    simulationMode++; // Chuyển mode
    if (simulationMode > 2) simulationMode = 0; // Sau mode 2 thì về lại mode 0
    Serial.printf("\n>>> CHUYỂN MODE GIẢ LẬP: %d <<<\n", simulationMode);
    if (simulationMode == 0) { 
      for (int i = 0; i < 3; i++) { // Về mode 0 thì 3 còi ngắn
        digitalWrite(PIN_BUZZER, HIGH); 
        vTaskDelay(100 / portTICK_PERIOD_MS); 
        digitalWrite(PIN_BUZZER, LOW); 
        vTaskDelay(100 / portTICK_PERIOD_MS); 
      }  
    } 
    else { 
      for (int i = 0; i < 2; i++) { // Chuyển mode thì 2 còi ngắn
        digitalWrite(PIN_BUZZER, HIGH); 
        vTaskDelay(100 / portTICK_PERIOD_MS); 
        digitalWrite(PIN_BUZZER, LOW); 
        vTaskDelay(100 / portTICK_PERIOD_MS); 
      } 
    }
    isBtnHolding = false;
    clickCount = 0;
    btnPressTime = currentMillis + 999999; // Đẩy mốc tgian đến tương lai để làm sai điều kiện >= 5000 (reset)
  }
  if (clickCount > 0 && !isBtnHolding && (currentMillis - btnPressTime) > 400) {
    if (clickCount == 1) { // Nếu nhấn 1 lần là mute còi báo động
      isMuted = true; 
      digitalWrite (PIN_BUZZER, HIGH);
      vTaskDelay (pdMS_TO_TICKS (50));
      digitalWrite (PIN_BUZZER, LOW);
      Serial.println(">>> ĐÃ XÁC NHẬN TÌNH HUỐNG, NGƯNG BÁO ĐỘNG <<<"); 
    } 
    else if (clickCount >= 2) { // Nhấn 2 lần thì vào chế độ SLEEP
      digitalWrite (PIN_BUZZER, HIGH);
      vTaskDelay (pdMS_TO_TICKS (50));
      digitalWrite (PIN_BUZZER, LOW);
      vTaskDelay (pdMS_TO_TICKS (50));
      digitalWrite (PIN_BUZZER, HIGH);
      vTaskDelay (pdMS_TO_TICKS (50));
      digitalWrite (PIN_BUZZER, LOW);
      struct_send sendCmd; 
      strcpy(sendCmd.cmd, "SLEEP"); 
      esp_now_send(c3Address, (uint8_t *) &sendCmd, sizeof(sendCmd)); 
    }
    clickCount = 0; // Reset biến đếm
  }
}

// ---- Function tạo Simulation Mode ----
uint8_t evalSpO2, evalBPM;  // eval (evaluation) có ý nghĩa là giá trị của biến đã được xác định hoặc tính toán từ trước, kh phải tự nhiên mà có
uint8_t evalBat;
uint8_t evalAI;
uint8_t evalMode;
bool evalCharge; 
unsigned long evalLastDataTime;

void prepareEvalData (unsigned long currentMillis) {
  if (simulationMode == 0) {
    evalSpO2 = currentSpO2;
    evalBPM = currentBPM;
    evalBat = currentBat;
    evalAI = currentAIState;
    evalMode = currentMode;
    evalCharge = currentCharge;
    evalLastDataTime = lastDataReceivedTime;
  }
  else {
    evalBat = currentBat; 
    evalAI = 0;
    evalMode = 0; 
    evalCharge = currentCharge; 
    evalLastDataTime = currentMillis; 
    if (simulationMode == 1) { // Giả lập LV2
      if ((currentMillis / 20000) % 2 == 0) {  // 20s chẵn: Báo SPO2 + BPM
        evalSpO2 = 91; 
        evalBPM = 127; 
      }
      else {  // 20s lẻ: Báo BPM
        evalSpO2 = 95; 
        evalBPM = 62; 
      }
    } 
    if (simulationMode == 2) { // Giả lập LV3
      if ((currentMillis / 60000) % 2 == 0) {  // 60s chẵn: Báo SPO2 + BPM
        evalSpO2 = 85; 
        evalBPM = 51; 
      }
      else {  // 60s lẻ: Báo SpO2
        evalSpO2 = 88; 
        evalBPM = 90; 
      }
    }
  }
}

// ---- Function cập nhật trạng thái Gateway ----
bool isDisconnected = false;  // Cờ báo mất kết nối C3
bool isLowBattery = false; // Cờ báo pin yếu
// Cờ báo dữ liệu vượt ngưỡng an toàn
bool israwDangerSpO2 = false;
bool israwDangerBPM = false;
// Cờ báo dữ liệu ở mức sát ngưỡng an toàn
bool israwWarningSpO2 = false;
bool israwWarningBPM = false;
unsigned long dangerStartTime = 0; // Biến đếm tgian trước khi báo động dữ liệu vượt ngưỡng (SPO2, BPM)
unsigned long warningStartTime = 0; // Biến đếm tgian trước khi báo động dữ liệu sát ngưỡng (SPO2, BPM)
const unsigned long FILTER_TIME_MS = 5000;  // Tgian lọc nhiễu trạng thái
unsigned long lastAlarmTime = 0; // Biến đếm tgian báo động trước khi chuyển về Normal

void updateStateGateway (unsigned long currentMillis) {
  // Setup cờ mỗi vòng lặp để đánh giá trạng thái
  isDisconnected = (evalLastDataTime == 0 && simulationMode == 0) || (evalLastDataTime > 0 && currentMillis - evalLastDataTime > 5000); 
  isLowBattery = (evalBat < 20 && !evalCharge);  
  // Điều kiện để nếu data thay đổi quanh mốc 90, cờ danger reset liên tục nhưng cờ warning vẫn kh bị reset -> đảm bảo báo động
  israwDangerSpO2 = (evalSpO2 > 0 && evalSpO2 < 90);
  israwDangerBPM = (evalBPM > 130 || (evalBPM > 0 && evalBPM < 60));
  israwWarningSpO2 = (evalSpO2 > 0 && evalSpO2 < 93);
  israwWarningBPM = (evalBPM > 120 || (evalBPM > 0 && evalBPM < 70));  

  // Set bộ đếm giờ lọc cảnh báo giả
  if (israwDangerSpO2 || israwDangerBPM) {
    if (dangerStartTime == 0) dangerStartTime = currentMillis;
  }
  else dangerStartTime = 0;
  if (israwWarningBPM || israwWarningSpO2) {
    if (warningStartTime == 0) warningStartTime = currentMillis;
  }
  else warningStartTime = 0;

  // Xét theo mức độ ưu tiên
  SystemState nextState;
  // 1. Đợi gói tin đầu tiên sau khi khởi động lần đầu hoặc khi C3 mới được đánh thức
  if ((evalLastDataTime == 0 && simulationMode == 0) || evalMode == 1) {
    nextState = WAITING_FIRST_DATA;
  }
  // 2. Chỉ số vượt ngưỡng an toàn / Có dấu hiệu co giật
  else if (evalAI == 1 || ((israwDangerBPM || israwDangerSpO2) && dangerStartTime != 0 && (currentMillis - dangerStartTime) >= FILTER_TIME_MS)) {
    nextState = ALARM_LV3;
  }
  // 3. Chỉ số đang sát ngưỡng an toàn / Thiết bị đeo bị tuột rớt
  else if (((israwWarningBPM || israwWarningSpO2) && warningStartTime != 0 && (currentMillis - warningStartTime) >= FILTER_TIME_MS) || evalMode == 3) {
    nextState = ALARM_LV2;
  }
  // 5. C3 chuyển sang Sleep Mode
  else if (evalMode == 2) nextState = STANDBY;
  // 4. Mất kết nối
  else if (isDisconnected || isLowBattery) nextState = ALARM_LV1;
  // 6. Không có gì thì Normal
  else nextState = NORMAL_STATE;

  // Nếu là trạng thái báo động thì chốt ngay và làm mới tgian liên tục
  if (nextState == ALARM_LV1 || nextState == ALARM_LV2 || nextState == ALARM_LV3) {
    if (currentState != nextState) {
      currentState = nextState;
      isMuted = false;
    }
    lastAlarmTime = currentMillis;
  }
  // Nếu từ báo động chuyển sang Normal thì giữ báo động thêm 10s rồi mới chuyển trạng thái
  else if (nextState == NORMAL_STATE && (currentState == ALARM_LV1 || currentState == ALARM_LV2 || currentState == ALARM_LV3)) {
    if (currentState != nextState && currentMillis - lastAlarmTime >= 10000) {
      currentState = nextState;
      isMuted = false;
    }
  }
  // Các trạng thái Waiting hay Standby thì cứ cập nhật kh cần giữ
  else {
    if (currentState != nextState) {
      currentState = nextState;
      isMuted = false;
    }
  }
}

// ---- Function hiệu ứng LED ----
//Biến điều khiển LED
bool ledFlashState = false;
float breatheAngle = 0.0; 
uint8_t breatheValue = 0;
unsigned long previousLedMillis = 0;
// Biến điều khiển Buzzer
bool buzzerState = false;
unsigned long previousBuzzerMillis = 0;
unsigned long BREATHE_PERIOD = 4000; // Chu kỳ sáng LED là 4s

void handleEffectLED (unsigned long currentMillis) {
  // Bộ tạo hiệu ứng LED thở: Vì càng gần 255 mắt càng khó nhận thấy sự thay đổi màu sắc, hàm e mũ sin tạo ra sử thay đổi gấp khi càng gần điểm max
  // Sử dụng hàm e^Sin(x) -> max: e^1 * 108 ~ 254 và min e^(-1) - 0.367...(~ e^(-1)) để ép về min = 0 vì độ sáng LED tính từ 0 - 255
  breatheAngle = (float) (currentMillis % BREATHE_PERIOD) / BREATHE_PERIOD * 2.0 * PI; 
  breatheValue = (exp(sin(breatheAngle)) - 0.36787944) * 108.0;
  
  // Bộ đếm tgian chớp tắt LED (chu kỳ 1s)
  if (currentMillis - previousLedMillis >= 500) { 
    previousLedMillis = currentMillis; 
    ledFlashState = !ledFlashState; 
  } 
}

// ==================================================
// 4. TASK SIM: THỰC HIỆN CUỘC GỌI, GỬI SMS (CORE 1)
// ==================================================
// ---- Cờ báo đã gửi SMS ----
bool smsLV1Sent = false;
bool smsLV2Sent = false;
bool smsLV3Sent = false;
// ---- Cờ báo  Call ----
bool isCallSent_LV1 = false;
bool isCallSent_LV2 = false;
bool isCallSent_LV3 = false;
// ---- Timer đếm tgian ----
unsigned long lastCallSent_LV1 = 0, lastCallSent_LV2 = 0, lastCallSent_LV3 = 0;
const unsigned long DELAY_CALL_TIME_LV1_MS = 60000; // Tgian chờ trước khi chuyển từ SMS sang nhá máy
const unsigned long DELAY_CALL_TIME_LV2_MS =  30000; // Tgian chờ trước khi chuyển từ SMS sang thực hiện cuộc gọi
const unsigned long COOLDOWN_TIME_MS = 60000; // Tgian nghỉ giữa 2 lần gọi
// ---- Tận dụng logic sửi sms của SIM để tạo message cho App
volatile bool pendingFirebaseAlert = false; // Cờ báo có message mới
char alertDetailMessage [100] = ""; // Chuỗi message

void Task_SIM (void *pvParameters) {
  for (;;) {
    unsigned long currentMillis = millis ();
    // ---- Trạm nghe Serial từ Module SIM (Đồng thời cũng là dọn rác Serial2 khi kh có SMS) ----
    if (Serial2.available ()) { // Ktra xem có ký tự nào được gửi đến kh
      // 1. Đọc dữ liệu đến khi hết dòng: Tìm mã +CMT (mã báo có tin nhắn tới) trong Header
      String incoming_header = Serial2.readStringUntil ('\n');
      if (incoming_header.indexOf ("+CMT") != -1) { // .indexOf ("") tra ve -1 neu kh co 
        // 2. Chuẩn bị chờ đọc ndung tin nhắn (Payload), nếu chưa có thì tạm nhường CPU cho task khác, chờ tối đa 1s nếu kh có coi như rớt gói tin hoặc tin rác
        unsigned long waitting = currentMillis;
        while (!Serial2.available () &&  (millis () - waitting) < 1000) {
          vTaskDelay (pdMS_TO_TICKS (50));
        }
        // 3. Nếu có Payload sẽ thoát khỏi vòng while, đọc ndung và xử lý
        String incoming_msg = Serial2.readStringUntil ('\n');
        incoming_msg.trim (); // Xóa hết khoảng cách và dấu Enter
        incoming_msg.toUpperCase ();  // In hoa tất cả ký tự
        Serial.printf (">>> NỘI DUNG SMS NHẬN ĐƯỢC LÀ [%s] <<<\r\n", incoming_msg.c_str ());
        if ((incoming_msg == "Y" || incoming_msg == "YES") && (currentState == ALARM_LV1 || currentState == ALARM_LV2)) {
          isMuted = true;
          digitalWrite (PIN_BUZZER, HIGH);
          vTaskDelay (pdMS_TO_TICKS (50));
          digitalWrite (PIN_BUZZER, LOW);
          Serial.println (">>> ĐÃ BẬT MUTE QUA SMS <<<");
        }
      }
    }
    // ---- Logic gửi SMS và Call ---- 
    currentMillis = millis ();
    switch (currentState) {
    case ALARM_LV3:
      if (smsLV3Sent == false) {
        Serial.println (">>> ĐANG GỬI SMS BÁO ĐỘNG LV3 <<<");
        String msgLV3 = "CANH BAO LV3:\r\n";
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy trước
        vTaskDelay (pdMS_TO_TICKS (1000));
        // Gửi 1 tin SMS chứa thông tin cảnh báo
        if (israwDangerBPM) {
          msgLV3 += "Nhip tim: "; msgLV3 += String(evalBPM); msgLV3 += "BPM\r\n";
        }
        if (israwDangerSpO2) {
          msgLV3 += "SpO2: "; msgLV3 += String(evalSpO2); msgLV3 += "%%\r\n";
        }
        if (evalAI == 1) {
          msgLV3 += "Phat hien co giat!\r\n";
          // Soạn message gửi cho app
          strcpy (alertDetailMessage, "Có dấu hiệu co giật!"); 
          pendingFirebaseAlert = true;
        }
        else {
          strcpy (alertDetailMessage, "Sinh hiệu nguy kịch!"); 
          pendingFirebaseAlert = true;
        }
        Serial2.printf ("AT+CMGS=\"%s\"\r\n", alertPhoneNumber.c_str());
        vTaskDelay (pdMS_TO_TICKS (100));
        Serial2.print (msgLV3);
        vTaskDelay (pdMS_TO_TICKS (100));
        Serial2.write (26);
        smsLV3Sent = true;
        Serial.println(">>> DANG DOI SMS GUI DI (5s)...");
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
      // Gọi điện báo động
      if (!isMuted && !isCallSent_LV3 && (currentMillis - lastCallSent_LV3 >= COOLDOWN_TIME_MS || lastCallSent_LV3 == 0)) {
        lastCallSent_LV3 = currentMillis;
        Serial.println (">>> ĐANG GỌI ĐIỆN BÁO ĐỘNG LV3 <<<");
        Serial2.printf("ATD%s;\r\n", alertPhoneNumber.c_str());
      }
      if (isMuted && !isCallSent_LV3) {
        isCallSent_LV3 = true;
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy
        vTaskDelay (pdMS_TO_TICKS (1000));
      }
      break;
    case ALARM_LV2:
      if (smsLV2Sent == false) {
        Serial.println (">>> ĐANG GỬI SMS CẢNH BÁO LV2 <<<");
        String msgLV2 = "CANH BAO LV2:\r\n";
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy
        vTaskDelay (pdMS_TO_TICKS (1000));
        if (israwWarningBPM) {
          msgLV2 += "Nhip tim:"; msgLV2 += String(evalBPM); msgLV2 += "BPM\r\n";
          // Soạn message gửi cho app
          pendingFirebaseAlert = true;
          strcpy (alertDetailMessage, "Nhịp tim sát ngưỡng an toàn!"); 
        }
        if (israwWarningSpO2) {
          msgLV2 += "Nong do Oxy:"; msgLV2 += String(evalSpO2); msgLV2 += "%%\r\n";
          // Soạn message gửi cho app
          pendingFirebaseAlert = true;
          strcpy (alertDetailMessage, "Nồng độ Oxy sát ngưỡng an toàn!");
        }
        if (evalMode == 3) {
          msgLV2 += "Vong deo bi tuot (khong co du lieu), kiem tra thiet bi!\r\n";
        }
        // Gửi tin SMS cảnh báo trước
        Serial2.printf ("AT+CMGS=\"%s\"\r\n", alertPhoneNumber.c_str());
        vTaskDelay (pdMS_TO_TICKS (100));
        msgLV2 += "Gui Y(Yes) de xac nhan, sau 30s he thong se goi dien bao dong.";
        Serial2.print (msgLV2);
        vTaskDelay (pdMS_TO_TICKS (100));
        Serial2.write (26);
        Serial.println(">>> DANG DOI SMS GUI DI (5s)...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        smsLV2Sent = true;
      }
      // Gọi điện báo động nếu kh có Mute sau khoảng tgian
      if (lastCallSent_LV2 == 0) lastCallSent_LV2 = currentMillis;
      if (!isCallSent_LV2 && currentMillis - lastCallSent_LV2 >= DELAY_CALL_TIME_LV2_MS) {
        isCallSent_LV2 = true;
        Serial.println (">>> ĐANG GỌI ĐIỆN BÁO ĐỘNG LV2 <<<");
        Serial2.printf("ATD%s;\r\n", alertPhoneNumber.c_str());
      }
      if (isMuted) {
        isCallSent_LV2 = true;
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy
        vTaskDelay (pdMS_TO_TICKS (1000));
      }
      break;
    case ALARM_LV1:
      if (smsLV1Sent == false) {
        Serial.println (">>> ĐANG GỬI SMS CẢNH BÁO LV1 <<<");
        String msgLV1 = "CANH BAO LV1:\r\n";
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy
        vTaskDelay (pdMS_TO_TICKS (1000));
        if (isLowBattery) {
          msgLV1 += "Muc pin hien tai:"; msgLV1 += String(evalBat); msgLV1 += "%%\r\n";
          msgLV1 += "Cam sac thiet bi!\r\n";
        }
        if (isDisconnected) {
          msgLV1 += "Mat ket noi thiet bi deo, kiem tra thiet bi!\r\n";
        }
        // Gửi tin SMS cảnh báo lần đầu
        Serial2.printf ("AT+CMGS=\"%s\"\r\n", alertPhoneNumber.c_str());
        vTaskDelay (pdMS_TO_TICKS (100));
        msgLV1 += "Gui Y(Yes) de xac nhan, sau 5p he thong se goi dien bao dong.";
        Serial2.print (msgLV1);
        vTaskDelay (pdMS_TO_TICKS (100));
        Serial2.write (26);
        Serial.println(">>> DANG DOI SMS GUI DI (5s)...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        smsLV1Sent = true;
      }
      // Gọi điện báo động sau tgian kh có xác nhận
      if (lastCallSent_LV1 == 0) lastCallSent_LV1 = currentMillis;
      if (!isCallSent_LV1 && currentMillis - lastCallSent_LV1 >= DELAY_CALL_TIME_LV1_MS) {
        isCallSent_LV1 = true;
        Serial.println (">>> ĐANG GỌI ĐIỆN BÁO ĐỘNG LV1 <<<");
        Serial2.printf("ATD%s;\r\n", alertPhoneNumber.c_str());
      }
      if (isMuted) {
        isCallSent_LV1 = true;
        Serial2.printf ("AT+CHUP\r\n"); // Gác máy
        vTaskDelay (pdMS_TO_TICKS (1000));
      }
      break;
    case NORMAL_STATE:
      // Nếu tình huống được can thiệp và trở về bthg, trước đó có thực hiện cuộc gọi thì gửi lệnh cúp máy
      if (lastCallSent_LV1 != 0 || lastCallSent_LV2 != 0 || lastCallSent_LV3 != 0) Serial2.printf ("AT+CHUP\r\n");
      lastCallSent_LV1 = 0; 
      lastCallSent_LV2 = 0;
      lastCallSent_LV3 = 0;
      smsLV1Sent = false;
      smsLV2Sent = false;
      smsLV3Sent = false;
      isCallSent_LV2 = false;
      isCallSent_LV3 = false;
      isCallSent_LV1 = false;
      break;
    }
    vTaskDelay (pdMS_TO_TICKS (10));
  }
}

// ==============================================================
// 5. TASK UI: BỘ NÃO UI, LOGIC HIỂN THỊ LED VÀ BÁO CÒI (CORE 1)
// ==============================================================
void Task_UI (void *pvParameters) {
  for (;;) {
    unsigned long currentMillis = millis();
    strip.clear(); // Tắt LED trước
    switch (currentState) {
      case WAITING_FIRST_DATA:
        digitalWrite(PIN_BUZZER, LOW);  // Tắt còi
        // Chớp tắt 4 LED: red = 50, blue = 255 (Màu tím)
        for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, 0, 0, ledFlashState ? 255 : 0); 
        break;

      case NORMAL_STATE:
        digitalWrite(PIN_BUZZER, LOW);
        for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, 0, breatheValue, 0); // Sáng thở xanh lá 4 LED
        break;

      case ALARM_LV1: // Mất kết nối C3, pin yếu
        digitalWrite(PIN_BUZZER, LOW);
        for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, ledFlashState ? 150 : 0, 0, ledFlashState ? 255 : 0); // 4 LED xanh tím
        break;

      case ALARM_LV2:
        if (evalMode == 3) {
          digitalWrite(PIN_BUZZER, LOW);
          for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, ledFlashState ? 255 : 0, ledFlashState ? 100 : 0, 0);
        }
        else {
          if (israwWarningSpO2) { 
            setLEDColor(0, ledFlashState?255:0, ledFlashState?100:0, 0); 
            setLEDColor(3, ledFlashState?255:0, ledFlashState?100:0, 0); 
          }
          else { 
            setLEDColor(0, 0, breatheValue, 0); 
            setLEDColor(3, 0, breatheValue, 0); 
          }
          if (israwWarningBPM) { 
            setLEDColor(1, ledFlashState?255:0, ledFlashState?100:0, 0); 
            setLEDColor(2, ledFlashState?255:0, ledFlashState?100:0, 0); 
          }
          else { 
            setLEDColor(1, 0, breatheValue, 0); 
            setLEDColor(2, 0, breatheValue, 0); 
          }
  
          if (!isMuted) {
            // Chỉ kêu tiếng bíp ngắn 0.1s trong mỗi 2s
            if (currentMillis % 2000 < 100) digitalWrite(PIN_BUZZER, HIGH); 
            else digitalWrite(PIN_BUZZER, LOW);
          } 
          else { 
            digitalWrite(PIN_BUZZER, LOW); 
          }
        }
        break;

      case ALARM_LV3: 
        for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, ledFlashState ? 255 : 0, 0, 0); 
        
        if (!isMuted) {
          if (buzzerState && (currentMillis - previousBuzzerMillis >= 50)) { // Báo còi 50ms
            buzzerState = false; 
            previousBuzzerMillis = currentMillis; 
            digitalWrite(PIN_BUZZER, LOW); 
          } 
          else if (!buzzerState && (currentMillis - previousBuzzerMillis >= 100)) { // Nghỉ 100ms
            buzzerState = true; 
            previousBuzzerMillis = currentMillis; 
            digitalWrite(PIN_BUZZER, HIGH); 
          }
        } 
        else { 
          digitalWrite(PIN_BUZZER, LOW); 
        }

        break;

      case STANDBY: 
        // Sáng thở trắng nhưng độ sáng 1 nửa
        for(int i=0; i<NUM_LEDS; i++) setLEDColor(i, breatheValue/2, breatheValue/2, breatheValue/2); 
        digitalWrite(PIN_BUZZER, LOW);
        break;
    }
    strip.show(); // Chốt xuất dữ liệu ra LED
    vTaskDelay(pdMS_TO_TICKS (10)); 
  }
}

// ================================================
// TASK 2: ĐỒNG BỘ CLOUD VÀ SỐ ĐIỆN THOẠI (CORE 0)
// ================================================
void Task_RTDB(void *pvParameters) {
  unsigned long lastSecUpdate = 0;
  unsigned long lastMinUpdate = 0;
  unsigned long lastPhoneCheck = 0; // Check update SĐT từ app
  long sumSpO2 = 0, sumBPM = 0;
  int sampleCount = 0;
  for(;;) {
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      unsigned long now = millis();
      // Check SĐT mới từ app 30s/lần
      if (now - lastPhoneCheck >= 30000) {
        lastPhoneCheck = now;
        if (Firebase.RTDB.getString(&fbdo_read, "/phone_number")) {
          String fetchedPhone = fbdo_read.stringData();
          if (fetchedPhone != "" && fetchedPhone != alertPhoneNumber) {
            alertPhoneNumber = fetchedPhone;
            preferences.putString("phone", alertPhoneNumber);
            Serial.print(">>> Đã cập nhật Số điện thoại mới vào bộ nhớ: ");
            Serial.println(alertPhoneNumber);
          }
        }
      }
      // Cập nhật dữ liệu mỗi giây lên app
      if (now - lastSecUpdate >= 1000) {
        lastSecUpdate = now;
        uint8_t sendSpO2 = evalSpO2, sendBPM = evalBPM, sendBat = evalBat;
        bool sendCharge = evalCharge;
        if (isDisconnected) {
          sendSpO2 = 0;
          sendBPM = 0;
        }
        FirebaseJson jsonLive; // Tạo thùng hàng
        jsonLive.add("spo2", sendSpO2);
        jsonLive.add("bpm", sendBPM);
        jsonLive.add("bat", sendBat);
        jsonLive.add("charge", sendCharge); 
        jsonLive.add("state", (int)currentState); // Ép kiểu Enum về Int để sửa lỗi FirebaseJson
        Firebase.RTDB.setJSON(&fbdo_rtdb, "/live", &jsonLive); // Lệnh set: Ghi đè cái mới lên cái cũ
        // Dữ liệu mỗi giây được gom lại để tính trung bình
        if (sendSpO2 > 0 && sendBPM > 0) {
          sumSpO2 += sendSpO2; 
          sumBPM += sendBPM; 
          sampleCount++;
        }
      }
      // Check xem có message thì gửi lên app
      if (pendingFirebaseAlert) {
        FirebaseJson jsonAlert;
        jsonAlert.set ("spo2", evalSpO2);
        jsonAlert.set ("bpm", evalBPM);
        jsonAlert.set ("detail", alertDetailMessage);
        jsonAlert.set ("timestamp/.sv", "timestamp");
        Firebase.RTDB.pushJSON(&fbdo_rtdb, "/history_alert", &jsonAlert);
        pendingFirebaseAlert = false; // Gửi xong thì hạ cờ
      }
      // Cập nhật dữ liệu trung bình mỗi phút lên app
      if (now - lastMinUpdate >= 60000) {
        lastMinUpdate = now;
        if (sampleCount > 0) {
          int avgSpO2 = sumSpO2 / sampleCount;
          int avgBPM = sumBPM / sampleCount;
          FirebaseJson jsonHistory;
          jsonHistory.set("spo2", avgSpO2);
          jsonHistory.set("bpm", avgBPM);
          jsonHistory.set("timestamp/.sv", "timestamp"); 
          Firebase.RTDB.pushJSON(&fbdo_rtdb, "/history_chart", &jsonHistory); // Lệnh push: Tạo ID riêng -> Cái mới xếp sau cái cũ như list
          sumSpO2 = 0; sumBPM = 0; sampleCount = 0; 
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


void setup() {
  Serial.begin(115200);
  // Kiểu dữ liệu 8 bit, No Parity, 1 stop bit
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  pinMode(PIN_BUZZER, OUTPUT); 
  pinMode(PIN_BTN, INPUT_PULLUP);
  digitalWrite(PIN_BUZZER, LOW);
  strip.begin();
  strip.setBrightness(200); // Set độ sáng LED
  strip.show(); // Chốt 
  preferences.begin("baby_wear", false); // Mở namespace "baby_wear", false: READ/WRITE
  alertPhoneNumber = preferences.getString("phone", ""); // Lấy dữ liệu thẻ "phone", kh có thì trả về ""
  Serial.println("===========================================");
  Serial.print(">>> Số điện thoại được lưu trong máy: ");
  Serial.println(alertPhoneNumber);

  WiFi.mode(WIFI_STA); 
  WiFi.setSleep(false); // Kh được tắt anten để C3 ném gói tin tới bất cứ lúc nào
  WiFi.begin(ssid, password);
  Serial.print(">>> Connecting Wi-Fi ....");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.println("===========================================");

  // Điền đơn đky Firebase
  config.database_url = FIREBASE_DATABASE_URL;
  config.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
  config.service_account.data.project_id = FIREBASE_PROJECT_ID;
  config.service_account.data.private_key = PRIVATE_KEY;
  config.token_status_callback = tokenStatusCallback; // In ra Serial quá trình giải mã PRIVATE_KEY để biết hthong kh bị treo mà đang trong quá trình giải mã
  Firebase.begin(&config, &auth); // Nộp cấu hình
  Firebase.reconnectWiFi(true); // Tự động kết nối lại
  
  if (esp_now_init() != ESP_OK) ESP.restart();
  esp_now_register_recv_cb(OnDataRecv); // Hứng data từ C3

  esp_now_peer_info_t peerInfo = {}; 
  memcpy(peerInfo.peer_addr, c3Address, 6); // Địa chỉ MAC C3
  peerInfo.channel = 0; // Tự động chỉnh ESPNOW chạy cùng kênh với WiFi vì xài chung anten
  peerInfo.encrypt = false; // Kh cần mã hóa
  esp_now_add_peer(&peerInfo); // Nộp cấu hình

  Serial.println(">>> DANG CHO MODULE SIM BAT SONG (5s) <<<");
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  Serial2.println("AT"); // Đánh thức module SIM
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Serial2.println("AT+CMGF=1"); // Gửi SMS đi dạng text
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Serial2.println("AT+CNMI=2,2,0,0,0"); // Nhận SMS được gửi tới
  vTaskDelay(200 / portTICK_PERIOD_MS);
  while (Serial2.available()) {
    Serial2.read(); // Có chữ nào trong bộ đệm thì lôi ra đọc xong xóa -> Dọn rác cổng Serial2
  }

  xTaskCreatePinnedToCore (Task_SIM, "TaskSIM", 8192, NULL, 1, NULL, 1); // Core 1
  xTaskCreatePinnedToCore (Task_UI, "TaskUI", 8192, NULL, 1, NULL, 1); // Core 1
  xTaskCreatePinnedToCore (Task_RTDB, "TaskRTDB", 8192, NULL, 0, NULL, 0); // Core 0
}

void loop() { 
  unsigned long currentMillis = millis ();
  handleButton (currentMillis);
  handleEffectLED (currentMillis);
  prepareEvalData (currentMillis);
  updateStateGateway (currentMillis);
  vTaskDelay (pdMS_TO_TICKS (10)); 
}