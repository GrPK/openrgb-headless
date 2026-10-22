/*---------------------------------------------------------*\
| CorsairCommanderCoreController.cpp                        |
|                                                           |
|   Driver for Corsair Commander Core                       |
|                                                           |
|   Jeff P.                                                 |
|   Nikola Jurkovic (jurkovic.nikola)           14 Aug 2025 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include "CorsairCommanderCoreController.h"
#include "CorsairDeviceGuard.h"

using namespace std::chrono_literals;
using namespace CorsairCommanderCoreCooling;

CorsairCommanderCoreController::CorsairCommanderCoreController(hid_device* dev_handle, const char* path, int pid, std::string dev_name)
{
    dev                     = dev_handle;
    location                = path;
    name                    = dev_name;
    keepalive_thread_run    = 1;
    controller_ready        = 0;
    packet_size             = CORSAIR_COMMANDER_CORE_PACKET_SIZE_V2;
    command_res_size        = packet_size - 4;
    this->pid               = pid;
    guard_manager_ptr       = new DeviceGuardManager(new CorsairDeviceGuard());

    if(pid == CORSAIR_COMMANDER_CORE2_PID)
    {
        packet_size         = CORSAIR_COMMANDER_CORE_PACKET_SIZE_V3;
        command_res_size    = packet_size - 4;
    }
    else if(pid == CORSAIR_COMMANDER_CORE_XT_PID)
    {
        /*-----------------------------------------------------*\
        | Commander Core XT                                     |
        \*-----------------------------------------------------*/
        packet_size         = CORSAIR_COMMANDER_CORE_XT_PACKET_SIZE;
        command_res_size    = packet_size - 4;
    }

    /*-----------------------------------------------------*\
    | Initialize controller                                 |
    \*-----------------------------------------------------*/
    InitController();

    /*-----------------------------------------------------*\
    | Start keepalive thread                                |
    \*-----------------------------------------------------*/
    keepalive_thread = new std::thread(&CorsairCommanderCoreController::KeepaliveThread, this);
}

CorsairCommanderCoreController::~CorsairCommanderCoreController()
{
    /*-----------------------------------------------------*\
    | Close the cooling worker first, then the keepalive    |
    | thread, before the last command so none of them can   |
    | read each other's reply                               |
    \*-----------------------------------------------------*/
    StopCoolingThread();

    keepalive_thread_run = 0;
    keepalive_thread->join();
    delete keepalive_thread;

    /*-----------------------------------------------------*\
    | Hardware mode                                         |
    \*-----------------------------------------------------*/
    unsigned char command[2] = {0x01, 0x03};
    unsigned char cmd_data[2] = {0x00, 0x01};
    SendCommand(command, cmd_data, 2, NULL);

    /*-----------------------------------------------------*\
    | Close HID device                                      |
    \*-----------------------------------------------------*/
    hid_close(dev);
    delete guard_manager_ptr;
}

void CorsairCommanderCoreController::InitController()
{
    /*-----------------------------------------------------*\
    | Get version                                           |
    \*-----------------------------------------------------*/
    unsigned char command[2] = {0x02, 0x13};
    unsigned char* res = new unsigned char[command_res_size];

    SendCommand(command, NULL, 0, res);
    version[0] = res[0];
    version[1] = res[1];
    version[2] = res[2];
    delete[] res;

    if(pid == CORSAIR_COMMANDER_CORE_PID && version[0] == 1)
    {
        packet_size = CORSAIR_COMMANDER_CORE_PACKET_SIZE_V1;
        command_res_size = packet_size - 4;
    }

    SetFanMode(false);
}

std::string CorsairCommanderCoreController::GetFirmwareString()
{
    return "v"+std::to_string(version[0]) + "." + std::to_string(version[1]) + "." + std::to_string(version[2]);
}

std::string CorsairCommanderCoreController::GetLocationString()
{
    return("HID: " + location);
}

std::string CorsairCommanderCoreController::GetNameString()
{
    return(name);
}

int CorsairCommanderCoreController::GetPidInt()
{
    return(this->pid);
}

