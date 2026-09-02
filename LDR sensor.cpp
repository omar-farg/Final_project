int temp;
bool baby_awake = false;
bool baby_crying = false;
bool LED_ON = false;
int current_state = 0;
int angle = 0;
int sin_val = 0;
unsigned long time = millis();

float LDR_Sensor()
{
    int light_level = analogRead(LDR);
    float voltage = (light_level * 3.3) / 4095;
    return voltage;
}

bool light_on(float voltage, bool LED_ON, bool baby_awake, bool baby_crying)
{
    if (voltage > 2.5 && !LED_ON && (baby_awake || baby_crying)) {
        LED_ON = 1;
        serial.println("lights on");
    } else {
        LED_ON = 0;
        serial.println("lights off");
    }
    return LED_ON;
}

void cooling_motor(int temp)
{
    if (temp < 25) {
        analogWrite(Motor_thermo, 0);
        serial.println("room is cold");
    } else if (temp >= 25 && temp <= 30) {

        analogWrite(Motor_thermo, 255 / 2);
        serial.println("room is a bit hot");

    } else {
        analogWrite(Motor_thermo, 255);
        serial.println("room is hot");
    }
}
void RGB_Led(int temp)
{
    if (temp < 25) {
        analogWrite(RGB_blue, 255);
        analogWrite(RGB_green, 0);
        analogWrite(RGB_red, 0);

    } else if (temp >= 25 && temp <= 30) {

        analogWrite(RGB_green, 255);
        analogWrite(RGB_red, 0);
        analogWrite(RGB_blue, 0);

    } else {
        analogWrite(RGB_red, 255);
        analogWrite(RGB_blue, 0);
        analogWrite(RGB_green, 0);
    }
}

void servo_bed(bool baby_crying)
{
    if (baby_crying) {
        if (millis() - time >= 500) {
            time = millis();
            sin_val = sin(current_state * DEG_TO_RAD);
            angle = round(sin_val);
            switch (sin_val) {
            case 0:
                angle = 0;
                break;
            case 1:
                angle = 90;
                break;
            case -1:
                angle = -90;
                break;
            default:
                break;
            }
            servo.write(angle);
            current_state = (current_state + 90) % 360;
        }
    }
}
}

