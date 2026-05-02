// SPDX-License-Identifier: MIT
/*!
 * Copyright (C) 2020-2023, duagon Germany GmbH
 *
 * \file      13Y063.c
 * \brief     Tool to control duagon boards with LTE (e.g. F229) or 5G modems
 *            (e.g. ME10, G239) using HID commands.
 * \author    Bernhard Hoeher <bernhard.hoeher@duagon.com>
 */

/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*--------------------------------------+
| INCLUDES                              |
+--------------------------------------*/
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON/cJSON.h"
#include "hidapi.h"

/*--------------------------------------+
|    DEFINES                            |
+--------------------------------------*/
#ifdef _WIN32
#define TOOL "13Y063-70"
#elif __linux__
#define TOOL "13Y063-90"
#endif

#define VERSION "_02_02"

/* Supported boards */
#define BOARD_NAME_F229 "F229"
#define BOARD_NAME_ME10 "ME10"
#define BOARD_NAME_G239 "G239"

/* NOT supported boards */
#define BOARD_NAME_G227 "G227"

#define FW_USB_VID 0x1b02

#define FW_USB_PID_F229 0x0004
#define FW_USB_PID_ME10 0x0005
#define FW_USB_PID_G239 0x0006

/* Show deprecation warning for <= FW_VERSION_WARNING */
#define FW_VERSION_WARNING_F229 101
#define FW_VERSION_WARNING_ME10 202
#define FW_VERSION_WARNING_G239 202

/* Previously ME10 and G239 were both using 0x004 as USB PID */
#define FW_USB_PID_ME10_G239_OBSOLETE 0x0004

/* USB Command Interface */
#define MODEM_1_SIM_REPORT_ID             1
#define MODEM_2_SIM_REPORT_ID             2
#define I2S_OR_TP_REPORT_ID               3
#define CONF_DATA_REPORT_ID               4
#define LED_1_REPORT_ID                   5
#define LED_2_REPORT_ID                   6
#define LED_3_REPORT_ID                   7
#define TIME_PULSE_REPORT_ID              8
#define MODULE_POWER_REPORT_ID            9
#define MODEM_OFF_TIME_REPORT_ID          10
#define BOARD_TEMPERATURE_REPORT_ID       11
#define MODEM_WWAN_GNSS_DISABLE_REPORT_ID 12
#define MODEM_RESET_REPORT_ID             14
#define BOARD_POWERGOOD_SIGNAL_REPORT_ID  15
#define BOOTLOADER_REPORT_ID              99

/* Misc */
#define CHAR_BUFFER_SIZE 50

/* Temperature values can be negative.
   So, we choose out of range negative
   values to indicate failure of teperature
   read.
*/
#define TEMPERATURE_ADC_INIT_ERROR -125
#define TEMPERATURE_ADC_MEAS_FAIL  -126
#define TEMPERATURE_ADC_BAD_VALUE  -127

/* Check if optional argument is present */
#define HAS_OPTIONAL_ARG (!optarg && NULL != argv[optind] && '-' != argv[optind][0])

/*--------------------------------------+
|    TYPEDEFS                           |
+--------------------------------------*/

/*--------------------------------------+
|    GLOBALS                            |
+--------------------------------------*/
static const char* const   short_options  = ":hli:g:n:p::s::f::cte::r:w::TGuj";
static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"list", no_argument, NULL, 'l'},
    {"index", required_argument, NULL, 'i'},
    {"geo", required_argument, NULL, 'g'},
    {"serial", required_argument, NULL, 'n'},
    {"power", optional_argument, NULL, 'p'},
    {"sim", optional_argument, NULL, 's'},
    {"off-time", optional_argument, NULL, 'f'},
    {"show-config", no_argument, NULL, 'c'},
    {"save-config", no_argument, NULL, 'S'},
    {"timepulse", no_argument, NULL, 't'},
    {"led", optional_argument, NULL, 'e'},
    {"reset", required_argument, NULL, 'r'},
    {"wwan", optional_argument, NULL, 'w'},
    {"temperature", no_argument, NULL, 'T'},
    {"powergood", no_argument, NULL, 'G'},
    {"update", no_argument, NULL, 'u'},
    {"output-json", no_argument, NULL, 'j'},
    {NULL, 0, NULL, 0},
};

const char*    g_board_name = NULL;
unsigned char  g_fw_version;
const char*    valid_board_names[] = {BOARD_NAME_F229, BOARD_NAME_ME10, BOARD_NAME_G239};
unsigned short valid_usb_pids[]    = {FW_USB_PID_F229, FW_USB_PID_ME10, FW_USB_PID_G239};

/*--------------------------------------+
|    PROTOTYPES                         |
+--------------------------------------*/
static void
usage();
static int
GetSimCount();
static int
GetModulePower(
    hid_device* hdev,
    bool        print_json
);
static int
SetModulePower(
    hid_device*   hdev,
    unsigned char module,
    unsigned char powerState
);
static int
GetModemSim(
    hid_device* hdev,
    bool        print_json
);
static int
SetModemSim(
    hid_device*   hdev,
    unsigned char modem,
    unsigned char sim
);
static int
GetModemOffTime(
    hid_device* hdev,
    bool        print_json
);
static int
SetModemOffTime(
    hid_device*  hdev,
    unsigned int offTime
);
static int
GetConfigurationData(
    hid_device* hdev,
    bool        print_json
);
static int
SetConfigurationData(hid_device* hdev);
static int
GetTimePulse(
    hid_device* hdev,
    bool        print_json
);
static int
GetLedStatus(
    hid_device* hdev,
    bool        print_json
);
static int
SetLedOnOff(
    hid_device*   hdev,
    unsigned char led,
    unsigned char powerState
);
static int
SetLedBlinking(
    hid_device*   hdev,
    unsigned char led,
    unsigned int  period,
    unsigned char dutyCycle
);
static int
GetBoardTemperature(
    hid_device* hdev,
    bool        print_json
);
static int
SetWWANSignal(
    hid_device*   hdev,
    unsigned char modem,
    unsigned char signal,
    unsigned char disable
);
static int
GetWWANSignal(
    hid_device* hdev,
    bool        print_json
);
static int
ResetModem(
    hid_device*   hdev,
    unsigned char modem
);
static int
GetPowerGoodStatus(
    hid_device* hdev,
    bool        print_json
);
static void
SetBootloaderMode(hid_device* hdev);

static int
PrintDeviceList(
    struct hid_device_info* hdev_info,
    bool                    print_json
);
static int
CheckAvailableDevices(
    struct hid_device_info* hdev_info,
    int*                    p_count
);
static void
PrintCompatibilityWarnings(struct hid_device_info* hdev_info);
static int
ParseBoardnameFromProductString(
    const wchar_t* w_product_string,
    const char**   board_name
);

static int
json_obj_print_and_delete(cJSON* json_obj);
static unsigned int
GetFwVersionAsInt(unsigned short release_number);
static bool
IsSupportedPID(unsigned short pid);
static int
GetSimCount();

/**
 *  \brief  Helper function to print the content of a json object and delete it afterwards.
 *  \param  json_obj the cJSON object
 *  \return 0 on success, 1 on error
 */
