#include <cmath>
#include <Servo.h>
//rgb led pin
#define RED PB4
#define GREEN PB5
#define BLUE PA4
//ntc module pin (analog OUT pin from the module)
#define THERMISTOR_PIN PA1

//motor driver pins
#define ENA_PIN PB0
#define IN1_PIN PB2
#define IN2_PIN PB3
//pir sensor pin
#define PIR_PIN PB12
//gas sensor pin (now DIGITAL output, not analog)
#define GAS_PIN PA5
//buzzer pin
#define BUZZER_PIN PA8//does not require PWM pin

//servo pin
#define SERVO PA7
//ldr sensor pin
#define LDR_PIN PA0
//room lamp LED pin (turns on when dark AND baby awake or crying)
#define LAMP_PIN PB1
//constants that will be used
//tempreature constants
// NOTE: using a ready-made NTC module now (VCC / GND / OUT pins).
// The voltage-divider resistor is INSIDE the module, not on our board anymore.
// MODULE_FIXED_RESISTOR below is our best guess for that internal resistor's
// value (10k is the common default on cheap modules) — change it if your
// module's datasheet says otherwise.
#define BETA_COEFFICIENT 3950.0
#define MODULE_FIXED_RESISTOR 10000.0
#define ROOM_TEMP_RESISTANCE 10000.0
#define ROOM_TEMP_KELVIN 298.15
//8 seconds =8000 milli seconds
#define WINDOW_TIME 8000


//intializing servo
Servo serv;


//global variables for the pir sensor
unsigned long time_of_motion[4]={0,0,0,0};
uint8_t  motion_index=0;//i found that uint8_t is more memory effectient since its just 1 byte
bool last_sensor_state=LOW;
bool baby_awake=false;
bool baby_crying = false;//we will know this from the AI part
bool LED_ON = false;
unsigned long current_millis = millis();
unsigned long last_servo_time=0;
uint8_t servo_step=0;
bool gas_alert=false;


//getting the tempreature from the NTC module
// The module's OUT pin gives an analog voltage that already reflects its
// internal divider (module VCC -> internal fixed resistor -> OUT -> NTC -> GND,
// or the reverse depending on the module — same math applies either way as long
// as the wiring direction matches how MODULE_FIXED_RESISTOR is used below).
float get_temprerature(void){
  float acd_read=analogRead(THERMISTOR_PIN);
  float volt=(acd_read/4095.0)*3.3;
  if(volt<=0.01)volt=0.01;//saftey to avoid divide by 0
  if(volt>=3.29)volt=3.29;//saftey to avoid divide by 0 on the other side
  float ntc_resistance=MODULE_FIXED_RESISTOR*volt/(3.3-volt);
  //the following lines will calculate the temp using the beta formula which is 1/T=1 / T0 + 1 / β * ln(R / R0)
  float temp=1.0/ROOM_TEMP_KELVIN+log(ntc_resistance/ROOM_TEMP_RESISTANCE)/BETA_COEFFICIENT;
  temp=(1.0/temp)-273.15;//converting to celcuis
  return temp;
}
//updates the states of the fans and rgb led
// NOTE: RGB LED is Common Anode (D1 RGBLED-CA in schematic) -> LOW = ON, HIGH = OFF
// All digitalWrite calls below are inverted compared to a common-cathode LED.
void update_fan_and_rgb(float temp){
  if(temp<25){
    digitalWrite(BLUE, LOW);    // ON
    digitalWrite(GREEN, HIGH);  // OFF
    digitalWrite(RED, HIGH);    // OFF
    
    digitalWrite(IN1_PIN,LOW);
    digitalWrite(IN2_PIN,LOW);
    analogWrite(ENA_PIN, 0);
  }
  else if (temp>=25&&temp<=30) {
    digitalWrite(BLUE, HIGH);   // OFF
    digitalWrite(GREEN, LOW);   // ON
    digitalWrite(RED, HIGH);    // OFF

    digitalWrite(IN1_PIN,HIGH);
    digitalWrite(IN2_PIN,LOW);
    analogWrite(ENA_PIN, 127);
  }
  else{
    digitalWrite(BLUE, HIGH);   // OFF
    digitalWrite(GREEN, HIGH);  // OFF
    digitalWrite(RED, LOW);     // ON

    digitalWrite(IN1_PIN,HIGH);
    digitalWrite(IN2_PIN,LOW);
    analogWrite(ENA_PIN, 255);
  }
}