std::vector<unsigned short int> CorsairCommanderCoreController::GetLedCounts()
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    /*-----------------------------------------------------*\
    | Get the LED count per device. Retry when the reply is |
    | not an LED count reply or the pump count is too high  |
    \*-----------------------------------------------------*/
    std::vector<unsigned short int> led_counts;
    unsigned char endpoint[2] = {0x20, 0x00};
    unsigned char* res = new unsigned char[command_res_size];

    for(int attempt = 0; attempt < 3; attempt++)
    {
        memset(res, 0x00, command_res_size);
        ReadData(endpoint, res);

        unsigned int num_devices = res[2];
        bool         valid       = (res[0] == 0x0F) && (res[1] == 0x00)
                                && (num_devices <= CORSAIR_COMMANDER_CORE_NUM_CHANNELS + 1)
                                && ((int)(num_devices * 4 + 3) <= command_res_size);

        led_counts.clear();

        for(unsigned int i = 0; valid && i < num_devices; i++)
        {
            led_counts.push_back(res[i*4+6] << 8 | res[i*4+5]);
        }

        /*-------------------------------------------------*\
        | Only the pump count (index 0) is used             |
        \*-------------------------------------------------*/
        if(valid && (led_counts.empty() || led_counts[0] <= CORSAIR_COMMANDER_CORE_MAX_LEDS_PER_CHANNEL))
        {
            break;
        }

        led_counts.clear();
    }
    delete[] res;

    return led_counts;
}

void CorsairCommanderCoreController::KeepaliveThread()
{
    while(keepalive_thread_run.load())
    {
        if(controller_ready)
        {
            if((std::chrono::steady_clock::now() - last_commit_time) > std::chrono::seconds(10))
            {
                SendCommit();
            }
        }
        std::this_thread::sleep_for(1s);
    }
}

void CorsairCommanderCoreController::SendCommit()
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    if(!lastcolors.empty())
    {
        /*-----------------------------------------------------*\
        | If colors remain to be sent, send them                |
        \*-----------------------------------------------------*/
        SetDirectColor(lastcolors, lastzones);
    }
    else
    {
        /*-----------------------------------------------------*\
        | Update last commit time                               |
        \*-----------------------------------------------------*/
        last_commit_time    = std::chrono::steady_clock::now();

        /*-----------------------------------------------------*\
        | Keepalive, read the reply so it is not taken as the   |
        | reply to the next command                             |
        \*-----------------------------------------------------*/
        unsigned char command[2] = {0x02, 0x13};
        SendCommand(command, NULL, 2, NULL);
    }
}


void CorsairCommanderCoreController::SendCommand(unsigned char command[2], unsigned char data[], unsigned short int data_len, unsigned char res[], bool dev_read)
{
    /*---------------------------------------------------------*\
    | Private function to send a command                        |
    | data_len must be <= 93 for V2 or <= 1021 for V1           |
    \*---------------------------------------------------------*/
    unsigned char* buf = new unsigned char[packet_size];

    memset(buf, 0, packet_size);
    buf[0] = 0x00;
    buf[1] = 0x08;

    memcpy(&buf[2], command, 2);
    if(data != NULL)
    {
        memcpy(&buf[4], data, data_len);
    }

    /*---------------------------------------------------------*\
    | HID I/O start                                             |
    \*---------------------------------------------------------*/
    {
        DeviceGuardLock _ = guard_manager_ptr->AwaitExclusiveAccess();

        hid_write(dev, buf, packet_size);
        if(dev_read)
        {
            do
            {
                hid_read(dev, buf, packet_size);
            }
            while(buf[0] != 0x00);
        }
    }

    /*---------------------------------------------------------*\
    | HID I/O end (lock released)                               |
    \*---------------------------------------------------------*/

    if(res != NULL)
    {
        memcpy(res, &buf[3], command_res_size);
    }

    delete[] buf;
}