static int
json_obj_print_and_delete(cJSON* json_obj)
{
    char* json_string = NULL;
    if ((json_string = cJSON_Print(json_obj)) == NULL)
    {
        fprintf(stderr, "Error: json encoding failed\n");
        return 1;
    }
    printf("%s\n", json_string);
    free(json_string);
    cJSON_Delete(json_obj);
    return 0;
}

/**
 *  \brief  Helper function to get combined firmware version as int.
 *          The lowest two digits represent the minor version number.
 *  \param  release_number as reported by USB HID device
 *  \return Firmware version as continuous integer
 */
static unsigned int
GetFwVersionAsInt(unsigned short release_number)
{
    unsigned char ver_maj, ver_min;
    ver_maj = release_number >> 8;
    ver_min = release_number & 0xFF;

    return ver_maj * 100 + ver_min;
}

/**
 *  \brief  Helper function to check if a given PID is supported.
 *  \return True if supported
 */
static bool
IsSupportedPID(unsigned short pid)
{
    size_t len = (sizeof(valid_usb_pids) / sizeof(valid_usb_pids[0]));

    for (size_t i = 0; i < len; i++)
    {
        if (valid_usb_pids[i] == pid)
        {
            return true;
        }
    }
    return false;
}

/**
 *  \brief  Helper function to get max. supported SIM cards for the hardware model in use.
 *  \return Number of supported SIM cards
 */
static int
GetSimCount()
{
    if (!strcmp(g_board_name, BOARD_NAME_ME10))
        return 6;
    else
        return 10;
}

/**
 *  \brief  Print program usage.
 */
static void
usage()
{
    printf(
        "===============================================\n"
        "==              %s%s              ==\n"
        "===============================================\n"
        "(c) Copyright by duagon AG\n\n"
        "== This tool controls the F229, ME10 and G239 firmware ==\n\n"
        "This program must be run as super user\n"
        "\n"
        "Usage: %s [OPTIONS]\n"
        "Options:\n"
        " -h, --help                 Show this help message\n"
        " -l, --list                 List devices\n"
        " -i, --index <IDX>          Select device by index in list\n"
        " -g, --geo <ADDR>           Select device by geographical address (hexadecimal)\n"
        "                            ADDR: Geographical address of device\n"
        " -n, --serial <SN>          Select device by serial number (decimal).\n"
        "                            (Not supported on ME10.)\n"
        "                            SN: Serial number of device\n"
        " -p, --power                Get module (Modem or PCI Mini Card) power\n"
        " -p, --power <MOD> <PWR>    Set module power\n"
        "                            MOD: 1 .. 3\n"
        "                            PWR: 0 (off) or 1 (on)\n\n"
        "        Note: In case of powering off of a module:\n"
        "            1. Module is signaled to shut down.\n"
        "            2. Wait for modem to shut down or 'modem off time' to elapse (see option -f)\n"
        "            3. Power (Vcc) is switched off.\n\n"
        "        Note: PCI Mini Cards (PMC) are not hotplug capable.\n"
        "            That means, if they are switched off and then switched on, \n"
        "            they can not be detected by the operating system. In that case \n"
        "            you need to reboot to use the PMC again.\n"
        "            Modems use the USB interface and therefore have hotplug capability.\n"
        "            Their detection does not require reboot.\n\n"
        " -s, --sim                  Get modems sim\n"
        " -s, --sim <MOD> <SIM>      Set modem sim\n"
        "                            MOD: 1 .. 2\n"
        "                            SIM: 0 (none), 1 .. N (ME10: N = 6, others: N = 10)\n"
        " -f, --off-time             Get modems off time (Modem off time is minimum wait\n"
        "                            in milliseconds (ms) before switching the module OFF\n"
        "                            to ensure a safe shutdown.)\n"
        " -f, --off-time <TIME>      Set modems off time\n"
        "                            TIME: 0 .. 65535 ms\n"
        " -c, --show-config          Show configuration data stored in HW\n"
        "     --save-config          Save current configuration to HW\n"
        " -t, --timepulse            Get time pulse counter\n"
        " -e, --led                  Get LEDs status\n"
        " -e, --led <LED> <MODE> [<PERIOD> [<DUTYCYCLE>]]  Set LED status\n"
        "                            For MODE=2 at least PERIOD is required.\n"
        "                            LED: 1 .. 3\n"
        "                            MODE: 0 (off), 1 (on) or 2 (blinking)\n"
        "                            PERIOD: 0 .. 65535 ms\n"
        "                            DUTYCYCLE: 0 .. 100 %%\n"
        " -r, --reset <MOD>          Reset modem (Not supported on F229)\n"
        "                            MOD: 1 .. 2\n"
        "        Note: The feature might not be supported by all modems.\n"
        "              Please check modem specification.\n\n"
        " -w, --wwan                 Get WWAN and GNSS state of modem\n"
        " -w, --wwan <MOD> <SIGNAL> <STATE>  Disables or enables WWAN or GNSS of modem\n"
        "                            (Not supported on F229)\n"
        "                            MOD: 1 .. 2\n"
        "                            SIGNAL: 0 (WWAN), 1 (GNSS)\n"
        "                            STATE: 0 (disable), 1 (enable)\n"
        " -T, --temperature          Get board temperature (Not supported on F229)\n"
        " -G, --powergood            Get status of powergood signal of board\n"
        "                            (Not supported on F229)\n"
        " -u, --update-mode          Set device into DFU bootloader mode for firmware update\n"
        " -j, --output-json          Print command output in machine readable JSON format\n",
        TOOL,
        VERSION,
        TOOL
    );
    exit(1);
}

/****************************************************************************/
/** GetModulePower
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetModulePower(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[4] = {MODULE_POWER_REPORT_ID, 0, 0, 0};
    int           ret;
    int           i;

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    if (print_json)
    {
        cJSON *json = NULL, *json_array;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_module_power");
        json_array = cJSON_AddArrayToObject(json, "module");
        for (i = 1; i < 4; i++)
        {
            cJSON* json_element = cJSON_CreateObject();
            cJSON_AddNumberToObject(json_element, "module_number", i);
            cJSON_AddBoolToObject(json_element, "is_enabled", (buf[i] ? 1 : 0));
            cJSON_AddItemToArray(json_array, json_element);
        }
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        for (i = 1; i < 4; i++)
        {
            printf("Module #%d is %s\n", i, buf[i] ? "enabled" : "disabled");
        }
    }

    return 0;
}

/**
 *  \brief  Set power state of a module.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  module      Module index
 *  \param  powerState  Power state
 *
 *  \return 0 on success, -1 on error
 */
