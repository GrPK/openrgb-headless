/*---------------------------------------------------------*\
| RGBController_MSIGPUv2.cpp                                |
|                                                           |
|   RGBController for MSI V2 GPU (ITE9)                     |
|                                                           |
|   Wojciech Lazarski                           03 Jan 2023 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <array>
#include <string>
#include "RGBController_MSIGPUv2.h"
#include "LogManager.h"

static const unsigned char speed_values[3]      = { 0x04, 0x02, 0x01 };

/**------------------------------------------------------------------*\
    @name MSI GPU v2
    @category GPU
    @type I2C
    @save :white_check_mark:
    @direct :white_check_mark:
    @effects :white_check_mark:
    @detectors DetectMSIGPUv2Controllers
    @comment
\*-------------------------------------------------------------------*/

RGBController_MSIGPUv2::RGBController_MSIGPUv2(MSIGPUv2Controller * controller_ptr, int msi_gpu_id)
{
    controller                      = controller_ptr;

    name                            = controller->GetDeviceName();
    vendor                          = "MSI";
    type                            = DEVICE_TYPE_GPU;
    description                     = "MSI GPU V2 Device";
    location                        = controller->GetDeviceLocation();

    mode Off;
    Off.name                        = "Off";
    Off.value                       = MSI_GPU_V2_MODE_OFF;
    Off.flags                       = MODE_FLAG_MANUAL_SAVE;
    Off.color_mode                  = MODE_COLORS_NONE;
    modes.push_back(Off);

    mode Direct;
    Direct.name                     = "Direct";
    Direct.value                    = MSI_GPU_V2_MODE_STATIC;
    Direct.flags                    = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_MANUAL_SAVE;
    Direct.brightness_min           = MSI_GPU_V2_BRIGHTNESS_MIN;
    Direct.brightness               = MSI_GPU_V2_BRIGHTNESS_MAX;
    Direct.brightness_max           = MSI_GPU_V2_BRIGHTNESS_MAX;
    Direct.color_mode               = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    mode Rainbow;
    Rainbow.name                    = "Rainbow Wave";
    Rainbow.value                   = MSI_GPU_V2_MODE_RAINBOW;
    Rainbow.flags                   = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
    Rainbow.speed_min               = MSI_GPU_V2_SPEED_MIN;
    Rainbow.speed                   = MSI_GPU_V2_SPEED_MID;
    Rainbow.speed_max               = MSI_GPU_V2_SPEED_MAX;
    Rainbow.brightness_min          = MSI_GPU_V2_BRIGHTNESS_MIN;
    Rainbow.brightness              = MSI_GPU_V2_BRIGHTNESS_MAX;
    Rainbow.brightness_max          = MSI_GPU_V2_BRIGHTNESS_MAX;
    Rainbow.color_mode              = MODE_COLORS_RANDOM;
    modes.push_back(Rainbow);

    mode Magic;
    Magic.name                      = "Magic";
    Magic.value                     = MSI_GPU_V2_MODE_MAGIC;
    Magic.flags                     = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
    Magic.speed_min                 = MSI_GPU_V2_SPEED_MIN;
    Magic.speed                     = MSI_GPU_V2_SPEED_MID;
    Magic.speed_max                 = MSI_GPU_V2_SPEED_MAX;
    Magic.brightness_min            = MSI_GPU_V2_BRIGHTNESS_MIN;
    Magic.brightness                = MSI_GPU_V2_BRIGHTNESS_MAX;
    Magic.brightness_max            = MSI_GPU_V2_BRIGHTNESS_MAX;
    Magic.color_mode                = MODE_COLORS_RANDOM;
    modes.push_back(Magic);

    mode ColorCycle;
    ColorCycle.name                 = "Color Cycle";
    ColorCycle.value                = MSI_GPU_V2_MODE_MAGIC;
    ColorCycle.flags                = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_MANUAL_SAVE;
    ColorCycle.speed_min            = MSI_GPU_V2_SPEED_MIN;
    ColorCycle.speed                = MSI_GPU_V2_SPEED_MID;
    ColorCycle.speed_max            = MSI_GPU_V2_SPEED_MAX;
    ColorCycle.brightness_min       = MSI_GPU_V2_BRIGHTNESS_MIN;
    ColorCycle.brightness           = MSI_GPU_V2_BRIGHTNESS_MAX;
    ColorCycle.brightness_max       = MSI_GPU_V2_BRIGHTNESS_MAX;
    ColorCycle.colors_min           = 1;
    ColorCycle.colors_max           = 3;
    ColorCycle.colors.resize(3);
    ColorCycle.color_mode           = MODE_COLORS_MODE_SPECIFIC;
    modes.push_back(ColorCycle);


    mode Patrolling;
    Patrolling.name                 = "Patrolling";
    Patrolling.value                = MSI_GPU_V2_MODE_PATROLLING;
    Patrolling.flags                = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    Patrolling.speed_min            = MSI_GPU_V2_SPEED_MIN;
    Patrolling.speed                = MSI_GPU_V2_SPEED_MID;
    Patrolling.speed_max            = MSI_GPU_V2_SPEED_MAX;
    Patrolling.brightness_min       = MSI_GPU_V2_BRIGHTNESS_MIN;
    Patrolling.brightness           = MSI_GPU_V2_BRIGHTNESS_MAX;
    Patrolling.brightness_max       = MSI_GPU_V2_BRIGHTNESS_MAX;
    Patrolling.color_mode           = MODE_COLORS_PER_LED;
    modes.push_back(Patrolling);

    mode Streaming;
    Streaming.name                  = "Streaming";
    Streaming.value                 = MSI_GPU_V2_MODE_STREAMING;
    Streaming.flags                 = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
    Streaming.speed_min             = MSI_GPU_V2_SPEED_MIN;
    Streaming.speed                 = MSI_GPU_V2_SPEED_MID;
    Streaming.speed_max             = MSI_GPU_V2_SPEED_MAX;
    Streaming.brightness_min        = MSI_GPU_V2_BRIGHTNESS_MIN;
    Streaming.brightness            = MSI_GPU_V2_BRIGHTNESS_MAX;
    Streaming.brightness_max        = MSI_GPU_V2_BRIGHTNESS_MAX;
    Streaming.color_mode            = MODE_COLORS_RANDOM;
    modes.push_back(Streaming);

    mode Lightning;
    Lightning.name                  = "Lightning";
    Lightning.value                 = MSI_GPU_V2_MODE_LIGHTNING;
    Lightning.flags                 = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
    Lightning.speed_min             = MSI_GPU_V2_SPEED_MIN;
    Lightning.speed                 = MSI_GPU_V2_SPEED_MID;
    Lightning.speed_max             = MSI_GPU_V2_SPEED_MAX;
    Lightning.brightness_min        = MSI_GPU_V2_BRIGHTNESS_MIN;
    Lightning.brightness            = MSI_GPU_V2_BRIGHTNESS_MAX;
    Lightning.brightness_max        = MSI_GPU_V2_BRIGHTNESS_MAX;
    Lightning.color_mode            = MODE_COLORS_RANDOM;
    modes.push_back(Lightning);

    mode Wave;
    Wave.name                       = "Wave";
    Wave.value                      = MSI_GPU_V2_MODE_RAINBOW;      //Rainbow has two modes now
    Wave.flags                      = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_MANUAL_SAVE;
    Wave.speed_min                  = MSI_GPU_V2_SPEED_MIN;
    Wave.speed                      = MSI_GPU_V2_SPEED_MID;
    Wave.speed_max                  = MSI_GPU_V2_SPEED_MAX;
    Wave.brightness_min             = MSI_GPU_V2_BRIGHTNESS_MIN;
    Wave.brightness                 = MSI_GPU_V2_BRIGHTNESS_MAX;
    Wave.brightness_max             = MSI_GPU_V2_BRIGHTNESS_MAX;
    Wave.colors_min                 = 1;
    Wave.colors_max                 = 3;
    Wave.colors.resize(3);
    Wave.color_mode                 = MODE_COLORS_MODE_SPECIFIC;
    modes.push_back(Wave);

    mode Meteor;
    Meteor.name                     = "Meteor";
    Meteor.value                    = MSI_GPU_V2_MODE_METEOR;
    Meteor.flags                    = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    Meteor.speed_min                = MSI_GPU_V2_SPEED_MIN;
    Meteor.speed                    = MSI_GPU_V2_SPEED_MID;
    Meteor.speed_max                = MSI_GPU_V2_SPEED_MAX;
    Meteor.brightness_min           = MSI_GPU_V2_BRIGHTNESS_MIN;
    Meteor.brightness               = MSI_GPU_V2_BRIGHTNESS_MAX;
    Meteor.brightness_max           = MSI_GPU_V2_BRIGHTNESS_MAX;
    Meteor.color_mode               = MODE_COLORS_PER_LED;
    modes.push_back(Meteor);

    switch(msi_gpu_id)
    {
        case MSI_RTX4060TI_GAMING_X_16G_SLIM_WHITE_SUB_DEV | NVIDIA_RTX4060TI_16G_DEV << 16:
        case MSI_RTX4070S_GAMING_X_SLIM_SUB_DEV | NVIDIA_RTX4070S_DEV << 16:
        case MSI_RTX4070TI_GAMING_X_TRIO_WHITE_SUB_DEV | NVIDIA_RTX4070TIS_DEV << 16:
        case MSI_RTX4080S_GAMING_X_TRIO_SUB_DEV | NVIDIA_RTX4080S_DEV << 16:
        case MSI_RTX4080S_GAMING_X_SLIM_WHITE_SUB_DEV | NVIDIA_RTX4080_DEV << 16:
        case MSI_RTX4080S_GAMING_X_SLIM_WHITE_SUB_DEV | NVIDIA_RTX4080S_DEV << 16:
            break;

        default:
            mode Stack;
            Stack.name                      = "Stack";
            Stack.value                     = MSI_GPU_V2_MODE_STACK;
            Stack.flags                     = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
            Stack.speed_min                 = MSI_GPU_V2_SPEED_MIN;
            Stack.speed                     = MSI_GPU_V2_SPEED_MID;
            Stack.speed_max                 = MSI_GPU_V2_SPEED_MAX;
            Stack.brightness_min            = MSI_GPU_V2_BRIGHTNESS_MIN;
            Stack.brightness                = MSI_GPU_V2_BRIGHTNESS_MAX;
            Stack.brightness_max            = MSI_GPU_V2_BRIGHTNESS_MAX;
            Stack.color_mode                = MODE_COLORS_PER_LED;
            modes.push_back(Stack);

            mode Rhythm;
            Rhythm.name                     = "Rhythm";
            Rhythm.value                    = MSI_GPU_V2_MODE_RHYTHM;
            Rhythm.flags                    = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
            Rhythm.speed_min                = MSI_GPU_V2_SPEED_MIN;
            Rhythm.speed                    = MSI_GPU_V2_SPEED_MID;
            Rhythm.speed_max                = MSI_GPU_V2_SPEED_MAX;
            Rhythm.brightness_min           = MSI_GPU_V2_BRIGHTNESS_MIN;
            Rhythm.brightness               = MSI_GPU_V2_BRIGHTNESS_MAX;
            Rhythm.brightness_max           = MSI_GPU_V2_BRIGHTNESS_MAX;
            Rhythm.color_mode               = MODE_COLORS_PER_LED;
            modes.push_back(Rhythm);
    }

    mode Flowing;
    Flowing.name                    = "Flowing";
    Flowing.value                   = MSI_GPU_V2_MODE_FLOWING;
    Flowing.flags                   = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    Flowing.speed_min               = MSI_GPU_V2_SPEED_MIN;
    Flowing.speed                   = MSI_GPU_V2_SPEED_MID;
    Flowing.speed_max               = MSI_GPU_V2_SPEED_MAX;
    Flowing.brightness_min          = MSI_GPU_V2_BRIGHTNESS_MIN;
    Flowing.brightness              = MSI_GPU_V2_BRIGHTNESS_MAX;
    Flowing.brightness_max          = MSI_GPU_V2_BRIGHTNESS_MAX;
    Flowing.color_mode              = MODE_COLORS_PER_LED;
    modes.push_back(Flowing);

    mode Whirling;
    Whirling.name                   = "Whirling";
    Whirling.value                  = MSI_GPU_V2_MODE_WHIRLING;
    Whirling.flags                  = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    Whirling.speed_min              = MSI_GPU_V2_SPEED_MIN;
    Whirling.speed                  = MSI_GPU_V2_SPEED_MID;
    Whirling.speed_max              = MSI_GPU_V2_SPEED_MAX;
    Whirling.brightness_min         = MSI_GPU_V2_BRIGHTNESS_MIN;
    Whirling.brightness             = MSI_GPU_V2_BRIGHTNESS_MAX;
    Whirling.brightness_max         = MSI_GPU_V2_BRIGHTNESS_MAX;
    Whirling.color_mode             = MODE_COLORS_PER_LED;
    modes.push_back(Whirling);

    mode Fadein;
    Fadein.name                     = "Fade In";
    Fadein.value                    = MSI_GPU_V2_MODE_FADEIN;
    Fadein.flags                    = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_MANUAL_SAVE;
    Fadein.speed_min                = MSI_GPU_V2_SPEED_MIN;
    Fadein.speed                    = MSI_GPU_V2_SPEED_MID;
    Fadein.speed_max                = MSI_GPU_V2_SPEED_MAX;
    Fadein.brightness_min           = MSI_GPU_V2_BRIGHTNESS_MIN;
    Fadein.brightness               = MSI_GPU_V2_BRIGHTNESS_MAX;
    Fadein.brightness_max           = MSI_GPU_V2_BRIGHTNESS_MAX;
    Fadein.colors_min               = 1;
    Fadein.colors_max               = 2;
    Fadein.colors.resize(2);
    Fadein.color_mode               = MODE_COLORS_MODE_SPECIFIC;
    modes.push_back(Fadein);

    mode Breathing;
    Breathing.name                  = "Breathing";
    Breathing.value                 = MSI_GPU_V2_MODE_BREATHING;
    Breathing.flags                 = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_MANUAL_SAVE;
    Breathing.speed_min             = MSI_GPU_V2_SPEED_MIN;
    Breathing.speed                 = MSI_GPU_V2_SPEED_MID;
    Breathing.speed_max             = MSI_GPU_V2_SPEED_MAX;
    Breathing.brightness_min        = MSI_GPU_V2_BRIGHTNESS_MIN;
    Breathing.brightness            = MSI_GPU_V2_BRIGHTNESS_MAX;
    Breathing.brightness_max        = MSI_GPU_V2_BRIGHTNESS_MAX;
    Breathing.colors_min            = 1;
    Breathing.colors_max            = 2;
    Breathing.colors.resize(2);
    Breathing.color_mode            = MODE_COLORS_MODE_SPECIFIC;
    modes.push_back(Breathing);

    mode flashing;
    flashing.name                   = "Flashing";
    flashing.value                  = MSI_GPU_V2_MODE_FLASHING;
    flashing.flags                  = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    flashing.speed_min              = MSI_GPU_V2_SPEED_MIN;
    flashing.speed                  = MSI_GPU_V2_SPEED_MID;
    flashing.speed_max              = MSI_GPU_V2_SPEED_MAX;
    flashing.brightness_min         = MSI_GPU_V2_BRIGHTNESS_MIN;
    flashing.brightness             = MSI_GPU_V2_BRIGHTNESS_MAX;
    flashing.brightness_max         = MSI_GPU_V2_BRIGHTNESS_MAX;
    flashing.color_mode             = MODE_COLORS_PER_LED;
    modes.push_back(flashing);

    mode doubleflashing;
    doubleflashing.name             = "Double Flashing";
    doubleflashing.value            = MSI_GPU_V2_MODE_DOUBLEFLASHING;
    doubleflashing.flags            = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE;
    doubleflashing.speed_min        = MSI_GPU_V2_SPEED_MIN;
    doubleflashing.speed            = MSI_GPU_V2_SPEED_MID;
    doubleflashing.speed_max        = MSI_GPU_V2_SPEED_MAX;
    doubleflashing.brightness_min   = MSI_GPU_V2_BRIGHTNESS_MIN;
    doubleflashing.brightness       = MSI_GPU_V2_BRIGHTNESS_MAX;
    doubleflashing.brightness_max   = MSI_GPU_V2_BRIGHTNESS_MAX;
    doubleflashing.color_mode       = MODE_COLORS_PER_LED;
    modes.push_back(doubleflashing);

    /*---------------------------------------------------------*\
    | The RTX 5090 and 5070 Ti Gaming Trio take Direct colors   |
    | through a per-LED block, not the R1/G1/B1 registers. The  |
    | 5070 Ti subsystem id is shared with the 5080 Trio, so the |
    | full PCI id is compared. The 5070 Ti block keeps the full |
    | block but its last slot drives no LED, so it is sent      |
    | black and not exposed                                     |
    \*---------------------------------------------------------*/
    if(msi_gpu_id == ((NVIDIA_RTX5090_DEV << 16) | MSI_RTX5090_GAMING_TRIO_SUB_DEV))
    {
        per_led_protocol    = true;
        per_led_count       = MSI_GPU_V2_PER_LED_SLOTS;
    }
    else if(msi_gpu_id == ((NVIDIA_RTX5070TI_DEV << 16) | MSI_RTX5070TI_GAMING_TRIO_SUB_DEV))
    {
        per_led_protocol    = true;
        per_led_count       = MSI_GPU_V2_5070TI_PER_LED_COUNT;
    }

    if(per_led_protocol)
    {
        LOG_WARNING("[MSI GPU v2] %s (%s) PCI id 0x%08X, Direct via per-LED block 0x04", name.c_str(), location.c_str(), (unsigned int)msi_gpu_id);
    }

    /*---------------------------------------------------------*\
    | RTX 50 series cards have more effects, and a rainbow      |
    | variant of the single color ones at the next mode value   |
    \*---------------------------------------------------------*/
    switch(msi_gpu_id >> 16)
    {
        case NVIDIA_RTX5060_DEV:
        case NVIDIA_RTX5060TI_DEV:
        case NVIDIA_RTX5070_DEV:
        case NVIDIA_RTX5070TI_DEV:
        case NVIDIA_RTX5080_DEV:
        case NVIDIA_RTX5090_DEV:
        case NVIDIA_RTX5090D_DEV:
        case NVIDIA_RTX5090D_V2_DEV:
            rainbow_variants = true;
            rtx50            = true;
            break;

        default:
            rainbow_variants = false;
            break;
    }

    if(rainbow_variants)
    {
        for(std::size_t mode_idx = 0; mode_idx < modes.size(); mode_idx++)
        {
            switch(modes[mode_idx].value)
            {
                case MSI_GPU_V2_MODE_RHYTHM:
                    /*-----------------------------------------*\
                    | The value used so far is the rainbow one  |
                    \*-----------------------------------------*/
                    modes[mode_idx].value       = MSI_GPU_V2_MODE_RHYTHM_COLOR;
                    modes[mode_idx].flags      |= MODE_FLAG_HAS_RANDOM_COLOR;
                    modes[mode_idx].color_mode  = MODE_COLORS_RANDOM;
                    break;

                case MSI_GPU_V2_MODE_FLOWING:
                case MSI_GPU_V2_MODE_WHIRLING:
                case MSI_GPU_V2_MODE_STACK:
                case MSI_GPU_V2_MODE_METEOR:
                    modes[mode_idx].flags      |= MODE_FLAG_HAS_RANDOM_COLOR;
                    break;
            }
        }

        mode Twisting;
        Twisting.name                   = "Twisting";
        Twisting.value                  = MSI_GPU_V2_MODE_TWISTING;
        Twisting.flags                  = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
        Twisting.speed_min              = MSI_GPU_V2_SPEED_MIN;
        Twisting.speed                  = MSI_GPU_V2_SPEED_MID;
        Twisting.speed_max              = MSI_GPU_V2_SPEED_MAX;
        Twisting.brightness_min         = MSI_GPU_V2_BRIGHTNESS_MIN;
        Twisting.brightness             = MSI_GPU_V2_BRIGHTNESS_MAX;
        Twisting.brightness_max         = MSI_GPU_V2_BRIGHTNESS_MAX;
        Twisting.color_mode             = MODE_COLORS_PER_LED;
        modes.push_back(Twisting);

        mode Laminating;
        Laminating.name                 = "Laminating";
        Laminating.value                = MSI_GPU_V2_MODE_LAMINATING;
        Laminating.flags                = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
        Laminating.speed_min            = MSI_GPU_V2_SPEED_MIN;
        Laminating.speed                = MSI_GPU_V2_SPEED_MID;
        Laminating.speed_max            = MSI_GPU_V2_SPEED_MAX;
        Laminating.brightness_min       = MSI_GPU_V2_BRIGHTNESS_MIN;
        Laminating.brightness           = MSI_GPU_V2_BRIGHTNESS_MAX;
        Laminating.brightness_max       = MSI_GPU_V2_BRIGHTNESS_MAX;
        Laminating.color_mode           = MODE_COLORS_PER_LED;
        modes.push_back(Laminating);

        mode Marquee;
        Marquee.name                    = "Marquee";
        Marquee.value                   = MSI_GPU_V2_MODE_MARQUEE;
        Marquee.flags                   = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_MANUAL_SAVE;
        Marquee.speed_min               = MSI_GPU_V2_SPEED_MIN;
        Marquee.speed                   = MSI_GPU_V2_SPEED_MID;
        Marquee.speed_max               = MSI_GPU_V2_SPEED_MAX;
        Marquee.brightness_min          = MSI_GPU_V2_BRIGHTNESS_MIN;
        Marquee.brightness              = MSI_GPU_V2_BRIGHTNESS_MAX;
        Marquee.brightness_max          = MSI_GPU_V2_BRIGHTNESS_MAX;
        Marquee.color_mode              = MODE_COLORS_PER_LED;
        modes.push_back(Marquee);

        mode Arrow;
        Arrow.name                      = "Arrow";
        Arrow.value                     = MSI_GPU_V2_MODE_ARROW;
        Arrow.flags                     = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_MANUAL_SAVE;
        Arrow.speed_min                 = MSI_GPU_V2_SPEED_MIN;
        Arrow.speed                     = MSI_GPU_V2_SPEED_MID;
        Arrow.speed_max                 = MSI_GPU_V2_SPEED_MAX;
        Arrow.brightness_min            = MSI_GPU_V2_BRIGHTNESS_MIN;
        Arrow.brightness                = MSI_GPU_V2_BRIGHTNESS_MAX;
        Arrow.brightness_max            = MSI_GPU_V2_BRIGHTNESS_MAX;
        Arrow.colors_min                = 1;
        Arrow.colors_max                = 2;
        Arrow.colors.resize(2);
        Arrow.color_mode                = MODE_COLORS_MODE_SPECIFIC;
        modes.push_back(Arrow);
    }

    SetupZones();

    modes[active_mode].speed        = MSI_GPU_V2_SPEED_MID;
    modes[active_mode].brightness   = MSI_GPU_V2_BRIGHTNESS_MAX;

    if(per_led_protocol)
    {
        direct_thread               = std::thread(&RGBController_MSIGPUv2::DirectThreadFunction, this);
    }
}

