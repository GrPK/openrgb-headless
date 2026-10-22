/*---------------------------------------------------------*\
| CorsairCommanderCoreController.h                          |
|                                                           |
|   Driver for Corsair Commander Core                       |
|                                                           |
|   Jeff P.                                                 |
|   Nikola Jurkovic (jurkovic.nikola)           14 Aug 2025 |
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
#include <vector>
#include <hidapi.h>
#include "RGBController.h"
#include "DeviceGuardManager.h"
#include "CorsairCommanderCoreCooling.h"

/*-----------------------------------------------------*\
| Packet size per device                                |
\*-----------------------------------------------------*/
#define CORSAIR_COMMANDER_CORE_PACKET_SIZE_V1       1025
#define CORSAIR_COMMANDER_CORE_PACKET_SIZE_V2       97
#define CORSAIR_COMMANDER_CORE_PACKET_SIZE_V3       65
#define CORSAIR_COMMANDER_CORE_XT_PACKET_SIZE       385

#define CORSAIR_COMMANDER_CORE_RGB_DATA_LENGTH      699
#define CORSAIR_COMMANDER_CORE_XT_RGB_DATA_LENGTH   1224

#define CORSAIR_QL_FAN_ZONE_OFFSET                  102
#define CORSAIR_COMMANDER_CORE_NUM_CHANNELS         6
#define CORSAIR_COMMANDER_CORE_MAX_LEDS_PER_CHANNEL 34

#define CORSAIR_COMMANDER_CORE_PID                  0x0C1C
#define CORSAIR_COMMANDER_CORE2_PID                 0x0C32
#define CORSAIR_COMMANDER_CORE3_PID                 0x0C1D
#define CORSAIR_COMMANDER_CORE4_PID                 0x0C3C
#define CORSAIR_COMMANDER_CORE5_PID                 0x0C3D
#define CORSAIR_COMMANDER_CORE6_PID                 0x0C3E
#define CORSAIR_COMMANDER_CORE_XT_PID               0x0C2A

enum
{
    CORSAIR_COMMANDER_CORE_MODE_DIRECT          = 0x00,
};

class CorsairCommanderCoreController
{
public:
    CorsairCommanderCoreController(hid_device* dev_handle, const char* path, int pid, std::string dev_name);
    ~CorsairCommanderCoreController();

    std::string GetFirmwareString();
    std::vector<unsigned short int> GetLedCounts();
    std::string GetLocationString();
    std::string GetNameString();
    int GetPidInt();

    void        SetDirectColor
                (
                    std::vector<RGBColor>,
                    std::vector<zone>
                );

    void        KeepaliveThread();
    void        SetFanMode(bool external_rgb_port);
    void        SetLedAmount(int led_amount);

    /*-----------------------------------------------------*\
    | Cooling, served to Nexus over the SDK. Both answer    |
    | from cached state; a worker thread, started by the    |
    | first call, does the device transactions              |
    \*-----------------------------------------------------*/
    bool        NexusCoolingGetStatus(std::vector<unsigned char>& payload);
    bool        NexusCoolingSetDuties(const unsigned char* payload, unsigned int size);

private:
    hid_device*                                         dev;
    std::thread*                                        keepalive_thread;
    std::atomic<bool>                                   keepalive_thread_run;
    std::atomic<bool>                                   controller_ready;
    std::string                                         location;
    std::vector<RGBColor>                               lastcolors;
    std::vector<zone>                                   lastzones;
    std::string                                         name;
    unsigned short int                                  version[3] = {0, 0, 0};
    int                                                 packet_size;
    int                                                 command_res_size;
    int                                                 pid;
    std::chrono::time_point<std::chrono::steady_clock>  last_commit_time;
    DeviceGuardManager*                                 guard_manager_ptr;
    std::recursive_mutex                                controller_mutex;

    /*-----------------------------------------------------*\
    | Cooling state. cooling_mutex is never held across     |
    | device I/O and never taken while holding              |
    | controller_mutex                                      |
    \*-----------------------------------------------------*/
    std::atomic<unsigned int>                           init_count{0};
    std::thread*                                        cooling_thread          = nullptr;
    std::atomic<bool>                                   cooling_thread_run{false};
    std::mutex                                          cooling_thread_mutex;
    std::mutex                                          cooling_mutex;
    std::condition_variable                             cooling_cv;
    CorsairCommanderCoreCooling::Status                 cooling_status;
    std::chrono::steady_clock::time_point               cooling_status_time;
    std::chrono::steady_clock::time_point               cooling_last_request;
    std::vector<CorsairCommanderCoreCooling::DutyEntry> cooling_target;
    std::vector<unsigned char>                          cooling_written_table;
    std::vector<unsigned char>                          cooling_applied_duty;
    unsigned int                                        cooling_written_init    = 0;
    unsigned int                                        cooling_failures        = 0;

    void        SendCommand(unsigned char command[2], unsigned char data[], unsigned short int data_len, unsigned char res[], bool dev_read = true);
    void        WriteData(unsigned char endpoint[2], unsigned char data_type[2], unsigned char data[], unsigned short int data_len);
    void        ReadData(unsigned char endpoint[2], unsigned char data[]);

    void        SendCommit();
    void        InitController();

    void        StartCoolingThread();
    void        StopCoolingThread();
    void        CoolingThreadFunction();
    void        CoolingPoll(bool read_connected);
    void        CoolingApplyDuties();
    bool        CoolingExchange(unsigned char command, const std::vector<unsigned char>& data, unsigned char* reply, bool reply_required);
    void        DiscardStaleReplies();
    bool        CoolingReadEndpoint(unsigned char endpoint, unsigned char data_type, std::vector<unsigned char>& res);
    bool        CoolingWriteEndpoint(unsigned char endpoint, unsigned char data_type, const std::vector<unsigned char>& data);
};
