// SPDX-License-Identifier: MIT
/*!
 * Copyright (C) 2020-2023, duagon Germany GmbH
 *
 * \file      13Y063-90_minimal.c
 * \brief     Tool to control duagon boards with LTE (e.g. F229) or 5G modems
 *            (e.g. ME10, G239) using HID commands.
 * \author    Abhijeet Badurkar
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

#include "hidapi_minimal_13y063-90.h"

#define TOOL     "13Y063-90_minimal"
#define VERSION  "_02_00"
#define LIB_NAME "libhidapi_minimal_13y063-90"

/* Unix */
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

/* C */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Check if optional argument is present */
#define HAS_OPTIONAL_ARG (!optarg && NULL != argv[optind] && '-' != argv[optind][0])

/*--------------------------------------+
|    GLOBALS                            |
+--------------------------------------*/
static const char* const   short_options  = ":hli:d:p::s::f::cte::r:w::TGu";
static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"list", no_argument, NULL, 'l'},
    {"index", required_argument, NULL, 'i'},
    {"device", required_argument, NULL, 'd'},
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

    {NULL, 0, NULL, 0},
};

const char* g_board_name = NULL;

/*--------------------------------------+
|    PROTOTYPES                         |
+--------------------------------------*/
static void
header();
static void
printLibVersion();
static void
usage();

/********************************* header **********************************/
/**  Prints the headline
 */
static void
header()
{
    printf(
        "\n"
        "===============================================\n"
        "==          %s%s          ==\n"
        "===============================================\n"
        "(c) Copyright by duagon AG\n\n",
        TOOL,
        VERSION
    );
}

/********************************* printLibVersion *************************/
/**  Prints the version of hidapi_minimal_13y063-90 library
 */
static void
printLibVersion()
{
    const LIB_VERSION_STRUCT* lib_version;

    lib_version = GetLibVersion();
    printf(
        "Using version %d.%d of %s\n"
        "===============================================\n\n",
        lib_version->major,
        lib_version->minor,
        LIB_NAME
    );
}

/*********************************** Usage *********************************/
/** Prints the program usage
 *
 */
static void
usage()
{
    printf(
        "== This tool controls the F229, ME10 and G239 firmware ==\n\n"
        "This program must be run as super user\n\n"

        "Usage: %s [OPTIONS]\n"
        "Options:\n"
        " -h, --help                 Show this help message\n"
        " -l, --list                 List devices\n"
        " -i, --index <IDX>          Select device by index in list\n"
        " -d, --device <PATH>        Set device path (default: search /dev/hidraw[0-255])\n"
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
        " -u, --update-mode          Set device into DFU bootloader mode for firmware update\n",
        TOOL
    );
    exit(1);
}