RGBController_MSIGPUv2::~RGBController_MSIGPUv2()
{
    Shutdown();

    {
        std::lock_guard<std::mutex> lock(direct_mutex);
        direct_running = false;
    }
    direct_cv.notify_all();
    if(direct_thread.joinable())
    {
        direct_thread.join();
    }

    delete controller;
}

void RGBController_MSIGPUv2::SetupZones()
{
    /*---------------------------------------------------------*\
    | Set up zone                                               |
    \*---------------------------------------------------------*/
    zone msi_gpu_zone;
    msi_gpu_zone.name          = "GPU";

    if(per_led_protocol)
    {
        /*-----------------------------------------------------*\
        | The per-LED block addresses each LED of the card      |
        \*-----------------------------------------------------*/
        msi_gpu_zone.type       = ZONE_TYPE_LINEAR;
        msi_gpu_zone.leds_min   = per_led_count;
        msi_gpu_zone.leds_max   = per_led_count;
        msi_gpu_zone.leds_count = per_led_count;
        zones.push_back(msi_gpu_zone);

        for(unsigned int led_idx = 0; led_idx < per_led_count; led_idx++)
        {
            led new_led;
            new_led.name = "LED " + std::to_string(led_idx + 1);
            leds.push_back(new_led);
        }
    }
    else
    {
        msi_gpu_zone.type       = ZONE_TYPE_SINGLE;
        msi_gpu_zone.leds_min   = 1;
        msi_gpu_zone.leds_max   = 1;
        msi_gpu_zone.leds_count = 1;
        zones.push_back(msi_gpu_zone);

        /*-----------------------------------------------------*\
        | Set up LED                                            |
        \*-----------------------------------------------------*/
        led led1;
        led1.name = "Color";
        leds.push_back(led1);
    }

    SetupColors();

    /*-------------------------------------------------------------*\
    | Initialize colors                                             |
    | This controller doesn't support reading colors from device    |
    \*-------------------------------------------------------------*/
    for(std::size_t led_idx = 0; led_idx < colors.size(); led_idx++)
    {
        colors[led_idx] = ToRGBColor(0xFF, 0, 0);
    }

    for(unsigned int mode_idx = 0; mode_idx < modes.size(); mode_idx++)
    {
        if(modes[mode_idx].color_mode == MODE_COLORS_MODE_SPECIFIC)
        {
            if(modes[mode_idx].colors.size()>2)
            {
               modes[mode_idx].colors[2] = ToRGBColor(0, 0, 0xFF);
            }
            if(modes[mode_idx].colors.size()>1)
            {
               modes[mode_idx].colors[1] = ToRGBColor(0, 0xFF, 0);
            }
            modes[mode_idx].colors[0] = ToRGBColor(0xFF, 0, 0);
        }
    }

}

