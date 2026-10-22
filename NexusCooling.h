/*---------------------------------------------------------*\
| NexusCooling.h                                            |
|                                                           |
|   Cooling access for controllers that also drive fans or  |
|   pumps, served over the Nexus SDK packets. Both calls    |
|   answer from cached state and never block on the device. |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <vector>

class NexusCoolingDevice
{
public:
    virtual ~NexusCoolingDevice() {}

    virtual bool NexusCoolingGetStatus(std::vector<unsigned char>& payload) = 0;
    virtual bool NexusCoolingSetDuties(const unsigned char* payload, unsigned int size) = 0;
};
