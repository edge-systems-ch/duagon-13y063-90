/*!
 *         \file  hidapi_minimal_13y063-90.c
 *       \author  Abhijeet Badurkar
 *
 *       \brief   This source file contains function definitions to communicate
 *                with duagon boards with wireless capabilities, e.g. with LTE
 *                modem (F229) or 5G modem (ME10, G239), using hidraw interface
 *                of Linux kernel.
 *
 *     Required:  hidraw driver of Linux kernel
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2022 by duagon AG, Nuremberg, Germany
 ****************************************************************************/
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

/* Linux */
#include "hidapi_minimal_13y063-90.h"

#include <linux/hidraw.h>
#include <linux/input.h>

#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

#define MAJOR_REV 1
#define MINOR_REV 4

#define FW_USB_VID 0x1b02

#define FW_USB_PID_F229 0x0004
#define FW_USB_PID_ME10 0x0005
#define FW_USB_PID_G239 0x0006

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

/* Unix */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* C */
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char*          valid_board_names[] = {BOARD_NAME_F229, BOARD_NAME_ME10, BOARD_NAME_G239};
static const unsigned short valid_usb_pids[]    = {FW_USB_PID_F229, FW_USB_PID_ME10, FW_USB_PID_G239};
static const char*          g_board_name        = NULL;

unsigned short board_usb_pid;

static const LIB_VERSION_STRUCT lib_version_internal = {
    LIB_HIDAPI_MINIMAL_13Y063_90_MAJ_VER,
    LIB_HIDAPI_MINIMAL_13Y063_90_MIN_VER
};

#ifdef DEBUG
static const char*
bus_str(int bus)
{
    switch (bus)
    {
        case BUS_USB:
            return "USB";
            break;
        case BUS_HIL:
            return "HIL";
            break;
        case BUS_BLUETOOTH:
            return "Bluetooth";
            break;
        case BUS_VIRTUAL:
            return "Virtual";
            break;
        default:
            return "Other";
            break;
    }
}
#endif

int
GetModulePower(
    int           hdev,
    unsigned char module
)
{
    unsigned char buf[4] = {MODULE_POWER_REPORT_ID, 0, 0, 0};
    int           ret;

    if ((module < MODULE_MIN_CNT) || (module > MODULE_MAX_CNT))
    {
#ifdef DEBUG
        printf("Module #%d doesn't exist\n", module);
#endif
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(4), buf);
    if (ret < 0) return ret;

    return buf[module];
}

int
SetModulePower(
    int           hdev,
    unsigned char module,
    unsigned char powerOn
)
{
    unsigned char buf[3] = {MODULE_POWER_REPORT_ID, 0, 0};
    int           ret;

    if ((module < MODULE_MIN_CNT) || (module > MODULE_MAX_CNT))
    {
#ifdef DEBUG
        printf("Module #%d doesn't exist\n", module);
#endif
        return -1;
    }

    buf[1] = module;

    if ((powerOn != 0) && (powerOn != 1))
    {
        printf("Invalid module power mode\n");
        return -1;
    }
    buf[2] = powerOn;

    ret = ioctl(hdev, HIDIOCSFEATURE(3), buf);
    if (ret < 0) return ret;

    return 0;
}

int
GetModemSim(
    int            hdev,
    unsigned char  modem,
    unsigned char* sim
)
{
    unsigned char buf[2];
    int           ret;

    if ((modem < MODULE_MIN_CNT) || (modem > MODULE_MAX_CNT))
    {
#ifdef DEBUG
        printf("Module #%d doesn't exist\n", modem);
#endif
        return -1;
    }

    if (modem == 1)
        buf[0] = MODEM_1_SIM_REPORT_ID;
    else if (modem == 2)
        buf[0] = MODEM_2_SIM_REPORT_ID;
    else
    {
#ifdef DEBUG
        printf("Modem #%d doesn't exist\n", modem);
#endif
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(2), buf);
    if (ret < 0) return ret;

    *sim = buf[1];
    return 0;
}

