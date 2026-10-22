/*---------------------------------------------------------*\
| CorsairCommanderCoreCoolingTest.cpp                       |
|                                                           |
|   Checks the Commander Core cooling formats against reply |
|   bytes from the liquidctl test fixtures and outgoing     |
|   bytes captured from the vendor software.                |
|                                                           |
|   Build and run, no OpenRGB build needed:                 |
|     c++ -std=c++17 \                                      |
|       -I Controllers/CorsairCommanderCoreController \     |
|       tests/CorsairCommanderCoreCoolingTest.cpp -o t      |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstdio>
#include <cstring>
#include "CorsairCommanderCoreCooling.h"

using namespace CorsairCommanderCoreCooling;

typedef std::vector<unsigned char> Bytes;

static int failures = 0;

#define CHECK(cond)                                                                 \
    do                                                                              \
    {                                                                               \
        if(!(cond))                                                                 \
        {                                                                           \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                          \
            failures++;                                                             \
        }                                                                           \
    } while(0)

/*---------------------------------------------------------*\
| A packet with the trailing zero padding removed, the form |
| the capture notes use                                     |
\*---------------------------------------------------------*/
static Bytes Trimmed(Bytes packet)
{
    while(!packet.empty() && packet.back() == 0x00)
    {
        packet.pop_back();
    }

    return(packet);
}

static Bytes Hex(const char* text)
{
    Bytes out;

    for(const char* p = text; *p;)
    {
        if(*p == ' ')
        {
            p++;
            continue;
        }

        unsigned int value;
        sscanf(p, "%2x", &value);
        out.push_back((unsigned char)value);
        p += 2;
    }

    return(out);
}

static void TestParseSpeeds()
{
    /* liquidctl fixture: speeds 2357 918 903 501 1104 1824 104 */
    Bytes res = Hex("06 00 07 35 09 96 03 87 03 f5 01 50 04 20 07 68 00");
    std::vector<uint16_t> rpm;

    CHECK(ParseSpeeds(res.data(), res.size(), rpm));
    CHECK(rpm.size() == 7);
    CHECK(rpm[0] == 2357 && rpm[1] == 918 && rpm[2] == 903 && rpm[3] == 501 && rpm[4] == 1104 && rpm[5] == 1824 && rpm[6] == 104);

    /* liquidctl fixture: a port with nothing reads zero */
    res = Hex("06 00 07 00 00 68 00 00 00 00 00 00 00 00 00 96 03");
    CHECK(ParseSpeeds(res.data(), res.size(), rpm));
    CHECK(rpm[0] == 0 && rpm[1] == 104 && rpm[6] == 918);

    /* wrong data type, truncated body, absurd count */
    res = Hex("10 00 07 35 09");
    CHECK(!ParseSpeeds(res.data(), res.size(), rpm));
    res = Hex("06 00 07 35 09 96 03");
    CHECK(!ParseSpeeds(res.data(), res.size(), rpm));
    res = Hex("06 00 ff 35 09");
    CHECK(!ParseSpeeds(res.data(), res.size(), rpm));
    CHECK(!ParseSpeeds(res.data(), 2, rpm));
}

static void TestParseConnected()
{
    /* liquidctl fixture: only ports 1 and 6 connected */
    Bytes res = Hex("09 00 07 01 07 01 01 01 01 07");
    std::vector<bool> connected;

    CHECK(ParseConnected(res.data(), res.size(), connected));
    CHECK(connected.size() == 7);
    CHECK(!connected[0] && connected[1] && !connected[2] && !connected[5] && connected[6]);

    res = Hex("06 00 07 01 07");
    CHECK(!ParseConnected(res.data(), res.size(), connected));
    res = Hex("09 00 07 01 07");
    CHECK(!ParseConnected(res.data(), res.size(), connected));
}

static void TestParseTemperatures()
{
    /* liquidctl fixture: 12.3 and 45.6 */
    Bytes res = Hex("10 00 02 00 7b 00 00 c8 01");
    std::vector<Temperature> temperatures;

    CHECK(ParseTemperatures(res.data(), res.size(), temperatures));
    CHECK(temperatures.size() == 2);
    CHECK(temperatures[0].connected && temperatures[0].tenths == 123);
    CHECK(temperatures[1].connected && temperatures[1].tenths == 456);

    /* liquidctl fixture: first probe missing */
    res = Hex("10 00 02 01 00 00 00 c8 01");
    CHECK(ParseTemperatures(res.data(), res.size(), temperatures));
    CHECK(!temperatures[0].connected && temperatures[1].connected && temperatures[1].tenths == 456);

    res = Hex("10 00 02 00 7b 00");
    CHECK(!ParseTemperatures(res.data(), res.size(), temperatures));
}

static void TestSoftwareDutyTable()
{
    /* Vendor software on a Commander Core: pump 75, fans 100 100 100 22 */
    std::vector<DutyEntry> entries = {{0, 0x4b}, {1, 0x64}, {2, 0x64}, {3, 0x64}, {4, 0x16}};
    Bytes table = BuildSoftwareDutyTable(entries);

    CHECK(Trimmed(table) == Hex("05 00 00 4b 00 01 00 64 00 02 00 64 00 03 00 64 00 04 00 16"));

    /* The same table inside the write command, as sent on the wire */
    Bytes packet = BuildCommand(COMMAND_WRITE, BuildWriteData(TYPE_SOFTWARE_DUTY, table), 97);
    CHECK(packet.size() == 97);
    CHECK(Trimmed(packet) == Hex("00 08 06 01 17 00 00 00 07 00 05 00 00 4b 00 01 00 64 00 02 00 64 00 03 00 64 00 04 00 16"));

    /* Vendor software on a Commander ST: one fan on channel 6 at 100 */
    entries = {{6, 100}};
    packet = BuildCommand(COMMAND_WRITE, BuildWriteData(TYPE_SOFTWARE_DUTY, BuildSoftwareDutyTable(entries)), 65);
    CHECK(packet.size() == 65);
    CHECK(Trimmed(packet) == Hex("00 08 06 01 07 00 00 00 07 00 01 06 00 64"));
}