void CorsairCommanderCoreController::WriteData(unsigned char endpoint[2], unsigned char data_type[2], unsigned char data[], unsigned short int data_len)
{
    /*---------------------------------------------------------*\
    | Private function to write data to an endpoint             |
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | Open endpoint                                             |
    \*---------------------------------------------------------*/
    unsigned char command[2] = {0x0D, 0x00};
    SendCommand(command, endpoint, 2, NULL);

    /*---------------------------------------------------------*\
    | Write data                                                |
    \*---------------------------------------------------------*/
    unsigned short int data_start_index = 0;
    while(data_start_index < data_len)
    {
        if(data_start_index == 0)
        {
            /*---------------------------------------------------------*\
            | First packet                                              |
            \*---------------------------------------------------------*/
            int packet_data_len = packet_size - 10;
            if(data_len < packet_data_len)
            {
                packet_data_len = data_len;
            }
            unsigned char* buf = new unsigned char[packet_data_len+6];
            unsigned short int real_len = data_len+2;
            /*---------------------------------------------------------*\
            | Convert length to little endian                           |
            \*---------------------------------------------------------*/
            buf[0] = (unsigned char) real_len & 0xFF;
            buf[1] = (unsigned char) (real_len >> 8) & 0xFF;
            buf[2] = 0x00;
            buf[3] = 0x00;
            memcpy(&buf[4], data_type, 2);
            memcpy(&buf[6], data, packet_data_len);

            command[0] = 0x06;
            command[1] = 0x00;
            SendCommand(command, buf, packet_data_len+6, NULL);
            delete[] buf;
            data_start_index += packet_data_len;
        }
        else
        {
            /*-----------------------------------------------------------------------------------------------------*\
            | The rest of the packets                                                                               |
            | This command is not in v1 but it should never be reached as all data should fit in the first packet   |
            \*-----------------------------------------------------------------------------------------------------*/
            int packet_data_len = packet_size - 4;
            if(data_len-data_start_index < packet_data_len)
            {
                packet_data_len = data_len-data_start_index;
            }

            command[0] = 0x07;
            command[1] = 0x00;
            SendCommand(command, &data[data_start_index], packet_data_len, NULL);
            data_start_index += packet_data_len;
        }
    }

    /*---------------------------------------------------------*\
    | Close endpoint                                            |
    \*---------------------------------------------------------*/
    command[0] = 0x05;
    command[1] = 0x01;
    SendCommand(command, NULL, 0, NULL);
}

void CorsairCommanderCoreController::ReadData(unsigned char endpoint[2], unsigned char data[])
{
    /*---------------------------------------------------------*\
    | Private function to read data from an endpoint            |
    | Note: Right now we only know how to read the first packet.|
    |       It is not currently know how to read more.          |
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | Open endpoint                                             |
    \*---------------------------------------------------------*/
    unsigned char command[2] = {0x0D, 0x00};
    SendCommand(command, endpoint, 2, NULL);

    /*---------------------------------------------------------*\
    | Read data                                                 |
    \*---------------------------------------------------------*/
    command[0] = 0x08;
    command[1] = 0x00;
    SendCommand(command, NULL, 0, data);

    /*---------------------------------------------------------*\
    | Close endpoint                                            |
    \*---------------------------------------------------------*/
    command[0] = 0x05;
    command[1] = 0x01;
    SendCommand(command, NULL, 0, NULL);
}

