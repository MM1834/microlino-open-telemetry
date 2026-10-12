#pragma once

void motGestureSetup();
enum class MotGestureEvent { None, TiltLeft, TiltRight };
MotGestureEvent motGesturePoll(bool serviceHoldMode = false);
bool motGestureAvailable();
