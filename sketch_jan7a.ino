#define BLUE 6
#define GREEN 5
#define RED 7
#define VIBRATION_PIN 9  // 진동 모터 핀 (기존 9번 핀)
#define MODE_SWITCH_PIN 3  // 모드 전환 스위치 핀

#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// 온도 센서 설정
#define ONE_WIRE_BUS 12 // 온도 센서 데이터 핀
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// DFPlayer 설정
SoftwareSerial mySerial(10, 11); // RX: 10, TX: 11 (DFPlayer: RX → Arduino TX 10, TX → Arduino RX 11)
DFRobotDFPlayerMini myDFPlayer;   // DFPlayer 객체

// 상태 변수들
bool isPlaying = false; // 재생 중인지 추적하는 플래그
bool isVibrating = false; // 진동 중인지 추적하는 플래그
unsigned long lastTempUpdate = 0; // 온도 업데이트 시간 추적
unsigned long vibrationStartTime = 0; // 진동 시작 시간
bool soundMode = true; // true: 소리 모드, false: 진동 모드
bool lastSwitchState = HIGH;
bool currentSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
const unsigned long vibrationDuration = 3000; // 진동 지속 시간 (3초)

// 시작 시 한 번만 실행되지 않도록 하는 변수
unsigned long startupTime = 0;
bool firstLoop = true;

void setup() {
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT); // 진동 모터 핀 설정
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP); // 모드 전환 스위치 핀 설정 (풀업 저항 사용)
  
  Serial.begin(115200); // 시리얼 모니터용 (디버깅용, 높은 속도 사용)
  mySerial.begin(9600); // DFPlayer 통신용
  sensors.begin(); // 온도 센서 라이브러리 시작

  Serial.println("시스템 초기화 중...");
  delay(2000); // 초기화 대기 시간

  // DFPlayer 초기화
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer 연결 실패! 확인 후 다시 시도하세요.");
    while (true); // 무한 루프, 수동 리셋 필요
  }

  Serial.println("DFPlayer 연결 성공!");
  myDFPlayer.volume(25); // 볼륨 설정
    
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  
  // 초기 LCD 화면 설정
  updateLCDDisplay(0.0); // 초기 온도 0.0으로 표시
  
  delay(1000); // 볼륨 적용 대기
  
  // 시작 시간 기록
  startupTime = millis();
  
  Serial.println("시스템 초기화 완료!");
}

void updateRGBLED(float temperature) {
  // 모든 LED 끄기
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);
  
  if (temperature <= 5.0) {
    // 5°C 이하: 파랑색
    digitalWrite(BLUE, HIGH);
  } else if (temperature >= 60.0) {
    // 60°C 이상: 빨강색
    digitalWrite(RED, HIGH);
  } else {
    // 5°C ~ 60°C: 초록색
    digitalWrite(GREEN, HIGH);
  }
}

void updateLCDDisplay(float temperature) {
  // 첫 번째 줄: 온도 표시
  lcd.setCursor(0,0);
  if (temperature != DEVICE_DISCONNECTED_C) {
    lcd.print("Temp: ");
    lcd.print(temperature, 1); // 소수점 1자리까지 표시
    lcd.print((char)223); // 도(°) 기호
    lcd.print("C   "); // 공백으로 이전 값 지우기
  } else {
    lcd.print("Temp: ERROR   ");
  }
  
  // 두 번째 줄: 모드 표시
  lcd.setCursor(0,1);
  if (soundMode) {
    lcd.print("Mode: SOUND   ");
  } else {
    lcd.print("Mode: VIBE ");
  }
}

