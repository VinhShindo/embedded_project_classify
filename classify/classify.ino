// ======================== THƯ VIỆN ========================
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ======================== KHAI BÁO CHÂN ========================
// Định nghĩa các chân Arduino kết nối với Servo
#define PIN_DE 3        // Chân cho Servo Đế (Base)
#define PIN_TAY_DON 5  // Chân cho Servo Tay đòn (Shoulder)
#define PIN_CANG 6     // Chân cho Servo Càng (Elbow)
#define PIN_KEP 9      // Chân cho Servo Kẹp (Gripper)

#define RELAY_PIN 2
#define TRIG_PIN 8
#define ECHO_PIN 10
#define IR_PIN 11

// GY-31
#define S0 A2
#define S1 A3
#define S2 A0
#define S3 A1
#define sensorOut 7
#define LED 4

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Tạo đối tượng Servo
Servo servoDe;
Servo servoTayDon;
Servo servoCang;
Servo servoKep;

// ======================== HẰNG SỐ & BIẾN ========================
struct VungDatVat {
  String maVatThe;
  int gocXoayDe;
  int gocVuonCang;
  int gocHaTayDon;
};

const VungDatVat GocPhanLoai[] = {
    // Mã | Góc Đế | Góc Càng | Góc Tay Đòn
    {"T_D",    130,     80,        40}, // Tròn Đỏ (Khu vực 1)
    {"V_D",    120,      80,        10}, // Vuông Đỏ (Khu vực 2)
    {"T_V",     105,      70,        70}, // Tròn Vàng (Khu vực 3)
    {"V_V",     105,      70,        10}, // Vuông Vàng (Khu vực 4)
    {"T_X",      80,       90,        60}, // Tròn Xanh Dương (Khu vực 5)
    {"V_X",    80,      90,        10}  // Vuông Xanh Dương (Khu vực 6)
};

const int SO_LUONG_VAT_THE = sizeof(GocPhanLoai) / sizeof(GocPhanLoai[0]);
VungDatVat vatTheCanXuLy = {"KHAC", 90, 80, 100};

//Góc ban đầu
const int GOC_DE_BAN_DAU = 90;
const int GOC_TAY_DON_BAN_DAU = 100;
const int GOC_CANG_BAN_DAU = 70;
const int GOC_KEP_BAN_DAU = 90;  

const float D_REF = 8;
const float THRESHOLD = 2.5;
const int SCAN_POINTS = 20;
float profile_data[SCAN_POINTS];

// Giá trị min/max hiệu chỉnh thực nghiệm
int minR = 20, maxR = 200; 
int minG = 25, maxG = 210;
int minB = 30, maxB = 220;

// biến tần số và RGB
int redFrequency, greenFrequency, blueFrequency;
int R, G, B;

int count_TD = 0; // Tròn Đỏ
int count_VD = 0; // Vuông Đỏ
int count_TV = 0; // Tròn Vàng
int count_VV = 0; // Vuông Vàng
int count_TX = 0; // Tròn Xanh Dương
int count_VX = 0; // Vuông Xanh Dương

// ======================== HÀM HỖ TRỢ ========================
void mapAndWrite(Servo& servo, int angle) {
  int pulseWidth = map(angle, 0, 180, 500, 2500); 
  servo.writeMicroseconds(pulseWidth);
}

void moveServo(Servo& servo, int targetAngle, int delayTime) {
  int currentAngle = servo.read(); 
  int step = (targetAngle > currentAngle) ? 1 : -1;

  for (int angle = currentAngle; angle != targetAngle; angle += step) {
    mapAndWrite(servo, angle);
    delay(delayTime);
  }
}

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2.0;
  if (distance > 200 || distance <= 0) return D_REF;
  return distance;
}

float averageHeight(float data[], int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) sum += data[i];
  return sum / size;
}

int mapColor(int value, int minVal, int maxVal) {
  value = constrain(value, minVal, maxVal);
  return map(value, minVal, maxVal, 255, 0);
}

String detectColor() {
  // Đọc tần số 3 màu
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  redFrequency = pulseIn(sensorOut, LOW, 30000);

  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  greenFrequency = pulseIn(sensorOut, LOW, 30000);

  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blueFrequency = pulseIn(sensorOut, LOW, 30000);

  // Chuyển đổi sang RGB
  R = mapColor(redFrequency, minR, maxR);
  G = mapColor(greenFrequency, minG, maxG);
  B = mapColor(blueFrequency, minB, maxB);

  // Nhận diện màu
  if (R > 80 && G < 100 && B < 100)
    return "Do";
  else if (R < 80 && G < 130 && B > 100)
    return "Xanh Duong";
  else if (R > 130 && G > 100 && B < 180)
    return "Vang";
  else
    return "Khac";
}
void displayCount() {
  // Hàng 0: T_D:X V_D:Y T_V:Z
  lcd.setCursor(0, 0);
  lcd.print("T_D:");
  lcd.print(count_TD);
  lcd.print(" V_D:");
  lcd.print(count_VD);
  lcd.print(" T_V:");
  lcd.print(count_TV);

  // Hàng 1: V_V:A T_X:B V_X:C
  lcd.setCursor(0, 1);
  lcd.print("V_V:");
  lcd.print(count_VV);
  lcd.print(" T_X:");
  lcd.print(count_TX);
  lcd.print(" V_X:");
  lcd.print(count_VX);
}

