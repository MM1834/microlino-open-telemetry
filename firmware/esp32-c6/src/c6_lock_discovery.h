#pragma once

#include "can/can_types.h"

void c6LockDiscoveryReset();
void c6LockDiscoveryObserve(size_t channel, const MotCanFrame &frame);
void c6LockDiscoveryStart();
void c6LockDiscoveryStop();
void c6LockDiscoveryDump();
void c6LockDiscoveryDiff();
void c6LockDiscoveryVersionSearch();