int
main(
    int    argc,
    char** argv
)
{
    int               hdev  = 0;
    unsigned int      count = 0;
    int               i, ret;
    char              device_buf[sizeof("/dev/hidraw255")]     = "";
    char              arg_device_buf[sizeof("/dev/hidraw255")] = "";
    char*             device                                   = device_buf;
    int               first_valid_hidraw                       = 0;
    int               index_selected_hidraw                    = 0;
    CONF_DATA_STRUCT  config_data;
    LED_STATUS_STRUCT led_status;
    char              buf[256];

    int c, flag_l = 0, flag_i = 0, flag_d = 0, flag_t = 0, flag_T = 0, flag_G = 0, flag_r = 0, flag_u = 0, flag_f = 0,
           flag_p = 0, flag_s = 0, flag_e = 0, flag_w = 0, flag_c = 0;

    /* Arguments */
    int   arg_i_index    = 0;
    char* arg_d_device   = arg_device_buf;
    int   arg_p_module   = 0, arg_p_state;
    int   arg_w_modem    = 0, arg_w_signal, arg_w_value;
    int   arg_s_modem    = 0, arg_s_sim;
    int   arg_f_off_time = -1;
    int   arg_c_save_cfg = 0;
    int   arg_r_modem    = 0;
    int   arg_e_led      = 0, arg_e_mode, arg_e_period, arg_e_dutycycle;

    header();
    printLibVersion();

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
                sscanf(optarg, "%d", &arg_i_index);  // cppcheck-suppress invalidscanf
                if (arg_i_index <= 0)
                {
                    fprintf(stderr, "Invalid argument value '%s' for -%c\n", optarg, c);
                    goto error_exit;
                }
                break;
            case 'd':
                flag_d = 1;
                sscanf(optarg, "%s", arg_d_device);  // cppcheck-suppress invalidscanf
                break;
            case 'p':
                flag_p = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_p_module);  // cppcheck-suppress invalidscanf

                    /* Additional arguments */
                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <PWR> for -p option\n");
                        exit(EXIT_FAILURE);
                    }
                    arg_p_state = atoi(argv[optind++]);
                }
                break;
            case 's':
                flag_s = 1;
                if (HAS_OPTIONAL_ARG)
                {
                    sscanf(argv[optind++], "%d", &arg_s_modem);  // cppcheck-suppress invalidscanf

                    /* Additional arguments */
                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <SIM> for -s option\n");
                        exit(EXIT_FAILURE);
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
                    printf(
                        "Warning: Option '-c -s' is depricated. Please use the --save-config "
                        "option.\n"
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
                    sscanf(argv[optind++], "%d", &arg_e_led);  // cppcheck-suppress invalidscanf

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <MODE> for -e option\n");
                        exit(EXIT_FAILURE);
                    }
                    arg_e_mode = atoi(argv[optind++]);

                    /* Extra arguments for blink mode */
                    if (arg_e_mode == 2)
                    {
                        if (optind >= argc || !isdigit(*argv[optind]))
                        {
                            fprintf(stderr, "Missing argument <PERIOD> for -e option with LED mode=2\n");
                            exit(EXIT_FAILURE);
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
                    sscanf(argv[optind++], "%d", &arg_w_modem);  // cppcheck-suppress invalidscanf

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <SIGNAL> for -w option\n");
                        exit(EXIT_FAILURE);
                    }
                    arg_w_signal = atoi(argv[optind++]);
                    printf("#DBG: signal %d", arg_w_signal);

                    if (optind >= argc || !isdigit(*argv[optind]))
                    {
                        fprintf(stderr, "Missing argument <STATE> for -w option\n");
                        exit(EXIT_FAILURE);
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
                sscanf(optarg, "%d", &arg_r_modem);  // cppcheck-suppress invalidscanf
                break;
            case 'u':
                flag_u = 1;
                break;
            case ':':
                fprintf(stderr, "Missing argument for %s\n", argv[optind - 1]);
                exit(EXIT_FAILURE);
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

    if (!flag_d)
    {
        for (i = 0; i <= 255; i++)
        {
            snprintf(device_buf, sizeof(device_buf), "/dev/hidraw%d", i);
            hdev = OpenDevice(device);
            /* Found compatible device*/
            if (hdev >= 0)
            {
                count++;
                if (!first_valid_hidraw)
                {
                    first_valid_hidraw = i;
                }
                g_board_name = GetBoardName();
                printf("%u) %s: Device Path: %s\n", count, g_board_name, device);
                if (flag_i && arg_i_index == count)
                {
                    index_selected_hidraw = i;
                }
                CloseDevice(hdev);
            }
        }
    }

    if (flag_l)
    {
        /* list is usually printed - see above */
        return 0;
    }

    printf("\n");

    /* Several boards are available, user has to select one */
    if (flag_d)
    {
        /* Select board by path */
        hdev = OpenDevice(arg_d_device);

        if (hdev < 0)
        {
            fprintf(stderr, "Not a compatible device: %s\n", arg_d_device);
            return 1;
        }
    }
    else if (flag_i)
    {
        /* Select board by index */
        snprintf(device_buf, sizeof(device_buf), "/dev/hidraw%d", index_selected_hidraw);
        hdev = OpenDevice(device);

        if (arg_i_index > count)
        {
            fprintf(stderr, "%s with index %d not available!\n", g_board_name, arg_i_index);
            goto error_exit;
        }
    }
    else if (count == 1)
    {
        /* Only one board is available */
        snprintf(device_buf, sizeof(device_buf), "/dev/hidraw%d", first_valid_hidraw);
        hdev = OpenDevice(device);

        if (!hdev)
        {
            fprintf(stderr, "Failed to open device %s. Already in use?\n", device);
            goto error_exit;
        }
    }
    else
    {
        fprintf(stderr, "Several %s, please select by index (-i) or device path (-d).\n", g_board_name);
        goto error_exit;
    }

    if (!hdev)
    {
        fprintf(stderr, "Did not find any compatible device.\n");
        return 1;
    }

    g_board_name = GetBoardName();
    header();
    printLibVersion();

    printf("Using tool for %s\n\n", g_board_name);

    /* Module power */
    if (flag_p)
    {
        int power_state;
        if (arg_p_module)
        {
            if (SetModulePower(hdev, arg_p_module, arg_p_state))
            {
                fprintf(stderr, "Cannot set module power\n");
                goto error_cleanup;
            }
            else
            {
                printf("Power-%s request for Module #%d sent.\n", arg_p_state ? "on" : "off", arg_p_module);
                printf("Wait for %s of Module.\n", arg_p_state ? "startup" : "shutdown");

                /* Block until modem is powered on/off successfully.
                 * This takes at maximum the set "modems_off_time". */
                do
                {
                    /* Read back power state */
                    if ((power_state = GetModulePower(hdev, arg_p_module)) < 0)
                    {
                        fprintf(stderr, "Cannot get module power\n");
                        goto error_cleanup;
                    }
                    /* Check success */
                    if (power_state == arg_p_state)
                    {
                        break;
                    }
                    usleep(1000 * 1000);
                } while (1);

                printf("Power for Module #%d reported as %s.\n", arg_p_module, power_state ? "on" : "off");
            }
        }
        else
        {
            for (i = 1; i < 4; i++)
            {
                if ((power_state = GetModulePower(hdev, i)) < 0)
                {
                    fprintf(stderr, "Cannot get module power\n");
                    goto error_cleanup;
                }
                else
                {
                    printf("Module #%d is %s\n", i, power_state ? "enabled" : "disabled");
                }
            }
        }
    }

    /* Modem sim */
    if (flag_s)
    {
        unsigned char sim;
        if (arg_s_modem)
        {
            if (SetModemSim(hdev, arg_s_modem, arg_s_sim))
            {
                fprintf(stderr, "Cannot set modem SIM card\n");
                if ((GetModemSim(hdev, ((arg_s_modem == 1) ? 2 : 1), &sim)) < 0)
                {
                    goto error_cleanup;
                }
                else
                {
                    if (sim == arg_s_sim) fprintf(stderr, "SIM card %d already used by another modem\n", arg_s_sim);
                }
                goto error_cleanup;
            }
            else
            {
                printf("Modem #%d set with ", arg_s_modem);
                if (arg_s_sim == 0)
                    printf("no SIM card\n");
                else
                    printf("SIM card #%d\n", arg_s_sim);
            }
        }
        else
        {
            for (i = 1; i < 3; i++)
            {
                if ((GetModemSim(hdev, i, &sim)) < 0)
                {
                    fprintf(stderr, "Cannot get modems SIM card\n");
                    goto error_cleanup;
                }
                else
                {
                    printf("Modem #%d uses ", i);
                    if (sim == 0)
                        printf("no SIM card\n");
                    else
                        printf("SIM card #%d\n", sim);
                }
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
                goto error_cleanup;
            }
            else
            {
                printf("Modems off time set to %d ms\n", arg_f_off_time);
            }
        }
        else
        {
            unsigned int modem_off_time;
            if ((ret = GetModemOffTime(hdev, &modem_off_time)) != 0)
            {
                fprintf(stderr, "Cannot get modems off time\n");
                goto error_cleanup;
            }
            else
            {
                printf("Modems off time is %u ms\n", modem_off_time);
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
                fprintf(stderr, "Cannot set configuration data\n");
                goto error_cleanup;
            }
            else
            {
                printf("Configuration data saved in flash\n");
            }
        }
        else
        {
            if (0 != GetConfigurationData(hdev, &config_data))
            {
                fprintf(stderr, "Cannot get configuration data\n");
                goto error_cleanup;
            }
            else
            {
                unsigned char sim1, sim2;

                sim1 = config_data.modemSimId[0];
                sim2 = config_data.modemSimId[1];
                printf("Configuration data used at startup:\n\n");

                /* Modem #1 */
                printf("Modem  #1 is %s with ", config_data.modulePowerOn[0] ? "enabled " : "disabled");
                if (sim1 == 0)
                    printf("no SIM card\n");
                else
                    printf("SIM card #%d\n", sim1);

                /* Modem #2 */
                printf("Modem  #2 is %s with ", config_data.modulePowerOn[1] ? "enabled " : "disabled");
                if (sim2 == 0)
                    printf("no SIM card\n");
                else
                    printf("SIM card #%d\n", sim2);

                /* Module #3 */
                printf("Module #3 is %s\n", config_data.modulePowerOn[2] ? "enabled " : "disabled");

                /* Modems off time */
                printf("Modems are off for %d ms when setting SIM card\n", config_data.modemOffTime);
            }
        }
    }

    /* Time Pulse */
    if (flag_t)
    {
        unsigned int time_pulse;
        if (0 != GetTimePulse(hdev, &time_pulse))
        {
            fprintf(stderr, "Cannot get Time Pulse counter\n");
            goto error_cleanup;
        }
        else
        {
            printf("Time Pulse counter = %u\n", time_pulse);
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
                    goto error_cleanup;
                }
                else
                {
                    printf("LED #%d %s\n", arg_e_led, arg_e_mode ? "enabled" : "disabled");
                }
            }
            else if (arg_e_mode == 2)
            {
                if (SetLedBlinking(hdev, arg_e_led, arg_e_period, arg_e_dutycycle))
                {
                    fprintf(stderr, "Cannot set LED status\n");
                    goto error_cleanup;
                }
                else
                {
                    printf(
                        "LED #%d set to blinking mode: period = %d ms and duty cycle = %d %%\n",
                        arg_e_led,
                        arg_e_period,
                        arg_e_dutycycle
                    );
                }
            }
            else
            {
                fprintf(stderr, "Invalid argument for -e\n");
                goto error_cleanup;
            }
        }
        else
        {
            for (i = 1; i < 4; i++)
            {
                if (0 != GetLedStatus(hdev, i, &led_status))
                {
                    fprintf(stderr, "Cannot get LED status\n");
                    goto error_cleanup;
                }
                else
                {
                    if (led_status.mode == 2)
                        printf(
                            "LED #%d is blinking: period = %d ms and duty cycle = %d %%\n",
                            i,
                            led_status.period,
                            led_status.duty_cycle
                        );
                    else
                        printf("LED #%d is %s\n", i, led_status.mode == 1 ? "on" : "off");
                }
            }
        }
    }

    /* Get board temperature */
    if (flag_T)
    {
        char temperature;
        if ((ret = GetBoardTemperature(hdev, &temperature)))
        {
            fprintf(stderr, "Cannot get board temperature: %d\n", ret);
            goto error_cleanup;
        }
        printf("Board Temperature = %d°C\n", temperature);
    }

    if (flag_w)
    {
        if (arg_w_modem)
        {
            if (SetWWANSignal(hdev, arg_w_modem, arg_w_signal, arg_w_value) < 0)
            {
                fprintf(stderr, "Cannot set signal\n");
                goto error_cleanup;
            }
            printf(
                "Successfully set %s of modem%d to %s\n",
                arg_w_signal ? "GNSS" : "WWAN",
                arg_w_modem,
                arg_w_value ? "enable" : "disable"
            );
        }
        else
        {
            if (GetWWANSignal(hdev, buf, 4) < 0)
            {
                fprintf(stderr, "Cannot get signal status: %d\n", ret);
                goto error_cleanup;
            }
            printf("Modem1: WWAN:%s and GNSS:%s\n", buf[0] ? "Enabled" : "Disabled", buf[1] ? "Enabled" : "Disabled");
            printf("Modem2: WWAN:%s and GNSS:%s\n", buf[2] ? "Enabled" : "Disabled", buf[3] ? "Enabled" : "Disabled");
        }
    }
    /* Reset modem */
    if (flag_r)
    {
        if ((ret = ResetModem(hdev, arg_r_modem)))
        {
            fprintf(stderr, "Cannot reset modem %d: %d\n", arg_r_modem, ret);
            goto error_cleanup;
        }
        printf("Modem %d has been reset successfully\n", arg_r_modem);
    }
    /* Get powergood signal status */
    if (flag_G)
    {
        if ((ret = GetPowerGoodStatus(hdev)) < 0)
        {
            fprintf(stderr, "Cannot get powergood signal status of board: %d\n", ret);
            goto error_cleanup;
        }
        printf("Powergood signal status = %d\n", ret);
    }

    /* Enter into internal DFU bootloader of STM32 for firmware update */
    if (flag_u)
    {
        SetBootloaderMode(hdev);
    }

    CloseDevice(hdev);
    return 0;

error_cleanup:
    CloseDevice(hdev);
error_exit:
    exit(EXIT_FAILURE);
}