void CorsairCommanderCoreController::SetDirectColor
    (
        std::vector<RGBColor> colors,
        std::vector<zone> zones
    )
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    if(controller_ready == 1 && ((std::chrono::steady_clock::now() - last_commit_time) > std::chrono::milliseconds(33)))
    {
        lastcolors              = colors;
        lastzones               = zones;
        int packet_offset       = 0;
        int led_idx             = 0;
        int packet_len          = CORSAIR_COMMANDER_CORE_RGB_DATA_LENGTH;

        if(pid == CORSAIR_COMMANDER_CORE_XT_PID)
        {
            packet_len          = CORSAIR_COMMANDER_CORE_XT_RGB_DATA_LENGTH;
        }

        /*-----------------------------------------------------*\
        | Zone 0 gets what the fan headers leave of the packet  |
        \*-----------------------------------------------------*/
        unsigned int zone0_max  = packet_len / 3 - CORSAIR_COMMANDER_CORE_NUM_CHANNELS * CORSAIR_COMMANDER_CORE_MAX_LEDS_PER_CHANNEL;

        unsigned char* usb_buf  = new unsigned char[packet_len]();

        for(unsigned int zone_idx = 0; zone_idx < zones.size() && zone_idx <= CORSAIR_COMMANDER_CORE_NUM_CHANNELS; zone_idx++)
        {
            unsigned int zone_max   = (zone_idx == 0) ? zone0_max : CORSAIR_COMMANDER_CORE_MAX_LEDS_PER_CHANNEL;
            unsigned int zone_leds  = std::min(zones[zone_idx].leds_count, zone_max);

            /*-------------------------------------------------*\
            | Add led colors                                    |
            \*-------------------------------------------------*/
            for(unsigned int i = 0; i < zone_leds && (led_idx + i) < colors.size(); i++)
            {
                usb_buf[packet_offset + i*3]     = RGBGetRValue(colors[led_idx + i]);
                usb_buf[packet_offset + i*3 + 1] = RGBGetGValue(colors[led_idx + i]);
                usb_buf[packet_offset + i*3 + 2] = RGBGetBValue(colors[led_idx + i]);
            }

            packet_offset += 3 * zone_leds;
            led_idx = led_idx + zones[zone_idx].leds_count;

            if(zone_idx != 0)
            {
                packet_offset += 3 * (CORSAIR_COMMANDER_CORE_MAX_LEDS_PER_CHANNEL - zone_leds);
            }
        }

        /*-----------------------------------------------------*\
        | Sending a direct mode color packet resets the timeout |
        \*-----------------------------------------------------*/
        last_commit_time = std::chrono::steady_clock::now();

        unsigned char endpoint[2] = {0x22, 0x00};
        unsigned char data_type[2] = {0x12, 0x00};
        WriteData(endpoint, data_type, usb_buf, packet_offset);

        delete[] usb_buf;
    }
}

void CorsairCommanderCoreController::SetFanMode(bool external_rgb_port)
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    controller_ready    = 0;
    DeviceGuardLock _   = guard_manager_ptr->AwaitExclusiveAccess();

    /*-----------------------------------------------------*\
    | Re-initialisation drops the software duty table, so   |
    | the cooling worker writes it again                    |
    \*-----------------------------------------------------*/
    init_count++;

    /*-----------------------------------------------------*\
    | Force controller to 6 QL fan mode to expose maximum   |
    | number of LEDs per rgb port (34 LEDs per port)        |
    \*-----------------------------------------------------*/
    unsigned int index          = 3;
    unsigned int max_index      = 15;
    unsigned char endpoint[2]   = {0x1E, 0x00};
    unsigned char data_type[2]  = {0x0D, 0x00};

    unsigned char buf[15];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(buf, 0x00, 15);

    buf[0]              = 0x07;
    if(pid == CORSAIR_COMMANDER_CORE_XT_PID)
    {
        /*-------------------------------------------------*\
        | Commander Core XT external RGB port               |
        \*-------------------------------------------------*/
        if(external_rgb_port)
        {
            /*---------------------------------------------*\
            | Enable external port                          |
            \*---------------------------------------------*/
            buf[1]      = 0x01;
            buf[2]      = 0x01;
        }
        else
        {
            /*---------------------------------------------*\
            | Shift packet start position and maximum index |
            \*---------------------------------------------*/
            buf[1]      = 0x00;
            buf[2]      = 0x00;
            index       = 2;
            max_index   = 14;
        }
    }
    else
    {
        /*-------------------------------------------------*\
        | Commander Core, Set AIO mode                      |
        \*-------------------------------------------------*/
        buf[1]          = 0x01;
        buf[2]          = 0x08;
    }

    /*-----------------------------------------------------*\
    | SET fan modes                                         |
    \*-----------------------------------------------------*/
    for(unsigned int i = index; i < max_index; i = i + 2)
    {
        buf[i]          = 0x01;
        buf[i + 1]      = 0x06;
    }

    WriteData(endpoint, data_type, buf, 15);
    controller_ready    = 1;

    /*-----------------------------------------------------*\
    | Wake up device, needs to be done after setting fan    |
    | mode to reinitialize device if fan mode has changed   |
    \*-----------------------------------------------------*/
    unsigned char command[2]    = {0x01, 0x03};
    unsigned char cmd_data[2]   = {0x00, 0x02};
    SendCommand(command, cmd_data, 2, NULL);
}

