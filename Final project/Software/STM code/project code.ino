#include <Servo.h>
#include <math.h>

// =====================================================
// RGB LED - COMMON CATHODE
// =====================================================
#define RED_PIN    PA2
#define GREEN_PIN  PA3
#define BLUE_PIN   PA4

// =====================================================
// NTC THERMISTOR (Powered by 5V - Standard Config)
// =====================================================
#define THERMISTOR_PIN PA1

#define SERIES_RESISTOR     10000.0
#define NOMINAL_RESISTANCE 10000.0
#define NOMINAL_TEMPERATURE 25.0
#define BETA_COEFFICIENT   3950.0

// =====================================================
// FAN MOTOR (L293D)
// =====================================================
#define ENA_PIN PB0 // PWM pin for speed control
#define IN1_PIN PB6 // Motor direction 1
#define IN2_PIN PB5 // Motor direction 2

// =====================================================
// PIR
// =====================================================
#define PIR_PIN PB13

// =====================================================
// GAS SENSOR DIGITAL
// =====================================================
#define GAS_PIN PA5

// =====================================================
// BUZZER
// =====================================================
#define BUZZER_PIN PA8

// =====================================================
// SERVO
// =====================================================
#define SERVO_PIN PA7
Servo serv;

// =====================================================
// LDR & LAMP
// =====================================================
#define LDR_PIN PA0
#define LDR_THRESHOLD 2000
#define LAMP_PIN PB1

// =====================================================
// SYSTEM TIMING & FLAGS
// =====================================================
unsigned long current_millis = 0;

// Servo variables
unsigned long last_servo_time = 0;
uint8_t servo_step = 0;
bool baby_crying = false;

// PIR variables
unsigned long time_of_motion[4] = {0, 0, 0, 0};
uint8_t motion_index = 0;
bool last_sensor_state = LOW;
bool baby_awake = false;

// Gas and Temp variables
bool gas_alert = false;
unsigned long last_temp_time = 0;


// =====================================================
// READ NTC TEMPERATURE 
// =====================================================
float readNTCTemperature()
{
  int adcValue = analogRead(THERMISTOR_PIN);

  if (adcValue <= 0 || adcValue >= 4095)
  {
    return NAN;
  }

  // 1. تحويل القراءة الرقمية لفولت (بناءً على إن الـ STM بيقرا بحد أقصى 3.3V)
  float voltageOut = ((float)adcValue / 4095.0) * 3.3;

  if(voltageOut >= 5.0 || voltageOut <= 0.0) return NAN;

  // 2. المعادلة الصحيحة للموديول بتاعك (NTC متوصل بالأرضي ومصدر الطاقة 5V)
  // R_ntc = 10k * Vout / (5V - Vout)
  float resistance = SERIES_RESISTOR * voltageOut / (5.0 - voltageOut);

  // 3. معادلة Beta
  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= BETA_COEFFICIENT;
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;

  return steinhart;
}


// =====================================================
// TEMPERATURE -> RGB & FAN MOTOR (L293D)
// =====================================================
void updateRGB_and_FAN(float temp)
{
  // أقل 25°C -> BLUE (Fan OFF)
  if (temp < 25.0)
  {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, HIGH);
    
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    analogWrite(ENA_PIN, 0);
  }
  // 25°C إلى 30°C -> GREEN (Fan 50%)
  else if (temp >= 25.0 && temp < 30.0)
  {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
    digitalWrite(BLUE_PIN, LOW);

    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
    analogWrite(ENA_PIN, 127);
  }
  // أكبر من 30°C -> RED (Fan 100%)
  else
  {
    digitalWrite(RED_PIN, HIGH);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);

    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
    analogWrite(ENA_PIN, 255);
  }
}


// =====================================================
// PIR
// =====================================================
void checkPIR()
{
  int current_sensor_state = digitalRead(PIR_PIN);
  static unsigned long last_motion_time = 0; 

  if (baby_awake && (current_millis - last_motion_time > 15000)) {
    baby_awake = false; 
    Serial.println("Baby Returned to sleep");
  }
  
  if (current_sensor_state == HIGH) {
    last_motion_time = current_millis;
  }

  if (current_sensor_state == HIGH && last_sensor_state == LOW)
  {
    if (!baby_awake) {
      Serial.println("BABY AWAKE ALERT!");
      baby_awake = true; 
    }
  }
  last_sensor_state = current_sensor_state;
}

// =====================================================
// GAS DIGITAL DO -> BUZZER
// =====================================================
void checkGas()
{
  int gasState = digitalRead(GAS_PIN);

  if (gasState == LOW)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    if (!gas_alert) {
      gas_alert = true; 
      Serial.println("ALERT:GAS DETECTED");
    }
  }
  else
  {
    digitalWrite(BUZZER_PIN, LOW);
    if (gas_alert) {
      gas_alert = false; 
      Serial.println("STATUS:GAS CLEAR");
    }
  }
}

// =====================================================
// LDR -> LAMP
// =====================================================
void checkLDR()
{
  int lightValue = analogRead(LDR_PIN);

  if (lightValue < LDR_THRESHOLD)
  {
    digitalWrite(LAMP_PIN, LOW);
  }
  else
  {
    digitalWrite(LAMP_PIN, HIGH);
  }
}

// =====================================================
// SERVO BED FUNCTION
// =====================================================
void servo_bed(void){
  if(baby_crying){
    if(current_millis - last_servo_time >= 500){
      last_servo_time = current_millis;
      switch (servo_step) {
      case 0: // 0 deg (center)
        serv.write(90);
        break;
      case 1: // +90 deg (to the right)
        serv.write(180);
        break;
      case 2: // -90 deg (to the left)
        serv.write(0);
        break;
      }
      servo_step = (servo_step + 1) % 3; 
    }
  }
  else {
    serv.write(90);
    servo_step = 0;
  }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  analogReadResolution(12);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  analogWrite(ENA_PIN, 0);

  pinMode(THERMISTOR_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(LAMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, LOW);

  serv.attach(SERVO_PIN);
  serv.write(90); 

  Serial.begin(9600);
  delay(1000);
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  current_millis = millis();

  if (Serial.available() > 0) {
    String recievd_message = Serial.readStringUntil('\n');
    recievd_message.trim();
    if (recievd_message == "CRY:ON") {
      baby_crying = true;
    }
    else if (recievd_message == "CRY:OFF") {
      baby_crying = false;
    }
  }

  // 1. Read Temperature
  float temperature = readNTCTemperature();
  
  if (!isnan(temperature))
  {
    if (current_millis - last_temp_time >= 3000) {
      last_temp_time = current_millis;
      Serial.print("temperature = ");
      Serial.print(temperature);
      Serial.println(" C");
    }
    
    updateRGB_and_FAN(temperature);
  }
  else
  {
    if (current_millis - last_temp_time >= 3000) {
      last_temp_time = current_millis;
      Serial.println("ERROR: NTC / ADC");
    }
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
    
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    analogWrite(ENA_PIN, 0);
  }

  // 2. Process other sensors
  checkPIR();
  checkGas();
  checkLDR();
  servo_bed();

  delay(100);
}