static int
SetModulePower(
    hid_device*   hdev,
    unsigned char module,
    unsigned char powerState
)
{
    unsigned char bufwrite[3] = {MODULE_POWER_REPORT_ID, 0, 0};
    unsigned char bufread[4]  = {MODULE_POWER_REPORT_ID, 0, 0, 0};
    int           ret;

    if ((module < 1) || (module > 3))
    {
        fprintf(stderr, "Module #%d doesn't exist\n", module);
        return -1;
    }
    bufwrite[1] = module;

    if ((powerState != 0) && (powerState != 1))
    {
        fprintf(stderr, "Invalid module power mode\n");
        return -1;
    }
    bufwrite[2] = powerState;

    /* Read power state */
    ret = hid_get_feature_report(hdev, bufread, sizeof(bufread));
    if (ret < 0)
    {
        return ret;
    }

    /* Current state equals requested state */
    if (bufread[module] == powerState)
    {
        printf("Module #%d already powered-%s. Nothing to do.\n", module, powerState ? "on" : "off");
        return 0;
    }

    /* Set power state */
    ret = hid_send_feature_report(hdev, bufwrite, sizeof(bufwrite));
    if (ret < 0)
    {
        return ret;
    }

    printf("Power-%s request for Module #%d sent.\n", powerState ? "on" : "off", module);
    printf("Wait for %s of Module.\n", powerState ? "startup" : "shutdown");

    /* Backwards compatibility - Workarounds for known issues in FW versions <= 2.02 */
    if (g_fw_version <= 202)
    {
        if (powerState)
        {
            /* Block for a short time after sending 'power-on' to slow down consecuitive calls.
             * The old FW discards any calls before the current one is handled.
             */
            usleep(1000 * 1000);
        }
        else
        {
            /* Block for modem_off_time before continuing because old FW is not verifying the power-down.
             * The normal read-back loop below will show success immediately.
             */
            unsigned char buf[3] = {MODEM_OFF_TIME_REPORT_ID, 0, 0};
            unsigned int  offTime;
            ret = hid_get_feature_report(hdev, buf, sizeof(buf));
            if (ret < 0) return ret;
            offTime = (buf[1] << 8) | buf[2];

            printf("Waiting for 'modem off time' of %u ms.\n", offTime);
            usleep(offTime * 1000);
        }
    }

    /* Block until modem is powered on/off successfully.
     * This takes at maximum the set "modems_off_time". */
    do
    {
        /* Read back power state */
        ret = hid_get_feature_report(hdev, bufread, sizeof(bufread));
        if (ret < 0)
        {
            return ret;
        }
        /* Check success */
        if (bufread[module] == powerState)
        {
            break;
        }
        usleep(1000 * 1000);
    } while (1);

    printf("Power for Module #%d reported as %s.\n", module, powerState ? "on" : "off");

    return 0;
}

/**
 *  \brief  Print modem <-> SIM assignments.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetModemSim(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[2];

    cJSON *json = NULL, *json_array, *json_element;
    if (print_json)
    {
        json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_modem_sim");
        json_array = cJSON_AddArrayToObject(json, "led");
    }

    for (int modem = 1; modem <= 2; modem++)
    {
        unsigned char sim;

        if (modem == 1)
            buf[0] = MODEM_1_SIM_REPORT_ID;
        else if (modem == 2)  // cppcheck-suppress knownConditionTrueFalse
            buf[0] = MODEM_2_SIM_REPORT_ID;
        else
        {
            fprintf(stderr, "Modem #%d doesn't exist\n", modem);
            return -1;
        }

        int ret = hid_get_feature_report(hdev, buf, sizeof(buf));
        if (ret < 0) return ret;
        sim = buf[1];

        if (print_json)
        {
            json_element = cJSON_CreateObject();
            cJSON_AddItemToArray(json_array, json_element);
            cJSON_AddNumberToObject(json_element, "modem_number", modem);
            cJSON_AddBoolToObject(json_element, "use_sim_card", ((sim == 0) ? 0 : 1));
            cJSON_AddNumberToObject(json_element, "sim_card_number", sim);
        }
        else
        {
            printf("Modem #%d uses ", modem);
            if (sim == 0)
                printf("no SIM card\n");
            else
                printf("SIM card #%d\n", sim);
        }
    }

    if (print_json)
    {
        if (json_obj_print_and_delete(json)) return -1;
    }

    return 0;
}

/**
 *  \brief  Assign SIM card to modem.
 *
 *  \param  hdev    Handle to the HID USB device
 *  \param  modem   Modem index
 *  \param  sim     SIM card index
 *
 *  \return 0 on success, -1 on error
 */
static int
SetModemSim(
    hid_device*   hdev,
    unsigned char modem,
    unsigned char sim
)
{
    unsigned char buf[2];
    int           ret;

    if ((modem != 1) && (modem != 2))
    {
        fprintf(stderr, "Modem #%d doesn't exist\n", modem);
        return -1;
    }

    if (sim > GetSimCount())
    {
        fprintf(stderr, "Invalid SIM card\n");
        return -1;
    }

    /* Check SIM card is not used by other modem */
    if (sim != 0)
    {
        buf[0] = (modem == 1) ? MODEM_2_SIM_REPORT_ID : MODEM_1_SIM_REPORT_ID;
        ret    = hid_get_feature_report(hdev, buf, sizeof(buf));
        if (ret < 0) return ret;
        if (sim == buf[1])
        {
            fprintf(stderr, "SIM card #%d already used by modem #%d\n", sim, buf[0]);
            return -1;
        }
    }
    buf[0] = (modem == 1) ? MODEM_1_SIM_REPORT_ID : MODEM_2_SIM_REPORT_ID;
    buf[1] = sim;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("Modem #%d set with ", modem);
    if (sim == 0)
        printf("no SIM card\n");
    else
        printf("SIM card #%d\n", sim);

    return 0;
}

/**
 *  \brief  Print modem off time.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetModemOffTime(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[3] = {MODEM_OFF_TIME_REPORT_ID, 0, 0};
    unsigned int  offTime;
    int           ret;

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;
    offTime = (buf[1] << 8) | buf[2];

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_modem_off_time");
        cJSON_AddNumberToObject(json, "modem_off_time_ms", offTime);
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Modems off time is %u ms\n", offTime);
    }

    return 0;
}

/**
 *  \brief  Set modem off time.
 *
 *  \param  hdev     Handle to the HID USB device
 *  \param  offTime  Modems off time in ms
 *
 *  \return 0 on success, -1 on error
 */
static int
SetModemOffTime(
    hid_device*  hdev,
    unsigned int offTime
)
{
    unsigned char buf[3] = {MODEM_OFF_TIME_REPORT_ID, 0, 0};
    int           ret;

    if (offTime > 0xffff)
    {
        fprintf(stderr, "Modems off time too large\n");
        return -1;
    }
    buf[1] = offTime >> 8;
    buf[2] = offTime;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("Modems off time set to %u ms\n", offTime);
    return 0;
}