int
SetModemSim(
    int           hdev,
    unsigned char modem,
    unsigned char sim
)
{
    unsigned char buf[2];
    int           ret;

    if ((modem < MODULE_MIN_CNT) || (modem > MODULE_MAX_CNT))
    {
#ifdef DEBUG
        printf("Module #%d doesn't exist\n", modem);
#endif
        return -1;
    }

    if (sim > SIM_MAX_CNT)
    {
#ifdef DEBUG
        printf("Invalid SIM card\n");
#endif
        return -1;
    }

    /* Check SIM card is not used by other modem */
    if (sim != 0)
    {
        buf[0] = (modem == 1) ? MODEM_2_SIM_REPORT_ID : MODEM_1_SIM_REPORT_ID;
        ret    = ioctl(hdev, HIDIOCGFEATURE(2), buf);
        if (ret < 0) return ret;
        if (sim == buf[1])
        {
#ifdef DEBUG
            printf("SIM card #%d", sim);
            printf(" already used by modem #%d\n", buf[0]);
#endif
            return -1;
        }
    }
    buf[0] = (modem == 1) ? MODEM_1_SIM_REPORT_ID : MODEM_2_SIM_REPORT_ID;
    buf[1] = sim;

    ret = ioctl(hdev, HIDIOCSFEATURE(2), buf);
    if (ret < 0) return ret;

    return 0;
}

int
GetModemOffTime(
    int           hdev,
    unsigned int* offTime
)
{
    unsigned char buf[3] = {MODEM_OFF_TIME_REPORT_ID, 0, 0};
    int           ret;

    ret = ioctl(hdev, HIDIOCGFEATURE(3), buf);
    if (ret < 0) return ret;
    *offTime = (buf[1] << 8) | buf[2];

    return 0;
}

int
SetModemOffTime(
    int          hdev,
    unsigned int offTime
)
{
    unsigned char buf[3] = {MODEM_OFF_TIME_REPORT_ID, 0, 0};
    int           ret;

    if (offTime > 0xffff)
    {
#ifdef DEBUG
        printf("Modems off time too large\n");
#endif
        return -1;
    }
    buf[1] = offTime >> 8;
    buf[2] = offTime;

    ret = ioctl(hdev, HIDIOCSFEATURE(3), buf);
    if (ret < 0) return ret;

    return 0;
}

int
GetConfigurationData(
    int               hdev,
    CONF_DATA_STRUCT* config_data
)
{
    unsigned char buf[9] = {CONF_DATA_REPORT_ID, 0, 0, 0, 0, 0, 0, 0, 0};
    int           ret;

    ret = ioctl(hdev, HIDIOCGFEATURE(9), buf);
    if (ret < 0) return ret;

    config_data->modulePowerOn[0] = buf[1];
    config_data->modulePowerOn[1] = buf[2];
    config_data->modulePowerOn[2] = buf[3];

    config_data->modemSimId[0] = buf[4];
    config_data->modemSimId[1] = buf[5];

    config_data->modemOffTime = (buf[6] << 8) | buf[7];

    return 0;
}

int
SetConfigurationData(int hdev)
{
    unsigned char buf[2] = {CONF_DATA_REPORT_ID, 0};
    int           ret;

    ret = ioctl(hdev, HIDIOCSFEATURE(2), buf);
    if (ret < 0) return ret;
#ifdef DEBUG
    printf("Configuration data saved in flash\n");
#endif

    return 0;
}

int
GetTimePulse(
    int           hdev,
    unsigned int* time_pulse
)
{
    unsigned char buf[5] = {TIME_PULSE_REPORT_ID, 0, 0, 0, 0};
    int           ret;

    ret = ioctl(hdev, HIDIOCGFEATURE(5), buf);
    if (ret < 0) return ret;
    *time_pulse = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];

    return 0;
}

int
GetLedStatus(
    int                hdev,
    unsigned char      led,
    LED_STATUS_STRUCT* led_status
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
#ifdef DEBUG
        printf("LED #%d doesn't exist\n", led);
#endif
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(5), buf);
    if (ret < 0) return ret;

    led_status->mode       = buf[1];
    led_status->period     = (buf[2] << 8) | buf[3];
    led_status->duty_cycle = buf[4];

    return 0;
}