void RGBController_MSIGPUv2::DeviceUpdateAll(const mode& current_mode, bool save)
{
    bool            wrote_rgb1          = false;
    int             written_brightness  = -1;
    unsigned char   written_mode;

    /*---------------------------------------------------------*\
    | On the per-LED path, keep the Direct worker off the bus   |
    | and drop any frame it took before this mode               |
    \*---------------------------------------------------------*/
    std::unique_lock<std::mutex> write_lock(write_mutex, std::defer_lock);

    if(per_led_protocol)
    {
        write_lock.lock();

        std::lock_guard<std::mutex> lock(direct_mutex);
        direct_generation++;
        direct_pending  = false;
        direct_primed   = false;
    }

    /*---------------------------------------------------------*\
    | An RTX 50 card left in per-LED stream mode ignores the    |
    | register path: leave the stream and set the control       |
    | register first. Once per mode change, and on the next     |
    | few updates if a write failed, so a card that keeps       |
    | refusing the write does not slow every Direct frame       |
    \*---------------------------------------------------------*/
    if(rtx50 && stream_exit_pending.exchange(false))
    {
        bool exit_ok    = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_PER_LED_ENABLE, 0x01);
        bool control_ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_NON_RGBMODE);

        if(exit_ok && control_ok)
        {
            stream_exit_failures = 0;
        }
        else if(++stream_exit_failures < 3)
        {
            stream_exit_pending = true;
        }
    }

    switch(current_mode.value)
    {
        case MSI_GPU_V2_MODE_RAINBOW:
            if(current_mode.flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
            {
                if(current_mode.direction == MODE_DIRECTION_LEFT)
                {
                    controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_DIRECTION_LEFT | MSI_GPU_V2_CONTROL_NON_RGBMODE);
                }
                else
                {
                    controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_DIRECTION_RIGHT | MSI_GPU_V2_CONTROL_NON_RGBMODE);
                }
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);

                controller->SetRGB1V2(RGBGetRValue(current_mode.colors[0]), RGBGetGValue(current_mode.colors[0]), RGBGetBValue(current_mode.colors[0]));
                controller->SetRGB2V2(RGBGetRValue(current_mode.colors[1]), RGBGetGValue(current_mode.colors[1]), RGBGetBValue(current_mode.colors[1]));
                controller->SetRGB3V2(RGBGetRValue(current_mode.colors[2]), RGBGetGValue(current_mode.colors[2]), RGBGetBValue(current_mode.colors[2]));
            }
            else
            {
                if(current_mode.direction == MODE_DIRECTION_LEFT)
                {
                    controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_DIRECTION_LEFT);
                }
                else
                {
                    controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_DIRECTION_RIGHT);
                }
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
            }
            break;

        case MSI_GPU_V2_MODE_MAGIC:
            if(current_mode.flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
            {
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_NON_RGBMODE);

                controller->SetRGB1V2(RGBGetRValue(current_mode.colors[0]), RGBGetGValue(current_mode.colors[0]), RGBGetBValue(current_mode.colors[0]));
                controller->SetRGB2V2(RGBGetRValue(current_mode.colors[1]), RGBGetGValue(current_mode.colors[1]), RGBGetBValue(current_mode.colors[1]));
                controller->SetRGB3V2(RGBGetRValue(current_mode.colors[2]), RGBGetGValue(current_mode.colors[2]), RGBGetBValue(current_mode.colors[2]));
            }
            else
            {
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);

                controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_CONTROL, MSI_GPU_V2_CONTROL_DIRECTION_RIGHT);
            }
            break;


        case MSI_GPU_V2_MODE_BREATHING:
        case MSI_GPU_V2_MODE_FADEIN:
        case MSI_GPU_V2_MODE_ARROW:
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);

            controller->SetRGB1V2(RGBGetRValue(current_mode.colors[0]), RGBGetGValue(current_mode.colors[0]), RGBGetBValue(current_mode.colors[0]));
            controller->SetRGB2V2(RGBGetRValue(current_mode.colors[1]), RGBGetGValue(current_mode.colors[1]), RGBGetBValue(current_mode.colors[1]));
            break;

        case MSI_GPU_V2_MODE_FLOWING:
        case MSI_GPU_V2_MODE_WHIRLING:
        case MSI_GPU_V2_MODE_PATROLLING:
        case MSI_GPU_V2_MODE_FLASHING:
        case MSI_GPU_V2_MODE_DOUBLEFLASHING:
        case MSI_GPU_V2_MODE_STATIC:
        case MSI_GPU_V2_MODE_RHYTHM:
        case MSI_GPU_V2_MODE_STACK:
        case MSI_GPU_V2_MODE_METEOR:
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
            controller->SetRGB1(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]));
            wrote_rgb1 = true;
            break;

        case MSI_GPU_V2_MODE_TWISTING:
        case MSI_GPU_V2_MODE_LAMINATING:
        case MSI_GPU_V2_MODE_MARQUEE:
        case MSI_GPU_V2_MODE_RHYTHM_COLOR:
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
            controller->SetRGB1V2(RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]));
            break;

        case MSI_GPU_V2_MODE_STREAMING:
        case MSI_GPU_V2_MODE_LIGHTNING:
        case MSI_GPU_V2_MODE_OFF:
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
            break;

        default:
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_UNKNOWN, 0x00);
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);
            controller->SetMode(MSI_GPU_V2_MODE_OFF);
    }


    if(current_mode.flags & MODE_FLAG_HAS_BRIGHTNESS)
    {
        written_brightness = (unsigned char)(MSI_GPU_V2_BRIGHTNESS_MULTI * modes[active_mode].brightness);
        controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_BRIGHTNESS, MSI_GPU_V2_BRIGHTNESS_MULTI * modes[active_mode].brightness);
        if(modes[active_mode].flags & MODE_FLAG_HAS_SPEED)
        {
            controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_SPEED, speed_values[current_mode.speed]);
        }

    }

    /*---------------------------------------------------------*\
    | The rainbow variant of a single color effect is the next  |
    | mode value                                                |
    \*---------------------------------------------------------*/
    if(rainbow_variants
    && (current_mode.flags & MODE_FLAG_HAS_PER_LED_COLOR)
    && (current_mode.flags & MODE_FLAG_HAS_RANDOM_COLOR)
    && (current_mode.color_mode == MODE_COLORS_RANDOM))
    {
        written_mode = current_mode.value + 1;
    }
    else
    {
        written_mode = current_mode.value;
    }
    controller->SetMode(written_mode);

    if(save)
    {
        controller->Save();
    }

    /*---------------------------------------------------------*\
    | Log the written values against a readback once per        |
    | controller; every mode or brightness change comes here,   |
    | and on the register path the first Direct frame too       |
    \*---------------------------------------------------------*/
    if(!first_apply_logged.exchange(true))
    {
        char brightness_str[8] = "-";
        char rgb_str[16]       = "-";

        if(written_brightness >= 0)
        {
            snprintf(brightness_str, sizeof(brightness_str), "0x%02X", written_brightness);
        }
        if(wrote_rgb1)
        {
            snprintf(rgb_str, sizeof(rgb_str), "0x%02X/0x%02X/0x%02X", RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]));
        }

        LOG_WARNING("[MSI GPU v2] %s (%s) first apply (%s): wrote mode 0x%02X brightness %s rgb %s, read back %s", name.c_str(), location.c_str(), current_mode.name.c_str(), written_mode, brightness_str, rgb_str, controller->ReadStateString().c_str());
    }

    /*---------------------------------------------------------*\
    | The card is now in static mode with this color, so the    |
    | next Direct color can skip the unknown register and the   |
    | brightness                                                |
    \*---------------------------------------------------------*/
    register_direct_ready = !per_led_protocol && (current_mode.value == MSI_GPU_V2_MODE_STATIC);
    register_direct_color = colors.empty() ? 0 : colors[0];
}

