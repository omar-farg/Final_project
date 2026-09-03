#include <cmath>
#include <Servo.h>
//rgb led pin
#define RED PA2
#define GREEN PA3
#define BLUE PA4
//ntc pin and beta formula constants
#define THERMISTOR_PIN PA1

//motor driver pins
#define ENA_PIN PB0
#define IN1_PIN PB2
#define IN2_PIN PB3
//pir sensor pin
#define PIR_PIN PB12
//gas sensor pin
#define GAS_PIN PA5
//buzzer pin
#define BUZZER_PIN PA8//does not require PWM pin
//servo pin
#define SERVO PA7
//ldr sensor pin
#define LDR_PIN PA0
//constants that will be used
//tempreature constants and voltage divider resistance
#define BETA_COEFFICIENT 3950.0
#define DIVIDER_RESISTOR 10000.0 //10k ohms resistor for voltage divider
#define ROOM_TEMP_RESISTANCE 10000.0
#define ROOM_TEMP_KELVIN 298.15
//8 seconds =8000 milli seconds
#define WINDOW_TIME 8000
//safe threshold for gas sensor
#define THRESHOLD 1500


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


//getting the tempreature 
float get_temprerature(void){
  float acd_read=analogRead(THERMISTOR_PIN);
  float volt=(acd_read/4095.0)*3.3;
  if(volt<=0.01)volt=0.01;//saftey to avoid divide by 0
  float ntc_resistance=DIVIDER_RESISTOR*((3.3/volt)-1);
  //the following lines will calculate the temp using the beta formula which is 1/T=1 / T0 + 1 / β * ln(R / R0)
  float temp=1.0/ROOM_TEMP_KELVIN+log(ntc_resistance/ROOM_TEMP_RESISTANCE)/BETA_COEFFICIENT;
  temp=(1.0/temp)-273.15;//converting to celcuis
  return temp;
}
//updates the states of the fans and rgb led
void update_fan_and_rgb(float temp){
  if(temp<25){
    digitalWrite(BLUE, HIGH);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
    
    digitalWrite(IN1_PIN,LOW);
    digitalWrite(IN2_PIN,LOW);
    analogWrite(ENA_PIN, 0);
  }
  else if (temp>=25&&temp<=30) {
    digitalWrite(BLUE, LOW);
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);

    digitalWrite(IN1_PIN,HIGH);
    digitalWrite(IN2_PIN,LOW);
    analogWrite(ENA_PIN, 127);
  }
  else{
    digitalWrite(BLUE, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);

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
void detect_gas(void){
  unsigned short int gas_value=analogRead(GAS_PIN);
  if(gas_value>THRESHOLD){
    
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

void light_on(float voltage){
  if (voltage > 2.5 && !LED_ON && (baby_awake || baby_crying)) {
    LED_ON = true;
    Serial.println("lights on");
    } 
    else if(LED_ON&&voltage<=2.5){
      LED_ON = false;
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
  //gas sensor pin
  pinMode(GAS_PIN, INPUT);
  //buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  //servo pin
  serv.attach(SERVO);
  serv.write(90);
  //ldr pin
  pinMode(LDR_PIN, INPUT);
  //serial monitor 
  Serial.begin(9600);
}

void loop(void) {
  current_millis=millis();
  float temp=get_temprerature();
  update_fan_and_rgb(temp);
  detect_motion();
  detect_gas();
  float volt=LDR_Sensor();
  light_on(volt);
  servo_bed();
}
