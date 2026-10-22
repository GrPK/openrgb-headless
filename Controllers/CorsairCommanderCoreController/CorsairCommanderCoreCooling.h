/*---------------------------------------------------------*\
| CorsairCommanderCoreCooling.h                             |
|                                                           |
|   Cooling data formats for Corsair Commander Core:        |
|   reply parsers, the software duty table and the Nexus    |
|   SDK payloads. Pure functions, no HID access.            |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CorsairCommanderCoreCooling
{

/*-----------------------------------------------------*\
| Endpoints (second endpoint byte is zero) and the data |
| type the device answers with                          |
\*-----------------------------------------------------*/
constexpr unsigned char ENDPOINT_SPEEDS             = 0x17;
constexpr unsigned char ENDPOINT_SOFTWARE_DUTY      = 0x18;
constexpr unsigned char ENDPOINT_CONNECTED          = 0x1A;
constexpr unsigned char ENDPOINT_TEMPERATURES       = 0x21;

constexpr unsigned char TYPE_SPEEDS                 = 0x06;
constexpr unsigned char TYPE_SOFTWARE_DUTY          = 0x07;
constexpr unsigned char TYPE_CONNECTED              = 0x09;
constexpr unsigned char TYPE_TEMPERATURES           = 0x10;

/*-----------------------------------------------------*\
| Commands of a cooling transaction, all on the second  |
| handle: open the endpoint, query it, read or write,   |
| close                                                 |
\*-----------------------------------------------------*/
constexpr unsigned char COMMAND_OPEN                = 0x0D;
constexpr unsigned char COMMAND_INFO                = 0x09;
constexpr unsigned char COMMAND_READ                = 0x08;
constexpr unsigned char COMMAND_WRITE               = 0x06;
constexpr unsigned char COMMAND_CLOSE               = 0x05;
constexpr unsigned char COOLING_HANDLE             = 0x01;

/*-----------------------------------------------------*\
| Upper bounds that reject a garbled reply              |
\*-----------------------------------------------------*/
constexpr unsigned int  MAX_CHANNELS                = 8;
constexpr unsigned int  MAX_TEMPERATURES            = 8;

constexpr unsigned char CONNECTED_FLAG              = 0x07;
constexpr unsigned char NO_DUTY                     = 0xFF;

/*-----------------------------------------------------*\
| Nexus SDK payloads, little-endian                     |
| Status reply:                                         |
|   u8 version, u8 flags (bit0 data valid), u16 pid,    |
|   u16 age in ms since the last good poll (saturates), |
|   u8 channel count, per channel: u8 connected,        |
|   u16 rpm, u8 duty last written (NO_DUTY if none),    |
|   u8 temperature count, per temperature: u8 connected,|
|   i16 tenths of a degree C                            |
| Set duties request:                                   |
|   u8 version, u8 count, per entry: u8 channel, u8 duty|
\*-----------------------------------------------------*/
constexpr unsigned char WIRE_VERSION                = 1;
constexpr unsigned char STATUS_FLAG_VALID           = 0x01;

struct DutyEntry
{
    unsigned char   channel;
    unsigned char   duty;
};

struct Temperature
{
    bool            connected;
    int16_t         tenths;
};

struct CoolingStatus
{
    bool                        valid   = false;
    uint16_t                    pid     = 0;
    uint16_t                    age_ms  = 0;
    std::vector<bool>           connected;
    std::vector<uint16_t>       rpm;
    std::vector<unsigned char>  duty;
    std::vector<Temperature>    temperatures;
};

/*-----------------------------------------------------*\
| One outgoing report: report id, framing byte, command,|
| handle, then the data, zero padded to the packet size |
\*-----------------------------------------------------*/
inline std::vector<unsigned char> BuildCommand(unsigned char command, const std::vector<unsigned char>& data, size_t packet_size)
{
    std::vector<unsigned char> packet(packet_size, 0x00);

    if(packet_size >= 4 + data.size())
    {
        packet[1] = 0x08;
        packet[2] = command;
        packet[3] = COOLING_HANDLE;

        for(size_t i = 0; i < data.size(); i++)
        {
            packet[4 + i] = data[i];
        }
    }

    return(packet);
}

inline std::vector<unsigned char> OpenData(unsigned char endpoint)
{
    return(std::vector<unsigned char>{endpoint, 0x00});
}

inline std::vector<unsigned char> CloseData()
{
    return(std::vector<unsigned char>{0x01});
}

/*-----------------------------------------------------*\
| Data of the write command: length including the data  |
| type, two reserved bytes, the data type, the data     |
\*-----------------------------------------------------*/
inline std::vector<unsigned char> BuildWriteData(unsigned char data_type, const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> packet;
    unsigned int               length = (unsigned int)data.size() + 2;

    packet.push_back((unsigned char)(length & 0xFF));
    packet.push_back((unsigned char)(length >> 8));
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(data_type);
    packet.push_back(0x00);
    packet.insert(packet.end(), data.begin(), data.end());

    return(packet);
}

