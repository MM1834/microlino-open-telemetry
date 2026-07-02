#include <Arduino.h>
#include "board_config.h"
#include "app/mot_app.h"

void setup()
{
    Serial.begin(115200);
    delay(1000);
    motAppSetup();
}

void loop()
{
    motAppLoop();
    delay(1000);
}