void CorsairCommanderCoreController::SetLedAmount(int led_amount)
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    controller_ready    = 0;
    DeviceGuardLock _   = guard_manager_ptr->AwaitExclusiveAccess();

    unsigned char buf[15];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(buf, 0x00, 15);

    unsigned char endpoint[2]  = {0x1D, 0x00};
    unsigned char data_type[2] = {0x0C, 0x00};

    buf[0]  = 0x07;
    buf[1]  = led_amount;

    WriteData(endpoint, data_type, buf, 15);
    controller_ready    = 1;
}


/*---------------------------------------------------------*\
| Cooling                                                   |
|                                                           |
| Transactions use the second handle and the info step like |
| the vendor software does for cooling, so they never share |
| a handle with the lighting stream. Each one runs under    |
| controller_mutex, which also serializes lighting writes.  |
\*---------------------------------------------------------*/

#define CORSAIR_COMMANDER_CORE_COOLING_REPLY_TIMEOUT    150ms
#define CORSAIR_COMMANDER_CORE_COOLING_INFO_TIMEOUT     40ms
#define CORSAIR_COMMANDER_CORE_COOLING_POLL_PERIOD      1s
#define CORSAIR_COMMANDER_CORE_COOLING_CONNECTED_PERIOD 10s
#define CORSAIR_COMMANDER_CORE_COOLING_IDLE_AFTER       15s
#define CORSAIR_COMMANDER_CORE_COOLING_STALE_AFTER      5s
#define CORSAIR_COMMANDER_CORE_COOLING_REWRITE_PERIOD   5s
#define CORSAIR_COMMANDER_CORE_COOLING_MAX_BACKOFF_STEP 3

void CorsairCommanderCoreController::StartCoolingThread()
{
    std::lock_guard<std::mutex> lock(cooling_thread_mutex);

    if(cooling_thread == nullptr && !cooling_stopping.load())
    {
        cooling_thread_run  = true;
        cooling_thread      = new std::thread(&CorsairCommanderCoreController::CoolingThreadFunction, this);
    }
}

void CorsairCommanderCoreController::StopCoolingThread()
{
    std::lock_guard<std::mutex> lock(cooling_thread_mutex);

    cooling_stopping = true;

    if(cooling_thread != nullptr)
    {
        cooling_thread_run = false;
        cooling_cv.notify_all();
        cooling_thread->join();
        delete cooling_thread;
        cooling_thread = nullptr;
    }
}

bool CorsairCommanderCoreController::NexusCoolingGetStatus(std::vector<unsigned char>& payload)
{
    if(cooling_stopping.load())
    {
        return(false);
    }

    StartCoolingThread();

    std::lock_guard<std::mutex> lock(cooling_mutex);

    auto now                = std::chrono::steady_clock::now();
    cooling_last_request    = now;

    CoolingStatus status    = cooling_status;
    status.pid              = (uint16_t)pid;

    /*-----------------------------------------------------*\
    | Old readings are reported as no data                  |
    \*-----------------------------------------------------*/
    long long age_ms        = std::chrono::duration_cast<std::chrono::milliseconds>(now - cooling_status_time).count();

    status.valid            = cooling_status.valid && age_ms < std::chrono::milliseconds(CORSAIR_COMMANDER_CORE_COOLING_STALE_AFTER).count();
    status.age_ms           = (uint16_t)std::min<long long>(std::max<long long>(age_ms, 0), 0xFFFF);
    status.duty             = cooling_applied_duty;

    payload                 = SerializeStatus(status);
    return(true);
}

