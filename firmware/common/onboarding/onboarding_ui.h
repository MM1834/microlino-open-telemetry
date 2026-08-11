#pragma once

#include <Arduino.h>

#include "onboarding.h"

inline uint8_t onboardingClampStep(int requested)
{
    if (requested < 1) return 1;
    if (requested > onboardingStepCount()) return onboardingStepCount();
    return static_cast<uint8_t>(requested);
}

inline String onboardingProgress(uint8_t step)
{
    return "<div class='muted'>Step " + String(step) + " of " +
           String(onboardingStepCount()) + "</div><progress value='" + String(step) +
           "' max='" + String(onboardingStepCount()) + "' style='width:100%'></progress>";
}

inline String onboardingNavigation(uint8_t step)
{
    String html = "<p>";
    if (step > 1) html += "<a href='/wizard?step=" + String(step - 1) + "'><button type='button'>Back</button></a> ";
    if (step < onboardingStepCount()) html += "<a href='/wizard?step=" + String(step + 1) + "'><button type='button'>Next</button></a>";
    html += "</p>";
    return html;
}
