#include <Arduino.h>

#define PIR PB12
bool baby_awake = false;

bool motion()
{
    baby_awake = false;
    int number_of_motion = 0;
    bool motion = digitalRead(PIR);
    bool last_State = false;
    if (motion) {
        unsigned long interval = millis();
        number_of_motion += 1;
        while (millis() - interval <= 8000) {
            last_State = motion;
            motion = digitalRead(PIR);
            if (motion && !last_State) {
                number_of_motion += 1;
            }
            unsigned long sample_time = millis();
            while (millis() - sample_time <= 500) {
            }
        }
        if (number_of_motion >= 4) {
            baby_awake = 1;
        }
    }
    return baby_awake;
}

void setup()
{
    pinMode(PIR, INPUT);
}

void loop()
{
}