bool CorsairCommanderCoreController::NexusCoolingSetDuties(const unsigned char* payload, unsigned int size)
{
    std::vector<DutyEntry> entries;

    if(cooling_stopping.load() || !ParseSetDuties(payload, size, entries))
    {
        return(false);
    }

    ApplyPumpFloor(entries, pid != CORSAIR_COMMANDER_CORE_XT_PID);

    StartCoolingThread();

    std::lock_guard<std::mutex> lock(cooling_mutex);

    cooling_last_request    = std::chrono::steady_clock::now();
    cooling_serial++;

    /*-----------------------------------------------------*\
    | An empty table hands the channels back; the commanded |
    | table stays until the hand-back table is written      |
    \*-----------------------------------------------------*/
    if(entries.empty())
    {
        cooling_release_pending = true;
    }
    else
    {
        cooling_target          = entries;
        cooling_release_pending = false;
        cooling_safe_active     = false;
    }

    return(true);
}

void CorsairCommanderCoreController::CoolingThreadFunction()
{
    auto next_poll          = std::chrono::steady_clock::now();
    auto next_connected     = next_poll;
    auto next_apply         = next_poll;

    while(cooling_thread_run.load())
    {
        {
            std::unique_lock<std::mutex> lock(cooling_mutex);
            cooling_cv.wait_for(lock, 200ms, [this]{ return(!cooling_thread_run.load()); });
        }

        if(!cooling_thread_run.load())
        {
            break;
        }

        auto now = std::chrono::steady_clock::now();

        if(!controller_ready.load())
        {
            continue;
        }

        if(now >= next_apply)
        {
            CoolingApplyDuties();
        }

        bool interested;
        unsigned int failures;

        {
            std::lock_guard<std::mutex> lock(cooling_mutex);

            interested  = (now - cooling_last_request) < CORSAIR_COMMANDER_CORE_COOLING_IDLE_AFTER;
            failures    = cooling_failures;
        }

        /*-------------------------------------------------*\
        | Back off while the device does not answer, so a   |
        | stuck device costs lighting at most a few frames  |
        \*-------------------------------------------------*/
        auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(CORSAIR_COMMANDER_CORE_COOLING_POLL_PERIOD) * (1u << std::min<unsigned int>(failures, CORSAIR_COMMANDER_CORE_COOLING_MAX_BACKOFF_STEP));

        next_apply = failures > 0 ? now + delay : now;

        if(interested && now >= next_poll)
        {
            bool read_connected = now >= next_connected;

            CoolingPoll(read_connected);

            now         = std::chrono::steady_clock::now();
            next_poll   = now + delay;

            if(read_connected)
            {
                next_connected = now + CORSAIR_COMMANDER_CORE_COOLING_CONNECTED_PERIOD;
            }
        }
    }
}

void CorsairCommanderCoreController::CoolingPoll(bool read_connected)
{
    std::vector<unsigned char>  res;
    std::vector<uint16_t>       rpm;
    std::vector<Temperature>    temperatures;
    std::vector<bool>           connected;
    bool                        have_connected = false;

    if(!CoolingReadEndpoint(ENDPOINT_SPEEDS, TYPE_SPEEDS, res) || !ParseSpeeds(res.data(), res.size(), rpm)
    || !CoolingReadEndpoint(ENDPOINT_TEMPERATURES, TYPE_TEMPERATURES, res) || !ParseTemperatures(res.data(), res.size(), temperatures))
    {
        std::lock_guard<std::mutex> lock(cooling_mutex);

        cooling_failures++;
        return;
    }

    if(read_connected && CoolingReadEndpoint(ENDPOINT_CONNECTED, TYPE_CONNECTED, res) && ParseConnected(res.data(), res.size(), connected))
    {
        have_connected = true;
    }

    std::lock_guard<std::mutex> lock(cooling_mutex);

    cooling_failures            = 0;
    cooling_status.valid        = true;
    cooling_status_time         = std::chrono::steady_clock::now();
    cooling_status.rpm          = rpm;
    cooling_status.temperatures = temperatures;

    if(have_connected)
    {
        cooling_status.connected = connected;
    }
    else if(cooling_status.connected.size() != rpm.size())
    {
        /*-------------------------------------------------*\
        | Until the connected list has been read, a spinning|
        | channel and the pump port count as connected      |
        \*-------------------------------------------------*/
        cooling_status.connected.assign(rpm.size(), false);

        for(size_t i = 0; i < rpm.size(); i++)
        {
            cooling_status.connected[i] = rpm[i] > 0 || (i == 0 && pid != CORSAIR_COMMANDER_CORE_XT_PID);
        }
    }
}