void checkModeSwitch() {
  // 스위치 상태 읽기
  int reading = digitalRead(MODE_SWITCH_PIN);
  
  // 디바운싱 처리
  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;
      
      // 스위치가 눌렸을 때 (HIGH → LOW)
      if (currentSwitchState == LOW) {
        soundMode = !soundMode; // 모드 토글
        Serial.print("모드 변경: ");
        Serial.println(soundMode ? "소리 모드" : "진동 모드");
        
        // 현재 재생/진동 중인 것 정지
        if (isPlaying) {
          myDFPlayer.stop();
          isPlaying = false;
        }
        if (isVibrating) {
          analogWrite(VIBRATION_PIN, 0); // 진동 모터 끄기
          isVibrating = false;
        }
      }
    }
  }
  
  lastSwitchState = reading;
}

void triggerSoundMode(float temperature) {
  if (!isPlaying) {
    isPlaying = true;
    
    // 온도를 정수로 변환
    int tempInt = (int)temperature;
    
    Serial.print("온도 음성 출력 시작: ");
    Serial.println(tempInt);
    
    // 1. "현재 온도는" 재생
    myDFPlayer.play(1); // 0001.mp3: "현재 온도는"
    delay(2000);
    
    // 2. 온도 값 음성 출력
    playTemperatureValue(tempInt);
    
    // 3. "도입니다" 재생
    myDFPlayer.play(2); // 0002.mp3: "도입니다"
    delay(2000);
    
    // 4. 마지막에 상태 메시지 재생 (저온/정상/고온)
    int statusTrack = getStatusTrack(temperature);
    myDFPlayer.play(statusTrack);
    delay(3000);
  }
}

int getStatusTrack(float temperature) {
  if (temperature <= 5.0) {
    Serial.println("저온 경고 음성 재생");
    return 4; // 0004.mp3: "차가우니 천천히 드세요"
  } else if (temperature >= 60.0) {
    Serial.println("고온 경고 음성 재생");
    return 3; // 0003.mp3: "뜨거우니 주의하세요"
  } else {
    Serial.println("정상 온도 음성 재생");
    return 5; // 0005.mp3: "마시기에 적절하네요"
  }
}

void playTemperatureValue(int temperature) {
  Serial.print("온도 값 음성 출력: ");
  Serial.println(temperature);
  
  // 100의 자리 처리
  int hundreds = temperature / 100;
  if (hundreds > 0) {
    // 100의 자리 숫자 재생
    myDFPlayer.play(hundreds + 5); // 0006~0014: 1~9
    delay(1500);
    
    // "백" 음성 재생
    myDFPlayer.play(16); // 0016.mp3: "100"
    delay(1000);
  }
  
  // 나머지 계산 (100으로 나눈 나머지)
  int remainder = temperature % 100;
  
  // 10의 자리와 1의 자리 처리
  if (remainder > 0) {
    playTensAndOnes(remainder);
  }
}

