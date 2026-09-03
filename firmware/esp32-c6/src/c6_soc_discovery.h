#pragma once

#include "can/can_types.h"

void c6SocDiscoveryReset();
void c6SocDiscoveryObserve(size_t channel, const MotCanFrame &frame);
void c6SocDiscoveryMark();
void c6SocDiscoveryDump();
void c6SocDiscovery48dDump();