/*-----------------------------------------------------*\
| res points at the data type bytes of a reply, len is  |
| the number of readable bytes from there               |
\*-----------------------------------------------------*/
inline bool ParseSpeeds(const unsigned char* res, size_t len, std::vector<uint16_t>& rpm)
{
    if(len < 3 || res[0] != TYPE_SPEEDS || res[1] != 0x00)
    {
        return(false);
    }

    size_t count = res[2];

    if(count > MAX_CHANNELS || len < 3 + count * 2)
    {
        return(false);
    }

    rpm.clear();

    for(size_t i = 0; i < count; i++)
    {
        rpm.push_back((uint16_t)(res[3 + i * 2] | (res[4 + i * 2] << 8)));
    }

    return(true);
}

inline bool ParseConnected(const unsigned char* res, size_t len, std::vector<bool>& connected)
{
    if(len < 3 || res[0] != TYPE_CONNECTED || res[1] != 0x00)
    {
        return(false);
    }

    size_t count = res[2];

    if(count > MAX_CHANNELS || len < 3 + count)
    {
        return(false);
    }

    connected.clear();

    for(size_t i = 0; i < count; i++)
    {
        connected.push_back(res[3 + i] == CONNECTED_FLAG);
    }

    return(true);
}

inline bool ParseTemperatures(const unsigned char* res, size_t len, std::vector<Temperature>& temperatures)
{
    if(len < 3 || res[0] != TYPE_TEMPERATURES || res[1] != 0x00)
    {
        return(false);
    }

    size_t count = res[2];

    if(count > MAX_TEMPERATURES || len < 3 + count * 3)
    {
        return(false);
    }

    temperatures.clear();

    for(size_t i = 0; i < count; i++)
    {
        Temperature t;

        t.connected = (res[3 + i * 3] == 0x00);
        t.tenths    = (int16_t)(res[4 + i * 3] | (res[5 + i * 3] << 8));

        temperatures.push_back(t);
    }

    return(true);
}

/*-----------------------------------------------------*\
| Software duty table written to ENDPOINT_SOFTWARE_DUTY |
| with data type TYPE_SOFTWARE_DUTY: u8 count, then per |
| entry u8 channel, a zero byte, u16 duty percent       |
\*-----------------------------------------------------*/
inline std::vector<unsigned char> BuildSoftwareDutyTable(const std::vector<DutyEntry>& entries)
{
    std::vector<unsigned char> table;

    table.push_back((unsigned char)entries.size());

    for(size_t i = 0; i < entries.size(); i++)
    {
        table.push_back(entries[i].channel);
        table.push_back(0x00);
        table.push_back(entries[i].duty);
        table.push_back(0x00);
    }

    return(table);
}

/*-----------------------------------------------------*\
| Rejects a version mismatch, a size that does not match|
| the count, a duplicate or out of range channel and a  |
| duty above full scale                                 |
\*-----------------------------------------------------*/
inline bool ParseSetDuties(const unsigned char* data, size_t size, std::vector<DutyEntry>& entries)
{
    if(data == nullptr || size < 2 || data[0] != WIRE_VERSION)
    {
        return(false);
    }

    size_t count = data[1];

    if(count > MAX_CHANNELS || size != 2 + count * 2)
    {
        return(false);
    }

    std::vector<DutyEntry> parsed;
    unsigned int           seen = 0;

    for(size_t i = 0; i < count; i++)
    {
        DutyEntry entry;

        entry.channel = data[2 + i * 2];
        entry.duty    = data[3 + i * 2];

        if(entry.channel >= MAX_CHANNELS || entry.duty > 100 || (seen & (1u << entry.channel)))
        {
            return(false);
        }

        seen |= (1u << entry.channel);
        parsed.push_back(entry);
    }

    entries = parsed;
    return(true);
}

inline std::vector<unsigned char> SerializeStatus(const CoolingStatus& status)
{
    std::vector<unsigned char> out;
    size_t                     channels = status.rpm.size();

    if(channels > MAX_CHANNELS)
    {
        channels = MAX_CHANNELS;
    }

    size_t temperatures = status.temperatures.size();

    if(temperatures > MAX_TEMPERATURES)
    {
        temperatures = MAX_TEMPERATURES;
    }

    out.push_back(WIRE_VERSION);
    out.push_back(status.valid ? STATUS_FLAG_VALID : 0x00);
    out.push_back((unsigned char)(status.pid & 0xFF));
    out.push_back((unsigned char)(status.pid >> 8));
    out.push_back((unsigned char)(status.age_ms & 0xFF));
    out.push_back((unsigned char)(status.age_ms >> 8));
    out.push_back((unsigned char)channels);

    for(size_t i = 0; i < channels; i++)
    {
        bool connected = (i < status.connected.size()) ? (bool)status.connected[i] : false;

        out.push_back(connected ? 0x01 : 0x00);
        out.push_back((unsigned char)(status.rpm[i] & 0xFF));
        out.push_back((unsigned char)(status.rpm[i] >> 8));
        out.push_back((i < status.duty.size()) ? status.duty[i] : NO_DUTY);
    }

    out.push_back((unsigned char)temperatures);

    for(size_t i = 0; i < temperatures; i++)
    {
        uint16_t raw = (uint16_t)status.temperatures[i].tenths;

        out.push_back(status.temperatures[i].connected ? 0x01 : 0x00);
        out.push_back((unsigned char)(raw & 0xFF));
        out.push_back((unsigned char)(raw >> 8));
    }

    return(out);
}

}