void CorsairCommanderCoreController::CoolingApplyDuties()
{
    std::vector<DutyEntry>      table_entries;
    std::vector<unsigned char>  written;
    std::vector<bool>           connected;
    unsigned int                epoch = init_count.load();
    bool                        epoch_changed;
    bool                        release;
    bool                        silent;
    unsigned long               serial;
    bool                        has_pump = (pid != CORSAIR_COMMANDER_CORE_XT_PID);
    auto                        now      = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point written_at;

    {
        std::lock_guard<std::mutex> lock(cooling_mutex);

        table_entries   = cooling_target;
        written         = cooling_written_table;
        written_at      = cooling_written_at;
        connected       = cooling_status.connected;
        epoch_changed   = (epoch != cooling_written_init);
        release         = cooling_release_pending;
        serial          = cooling_serial;
        silent          = !cooling_safe_active && !table_entries.empty() && (now - cooling_last_request) > CORSAIR_COMMANDER_CORE_COOLING_IDLE_AFTER;
    }

    /*-----------------------------------------------------*\
    | Nexus released the channels, or went silent while     |
    | duties were commanded: write a table the device can   |
    | run on its own. The sleep command would also drop the |
    | lighting stream, so it is not used. The table then    |
    | becomes the active one, so a re-init or a silent      |
    | reset is repaired like a commanded table              |
    \*-----------------------------------------------------*/
    if(release || silent)
    {
        if(connected.empty())
        {
            return;
        }

        std::vector<DutyEntry> safe = release
            ? BuildUniformTable(connected, has_pump, RELEASE_PUMP_DUTY, RELEASE_FAN_DUTY)
            : BuildUniformTable(connected, has_pump, FAILSAFE_DUTY, FAILSAFE_DUTY);

        if(safe.empty() || CoolingWriteTable(safe, epoch))
        {
            std::lock_guard<std::mutex> lock(cooling_mutex);

            /*-------------------------------------------------*\
            | A command that arrived during the write stands    |
            \*-------------------------------------------------*/
            if(cooling_serial == serial)
            {
                cooling_target          = safe;
                cooling_safe_active     = !safe.empty();
                cooling_release_pending = false;
            }
        }

        return;
    }

    /*-----------------------------------------------------*\
    | Nothing is written until Nexus commands a duty        |
    \*-----------------------------------------------------*/
    if(table_entries.empty())
    {
        return;
    }

    /*-----------------------------------------------------*\
    | A changed table is written at once. An unchanged one  |
    | is written again now and then, because a device reset |
    | that left the init count alone drops it silently      |
    \*-----------------------------------------------------*/
    if(!epoch_changed && BuildSoftwareDutyTable(table_entries) == written && (now - written_at) < CORSAIR_COMMANDER_CORE_COOLING_REWRITE_PERIOD)
    {
        return;
    }

    CoolingWriteTable(table_entries, epoch);
}

bool CorsairCommanderCoreController::CoolingWriteTable(const std::vector<DutyEntry>& entries, unsigned int epoch)
{
    std::vector<unsigned char> table = BuildSoftwareDutyTable(entries);
    bool                       ok    = CoolingWriteEndpoint(ENDPOINT_SOFTWARE_DUTY, TYPE_SOFTWARE_DUTY, table);

    std::lock_guard<std::mutex> lock(cooling_mutex);

    if(!ok)
    {
        cooling_failures++;
        return(false);
    }

    cooling_failures        = 0;
    cooling_written_table   = table;
    cooling_written_init    = epoch;
    cooling_written_at      = std::chrono::steady_clock::now();
    cooling_applied_duty.assign(MAX_CHANNELS, NO_DUTY);

    for(size_t i = 0; i < entries.size(); i++)
    {
        cooling_applied_duty[entries[i].channel] = entries[i].duty;
    }

    return(true);
}