int
SetLedOnOff(
    int           hdev,
    unsigned char led,
    unsigned char powerOn
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
#ifdef DEBUG
        printf("LED #%d doesn't exist\n", led);
#endif
        return -1;
    }

    if ((powerOn != 0) && (powerOn != 1))
    {
#ifdef DEBUG
        printf("Invalid LED power mode\n");
#endif
        return -1;
    }
    buf[1] = powerOn;

    ret = ioctl(hdev, HIDIOCSFEATURE(5), buf);
    if (ret < 0) return ret;

    return 0;
}

int
SetLedBlinking(
    int           hdev,
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
#ifdef DEBUG
        printf("LED #%d doesn't exist\n", led);
#endif
        return -1;
    }
    buf[1] = 2; /* Blinking mode */

    if (period > 0xffff)
    {
#ifdef DEBUG
        printf("LED blinking period too large\n");
#endif
        return -1;
    }
    buf[2] = period >> 8;
    buf[3] = period;

    if (dutyCycle > MAX_DUTY_CYCLE)
    {
#ifdef DEBUG
        printf("LED blinking duty cycle too large\n");
#endif
        return -1;
    }
    buf[4] = dutyCycle;

    ret = ioctl(hdev, HIDIOCSFEATURE(5), buf);
    if (ret < 0) return ret;

    return 0;
}