/*---------------------------------------------------------*\
| Register path Direct color change. Color registers        |
| written while the card stays in static mode do not show   |
| (seen on an RTX 4090 Gaming X Slim), so the card goes to  |
| idle mode for the write and back to static mode after it. |
| Only the channels that changed are written; an unchanged  |
| color writes nothing. A failed write sends the next frame |
| through the full sequence again                           |
\*---------------------------------------------------------*/
void RGBController_MSIGPUv2::UpdateRegisterDirectColor(RGBColor color)
{
    if(color == register_direct_color)
    {
        return;
    }

    bool ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_IDLE);

    if(RGBGetRValue(color) != RGBGetRValue(register_direct_color))
    {
        ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_R1, RGBGetRValue(color)) && ok;
    }

    if(RGBGetGValue(color) != RGBGetGValue(register_direct_color))
    {
        ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_G1, RGBGetGValue(color)) && ok;
    }

    if(RGBGetBValue(color) != RGBGetBValue(register_direct_color))
    {
        ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_B1, RGBGetBValue(color)) && ok;
    }

    ok = controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_MODE, MSI_GPU_V2_MODE_STATIC) && ok;

    register_direct_color = color;
    register_direct_ready = ok;
}


/*---------------------------------------------------------*\
| Per-LED Direct hands the frame to the worker and returns, |
| so the latest frame is kept even while the bus is busy    |
\*---------------------------------------------------------*/
void RGBController_MSIGPUv2::QueueDirect(bool reprime)
{
    {
        std::lock_guard<std::mutex> lock(direct_mutex);
        for(unsigned int led_idx = 0; led_idx < MSI_GPU_V2_PER_LED_SLOTS; led_idx++)
        {
            pending_colors[led_idx] = (led_idx < per_led_count && led_idx < colors.size()) ? colors[led_idx] : 0;
        }
        pending_brightness  = (unsigned char)(MSI_GPU_V2_BRIGHTNESS_MULTI * modes[active_mode].brightness);
        direct_pending      = true;
        if(reprime)
        {
            direct_primed   = false;
            direct_reprime++;
        }
    }
    direct_cv.notify_all();
}

