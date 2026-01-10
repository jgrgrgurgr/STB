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

LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial mySerial(10, 11);
DFRobotDFPlayerMini myDFPlayer;

bool isPlaying = false;
bool soundMode = true; 
unsigned long lastTempUpdate = 0;
const int FSR_THRESHOLD = 200; 

// 스위치 디바운싱
bool lastSwitchState = HIGH;
bool currentSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  pinMode(RED, OUTPUT); pinMode(GREEN, OUTPUT); pinMode(BLUE, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  mySerial.begin(9600);
  sensors.begin();
  
  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Init System...");

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer Error!");
    while (true); 
  }
  myDFPlayer.volume(25);
  delay(2000); // 초기화 안정화 대기
  lcd.clear();
}

// 강제 재생 함수 (시리얼에 로그 찍음)
void playHard(int trackNum, unsigned long waitMs) {
  Serial.print("[명령] 트랙 재생: "); Serial.print(trackNum); 
  Serial.print("번 (대기: "); Serial.print(waitMs); Serial.println("ms)");
  
  myDFPlayer.play(trackNum);
  delay(waitMs); 
}

void triggerSoundMode(float temperature) {
  if (isPlaying) return;
  isPlaying = true;

  int temp = (int)temperature;
  Serial.print(">>> 측정된 온도: "); Serial.println(temp);

  // 1. "현재 온도는" (0001.mp3)
  playHard(1, 1600);

  // 2. 숫자 처리
  // [100의 자리]
  if (temp >= 100) {
    int h = temp / 100;
    if (h > 1) playHard(h + 5, 800); // 이백, 삼백...
    playHard(16, 1000); // "백"
    temp %= 100;
  }

  // [10의 자리] - 13도일 경우 여기서 10(15번)이 나와야 함
  if (temp >= 10) {
    int t = (temp / 10) * 10; // 13 -> 10
    
    if (t == 90) playHard(25, 1200);
    else if (t == 80) playHard(24, 1200);
    else if (t == 70) playHard(23, 1200);
    else if (t == 60) playHard(22, 1200);
    else if (t == 50) playHard(21, 1200);
    else if (t == 40) playHard(20, 1200);
    else if (t == 30) playHard(19, 1200);
    else if (t == 20) playHard(18, 1200);
    else if (t == 10) playHard(15, 1000); // 13도면 여기서 15번(십) 실행
    
    temp %= 10; // 13 -> 3 남음
  }

  // [1의 자리] - 13도일 경우 여기서 3(8번)이 나와야 함
  if (temp > 0) {
    // 1=6, 2=7, 3=8 ...
    playHard(temp + 5, 1000); 
  }

  // 3. "도입니다" (0002.mp3)
  playHard(2, 1300);

  // 4. 조언
  if (temperature >= 60.0) playHard(3, 3000);
  else if (temperature <= 5.0) playHard(4, 3000);
  else playHard(5, 3000);

  Serial.println(">>> 안내 종료");
  isPlaying = false;
}

void checkModeSwitch() {
  int reading = digitalRead(MODE_SWITCH_PIN);
  if (reading != lastSwitchState) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;
      if (currentSwitchState == LOW) {
        soundMode = !soundMode;
        Serial.print("모드 변경: "); Serial.println(soundMode ? "소리" : "진동");
      }
    }
  }
  lastSwitchState = reading;
}

void updateDisplay(float t) {
  lcd.setCursor(0,0); lcd.print("Temp: "); lcd.print(t,1); lcd.print((char)223); lcd.print("C   ");
  lcd.setCursor(0,1); lcd.print(soundMode ? "Mode: SOUND   " : "Mode: VIBE    ");
  
  digitalWrite(RED, LOW); digitalWrite(GREEN, LOW); digitalWrite(BLUE, LOW);
  if (t <= 5.0) digitalWrite(BLUE, HIGH);
  else if (t >= 60.0) digitalWrite(RED, HIGH);
  else digitalWrite(GREEN, HIGH);
}

void loop() {
  checkModeSwitch();

  if (millis() - lastTempUpdate >= 500) {
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);
    
    updateDisplay(currentTemp);

    int fsr = analogRead(FSR_PIN);
    if (fsr >= FSR_THRESHOLD) {
      if (soundMode) triggerSoundMode(currentTemp);
      else {
        Serial.println("진동 작동");
        for(int i=50; i<200; i+=10) { analogWrite(VIBRATION_PIN, i); delay(50); }
        analogWrite(VIBRATION_PIN, 0);
      }
    }
    lastTempUpdate = millis();
  }
}