static void TestTransactionCommands()
{
    /* Read of the temperatures endpoint, as the vendor software issues it */
    CHECK(Trimmed(BuildCommand(COMMAND_OPEN, OpenData(ENDPOINT_TEMPERATURES), 97)) == Hex("00 08 0d 01 21"));
    CHECK(Trimmed(BuildCommand(COMMAND_INFO, Bytes(), 97)) == Hex("00 08 09 01"));
    CHECK(Trimmed(BuildCommand(COMMAND_READ, Bytes(), 97)) == Hex("00 08 08 01"));
    CHECK(Trimmed(BuildCommand(COMMAND_CLOSE, CloseData(), 97)) == Hex("00 08 05 01 01"));

    /* Software duty endpoint open */
    CHECK(Trimmed(BuildCommand(COMMAND_OPEN, OpenData(ENDPOINT_SOFTWARE_DUTY), 65)) == Hex("00 08 0d 01 18"));
    CHECK(Trimmed(BuildCommand(COMMAND_OPEN, OpenData(ENDPOINT_SPEEDS), 65)) == Hex("00 08 0d 01 17"));

    /* Data larger than the packet leaves an empty command instead of overrunning */
    CHECK(BuildCommand(COMMAND_WRITE, Bytes(100, 0xAA), 65) == Bytes(65, 0x00));
}

static void TestSetDuties()
{
    std::vector<DutyEntry> entries;

    CHECK(ParseSetDuties(Hex("01 02 00 4b 03 64").data(), 6, entries));
    CHECK(entries.size() == 2 && entries[0].channel == 0 && entries[0].duty == 0x4b && entries[1].channel == 3 && entries[1].duty == 100);

    /* An empty table is valid and clears the commanded set */
    CHECK(ParseSetDuties(Hex("01 00").data(), 2, entries));
    CHECK(entries.empty());

    /* Rejected: version, size, duplicate channel, channel range, duty range */
    Bytes bad = Hex("02 01 00 4b");
    CHECK(!ParseSetDuties(bad.data(), bad.size(), entries));
    bad = Hex("01 02 00 4b");
    CHECK(!ParseSetDuties(bad.data(), bad.size(), entries));
    bad = Hex("01 02 00 4b 00 4c");
    CHECK(!ParseSetDuties(bad.data(), bad.size(), entries));
    bad = Hex("01 01 08 4b");
    CHECK(!ParseSetDuties(bad.data(), bad.size(), entries));
    bad = Hex("01 01 00 65");
    CHECK(!ParseSetDuties(bad.data(), bad.size(), entries));
    CHECK(!ParseSetDuties(nullptr, 0, entries));
}

static void TestStatusPayload()
{
    CoolingStatus status;

    status.valid        = true;
    status.pid          = 0x0C1C;
    status.age_ms       = 250;
    status.connected    = {true, true, false};
    status.rpm          = {2357, 918, 0};
    status.duty         = {0x4b, NO_DUTY, NO_DUTY};
    status.temperatures = {{true, 123}, {false, 0}, {true, -15}};

    Bytes expected = Hex("01 01 1c 0c fa 00 03"
                         "01 35 09 4b"
                         "01 96 03 ff"
                         "00 00 00 ff"
                         "03"
                         "01 7b 00"
                         "00 00 00"
                         "01 f1 ff");

    CHECK(SerializeStatus(status) == expected);

    /* No data keeps the layout and clears the valid flag */
    status.valid = false;
    CHECK(SerializeStatus(status)[1] == 0x00);
}

static void TestPumpAndHandBackTables()
{
    std::vector<DutyEntry> entries = {{0, 10}, {1, 20}};

    ApplyPumpFloor(entries, true);
    CHECK(entries[0].duty == PUMP_FLOOR && entries[1].duty == 20);

    entries = {{0, 10}, {1, 20}};
    ApplyPumpFloor(entries, false);
    CHECK(entries[0].duty == 10);

    std::vector<bool> connected = {true, true, false, true};
    std::vector<DutyEntry> table = BuildUniformTable(connected, true, RELEASE_PUMP_DUTY, RELEASE_FAN_DUTY);
    CHECK(table.size() == 3);
    CHECK(Trimmed(BuildSoftwareDutyTable(table)) == Hex("03 00 00 46 00 01 00 32 00 03 00 32"));

    table = BuildUniformTable(connected, false, FAILSAFE_DUTY, FAILSAFE_DUTY);
    CHECK(Trimmed(BuildSoftwareDutyTable(table)) == Hex("03 00 00 64 00 01 00 64 00 03 00 64"));
}

int main()
{
    TestPumpAndHandBackTables();
    TestParseSpeeds();
    TestParseConnected();
    TestParseTemperatures();
    TestSoftwareDutyTable();
    TestTransactionCommands();
    TestSetDuties();
    TestStatusPayload();

    if(failures == 0)
    {
        printf("all passed\n");
    }

    return(failures == 0 ? 0 : 1);
}