VungDatVat timGocTheoMa(String maVatThe) {
  for (int i = 0; i < SO_LUONG_VAT_THE; i++) {
    if (GocPhanLoai[i].maVatThe == maVatThe) {
      return GocPhanLoai[i];
    }
  }

  return {"KHAC", 90, 80, 100}; 
}

// ======================== HÀM XỬ LÝ CẢM BIẾN HỒNG NGOẠI ========================
void ir_detect_delay() {
  Serial.println("--- CAM BIEN HONG NGOAI PHAT HIEN VAT ---");
  
  // Dừng băng tải ngay lập tức
  digitalWrite(RELAY_PIN, LOW); 
  
  Serial.println("Thuc hien delay 5 giay (Bang tai DUNG)...");
  if (vatTheCanXuLy.maVatThe == "KHAC") {
    digitalWrite(RELAY_PIN, HIGH);
    delay(400); // Tạm thời delay 5 giây
    return;
  }

  thucHienChuTrinh(
              vatTheCanXuLy.gocXoayDe, 
              vatTheCanXuLy.gocVuonCang, 
              vatTheCanXuLy.gocHaTayDon
          );

  // Chạy lại băng tải sau khi delay
  digitalWrite(RELAY_PIN, HIGH);
  delay(400); // Tạm thời delay 5 giây
  Serial.println("...Ket thuc delay. Bang tai CHAY lai.");
}

// ======================== QUÉT VẬT THỂ ========================
void scanObject() {
  for (int i = 0; i < SCAN_POINTS; i++) {
    float current_distance = readDistanceCM();
    profile_data[i] = D_REF - current_distance;
    delay(50);
  }

  float mean_height = averageHeight(profile_data, SCAN_POINTS);

  // Phân loại hình dạng
  String shape = (mean_height < 5.3) ? "Hinh Tron" : "Hinh Vuong";

  // Nhận diện màu
  String color = detectColor();

  String maVatThe = "KHAC";

  // In ra kết quả cuối cùng + RGB
  Serial.print("Vat the: ");
  Serial.print(shape);
  Serial.print(" | Mau: ");
  Serial.print(color);
  Serial.print(" | RGB = (");
  Serial.print(R); Serial.print(", ");
  Serial.print(G); Serial.print(", ");
  Serial.print(B); 
  Serial.println(")");

  if (shape == "Hinh Tron") {
    if (color == "Do") { count_TD++; maVatThe = "T_D"; Serial.println(" -> T_D"); }
    else if (color == "Vang") { count_TV++; maVatThe = "T_V"; Serial.println(" -> T_V"); }
    else if (color == "Xanh Duong") { count_TX++; maVatThe = "T_X"; Serial.println(" -> T_X"); }
    else { Serial.println(" -> T_K"); }
  } else if (shape == "Hinh Vuong") {
    if (color == "Do") { count_VD++; maVatThe = "V_D"; Serial.println(" -> V_D"); }
    else if (color == "Vang") { count_VV++; maVatThe = "V_V"; Serial.println(" -> V_V"); }
    else if (color == "Xanh Duong") { count_VX++; maVatThe = "V_X"; Serial.println(" -> V_X"); }
    else { Serial.println(" -> V_K"); }
  } else {
    Serial.println(" -> Khac");
  }
  
  vatTheCanXuLy = timGocTheoMa(maVatThe);
  Serial.println(vatTheCanXuLy.maVatThe);
  
  displayCount();
}