/*---------------------------------------------------------*\
| Writes the latest per-LED Direct frame: setup registers,  |
| then one per-LED block per changed frame. The setup runs  |
| on the first frame, a mode or brightness change and after |
| a failed write. An unchanged frame writes nothing, but the|
| setup and last colors are rewritten once older than the   |
| refresh interval, so a card reset from outside recovers   |
\*---------------------------------------------------------*/
void RGBController_MSIGPUv2::DirectThreadFunction()
{
    std::unique_lock<std::mutex> lock(direct_mutex);

    while(true)
    {
        bool refresh = false;

        if(direct_primed && !direct_pending)
        {
            std::chrono::steady_clock::time_point refresh_due = direct_write_time + std::chrono::milliseconds(MSI_GPU_V2_DIRECT_REFRESH_MS);

            if(!direct_cv.wait_until(lock, refresh_due, [this]{ return(!direct_running || direct_pending || !direct_primed); }))
            {
                refresh = true;
            }
        }
        else
        {
            direct_cv.wait(lock, [this]{ return(!direct_running || direct_pending); });
        }

        if(!direct_running)
        {
            break;
        }

        if(!refresh && !direct_pending)
        {
            continue;
        }

        bool full = refresh || !direct_primed || pending_brightness != applied_brightness;

        if(!refresh)
        {
            if(!full && std::equal(pending_colors, pending_colors + MSI_GPU_V2_PER_LED_SLOTS, applied_colors))
            {
                direct_pending = false;
                continue;
            }

            std::chrono::steady_clock::time_point due = direct_write_time + std::chrono::milliseconds(MSI_GPU_V2_PER_LED_MIN_INTERVAL_MS);

            if(std::chrono::steady_clock::now() < due)
            {
                direct_cv.wait_until(lock, due, [this]{ return(!direct_running); });
                continue;
            }
        }

        RGBColor            led_colors[MSI_GPU_V2_PER_LED_SLOTS];
        std::copy(refresh ? applied_colors : pending_colors, (refresh ? applied_colors : pending_colors) + MSI_GPU_V2_PER_LED_SLOTS, led_colors);

        const unsigned char brightness  = refresh ? applied_brightness : pending_brightness;
        const unsigned int  generation  = direct_generation;
        const unsigned int  reprime     = direct_reprime;

        if(!refresh)
        {
            direct_pending  = false;
        }
        lock.unlock();

        bool ok = false;
        {
            std::lock_guard<std::mutex> write_lock(write_mutex);

            lock.lock();
            bool current = (generation == direct_generation);
            lock.unlock();

            if(!current)
            {
                lock.lock();
                continue;
            }

            lock.lock();
            direct_write_time = std::chrono::steady_clock::now();
            lock.unlock();

            /*-----------------------------------------------------*\
            | Block: 0x01, then index, R, G, B, 0x01 per LED.       |
            | LED i's record goes in slot per_led_slots[i], which   |
            | swaps the last two; the block has no brightness, so   |
            | scale the color                                       |
            \*-----------------------------------------------------*/
            static const unsigned int per_led_slots[MSI_GPU_V2_PER_LED_SLOTS] = { 0, 1, 2, 3, 5, 4 };
            unsigned char block[1 + MSI_GPU_V2_PER_LED_SLOTS * 5];

            block[0] = 0x01;
            for(unsigned int led_idx = 0; led_idx < MSI_GPU_V2_PER_LED_SLOTS; led_idx++)
            {
                unsigned int offset     = 1 + per_led_slots[led_idx] * 5;

                block[offset]           = (unsigned char)led_idx;
                block[offset + 1]       = (unsigned char)(RGBGetRValue(led_colors[led_idx]) * brightness / 100);
                block[offset + 2]       = (unsigned char)(RGBGetGValue(led_colors[led_idx]) * brightness / 100);
                block[offset + 3]       = (unsigned char)(RGBGetBValue(led_colors[led_idx]) * brightness / 100);
                block[offset + 4]       = 0x01;
            }

            ok = true;
            if(full)
            {
                stream_exit_pending = true;
                ok = ok && controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_CONTROL, 0x01);
                ok = ok && controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_PER_LED_ENABLE, 0x00);
                ok = ok && controller->MSIGPURegisterWriteLogged(MSI_GPU_V2_REG_PER_LED_START, 0x00);
            }
            ok = ok && controller->MSIGPUI2CBlockWrite(MSI_GPU_V2_REG_PER_LED_COLORS, block, sizeof(block));

            if(full && !refresh && !first_apply_logged.exchange(true))
            {
                LOG_WARNING("[MSI GPU v2] %s (%s) first apply (Direct, per-LED block): brightness 0x%02X LED 1 rgb 0x%02X/0x%02X/0x%02X%s", name.c_str(), location.c_str(), brightness, RGBGetRValue(led_colors[0]), RGBGetGValue(led_colors[0]), RGBGetBValue(led_colors[0]), ok ? "" : ", write failed");
            }
        }

        lock.lock();
        if(generation == direct_generation)
        {
            direct_primed = ok && reprime == direct_reprime;
            if(ok)
            {
                std::copy(led_colors, led_colors + MSI_GPU_V2_PER_LED_SLOTS, applied_colors);
                applied_brightness  = brightness;
            }
        }
    }
}

