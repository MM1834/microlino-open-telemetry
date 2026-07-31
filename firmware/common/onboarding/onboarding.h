#pragma once
#include <Arduino.h>

enum class OnboardingStep : uint8_t {
    Welcome = 0,
    Hardware,
    Connectivity,
    Vehicle,
    Services,
    Validation,
    Finish
};

struct OnboardingCapabilities {
    const char* board;
    bool wifi;
    bool lte;
    bool gps;
    uint8_t canChannels;
};

inline const char* onboardingStepId(OnboardingStep step)
{
    switch (step) {
        case OnboardingStep::Welcome: return "welcome";
        case OnboardingStep::Hardware: return "hardware";
        case OnboardingStep::Connectivity: return "connectivity";
        case OnboardingStep::Vehicle: return "vehicle";
        case OnboardingStep::Services: return "services";
        case OnboardingStep::Validation: return "validation";
        case OnboardingStep::Finish: return "finish";
    }
    return "welcome";
}

inline uint8_t onboardingStepNumber(OnboardingStep step) { return static_cast<uint8_t>(step) + 1; }
inline constexpr uint8_t onboardingStepCount() { return 7; }