/**
 *  \brief  Print configuration data.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetConfigurationData(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[9] = {CONF_DATA_REPORT_ID, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sim1, sim2;
    unsigned int  offTime;
    int           ret;

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;
    sim1    = buf[4];
    sim2    = buf[5];
    offTime = (buf[6] << 8) | buf[7];

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_config_data");
        cJSON_AddBoolToObject(json, "modem_1_is_enabled", (buf[1] ? 1 : 0));
        cJSON_AddBoolToObject(json, "modem_1_sim_card_is_connected", (sim1 ? 1 : 0));
        cJSON_AddNumberToObject(json, "modem_1_sim_card_number", sim1);
        cJSON_AddBoolToObject(json, "modem_2_is_enabled", (buf[2] ? 1 : 0));
        cJSON_AddBoolToObject(json, "modem_2_sim_card_is_connected", (sim2 ? 1 : 0));
        cJSON_AddNumberToObject(json, "modem_2_sim_card_number", sim2);
        cJSON_AddBoolToObject(json, "module_3_is_enabled", (buf[3] ? 1 : 0));
        cJSON_AddNumberToObject(json, "modem_off_time_ms", offTime);
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Configuration data used at startup:\n\n");

        /* Modem #1 */
        printf("Modem  #1 is %s with ", buf[1] ? "enabled " : "disabled");
        if (sim1 == 0)
            printf("no SIM card\n");
        else
            printf("SIM card #%d\n", sim1);

        /* Modem #2 */
        printf("Modem  #2 is %s with ", buf[2] ? "enabled " : "disabled");
        if (sim2 == 0)
            printf("no SIM card\n");
        else
            printf("SIM card #%d\n", sim2);

        /* Module #3 */
        printf("Module #3 is %s\n", buf[3] ? "enabled " : "disabled");

        /* Modems off time */
        printf("Modems are off for %u ms when setting SIM card\n", offTime);
    }

    return 0;
}

/**
 *  \brief  Save configuration data to flash.
 *
 *  \param  hdev    Handle to the HID USB device
 *
 *  \return 0 on success, -1 on error
 */
static int
SetConfigurationData(hid_device* hdev)
{
    unsigned char buf[2] = {CONF_DATA_REPORT_ID, 0};
    int           ret;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("Configuration data saved in flash\n");

    return 0;
}

/**
 *  \brief  Print time pulse counter.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetTimePulse(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[5] = {TIME_PULSE_REPORT_ID, 0, 0, 0, 0};
    unsigned int  timePulse;
    int           ret;
    buf[0] = TIME_PULSE_REPORT_ID;
    ret    = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;
    timePulse = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_time_pulse_counter");
        cJSON_AddNumberToObject(json, "time_pulse_counter", timePulse);
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Time Pulse counter = %u\n", timePulse);
    }

    return 0;
}

/**
 *  \brief  Print LED status.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetLedStatus(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[5];

    cJSON *json = NULL, *json_array, *json_element;
    if (print_json)
    {
        json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_led_status");
        json_array = cJSON_AddArrayToObject(json, "led");
    }

    for (int led = 1; led <= 3; led++)
    {
        unsigned char mode;
        unsigned char dutyCycle;
        unsigned int  period;
        int           ret;

        if (led == 1)
            buf[0] = LED_1_REPORT_ID;
        else if (led == 2)
            buf[0] = LED_2_REPORT_ID;
        else if (led == 3)
            buf[0] = LED_3_REPORT_ID;
        else
        {
            fprintf(stderr, "LED #%d doesn't exist\n", led);
            return -1;
        }

        ret = hid_get_feature_report(hdev, buf, sizeof(buf));
        if (ret < 0) return ret;
        mode      = buf[1];
        period    = (buf[2] << 8) | buf[3];
        dutyCycle = buf[4];

        if (print_json)
        {
            json_element = cJSON_CreateObject();
            cJSON_AddItemToArray(json_array, json_element);
            cJSON_AddNumberToObject(json_element, "led_number", led);
            cJSON_AddNumberToObject(json_element, "led_mode", mode);
            cJSON_AddNumberToObject(json_element, "blink_period_ms", ((mode == 2) ? period : 0));
            cJSON_AddNumberToObject(json_element, "blink_duty_cycle", ((mode == 2) ? dutyCycle : 0));
        }
        else
        {
            if (mode == 2)
                printf("LED #%d is blinking: period = %u ms and duty cycle = %u %%\n", led, period, dutyCycle);
            else
                printf("LED #%d is %s\n", led, mode == 1 ? "on" : "off");
        }
    }

    if (print_json)
    {
        if (json_obj_print_and_delete(json)) return -1;
    }

    return 0;
}

/**
 *  \brief  Turn LED ON or OFF.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  led         LED index
 *  \param  powerState  Power state
 *
 *  \return 0 on success, -1 on error
 */
static int
SetLedOnOff(
    hid_device*   hdev,
    unsigned char led,
    unsigned char powerState
)
{
    unsigned char buf[5] = {0};
    int           ret;

    if (led == 1)
        buf[0] = LED_1_REPORT_ID;
    else if (led == 2)
        buf[0] = LED_2_REPORT_ID;
    else if (led == 3)
        buf[0] = LED_3_REPORT_ID;
    else
    {
        fprintf(stderr, "LED #%d doesn't exist\n", led);
        return -1;
    }

    if ((powerState != 0) && (powerState != 1))
    {
        fprintf(stderr, "Invalid LED power mode\n");
        return -1;
    }
    buf[1] = powerState;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("LED #%d %s\n", led, powerState ? "enabled" : "disabled");

    return 0;
}

/**
 *  \brief  Set LED to blinking mode.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  led         LED index
 *  \param  period      Blinking period in ms
 *  \param  dutyCycle   Blinking duty cycle in %
 *
 *  \return 0 on success, -1 on error
 */
static int
SetLedBlinking(
    hid_device*   hdev,
    unsigned char led,
    unsigned int  period,
    unsigned char dutyCycle
)
{
    unsigned char buf[5];
    int           ret;

    if (led == 1)
        buf[0] = LED_1_REPORT_ID;
    else if (led == 2)
        buf[0] = LED_2_REPORT_ID;
    else if (led == 3)
        buf[0] = LED_3_REPORT_ID;
    else
    {
        fprintf(stderr, "LED #%d doesn't exist\n", led);
        return -1;
    }
    buf[1] = 2; /* Blinking mode */

    if (period > 0xffff)
    {
        fprintf(stderr, "LED blinking period too large\n");
        return -1;
    }
    buf[2] = period >> 8;
    buf[3] = period;

    if (dutyCycle > 100)
    {
        fprintf(stderr, "LED blinking duty cycle too large\n");
        return -1;
    }
    buf[4] = dutyCycle;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("LED #%d set to blinking mode: period = %u ms and duty cycle = %u %%\n", led, period, dutyCycle);

    return 0;
}

/**
 *  \brief  Print board temperature.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetBoardTemperature(
    hid_device* hdev,
    bool        print_json
)
{
    char buf[2] = {BOARD_TEMPERATURE_REPORT_ID, 0};
    char temperature;
    int  ret;

    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        fprintf(stderr, "This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;
    temperature = buf[1];

    /* In ME10 and G239 firmware, if board temperature cannot be read
       then -127 value is returned as an indication of an error. */
    if (temperature == TEMPERATURE_ADC_INIT_ERROR)
    {
        fprintf(stderr, "Error: Temperature ADC of the board has not been initialized yet\n");
        return -1;
    }
    else if (temperature == TEMPERATURE_ADC_MEAS_FAIL)
    {
        fprintf(stderr, "Error: Temperature ADC has not measured any temperature.\n");
        return -1;
    }
    else if (temperature == TEMPERATURE_ADC_BAD_VALUE)
    {
        fprintf(stderr, "Error: Temperature ADC has incorrectly measured value\n");
        return -1;
    }

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_board_temperature");
        cJSON_AddNumberToObject(json, "temperature_degC", temperature);
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Board Temperature = %d°C\n", temperature);
    }

    return 0;
}

