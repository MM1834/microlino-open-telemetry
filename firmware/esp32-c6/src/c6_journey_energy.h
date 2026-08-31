#pragma once

#include <Arduino.h>

class MotAwsIotClient;

void c6JourneyEnergySetup();
void c6JourneyEnergyLoop();
bool c6JourneyEnergyPublish(MotAwsIotClient &client);
String c6JourneyEnergyStatusJson();
