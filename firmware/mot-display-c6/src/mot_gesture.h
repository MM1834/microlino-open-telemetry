#pragma once

void motGestureSetup();
enum class MotGestureEvent {
    None,
    TiltLeft,
    TiltRight,
    ServiceTiltLeft,
    ServiceTiltRight,
};
MotGestureEvent motGesturePoll(bool serviceHoldMode = false);
bool motGestureAvailable();
