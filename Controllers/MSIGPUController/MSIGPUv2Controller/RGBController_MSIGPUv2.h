/*---------------------------------------------------------*\
| RGBController_MSIGPUv2.h                                  |
|                                                           |
|   RGBController for MSI V2 GPU (ITE9)                     |
|                                                           |
|   Wojciech Lazarski                           03 Jan 2023 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "pci_ids.h"
#include "RGBController.h"
#include "MSIGPUv2Controller.h"

class RGBController_MSIGPUv2 : public RGBController
{
public:
    RGBController_MSIGPUv2(MSIGPUv2Controller* controller_ptr, int msi_gpu_id);
    ~RGBController_MSIGPUv2();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();
    void        DeviceSaveMode();

private:
    MSIGPUv2Controller* controller;
    bool                rainbow_variants;
    bool                per_led_protocol    = false;
    unsigned int        per_led_count       = MSI_GPU_V2_PER_LED_SLOTS;
    bool                rtx50               = false;
    std::atomic<bool>   stream_exit_pending{true};
    int                 stream_exit_failures = 0;
    std::atomic<bool>   first_apply_logged{false};

    /*-----------------------------------------------------*\
    | Register path (cards without per-LED Direct): once a  |
    | full update has put the card in static mode, a Direct |
    | color change writes only the registers that changed   |
    \*-----------------------------------------------------*/
    bool                register_direct_ready   = false;
    RGBColor            register_direct_color   = 0;

    /*-----------------------------------------------------*\
    | Per-LED Direct frames go to a worker that writes the  |
    | latest one, at most once per minimum interval.        |
    | write_mutex serializes its bus writes with the mode   |
    | writes; direct_mutex guards the fields below it. A    |
    | non-Direct mode bumps direct_generation so a frame    |
    | taken before it is never written after it             |
    \*-----------------------------------------------------*/
    std::mutex                              write_mutex;
    std::mutex                              direct_mutex;
    std::condition_variable                 direct_cv;
    std::thread                             direct_thread;
    bool                                    direct_running      = true;
    bool                                    direct_pending      = false;
    bool                                    direct_primed       = false;
    unsigned int                            direct_generation   = 0;
    unsigned int                            direct_reprime      = 0;
    RGBColor                                pending_colors[MSI_GPU_V2_PER_LED_SLOTS]    = {};
    unsigned char                           pending_brightness  = 0;
    RGBColor                                applied_colors[MSI_GPU_V2_PER_LED_SLOTS]    = {};
    unsigned char                           applied_brightness  = 0;
    std::chrono::steady_clock::time_point   direct_write_time;

    void       DeviceUpdateAll(const mode& current_mode, bool save = false);
    void       UpdateRegisterDirectColor(RGBColor color);
    void       QueueDirect(bool reprime);
    void       DirectThreadFunction();
};