void playTensAndOnes(int number) {
  // 새로운 방식: 10단위로 묶어서 출력
  
  // 90 이상인 경우
  if (number >= 90) {
    myDFPlayer.play(25); // 0025.mp3: "90"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 80대
  else if (number >= 80) {
    myDFPlayer.play(24); // 0024.mp3: "80"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 70대
  else if (number >= 70) {
    myDFPlayer.play(23); // 0023.mp3: "70"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 60대
  else if (number >= 60) {
    myDFPlayer.play(22); // 0022.mp3: "60"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 50대
  else if (number >= 50) {
    myDFPlayer.play(21); // 0021.mp3: "50"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 40대
  else if (number >= 40) {
    myDFPlayer.play(20); // 0020.mp3: "40"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 30대
  else if (number >= 30) {
    myDFPlayer.play(19); // 0019.mp3: "30"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 20대
  else if (number >= 20) {
    myDFPlayer.play(18); // 0018.mp3: "20"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 10~19
  else if (number >= 10) {
    myDFPlayer.play(15); // 0015.mp3: "10"
    delay(1500);
    int ones = number % 10;
    if (ones > 0) {
      myDFPlayer.play(ones + 5); // 0006~0014: 1~9
      delay(1000);
    }
  }
  // 1~9
  else if (number > 0) {
    myDFPlayer.play(number + 5); // 0006~0014: 1~9
    delay(1000);
  }
}

void triggerVibrationMode() {
  if (!isVibrating) {
    isVibrating = true;
    vibrationStartTime = millis(); // 진동 시작 시간 기록
    Serial.println("진동 모드 시작");
    
    // 진동 모터를 점진적으로 강도 증가
    for (int i = 0; i <= 10; i++) {
      analogWrite(VIBRATION_PIN, i * 25); // 진동 강도 점진적 증가 (0~255)
      delay(100);
    }
  }
}

void updateVibrationMode() {
  // 진동 중이고 지속 시간이 지났으면 진동 정지
  if (isVibrating && (millis() - vibrationStartTime >= vibrationDuration)) {
    analogWrite(VIBRATION_PIN, 0); // 진동 모터 끄기
    isVibrating = false;
    Serial.println("진동 모드 종료");
  }
}

void loop() {
  // 첫 번째 루프에서만 3초 기다림 (시작 시 오동작 방지)
  if (firstLoop) {
    Serial.println("첫 시작 - 3초 대기...");
    delay(3000);
    firstLoop = false;
    Serial.println("정상 작동 시작!");
  }
  
  // 모드 스위치 확인
  checkModeSwitch();
  
  // 진동 모드 상태 업데이트 (타이머 기반)
  updateVibrationMode();
  
  // 온도 센서 데이터 읽기 (0.1초마다 갱신)
  if (millis() - lastTempUpdate >= 100) {
    sensors.requestTemperatures(); // 모든 온도 센서에 온도 요청
    float temperature = sensors.getTempCByIndex(0); // 첫 번째 센서의 온도 읽기
    
    // 온도가 유효한 값인지 확인
    if (temperature != DEVICE_DISCONNECTED_C) {
      // RGB LED 색상 업데이트
      updateRGBLED(temperature);
      
      // 시리얼 모니터에 온도 출력
      Serial.print("현재 온도: ");
      Serial.print(temperature);
      Serial.println(" °C");
    } else {
      Serial.println("온도 센서 읽기 오류!");
    }
    
    // LCD 디스플레이 업데이트 (온도와 모드 정보)
    updateLCDDisplay(temperature);
    
    lastTempUpdate = millis();
  }

  // FSR 값 읽기 및 매핑
  int fsrReading = analogRead(A1);
  int mappedValue = map(fsrReading, 0, 1023, 0, 100); // 0~1023을 0~100으로 매핑

  // 디버깅 정보 출력
  Serial.print("FSR Reading: ");
  Serial.print(fsrReading);
  Serial.print(" | Mapped Value: ");
  Serial.print(mappedValue);
  Serial.print(" | Mode: ");
  Serial.println(soundMode ? "SOUND" : "VIBRATE");

  // FSR 값이 22 이상일 때 현재 모드에 따라 동작
  if (mappedValue >= 22) {
    if (soundMode) {
      // 현재 온도 값을 함께 전달
      sensors.requestTemperatures();
      float currentTemp = sensors.getTempCByIndex(0);
      if (currentTemp != DEVICE_DISCONNECTED_C) {
        triggerSoundMode(currentTemp); // 소리 모드 (온도 값 전달)
      } else {
        triggerSoundMode(20.0); // 센서 오류 시 기본값 사용
      }
    } else {
      triggerVibrationMode(); // 진동 모드
    }
  }

  // DFPlayer 피드백 확인 (소리 모드일 때만)
  if (soundMode && myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    uint8_t data = myDFPlayer.read();

    // 재생이 끝났을 경우 플래그 초기화
    if (type == DFPlayerPlayFinished) {
      isPlaying = false;
      Serial.println("재생 완료");
    } else {
      // 다른 피드백 타입 처리 (디버깅용)
      Serial.print("DFPlayer type: ");
      Serial.print(type);
      Serial.print(", data: ");
      Serial.println(data);
    }
  }

  delay(50); // 루프 간격
}