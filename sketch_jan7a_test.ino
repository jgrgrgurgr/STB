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
const int FSR_THRESHOLD = 200; // 눌렀을 때만 작동하도록 상향 조정

void setup() {
  pinMode(RED, OUTPUT); pinMode(GREEN, OUTPUT); pinMode(BLUE, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  mySerial.begin(9600);
  sensors.begin();
  
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer Error");
    while (true); 
  }
  myDFPlayer.volume(25);
  lcd.init(); lcd.backlight();
}

// 각 음성 파일의 길이에 맞춰 대기 시간을 주는 핵심 함수
void playTrack(int track, unsigned long waitTime) {
  myDFPlayer.play(track);
  delay(waitTime); 
}

void triggerSoundMode(float temperature) {
  if (isPlaying) return;
  isPlaying = true;

  int temp = (int)temperature;

  // 1. "현재 온도는"
  playTrack(1, 1600);

  // 2. 온도 숫자 조합 (예: 110도)
  // 백단위
  if (temp >= 100) {
    int hundreds = temp / 100;
    if (hundreds > 1) playTrack(hundreds + 5, 800); 
    playTrack(16, 1000); // "백"
    temp %= 100;
  }
  
  // 십단위 (10, 20, 30, 40...)
  if (temp >= 10) {
    int tens = (temp / 10) * 10;
    if (tens == 10) playTrack(15, 1000);
    else if (tens == 20) playTrack(18, 1000);
    else if (tens == 30) playTrack(19, 1000);
    else if (tens == 40) playTrack(20, 1000);
    // 50~90 파일은 아직 없으므로 일단 40까지만 구현
    temp %= 10;
  }
  
  // 일단위 (1~9)
  if (temp > 0) {
    playTrack(temp + 5, 900);
  }

  // 3. "도입니다"
  playTrack(2, 1200);

  // 4. 상태 조언
  if (temperature >= 60.0) playTrack(3, 2500);
  else if (temperature <= 5.0) playTrack(4, 2500);
  else playTrack(5, 2500);

  isPlaying = false;
}

void loop() {
  // 모드 전환
  if (digitalRead(MODE_SWITCH_PIN) == LOW) {
    soundMode = !soundMode;
    delay(500); 
  }

  if (millis() - lastTempUpdate >= 500) {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    updateDisplay(currentTemp);

    // FSR 감도 체크 (200 이상일 때만)
    if (analogRead(FSR_PIN) >= FSR_THRESHOLD) {
      if (soundMode) triggerSoundMode(currentTemp);
      else {
        digitalWrite(VIBRATION_PIN, HIGH);
        delay(2000);
        digitalWrite(VIBRATION_PIN, LOW);
      }
    }
    lastTempUpdate = millis();
  }
}

void updateDisplay(float t) {
  lcd.setCursor(0,0); lcd.print("Temp: "); lcd.print(t,1); lcd.print("C  ");
  lcd.setCursor(0,1); lcd.print(soundMode ? "Mode: SOUND   " : "Mode: VIBE    ");
  
  digitalWrite(RED, t >= 60.0);
  digitalWrite(BLUE, t <= 5.0);
  digitalWrite(GREEN, (t > 5.0 && t < 60.0));
}