#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define BLUE 6
#define GREEN 5
#define RED 7
#define VIBRATION_PIN 9
#define MODE_SWITCH_PIN 3
#define ONE_WIRE_BUS 12
#define FSR_PIN A1

LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial mySerial(10, 11);
DFRobotDFPlayerMini myDFPlayer;

bool isPlaying = false;
bool soundMode = true;
unsigned long lastTempUpdate = 0;
const int FSR_THRESHOLD = 150; // 영상 기반 적절한 수치

void setup() {
  pinMode(RED, OUTPUT); pinMode(GREEN, OUTPUT); pinMode(BLUE, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  mySerial.begin(9600);
  sensors.begin();
  
  if (!myDFPlayer.begin(mySerial)) {
    while (true); 
  }
  myDFPlayer.volume(25);
  lcd.init(); lcd.backlight();
}

// 음성 재생 후 대기를 포함한 함수
void playWait(int track, unsigned long ms) {
  myDFPlayer.play(track);
  delay(ms);
}

void triggerSoundMode(float temperature) {
  if (isPlaying) return;
  isPlaying = true;

  int temp = (int)temperature;

  // 1. "현재 온도는"
  playWait(1, 1600);

  // 2. 온도 숫자 재생 (현재 1~5번 파일만 있으므로 5 이하일 때 예시)
  // 만약 90, 10 등의 파일이 생기면 이 부분을 확장해야 합니다.
  if (temp >= 1 && temp <= 5) {
    playWait(temp + 5, 1000); // 1(0006) ~ 5(0010)
  } else {
    // 5도 초과 시 임시로 '5' 재생 혹은 생략
    playWait(10, 1000); 
  }

  // 3. "도입니다"
  playWait(2, 1200);

  // 4. 상태 조언
  if (temperature >= 60.0) playWait(3, 2500); // 뜨거우니
  else if (temperature <= 5.0) playWait(4, 2500); // 차가우니
  else playWait(5, 2500); // 적절

  isPlaying = false;
}

void loop() {
  // 모드 전환 스위치 (단순화)
  if (digitalRead(MODE_SWITCH_PIN) == LOW) {
    soundMode = !soundMode;
    delay(300);
  }

  if (millis() - lastTempUpdate >= 500) {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    
    // LCD & LED 업데이트
    updateLCD(currentTemp);
    
    // FSR 체크
    if (analogRead(FSR_PIN) >= FSR_THRESHOLD) {
      if (soundMode) triggerSoundMode(currentTemp);
      else {
        digitalWrite(VIBRATION_PIN, HIGH);
        delay(1000);
        digitalWrite(VIBRATION_PIN, LOW);
      }
    }
    lastTempUpdate = millis();
  }
}

void updateLCD(float t) {
  lcd.setCursor(0,0); lcd.print("Temp: "); lcd.print(t,1);
  lcd.setCursor(0,1); lcd.print(soundMode ? "Mode: SOUND   " : "Mode: VIBE    ");
}