void RGBController_MSIGPUv2::DeviceUpdateLEDs()
{
    /*---------------------------------------------------------*\
    | The full register sequence is seven writes with a 20 ms   |
    | wait after each, too slow for a client streaming colors.  |
    | On the register path, after the first full update a       |
    | Direct color change writes only idle mode, the color      |
    | registers that changed and static mode again              |
    \*---------------------------------------------------------*/
    if(per_led_protocol && modes[active_mode].value == MSI_GPU_V2_MODE_STATIC)
    {
        QueueDirect(false);
    }
    else if(register_direct_ready && modes[active_mode].value == MSI_GPU_V2_MODE_STATIC && !colors.empty())
    {
        UpdateRegisterDirectColor(colors[0]);
    }
    else
    {
        DeviceUpdateAll(modes[active_mode]);
    }
}

void RGBController_MSIGPUv2::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}


void RGBController_MSIGPUv2::DeviceUpdateSingleLED(int /*led*/)
{
    /*---------------------------------------------------------*\
    | Only the per-LED Direct path can address one LED, and it  |
    | writes all of them in one block                           |
    \*---------------------------------------------------------*/
    if(per_led_protocol && modes[active_mode].value == MSI_GPU_V2_MODE_STATIC)
    {
        QueueDirect(false);
    }
}

void RGBController_MSIGPUv2::DeviceUpdateMode()
{
    if(per_led_protocol && modes[active_mode].value == MSI_GPU_V2_MODE_STATIC)
    {
        QueueDirect(true);
    }
    else
    {
        stream_exit_pending = true;
        DeviceUpdateAll(modes[active_mode]);
    }
}

void RGBController_MSIGPUv2::DeviceSaveMode()
{
    /*---------------------------------------------------------*\
    | Save stores the register path state, which per-LED Direct |
    | never writes: leave the stream, write LED 1 as Static,    |
    | save, then re-enter the stream                            |
    \*---------------------------------------------------------*/
    if(per_led_protocol && modes[active_mode].value == MSI_GPU_V2_MODE_STATIC)
    {
        stream_exit_pending = true;
        DeviceUpdateAll(modes[active_mode], true);
        QueueDirect(true);
        return;
    }

    std::lock_guard<std::mutex> write_lock(write_mutex);

    if(per_led_protocol)
    {
        controller->MSIGPURegisterWrite(MSI_GPU_V2_REG_PER_LED_ENABLE, 0x01);
    }
    controller->Save();
}