void thucHienChuTrinh(int gocXoayDe, int gocVuonCang, int gocHaTayDon) {
  Serial.println("\n--- BAT DAU CHU TRINH (Voi goc De: " + String(gocXoayDe) + ", goc Cang: " + String(gocVuonCang) + ") ---");

  // 1. DI CHUYỂN ĐẾ VỀ GÓC 0 ĐỘ
  Serial.println("1. Đế quay về góc 0 độ.");
  moveServo(servoDe, 20, 15);
  delay(500);

  // 2. MỞ KẸP (Chuẩn bị gắp)
  Serial.println("2. Mở kẹp.");
  moveServo(servoKep, 120, 15);
  delay(500);

  // 3. CÀNG, TAY ĐÒN VƯƠN RA GẮP VẬT
  Serial.println("3. Càng, Tay đòn vươn ra gắp vật.");
  // Hạ Tay đòn (Tay đòn vươn ra)
  moveServo(servoTayDon, 10, 15); 
  delay(500);
  // Vươn Càng ra (sử dụng tham số gocVuonCang)
  moveServo(servoCang, 75, 15); 
  delay(1000);

  // 4. KẸP VẬT
  Serial.println("4. Kẹp vật.");
  moveServo(servoKep, 90, 15); 
  delay(1000); 

  // 5. THU CÀNG, TAY ĐÒN VỀ
  Serial.println("5. Thu càng, tay đòn về.");
  // Nâng Tay đòn lên (Tay đòn thu về)
  moveServo(servoTayDon, 105, 15); 
  delay(1000);

  // 6. ĐẾ XOAY ĐẾN GÓC CHỈ ĐỊNH (gocXoayDe)
  Serial.println("6. Đế xoay đến góc chỉ định (" + String(gocXoayDe) + " độ).");
  moveServo(servoDe, gocXoayDe, 15);
  delay(1000);

  // 7. VƯƠN CÀNG, TAY ĐÒN RA THEO GÓC TRUYỀN VÀO (Vị trí thả vật)
  Serial.println("7. Vươn càng, tay đòn ra vị trí thả vật.");
  // Hạ Tay đòn (Tay đòn vươn ra)
  moveServo(servoTayDon, gocHaTayDon, 15); 
  delay(500);
  // Vươn Càng ra (sử dụng tham số gocVuonCang)
  moveServo(servoCang, gocVuonCang, 15); 
  delay(1000);

  // 8. MỞ KẸP (Thả vật)
  Serial.println("8. Mở kẹp (Thả vật).");
  moveServo(servoKep, 120, 15); 
  delay(1000);

  // 9. ĐÓNG KẸP (Đóng lại để an toàn)
  Serial.println("9. Đóng kẹp.");
  moveServo(servoKep, 90, 15); 
  delay(500);

  // 10. THU CÀNG, TAY ĐÒN VỀ
  Serial.println("10. Thu càng, tay đòn về.");
  // Thu Càng về vị trí thu gọn
  moveServo(servoCang, 80, 15); 
  delay(500);
  // Nâng Tay đòn lên (Tay đòn thu về)
  moveServo(servoTayDon, 100, 15); 
  delay(1000);

  // 11. XOAY ĐẾ VỀ GÓC 90 ĐỘ (Vị trí ban đầu)
  Serial.println("11. Xoay Đế về góc 90 độ (Vị trí Home).");
  moveServo(servoDe, 90, 15);
  delay(1000);
  
  Serial.println("--- KET THUC CHU TRINH ---");
}

// ======================== SETUP ========================
void setup() {
  Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG");
  lcd.setCursor(0, 1);
  lcd.print("KHOI DONG...");
  Serial.println("He thong khoi dong...");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);

  servoDe.attach(PIN_DE);
  servoTayDon.attach(PIN_TAY_DON);
  servoCang.attach(PIN_CANG);
  servoKep.attach(PIN_KEP);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  pinMode(LED, OUTPUT);
  
  digitalWrite(LED, HIGH);
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  mapAndWrite(servoDe, GOC_DE_BAN_DAU);
  mapAndWrite(servoTayDon, GOC_TAY_DON_BAN_DAU);
  mapAndWrite(servoCang, GOC_CANG_BAN_DAU);
  mapAndWrite(servoKep, GOC_KEP_BAN_DAU);

  digitalWrite(RELAY_PIN, HIGH); // motor ON

  lcd.clear();
  delay(1000);
  displayCount();
}

// ======================== LOOP ========================
void loop() {
  
  // 1. LOGIC CỦA CẢM BIẾN SIÊU ÂM/MÀU (XỬ LÝ ƯU TIÊN 1)
  float current_distance = readDistanceCM();

  if (current_distance < (D_REF - THRESHOLD)) {
    Serial.println(">>> PHAT HIEN VAT THE QUA CAM BIEN SIÊU ÂM <<<");

    // Dừng băng tải
    digitalWrite(RELAY_PIN, LOW);
    delay(500);

    // Quét vật thể + đo màu
    scanObject();

    // Chạy lại băng tải để tiếp tục di chuyển
    digitalWrite(RELAY_PIN, HIGH);
    delay(800);
    
  }

  if (digitalRead(IR_PIN) == LOW) { 
      ir_detect_delay();
      lcd.clear();
      displayCount(); 
  }
  delay(100);
}