/**
 *  \brief  Set enable state of WWAN or GNSS.
 *
 *  \param  hdev    Handle to the HID USB device
 *  \param  modem   Modem index
 *  \param  signal  Signal index (WWAN | GNSS)
 *  \param  state   New enable state
 *
 *  \return 0 on success, -1 on error
 */
static int
SetWWANSignal(
    hid_device*   hdev,
    unsigned char modem,
    unsigned char signal,
    unsigned char state
)
{
    unsigned char buf[4];
    int           ret;

    if (modem > 2 || modem == 0 || signal > 1 || state > 1) return -1;

    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        fprintf(stderr, "This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    buf[0] = MODEM_WWAN_GNSS_DISABLE_REPORT_ID;
    buf[1] = modem;
    buf[2] = signal;
    buf[3] = state;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    printf("Successfully set %s of modem%d to %s\n", signal ? "GNSS" : "WWAN", modem, state ? "enable" : "disable");
    return 0;
}

/**
 *  \brief  Print enable state of WWAN and GNSS.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetWWANSignal(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[5];
    int           ret;

    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        fprintf(stderr, "This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    buf[0] = MODEM_WWAN_GNSS_DISABLE_REPORT_ID;

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0)
    {
        fprintf(stderr, "Error in getting signal\n");
        return ret;
    }

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_wwan");
        cJSON_AddBoolToObject(json, "modem_1_wwan_is_enabled", (buf[1] ? 1 : 0));
        cJSON_AddBoolToObject(json, "modem_1_gnss_is_enabled", (buf[2] ? 1 : 0));
        cJSON_AddBoolToObject(json, "modem_2_wwan_is_enabled", (buf[3] ? 1 : 0));
        cJSON_AddBoolToObject(json, "modem_2_gnss_is_enabled", (buf[4] ? 1 : 0));
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Modem1: WWAN:%s and GNSS:%s\n", buf[1] ? "Enabled" : "Disabled", buf[2] ? "Enabled" : "Disabled");
        printf("Modem2: WWAN:%s and GNSS:%s\n", buf[3] ? "Enabled" : "Disabled", buf[4] ? "Enabled" : "Disabled");
    }

    return 0;
}

/**
 *  \brief  Reset given modem via reset pin.
 *
 *  \param  hdev    Handle to the HID USB device
 *  \param  modem   Modem index
 *
 *  \return 0 on success, -1 on error
 */
static int
ResetModem(
    hid_device*   hdev,
    unsigned char modem
)
{
    unsigned char buf[2];
    int           ret;

    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        fprintf(stderr, "This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    if (modem > 2 || modem == 0) return -1;

    buf[0] = MODEM_RESET_REPORT_ID;
    buf[1] = modem;

    ret = hid_send_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    /* Backwards compatibility - Block for a short time to slow down consecutive reset calls.
     * The old FW discards any calls before the current one is handled.
     */
    if (g_fw_version <= 202)
    {
        usleep(200 * 1000);
    }

    printf("Modem %d has been reset successfully\n", modem);
    return 0;
}

/**
 *  \brief  Print status of power good pin.
 *
 *  \param  hdev        Handle to the HID USB device
 *  \param  print_json  Flag to print output as JSON
 *
 *  \return 0 on success, -1 on error
 */
static int
GetPowerGoodStatus(
    hid_device* hdev,
    bool        print_json
)
{
    unsigned char buf[2] = {BOARD_POWERGOOD_SIGNAL_REPORT_ID, 0};
    int           ret;

    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        fprintf(stderr, "This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    ret = hid_get_feature_report(hdev, buf, sizeof(buf));
    if (ret < 0) return ret;

    if (print_json)
    {
        cJSON* json = NULL;
        json        = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "command", "get_power_good_status");
        cJSON_AddBoolToObject(json, "power_good_status_is_set", (buf[1] ? 1 : 0));
        if (json_obj_print_and_delete(json)) return -1;
    }
    else
    {
        printf("Powergood signal status = %d\n", buf[1]);
    }

    return 0;
}

/**
 *  \brief  Set STM32 to internal DFU bootloader - used for updating.
 *
 *  \param  hdev    Handle to the HID USB device
 */
static void
SetBootloaderMode(hid_device* hdev)
{
    unsigned char buf[2] = {BOOTLOADER_REPORT_ID, 0};

    /* We will never receive 0 as return value for this call. This is
     * because, when firmare receives BOOTLOADER_REPORT_ID, it will
     * jump to internal bootloader of STM32 wihout returning any value.
     * Therefore, do not consider return value and inform user to check
     * if STM32 is available as DFU device i.e. if it entered in internal
     * DFU bootloader of STM32.
     */
    hid_send_feature_report(hdev, buf, sizeof(buf));

    printf(
        "Succesfully set bootloader mode. Please execute \"dfu-util -l\" and check if device is "
        "available as DFU device for firmare update. \n"
    );
}

/**
 *  \brief  Check if devices can be accessed
 *
 *  \param  hdev_info   Handle to the HID USB device list
 *  \param  print_json  Flag to print output as JSON
 */
static int
CheckAvailableDevices(
    struct hid_device_info* hdev_info,
    int*                    p_count
)
{
    const struct hid_device_info* p_hdev_info;
    int                           count = 0;
    for (p_hdev_info = hdev_info; p_hdev_info != NULL; p_hdev_info = p_hdev_info->next)
    {
        count++;
        if (!p_hdev_info->product_string)
        {
            fprintf(stderr, "Error: Found some device(s), but did not get a product string.\n");
            fprintf(stderr, "On linux this program must be run as super user.\n");
            fprintf(
                stderr,
                "Check if all drivers/dependencies were correctly installed according to the "
                "documentation\n"
            );
            return -1;
        }

        if (!p_hdev_info->serial_number)
        {
            fprintf(stderr, "Error: Found some device(s), but did not get a serial number string.\n");
            fprintf(stderr, "On linux this program must be run as super user.\n");
            fprintf(
                stderr,
                "Check if all drivers/dependencies were correctly installed according to the "
                "documentation\n"
            );
            return -1;
        }
    }
    *p_count = count;
    return 0;
}

/**
 *  \brief  Print a list of supported devices found.
 *
 *  \param  hdev_info   Handle to the HID USB device list
 *  \param  print_json  Flag to print output as JSON
 */
static int
PrintDeviceList(
    struct hid_device_info* hdev_info,
    bool                    print_json
)
{
    const struct hid_device_info* p_hdev_info;
    char                          buf[CHAR_BUFFER_SIZE] = "noserial";
    int                           count                 = 0;
    uint32_t                      serial_nr;
    unsigned int                  geographical_address;

    cJSON *json = NULL, *json_array;
    json        = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "command", "list_devices");
    json_array = cJSON_AddArrayToObject(json, "devices");

    if (!print_json)
    {
        printf("\nAvailable %s:\n", g_board_name);
    }

    for (p_hdev_info = hdev_info; p_hdev_info != NULL; p_hdev_info = p_hdev_info->next)
    {
        unsigned char ver_maj;
        unsigned char ver_min;
        count++;

        cJSON* json_element;
        json_element = cJSON_CreateObject();
        cJSON_AddItemToArray(json_array, json_element);
        cJSON_AddNumberToObject(json_element, "device_index", count);

        ver_maj = p_hdev_info->release_number >> 8;
        ver_min = p_hdev_info->release_number & 0xFF;

        wcstombs(buf, p_hdev_info->serial_number, CHAR_BUFFER_SIZE);
        sscanf(buf, "%x", &serial_nr);

        wcstombs(buf, p_hdev_info->duagon_geo_address, CHAR_BUFFER_SIZE);
        sscanf(buf, "%x", &geographical_address);

        if (print_json)
        {
            wcstombs(buf, p_hdev_info->product_string, CHAR_BUFFER_SIZE);
            cJSON_AddStringToObject(json_element, "product_string", buf);
            cJSON_AddNumberToObject(json_element, "firmware_version_major", ver_maj);
            cJSON_AddNumberToObject(json_element, "firmware_version_minor", ver_min);
            cJSON_AddNumberToObject(json_element, "geographical_address", geographical_address);
        }

        if (!strcmp(g_board_name, BOARD_NAME_ME10))
        {
            if (!print_json)
            {
                printf(
                    "%d) %ls: Firmware Version: %x.%.2x, Geographical Address: 0x%ls\n",
                    count,
                    p_hdev_info->product_string,
                    ver_maj,
                    ver_min,
                    p_hdev_info->duagon_geo_address
                );
            }
        }
        else
        {
            if (print_json)
            {
                cJSON_AddNumberToObject(json_element, "serial_number", serial_nr);
            }
            else
            {
                printf(
                    "%d) %ls: Firmware Version: %x.%.2x, Serial Number: %u (0x%ls), "
                    "Geographical Address: 0x%ls\n",
                    count,
                    p_hdev_info->product_string,
                    ver_maj,
                    ver_min,
                    serial_nr,
                    p_hdev_info->serial_number,
                    p_hdev_info->duagon_geo_address
                );
            }
        }
    }

    if (print_json)
    {
        if (json_obj_print_and_delete(json)) return -1;
    }

    return 0;
}

/**
 *  \brief  Look for known board names within the product string.
 *
 *  \param  w_product_string    Product string as reported by USB HID device
 *  \param  board_name          Pointer to write the found board name to
 */
static int
ParseBoardnameFromProductString(
    const wchar_t* w_product_string,
    const char**   board_name
)
{
    char product_string[255];

    if (w_product_string == NULL)
    {
        fprintf(stderr, "Error: No product string.\n");
        return 1;
    }

    wcstombs(product_string, w_product_string, sizeof(product_string));

    if (strstr(product_string, BOARD_NAME_F229) != NULL)
    {
        *board_name = valid_board_names[0];
    }
    else if (strstr(product_string, BOARD_NAME_ME10) != NULL)
    {
        *board_name = valid_board_names[1];
    }
    else if (strstr(product_string, BOARD_NAME_G239) != NULL)
    {
        *board_name = valid_board_names[2];
    }
    else if (strstr(product_string, BOARD_NAME_G227) != NULL)
    {
        fprintf(stderr, "Error: Board %s is not supported. Use Tool 13Y041 instead.\n", BOARD_NAME_G227);
        return 1;
    }
    else
    {
        fprintf(stderr, "Error: Unknown board.\n");
        return 99;
    }

    return 0;
}

/**
 *  \brief  Print warnings for legacy firmware versions.
 *
 *  \param  hdev_info   Handle to the HID USB device list
 */
static void
PrintCompatibilityWarnings(struct hid_device_info* hdev_info)
{
    const struct hid_device_info* p_hdev_info;
    int                           count = 0;
    const char*                   board_name;

    for (p_hdev_info = hdev_info; p_hdev_info != NULL; p_hdev_info = p_hdev_info->next)
    {
        unsigned int fw_ver;
        count++;

        fw_ver = GetFwVersionAsInt(p_hdev_info->release_number);
        ParseBoardnameFromProductString(p_hdev_info->product_string, &board_name);
        if ((!strcmp(board_name, BOARD_NAME_F229) && fw_ver <= FW_VERSION_WARNING_F229) ||
            (!strcmp(board_name, BOARD_NAME_ME10) && fw_ver <= FW_VERSION_WARNING_ME10) ||
            (!strcmp(board_name, BOARD_NAME_G239) && fw_ver <= FW_VERSION_WARNING_G239))
        {
            fprintf(
                stderr,
                "Warning: Firmware Version %u.%02u for %s is deprecated! Please consider updating.\n",
                fw_ver / 100,
                fw_ver % 100,
                board_name
            );
        }
    }
}

/**
 *  \brief  Program main function.
 *
 *  \param  argc    argument counter
 *  \param  argv    argument vector
 *
 *  \return 0 or error code
 */
int
main(
    int   argc,
    char* argv[]
)
{
    struct hid_device_info *hdev_info = NULL, *p_hdev_info = NULL;
    hid_device*             hdev                  = NULL;
    unsigned int            count                 = 0;
    uint32_t                serial_nr             = 0;
    char                    buf[CHAR_BUFFER_SIZE] = "noserial";
    int                     i                     = 0;
    int                     ret                   = -1;
    int c, flag_l = 0, flag_i = 0, flag_n = 0, flag_g = 0, flag_t = 0, flag_T = 0, flag_G = 0, flag_r = 0, flag_u = 0,
           flag_f = 0, flag_p = 0, flag_s = 0, flag_e = 0, flag_w = 0, flag_c = 0;
    bool print_json = 0;

    /* Arguments */
    int arg_i_index    = 0;
    int arg_g_geo_addr = 0;
    int arg_n_serial   = 0;
    int arg_p_module   = 0, arg_p_state;
    int arg_w_modem    = 0, arg_w_signal, arg_w_value;
    int arg_s_modem    = 0, arg_s_sim;
    int arg_f_off_time = -1;
    int arg_c_save_cfg = 0;
    int arg_r_modem    = 0;
    int arg_e_led      = 0, arg_e_mode, arg_e_period, arg_e_dutycycle;

    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
    {
        switch (c)
        {
            case 'h':
                usage();
                return -1;
            case 'l':
                flag_l = 1;
                break;
            case 'i':
                flag_i = 1;
                sscanf(optarg, "%d", &arg_i_index);
                if (arg_i_index <= 0)
                {
                    fprintf(stderr, "Invalid argument value '%s' for -%c\n", optarg, c);
                    goto error_exit;
                }
                break;
            case 'g':
                flag_g = 1;
                sscanf(optarg, "%x", (unsigned*)&arg_g_geo_addr);
                if (arg_g_geo_addr <= 0)
                {
                    fprintf(stderr, "Invalid argument value '%s' for -%c\n", optarg, c);
                    goto error_exit;
                }
                break;
            case 'n':
                flag_n = 1;
                sscanf(optarg, "%d", &arg_n_serial);
                if (arg_n_serial <= 0)
                {
                    fprintf(stderr, "Invalid argument value '%s' for -%c\n", optarg, c);
                    goto error_exit;
                }
                break;
            case 'p':
                flag_p = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_p_module);

                    /* Additional arguments */
                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <PWR> for -%c option\n", c);
                        goto error_exit;
                    }
                    arg_p_state = atoi(argv[optind++]);
                }
                break;
            case 's':
                flag_s = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_s_modem);

                    /* Additional arguments */
                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <SIM> for -%c option\n", c);
                        goto error_exit;
                    }
                    arg_s_sim = atoi(argv[optind++]);
                }
                break;
            case 'f':
                flag_f = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    if (!isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Invalid argument '%s' for -%c\n", argv[optind], c);
                        goto error_exit;
                    }
                    arg_f_off_time = atoi(argv[optind++]);
                }
                break;
            /* --save-config */
            case 'S':
                flag_c         = 1;
                arg_c_save_cfg = 1;
                break;
            case 'c':
                flag_c = 1;
                /* Legacy support - check for argument "-s" */
                if (optind < argc && !strcmp(argv[optind], "-s"))
                {
                    optind++;
                    arg_c_save_cfg = 1;
                    fprintf(
                        stderr,
                        "Warning: Option '-c -s' is deprecated. "
                        "Please use the --save-config option.\n"
                    );
                }
                break;
            case 't':
                flag_t = 1;
                break;
            case 'e':
                flag_e = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_e_led);

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <MODE> for -%c option\n", c);
                        goto error_exit;
                    }
                    arg_e_mode = atoi(argv[optind++]);

                    /* Extra arguments for blink mode */
                    if (arg_e_mode == 2)
                    {
                        if (optind >= argc || !isdigit(*argv[optind]))
                        {
                            fprintf(stderr, "Missing argument <PERIOD> for -%c option with LED mode=2\n", c);
                            goto error_exit;
                        }
                        arg_e_period = atoi(argv[optind++]);
                        if (optind >= argc || !isdigit(*argv[optind]))
                        {
                            arg_e_dutycycle = 50;
                        }
                        else
                        {
                            arg_e_dutycycle = atoi(argv[optind++]);
                        }
                    }
                }
                break;
            case 'T':
                flag_T = 1;
                break;
            case 'G':
                flag_G = 1;
                break;
            case 'w':
                flag_w = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_w_modem);

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <SIGNAL> for -%c option\n", c);
                        goto error_exit;
                    }
                    arg_w_signal = atoi(argv[optind++]);

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <STATE> for -%c option\n", c);
                        goto error_exit;
                    }
                    arg_w_value = atoi(argv[optind++]);
                }
                break;
            case 'r':
                flag_r = 1;
                if (!isdigit(*optarg))
                {
                    fprintf(stderr, "Invalid argument '%s' for -%c\n", optarg, c);
                    goto error_exit;
                }
                sscanf(optarg, "%d", &arg_r_modem);
                break;
            case 'u':
                flag_u = 1;
                break;
            case 'j':
                print_json = 1;
                break;
            case ':':
                fprintf(stderr, "Missing argument for %s\n", argv[optind - 1]);
                goto error_exit;
            case '?':
                usage();
                return -1;
                break;
            default:
                abort();
        }
    }

    /* Called with no options */
    if (optind == 1)
    {
        usage();
        return -1;
    }

    /* Enumerate all supported PIDs and concat linked lists */
    struct hid_device_info* dev_combined = NULL;
    for (i = 0; i < (sizeof(valid_usb_pids) / sizeof(valid_usb_pids[0])); i++)
    {
        struct hid_device_info* devices = hid_enumerate(FW_USB_VID, valid_usb_pids[i]);
        if (devices)
        {
            if (!dev_combined)
            {
                dev_combined = devices;
            }
            else
            {
                struct hid_device_info* last_dev;
                last_dev = dev_combined;
                while (last_dev->next)
                {
                    last_dev = last_dev->next;
                }
                last_dev->next = devices;
            }
        }
    }

    /* Contains all duagon devices supported by this tool */
    hdev_info = dev_combined;

    if (hdev_info == NULL)
    {
        fprintf(stderr, "No supported device available\n");
        goto error_exit;
    }

    if (CheckAvailableDevices(hdev_info, &count))
    {
        goto error_exit;
    }

    /* Detect board type of first board (first match is sufficient as board types can't be mixed in a system) */
    switch (hdev_info->product_id)
    {
        case FW_USB_PID_ME10:
            g_board_name = valid_board_names[1];
            break;
        case FW_USB_PID_G239:
            g_board_name = valid_board_names[2];
            break;
        case FW_USB_PID_F229:
            /* special case - this PID (0x4) is used by multiple old firmwares */
            if (ParseBoardnameFromProductString(hdev_info->product_string, &g_board_name))
            {
                goto error_exit;
            }
    }

    if (g_board_name == NULL)
    {
        fprintf(stderr, "Found supported devices, but could not get board name\n");
        goto error_exit;
    }

    if (!print_json)
    {
        printf("Using tool for %s\n", g_board_name);
    }

    if (flag_l)
    {
        if ((ret = PrintDeviceList(hdev_info, print_json)))
        {
            fprintf(stderr, "Cannot list devices: %d\n", ret);
            goto error_exit;
        }
        PrintCompatibilityWarnings(hdev_info);
        hid_free_enumeration(hdev_info);
        return 0;
    }

    /* Several boards are available, user has to select one */
    p_hdev_info = hdev_info;

    if (flag_n)
    {
        /* Select board by serial number */
        if (!strcmp(g_board_name, BOARD_NAME_ME10))
        {
            fprintf(stderr, "Selection by serial number not supported for the board %s\n", g_board_name);
            goto error_exit;
        }

        wcstombs(buf, p_hdev_info->serial_number, CHAR_BUFFER_SIZE);
        sscanf(buf, "%x", &serial_nr);

        while (arg_n_serial != serial_nr)
        {
            if ((p_hdev_info = p_hdev_info->next) != NULL)
            {
                wcstombs(buf, p_hdev_info->serial_number, CHAR_BUFFER_SIZE);
                sscanf(buf, "%x", &serial_nr);
            }
            else
            {
                fprintf(stderr, "%s with serial number %d not available!\n", g_board_name, arg_n_serial);
                ret = -1;
                goto error_exit;
            }
        }
        hdev = hid_open(FW_USB_VID, p_hdev_info->product_id, p_hdev_info->serial_number);
        if (!hdev)
        {
            fprintf(stderr, "Failed to open device %ls. Already in use?\n", p_hdev_info->product_string);
            goto error_exit;
        }
        if (!print_json)
            printf("You have selected %s with serial number %d (0x%x)\n", g_board_name, arg_n_serial, arg_n_serial);
    }
    else if (flag_g)
    {
        /* Select board by geographical address */
        uint32_t geo_addr;
        wcstombs(buf, p_hdev_info->duagon_geo_address, CHAR_BUFFER_SIZE);
        sscanf(buf, "%x", &geo_addr);

        while (arg_g_geo_addr != geo_addr)
        {
            if ((p_hdev_info = p_hdev_info->next) != NULL)
            {
                wcstombs(buf, p_hdev_info->duagon_geo_address, CHAR_BUFFER_SIZE);
                sscanf(buf, "%x", &geo_addr);
            }
            else
            {
                fprintf(stderr, "%s at geographical address 0x%x not available!\n", g_board_name, arg_g_geo_addr);
                ret = -1;
                goto error_exit;
            }
        }
        hdev = hid_open_path(p_hdev_info->path);
        if (!hdev)
        {
            fprintf(stderr, "Failed to open device %ls. Already in use?\n", p_hdev_info->product_string);
            goto error_exit;
        }
        if (!print_json) printf("You have selected %s with geographical address 0x%x\n", g_board_name, arg_g_geo_addr);
    }
    else if (flag_i)
    {
        /* Select board by index */
        uint32_t geo_addr;
        int      index = arg_i_index;
        while (--index)
        {
            if ((p_hdev_info = p_hdev_info->next) == NULL)
            {
                fprintf(stderr, "%s with index %d not available!\n", g_board_name, arg_i_index);
                goto error_exit;
            }
        }
        hdev = hid_open_path(p_hdev_info->path);
        if (!hdev)
        {
            fprintf(stderr, "Failed to open device %ls. Already in use?\n", p_hdev_info->product_string);
            goto error_exit;
        }
        wcstombs(buf, p_hdev_info->duagon_geo_address, CHAR_BUFFER_SIZE);
        sscanf(buf, "%x", &geo_addr);
        if (!print_json) printf("You have selected %s with geographical address 0x%x\n", g_board_name, geo_addr);
    }
    else if (count == 1)
    {
        /* Only one board is available */
        hdev = hid_open_path(p_hdev_info->path);
        if (!hdev)
        {
            fprintf(stderr, "Failed to open device %ls. Already in use?\n", p_hdev_info->product_string);
            goto error_exit;
        }
    }
    else
    {
        fprintf(
            stderr,
            "Warning: Several %s found, please select by index (-i), geographical address (-g) or serial "
            "number (-n).\n",
            g_board_name
        );
        goto error_exit;
    }

    /* Store firmware version of selected device */
    g_fw_version = GetFwVersionAsInt(p_hdev_info->release_number);

    if (!print_json)
    {
        printf("\n");
    }

    hid_free_enumeration(hdev_info);
    hdev_info = NULL;

    /* Module power */
    if (flag_p)
    {
        if (arg_p_module)
        {
            if (SetModulePower(hdev, arg_p_module, arg_p_state))
            {
                fprintf(stderr, "Cannot set module power\n");
                goto error_exit;
            }
        }
        else
        {
            if (GetModulePower(hdev, print_json))
            {
                fprintf(stderr, "Cannot get modules power\n");
                goto error_exit;
            }
        }
    }

    /* Modem sim */
    if (flag_s)
    {
        if (arg_s_modem)
        {
            if (SetModemSim(hdev, arg_s_modem, arg_s_sim))
            {
                fprintf(stderr, "Cannot set modem SIM card\n");
                goto error_exit;
            }
        }
        else
        {
            if (GetModemSim(hdev, print_json))
            {
                fprintf(stderr, "Cannot get modems SIM card\n");
                goto error_exit;
            }
        }
    }

    /* Modem off time */
    if (flag_f)
    {
        if (arg_f_off_time >= 0)
        {
            if (SetModemOffTime(hdev, arg_f_off_time))
            {
                fprintf(stderr, "Cannot set modems off time\n");
                goto error_exit;
            }
        }
        else
        {
            if (GetModemOffTime(hdev, print_json))
            {
                fprintf(stderr, "Cannot get modems off time\n");
                goto error_exit;
            }
        }
    }

    /* Configuration Data */
    if (flag_c)
    {
        if (arg_c_save_cfg)
        {
            if (SetConfigurationData(hdev))
            {
                fprintf(stderr, "Cannot save configuration data\n");
                goto error_exit;
            }
        }
        else
        {
            if (GetConfigurationData(hdev, print_json))
            {
                fprintf(stderr, "Cannot read configuration data\n");
                goto error_exit;
            }
        }
    }

    /* Time Pulse */
    if (flag_t)
    {
        if (GetTimePulse(hdev, print_json))
        {
            fprintf(stderr, "Cannot get Time Pulse counter\n");
            goto error_exit;
        }
    }

    /* LED */
    if (flag_e)
    {
        if (arg_e_led)
        {
            if (arg_e_mode < 2)
            {
                if (SetLedOnOff(hdev, arg_e_led, arg_e_mode))
                {
                    fprintf(stderr, "Cannot set LED status\n");
                    goto error_exit;
                }
            }
            else if (arg_e_mode == 2)
            {
                if (SetLedBlinking(hdev, arg_e_led, arg_e_period, arg_e_dutycycle))
                {
                    fprintf(stderr, "Cannot set LED status\n");
                    goto error_exit;
                }
            }
            else
            {
                fprintf(stderr, "Invalid argument for -e\n");
                goto error_exit;
            }
        }
        else
        {
            if (GetLedStatus(hdev, print_json))
            {
                fprintf(stderr, "Cannot get LED status\n");
                goto error_exit;
            }
        }
    }

    /* Only supported on ME10 and G239 */

    /* Get board temperature */
    if (flag_T)
    {
        if ((ret = GetBoardTemperature(hdev, print_json)))
        {
            fprintf(stderr, "Cannot get board temperature: %d\n", ret);
            goto error_exit;
        }
    }

    /* Set/Get WWAN and GNSS */
    if (flag_w)
    {
        if (arg_w_modem)
        {
            if (ret = SetWWANSignal(hdev, arg_w_modem, arg_w_signal, arg_w_value))
            {
                fprintf(stderr, "Cannot set signal of modem %d: %d\n", arg_w_modem, ret);
                goto error_exit;
            }
        }
        else
        {
            if (ret = GetWWANSignal(hdev, print_json))
            {
                fprintf(stderr, "Cannot get signal status of modem %d: %d\n", arg_w_modem, ret);
                goto error_exit;
            }
        }
    }

    /* Reset modem */
    if (flag_r)
    {
        if ((ret = ResetModem(hdev, arg_r_modem)))
        {
            fprintf(stderr, "Cannot reset modem %d: %d\n", arg_r_modem, ret);
            goto error_exit;
        }
    }

    /* Get powergood signal status */
    if (flag_G)
    {
        if (ret = GetPowerGoodStatus(hdev, print_json))
        {
            fprintf(stderr, "Cannot get powergood signal status of board: %d\n", ret);
            goto error_exit;
        }
    }

    /* Enter into internal DFU bootloader of STM32 for firmware update */
    if (flag_u)
    {
        SetBootloaderMode(hdev);
    }

    return 0;

error_exit:
    if (hdev_info)
    {
        hid_free_enumeration(hdev_info);
    }

    if (hdev)
    {
        hid_close(hdev);
    }

    exit(EXIT_FAILURE);
}