char
GetBoardTemperature(
    int   hdev,
    char* temperature
)
{
    char buf[2] = {BOARD_TEMPERATURE_REPORT_ID, 0};
    int  ret;

    if (!g_board_name)
    {
        printf("Board name not set\n");
        return -1;
    }
    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        printf("This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(2), buf);
    if (ret < 0) return ret;

    ret = buf[1];

    /* In ME10 and G239 firmware, if board temperature cannot be read
       then -127 value is returned as an indication of an error. */
    if (ret == TEMPERATURE_ADC_INIT_ERROR)
    {
#ifdef DEBUG
        printf("Error: Temperature ADC of the board has not been initialized yet\n");
#endif
    }
    else if (ret == TEMPERATURE_ADC_MEAS_FAIL)
    {
#ifdef DEBUG
        printf("Error: Temperature ADC has not measured any temperature.\n");
#endif
    }
    else if (ret == TEMPERATURE_ADC_BAD_VALUE)
    {
#ifdef DEBUG
        printf("Error: Temperature ADC has incorrectly measured value\n");
#endif
    }
    else
    {
        *temperature = ret;
        return 0; /* Temperature value has been correctly received and is within the range. */
    }

    return ret; /* Error */
}

char
SetWWANSignal(
    int           hdev,
    unsigned char modem,
    unsigned char signal,
    unsigned char disable
)
{
    unsigned char buf[4];
    int           ret;

    if (!g_board_name)
    {
        printf("Board name not set\n");
        return -1;
    }
    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        printf("This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    if (modem > 2 || modem == 0 || signal > 1 || disable > 1)
    {
#ifdef DEBUG
        printf("Arguments are incorrect, please verify!\n");
#endif
        return -1;
    }

    buf[0] = MODEM_WWAN_GNSS_DISABLE_REPORT_ID;
    buf[1] = modem;
    buf[2] = signal;
    buf[3] = disable;

    ret = ioctl(hdev, HIDIOCSFEATURE(4), buf);
    return ret;
}

char
GetWWANSignal(
    int            hdev,
    unsigned char* buf,
    unsigned char  buf_length
)
{
    int           ret;
    unsigned char tmp_buf[5] = {MODEM_WWAN_GNSS_DISABLE_REPORT_ID, 0, 0, 0, 0};

    if (!g_board_name)
    {
        printf("Board name not set\n");
        return -1;
    }
    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        printf("This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    if (buf_length < 4)
    {
#ifdef DEBUG
        printf("Argument buffer has incorrect size, shall be at least 4!\n");
#endif
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(5), tmp_buf);
    if (ret < 0)
    {
#ifdef DEBUG
        printf("Error in getting signal\n");
#endif
        return ret;
    }

    memcpy(buf, (tmp_buf + 1), 4);
    return 0;
}

char
ResetModem(
    int           hdev,
    unsigned char modem
)
{
    unsigned char buf[2];
    int           ret;

    if (!g_board_name)
    {
        printf("Board name not set\n");
        return -1;
    }
    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        printf("This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    if (modem > 2 || modem == 0) return -1;

    buf[0] = MODEM_RESET_REPORT_ID;
    buf[1] = modem;

    ret = ioctl(hdev, HIDIOCSFEATURE(2), buf);
    if (ret < 0) return ret;

    return 0;
}

char
GetPowerGoodStatus(int hdev)
{
    unsigned char buf[2] = {BOARD_POWERGOOD_SIGNAL_REPORT_ID, 0};
    int           ret;

    if (!g_board_name)
    {
        printf("Board name not set\n");
        return -1;
    }
    if (!strcmp(g_board_name, BOARD_NAME_F229))
    {
        printf("This feature is currently not supported for the board %s\n", g_board_name);
        return -1;
    }

    ret = ioctl(hdev, HIDIOCGFEATURE(2), buf);
    if (ret < 0) return ret;

    return buf[1];
}

void
SetBootloaderMode(int hdev)
{
    unsigned char buf[2] = {BOOTLOADER_REPORT_ID, 0};

    /* We will never receive 0 as return value for this call. This is
     * because, when firmare receives BOOTLOADER_REPORT_ID, it will
     * jump to internal bootloader of STM32 wihout returning any value.
     * Therefore, do not consider return value and inform user to check
     * if STM32 is available as DFU device i.e. if it entered in internal
     * DFU bootloader of STM32.
     */
    ioctl(hdev, HIDIOCSFEATURE(2), buf);

    printf(
        "Succesfully set bootloader mode. Please execute \"dfu-util -l\" and "
        "check if device is available as DFU device for firmare update. \n"
    );
}

int
OpenDevice(char* device)
{
    int                   hdev;
    int                   ret;
    int                   i;
    char                  buf[256];
    struct hidraw_devinfo info;

    hdev = open(device, O_RDWR | O_NONBLOCK);

    if (hdev < 0)
    {
#ifdef DEBUG
        perror("Unable to open device");
#endif
        return hdev;
    }

    memset(&info, 0x0, sizeof(info));
    memset(buf, 0x0, sizeof(buf));

    /* Get Raw Info */
    ret = ioctl(hdev, HIDIOCGRAWINFO, &info);
    if (ret < 0)
    {
#ifdef DEBUG
        perror("HIDIOCGRAWINFO");
#endif
        close(hdev);

        return ret;
    }
#ifdef DEBUG
    printf("Raw Info:\n");
    printf("\tbustype: %d (%s)\n", info.bustype, bus_str(info.bustype));
    printf("\tvendor: 0x%04hx\n", info.vendor);
    printf("\tproduct: 0x%04hx\n", info.product);
#endif
    if (info.vendor != FW_USB_VID)
    {
        close(hdev);
        return -1;
    }

    for (i = 0; i < (sizeof(valid_usb_pids) / sizeof(valid_usb_pids[0])); i++)
    {
        if (info.product == valid_usb_pids[i])
        {
#ifdef DEBUG
            printf(
                "Successfully opened the HID raw device with VID:0x%04hx and PID:0x%04hx\n",
                info.vendor,
                info.product
            );
#endif
            board_usb_pid = valid_usb_pids[i];
            g_board_name  = valid_board_names[i];
            return hdev;
        }
    }

    close(hdev);
    return -1;
}

int
CloseDevice(int hdev)
{
    return close(hdev);
}

int
GetRawDeviceName(
    int   hdev,
    char* buf
)
{
    int ret;

    /* Get Raw Name */
    ret = ioctl(hdev, HIDIOCGRAWNAME(256), buf);
#ifdef DEBUG
    if (ret < 0)
        printf("Could not get device name\n");
    else
        printf("Raw Name: %s\n", buf);
#endif

    return ret;
}

const char*
GetBoardName()
{
    return g_board_name;
}

const LIB_VERSION_STRUCT*
GetLibVersion()
{
    return &lib_version_internal;
}
