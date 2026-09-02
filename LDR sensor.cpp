int voltage_divider_Risistor = 10000;
bool baby_awake = false;
bool baby_crying = false;
bool LED_ON = false;

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
    }

    return LED_ON;
}

