#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// [1] 핀 번호 정의
#define BLUE 6              
#define GREEN 5             
#define RED 7               
#define VIBRATION_PIN 9     
#define MODE_SWITCH_PIN 3   
#define ONE_WIRE_BUS 12     
#define DF_RX 10            
#define DF_TX 11            

// [2] 설정값
const int VOLUME_LEVEL = 25;         
const unsigned long TEMP_INTERVAL = 500; 

// [3] 객체 생성
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
SoftwareSerial mySerial(DF_RX, DF_TX);
DFRobotDFPlayerMini myDFPlayer;

// 상태 변수
bool isPlaying = false;          
bool soundMode = true; // true=소리, false=진동
unsigned long lastTempUpdate = 0;

// 버튼 디바운싱
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
  lcd.setCursor(0,0); lcd.print("System Booting..");

  delay(1000); 

  // DFPlayer 연결 (재시도 로직 포함)
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println(F("Audio Connecting..."));
    delay(1000);
    if (!myDFPlayer.begin(mySerial)) {
       lcd.setCursor(0,1); lcd.print("Audio Error");
       delay(2000);
    }
  }
  
  myDFPlayer.volume(VOLUME_LEVEL);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  
  lcd.clear();
}

// [기능 1] 트랙 재생 (딜레이로 순서 보장)
void playHard(int trackNum, unsigned long waitMs) {
  myDFPlayer.play(trackNum);
  delay(waitMs); 
}

// [기능 2] 숫자 읽기 (1~199도)
void playNumberHardcoded(int temp) {
  // 백 단위
  if (temp >= 100) {
     playHard(16, 1100); // "백"
     temp %= 100;
  }
  // 십 단위
  if (temp >= 10) {
    int tens = (temp / 10) * 10;
    switch (tens) {
      case 90: playHard(25, 1100); break;
      case 80: playHard(24, 1100); break;
      case 70: playHard(23, 1100); break;
      case 60: playHard(22, 1100); break;
      case 50: playHard(21, 1100); break;
      case 40: playHard(20, 1100); break;
      case 30: playHard(19, 1100); break;
      case 20: playHard(18, 1100); break;
      case 10: playHard(15, 900); break;
    }
    temp %= 10;
  }
  // 일 단위
  if (temp > 0) {
    playHard(temp + 5, 900); // 1~9
  }
}

// [기능 3] 소리 모드 실행 시퀀스
void triggerSoundMode(float temperature) {
  if (isPlaying) return;
  isPlaying = true;
  
  int tempInt = (int)temperature;
  Serial.println(F(">>> 소리 안내 시작"));

  // 1. "현재 온도는"
  playHard(1, 1600);

  // 2. 숫자 읽기
  playNumberHardcoded(tempInt);

  // 3. "도입니다"
  playHard(2, 1300);

  // [수정됨] 멘트가 씹히지 않도록 약간의 틈을 줌
  delay(200); 

  // 4. 상태 멘트 (3, 4, 5번 트랙)
  if (temperature >= 60.0) {
    // 뜨거움
    playHard(3, 3000); 
  } else if (temperature <= 5.0) {
    // 차가움
    playHard(4, 3000); 
  } else {
    // 적절함 (5도 초과 ~ 60도 미만)
    playHard(5, 3000); 
  }

  isPlaying = false;
}

// [기능 4] 진동 효과 (뜨거울 때만 작동)
void runVibrationEffect() {
  Serial.println(F(">>> 위험 알림: 진동 작동"));
  // 징- 징- 징- (경고 느낌)
  for(int k=0; k<3; k++) {
    analogWrite(VIBRATION_PIN, 200);
    delay(300);
    analogWrite(VIBRATION_PIN, 0);
    delay(200);
  }
}

// [기능 5] 디스플레이 업데이트
void updateDisplay(float t) {
  lcd.setCursor(0,0); 
  if (t == DEVICE_DISCONNECTED_C) lcd.print("Temp: Error   ");
  else {
    lcd.print("Temp: "); lcd.print(t, 1); lcd.print((char)223); lcd.print("C   ");
  }
  
  lcd.setCursor(0,1); 
  lcd.print(soundMode ? "Mode: SOUND     " : "Mode: VIBE      ");

  // LED 색상 로직
  digitalWrite(RED, LOW); digitalWrite(GREEN, LOW); digitalWrite(BLUE, LOW);
  if (t <= 5.0) digitalWrite(BLUE, HIGH);
  else if (t >= 60.0) digitalWrite(RED, HIGH);
  else digitalWrite(GREEN, HIGH);
}

// [기능 6] 버튼 확인 및 실행 로직
void checkModeSwitch() {
  int reading = digitalRead(MODE_SWITCH_PIN);
  if (reading != lastSwitchState) lastDebounceTime = millis();
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;
      
      // 버튼 눌림 감지 (LOW)
      if (currentSwitchState == LOW) { 
        // 1. 모드 전환
        soundMode = !soundMode;
        
        // 2. 온도 즉시 측정
        sensors.requestTemperatures();
        float currentTemp = sensors.getTempCByIndex(0);
        updateDisplay(currentTemp);

        // 3. 모드별 동작 수행
        if (currentTemp != DEVICE_DISCONNECTED_C) {
          if (soundMode) {
            // [소리 모드] 무조건 안내
            triggerSoundMode(currentTemp); 
          } else {
            // [진동 모드] 요청하신대로 "뜨거울 때만" 작동
            if (currentTemp >= 60.0) {
              runVibrationEffect();
            } else {
              // 뜨겁지 않으면 진동 안 함 (안전함)
              Serial.println(F("진동 모드: 안전 범위라 반응 안함"));
            }
          }
        }
      }
    }
  }
  lastSwitchState = reading;
}

void loop() {
  checkModeSwitch(); // 버튼 누름 대기

  // 평상시 모니터링 (0.5초 주기)
  if (millis() - lastTempUpdate >= TEMP_INTERVAL) {
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    updateDisplay(t);
    lastTempUpdate = millis();
  }
}