bool CorsairCommanderCoreController::CoolingExchange(unsigned char command, const std::vector<unsigned char>& data, unsigned char* reply, bool reply_required)
{
    if(data.size() > (size_t)(packet_size - 4))
    {
        return(false);
    }

    std::vector<unsigned char> out = BuildCommand(command, data, packet_size);
    std::vector<unsigned char> in(packet_size, 0);

    if(hid_write(dev, out.data(), packet_size) < 0)
    {
        return(false);
    }

    auto deadline = std::chrono::steady_clock::now() + (reply_required ? CORSAIR_COMMANDER_CORE_COOLING_REPLY_TIMEOUT : CORSAIR_COMMANDER_CORE_COOLING_INFO_TIMEOUT);

    while(true)
    {
        long long remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();

        if(remaining <= 0)
        {
            return(!reply_required);
        }

        int bytes_read = hid_read_timeout(dev, in.data(), packet_size, (int)remaining);

        if(bytes_read < 0)
        {
            return(false);
        }

        /*-------------------------------------------------*\
        | Skip anything that does not answer this command   |
        \*-------------------------------------------------*/
        if(bytes_read >= 3 && in[0] == 0x00 && in[1] == command)
        {
            if(reply != nullptr)
            {
                memcpy(reply, in.data(), packet_size);
            }

            return(true);
        }
    }
}

bool CorsairCommanderCoreController::CoolingReadEndpoint(unsigned char endpoint, unsigned char data_type, std::vector<unsigned char>& res)
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    if(!controller_ready.load())
    {
        return(false);
    }

    DeviceGuardLock _ = guard_manager_ptr->AwaitExclusiveAccess();

    DiscardStaleReplies();

    std::vector<unsigned char> reply(packet_size, 0);
    std::vector<unsigned char> none;

    bool ok = CoolingExchange(COMMAND_OPEN, OpenData(endpoint), nullptr, true)
           && CoolingExchange(COMMAND_INFO, none, nullptr, false)
           && CoolingExchange(COMMAND_READ, none, reply.data(), true);

    /*-----------------------------------------------------*\
    | Always close, so a failed read does not leave the     |
    | endpoint open for the lighting stream to trip over    |
    \*-----------------------------------------------------*/
    CoolingExchange(COMMAND_CLOSE, CloseData(), nullptr, true);
    DiscardStaleReplies();

    if(!ok || reply[3] != data_type)
    {
        return(false);
    }

    res.assign(reply.begin() + 3, reply.end());
    return(true);
}

bool CorsairCommanderCoreController::CoolingWriteEndpoint(unsigned char endpoint, unsigned char data_type, const std::vector<unsigned char>& data)
{
    std::lock_guard<std::recursive_mutex> lock(controller_mutex);

    if(!controller_ready.load())
    {
        return(false);
    }

    DeviceGuardLock _ = guard_manager_ptr->AwaitExclusiveAccess();

    DiscardStaleReplies();

    std::vector<unsigned char> none;

    bool ok = CoolingExchange(COMMAND_OPEN, OpenData(endpoint), nullptr, true)
           && CoolingExchange(COMMAND_INFO, none, nullptr, false)
           && CoolingExchange(COMMAND_WRITE, BuildWriteData(data_type, data), nullptr, true);

    CoolingExchange(COMMAND_CLOSE, CloseData(), nullptr, true);
    DiscardStaleReplies();

    return(ok);
}

void CorsairCommanderCoreController::DiscardStaleReplies()
{
    /*-----------------------------------------------------*\
    | Discard a reply that a timed out transaction left     |
    | behind                                                |
    \*-----------------------------------------------------*/
    std::vector<unsigned char> stale(packet_size);

    for(int i = 0; i < 16 && hid_read_timeout(dev, stale.data(), packet_size, 0) > 0; i++)
    {
    }
}