//motion detection function
void detect_motion(void){
  int current_sensor_state=digitalRead(PIR_PIN);
  if(current_sensor_state==HIGH && last_sensor_state==LOW){
    time_of_motion[motion_index]=current_millis;
    motion_index=(motion_index+1)%4;//updates the index the modulus is there becuse the array size is just 4
    
    if(time_of_motion[motion_index]!=0 && current_millis-time_of_motion[motion_index]<=WINDOW_TIME){
      if(!baby_awake){
        Serial.println("BABY AWAKE ALERT!");
        baby_awake=true;
      }
      for(uint8_t i=0;i<4;i++)time_of_motion[i]=0;//resets the array of motions
    }   
  }
  last_sensor_state=current_sensor_state;
}

//gas detection function - NOW DIGITAL
// Most digital gas modules (LM393 comparator board) are ACTIVE-LOW:
//   no gas  -> pin reads HIGH
//   gas!    -> pin reads LOW
// If your module works the opposite way, just flip the condition below
// (change "gas_state == LOW" to "gas_state == HIGH").
void detect_gas(void){
  int gas_state = digitalRead(GAS_PIN);

  if(gas_state == LOW){
    digitalWrite(BUZZER_PIN, HIGH);
    if(!gas_alert){
      gas_alert=true;
      Serial.println("ALERT:GAS DETECTED");
    }
  }
  else{
    digitalWrite(BUZZER_PIN, LOW);
    if(gas_alert){
      gas_alert = false;
      Serial.println("STATUS:GAS CLEAR");
    }
  }
}

float LDR_Sensor(void){
  unsigned short int light_level = analogRead(LDR_PIN);
  float voltage = (light_level * 3.3) / 4095;
  return voltage;
}

// Turns the physical room lamp ON only when:
//   room is dark (voltage > 2.5)  AND  (baby is awake OR a cry is detected)
// Turns it OFF the moment the room becomes bright again, regardless of baby state.
void light_on(float voltage){
  bool should_be_dark = (voltage > 2.5);
  bool should_be_on = should_be_dark && (baby_awake || baby_crying);

  if (should_be_on && !LED_ON) {
    LED_ON = true;
    digitalWrite(LAMP_PIN, HIGH);
    Serial.println("lights on");
  }
  else if (!should_be_on && LED_ON) {
    LED_ON = false;
    digitalWrite(LAMP_PIN, LOW);
    Serial.println("lights off");
  }
}

void servo_bed(void){
  if(baby_crying){
    if(current_millis-last_servo_time>=500){
      last_servo_time=current_millis;
      switch (servo_step) {
      case 0://0 deg (center)
        serv.write(90);
        break;
      case 1://+90 deg (to the right)
        serv.write(180);
        break;
      case 2://-90 deg (to the left)
        serv.write(0);
        break;
      }
      servo_step=(servo_step+1)%3;//to keep it circulating between 0,1,2
    }
  }
  else {
    serv.write(90);
    servo_step=0;
  }
}
//to monitor the temperature 
void print_temp(float temp){
  static unsigned long last_temp_time = 0;
  if(current_millis-last_temp_time>=2000){
    last_temp_time=current_millis;
    Serial.print("temperature:")
    Serial.println(temp);
  }
}
void setup(void) {
  analogReadResolution(12);//uses the full 12 bit ADC resolution instead of the 10 bit that the arduino use
  pinMode(THERMISTOR_PIN, INPUT);
  //rgb led pins
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  //motor driver pins
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  //pir pin
  pinMode(PIR_PIN,INPUT);
  //gas sensor pin - now digital input
  pinMode(GAS_PIN, INPUT);
  //buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  //servo pin
  serv.attach(SERVO);
  serv.write(90);
  //ldr pin
  pinMode(LDR_PIN, INPUT);
  //room lamp pin
  pinMode(LAMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, LOW);
  //serial monitor 
  Serial.begin(9600);
}

void loop(void) {
  current_millis=millis();
  //for communication between the stm and python
  if(Serial.available() > 0){
    String recievd_message=Serial.readStringUntil('\n');
    recievd_message.trim();
    if(recievd_message=="CRY:ON"){
      baby_crying=true;
    }
    else if (recievd_message=="CRY:OFF"){
      baby_crying=false;
    }
  }
  float temp=get_temprerature();
  update_fan_and_rgb(temp);
  detect_motion();
  detect_gas();
  float volt=LDR_Sensor();
  light_on(volt);
  servo_bed();
  print_temp(temp);
}
