#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// 핀 정의
#define BLUE 6
#define GREEN 5
#define RED 7
#define VIBRATION_PIN 9
#define MODE_SWITCH_PIN 3
#define ONE_WIRE_BUS 12
#define FSR_PIN A1

// 설정 변수
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial mySerial(10, 11);
DFRobotDFPlayerMini myDFPlayer;

// 상태 변수
bool isPlaying = false;
bool isVibrating = false;
unsigned long lastTempUpdate = 0;
unsigned long vibrationStartTime = 0;
bool soundMode = true; 
const unsigned long vibrationDuration = 2000;

// 모드 전환 스위치용 변수
bool lastSwitchState = HIGH;
bool currentSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// [중요] FSR 민감도 설정: 눌러도 반응이 없으면 이 값을 50~100으로 낮추세요.
const int FSR_THRESHOLD = 150; 

void setup() {
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  mySerial.begin(9600);
  sensors.begin();
  
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer 연결 실패! SD카드와 배선을 확인하세요.");
    while (true);
  }
  
  myDFPlayer.volume(25);
  lcd.init();
  lcd.backlight();
  
  Serial.println("시스템 초기화 완료. FSR을 눌러보세요.");
}

// 음성 트랙 재생 보조 함수
void playTrack(int trackNum, unsigned long duration) {
  Serial.print("재생 중 트랙 번호: "); Serial.println(trackNum);
  myDFPlayer.play(trackNum);
  delay(duration); // 음성이 겹치지 않게 대기
}

// 음성 모드 실행 (온도 음성 조합)
void triggerSoundMode(float temperature) {
  if (isPlaying) return;
  isPlaying = true;

  int temp = (int)temperature;
  Serial.print("음성 출력 시작 - 온도: "); Serial.println(temp);

  // 1. "현재 온도는" (0001.mp3)
  playTrack(1, 1500);

  // 2. 온도 값 조합
  // 100단위
  if (temp >= 100) {
    int hundreds = temp / 100;
    if (hundreds > 1) playTrack(hundreds + 5, 800); // 200 이상일 때 '이'백...
    playTrack(16, 1000); // "백" (0016.mp3)
    temp %= 100;
  }
  
  // 10단위 (110일 경우 15번 트랙 재생)
  if (temp >= 10) {
    int tens = (temp / 10) * 10;
    if (tens == 10) playTrack(15, 1000); // "십" (0015.mp3)
    else if (tens == 20) playTrack(18, 1000); // "이십"
    else if (tens == 30) playTrack(19, 1000);
    else if (tens == 40) playTrack(20, 1000);
    else if (tens == 50) playTrack(21, 1000);
    else if (tens == 60) playTrack(22, 1000);
    else if (tens == 70) playTrack(23, 1000);
    else if (tens == 80) playTrack(24, 1000);
    else if (tens == 90) playTrack(25, 1000);
    temp %= 10;
  }
  
  // 1단위
  if (temp > 0) {
    playTrack(temp + 5, 800); // 1~9 (0006~0014.mp3)
  }

  // 3. "도입니다" (0002.mp3)
  playTrack(2, 1500);

  // 4. 상태 조언
  if (temperature >= 60.0) playTrack(3, 2500); // 뜨거워요
  else if (temperature <= 5.0) playTrack(4, 2500); // 차가워요
  else playTrack(5, 2500); // 적당해요

  isPlaying = false;
}

// 진동 모드 실행
void triggerVibrationMode() {
  if (!isVibrating) {
    isVibrating = true;
    vibrationStartTime = millis();
    Serial.println("진동 시작");
    analogWrite(VIBRATION_PIN, 200); // 진동 강도
  }
}

// 진동 정지 체크
void updateVibrationMode() {
  if (isVibrating && (millis() - vibrationStartTime >= vibrationDuration)) {
    analogWrite(VIBRATION_PIN, 0);
    isVibrating = false;
    Serial.println("진동 종료");
  }
}

// 모드 스위치 체크
void checkModeSwitch() {
  int reading = digitalRead(MODE_SWITCH_PIN);
  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;
      if (currentSwitchState == LOW) {
        soundMode = !soundMode;
        Serial.print("모드 변경: ");
        Serial.println(soundMode ? "소리" : "진동");
      }
    }
  }
  lastSwitchState = reading;
}

// LED 업데이트
void updateRGBLED(float temperature) {
  digitalWrite(RED, LOW); digitalWrite(GREEN, LOW); digitalWrite(BLUE, LOW);
  if (temperature <= 5.0) digitalWrite(BLUE, HIGH);
  else if (temperature >= 60.0) digitalWrite(RED, HIGH);
  else digitalWrite(GREEN, HIGH);
}

// LCD 업데이트
void updateLCDDisplay(float temperature) {
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  if (temperature != DEVICE_DISCONNECTED_C) {
    lcd.print(temperature, 1);
    lcd.print((char)223); lcd.print("C  ");
  } else lcd.print("ERROR   ");
  
  lcd.setCursor(0,1);
  lcd.print(soundMode ? "Mode: SOUND     " : "Mode: VIBRATE   ");
}

void loop() {
  checkModeSwitch();
  updateVibrationMode();

  // 0.5초마다 온도 체크 및 압력 감지
  if (millis() - lastTempUpdate >= 500) {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    
    updateRGBLED(currentTemp);
    updateLCDDisplay(currentTemp);

    // FSR 센서 값 읽기
    int fsrRaw = analogRead(FSR_PIN);
    Serial.print("FSR 값: "); Serial.println(fsrRaw);

    if (fsrRaw >= FSR_THRESHOLD) {
      if (soundMode) triggerSoundMode(currentTemp);
      else triggerVibrationMode();
    }
    lastTempUpdate = millis();
  }
}