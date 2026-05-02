/***********************  I n c l u d e  -  F i l e  ************************/
/*!
 *      \file    hidapi_minimal_13y063-90.h
 *
 *      \author  Abhijeet Badurkar
 *
 *      \brief   This is header file for source hidapi_minimal_13y063-90.c
 *
 */
/*-------------------------------[ History ]---------------------------------
 *
 *
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2022 by duagon, Nuremberg, Germany
 ****************************************************************************/

/** @file
 * @defgroup API libhidapi_minimal_13y063-90 API
 */

#ifndef HIDAPI_MINIMAL_13Y063_90_H
#define HIDAPI_MINIMAL_13Y063_90_H

/** Major version of library */
#define LIB_HIDAPI_MINIMAL_13Y063_90_MAJ_VER 3

/** Minor version of library */
#define LIB_HIDAPI_MINIMAL_13Y063_90_MIN_VER 1

#define BOARD_NAME_F229 "F229"

#define BOARD_NAME_ME10 "ME10"

#define BOARD_NAME_G239 "G239"

/** Minimum module count */
#define MODULE_MIN_CNT 1

/** Maximum module count */
#define MODULE_MAX_CNT 3

/** Maximum modem count */
#define MODEM_MAX_CNT 2

/** Maximum number of SIM cards */
#define SIM_MAX_CNT 10

/** Maximum duty cycle of LED blinking */
#define MAX_DUTY_CYCLE 100

/* Temperature values can be negative.
   So, we choose out of range negative
   values to indicate failure of teperature
   read.
*/
/** Temperature ADC of the board is not initialized. */
#define TEMPERATURE_ADC_INIT_ERROR -125

/** Temperature ADC of the board failed to measure any temperature. */
#define TEMPERATURE_ADC_MEAS_FAIL -126

/** Temperature ADC of the board has measured temperature incorrectly. */
#define TEMPERATURE_ADC_BAD_VALUE -127

/** Structure providing the version of the library libhidraw_13y063-90 */
// cppcheck-suppress-begin unusedStructMember
typedef struct
{
    /** Library major version. */
    const unsigned short major;

    /** Library minor version. */
    const unsigned short minor;
} LIB_VERSION_STRUCT;

/** LED status structure */
typedef struct
{
    /** LED mode - 0:off, 1:on, 2:blinking */
    unsigned char mode;
    /** Duty cycle - 0 to 100% */
    unsigned char duty_cycle;
    /** Period of blinking LED */
    unsigned short period;
} LED_STATUS_STRUCT;

/** Configuration data stored in HID device */
typedef struct
{
    /** Module power: Disabled (0) or Enabled (1) */
    unsigned char modulePowerOn[MODULE_MAX_CNT];
    /** Modem SIM: None (0), real SIM (1-10) */
    unsigned char modemSimId[MODEM_MAX_CNT];
    /** Time in ms to hold a modem off before changing its SIM card */
    unsigned short modemOffTime; /*  */
} CONF_DATA_STRUCT;
// cppcheck-suppress-end unusedStructMember

/**
 * \ingroup API
 * Get the version of this library.
 *
 * return pointer to LIB_VERSION struct
 */
const LIB_VERSION_STRUCT*
GetLibVersion();

/**
 * \ingroup API
 * Opens HID raw device from its given path.
 *
 * \param[in] device path to hidraw device from Linux system, e.g./dev/hidraw0
 *
 * return handle to hidraw device if successful, negative value for an error
 */
int
OpenDevice(char* device);

/**
 * \ingroup API
 * Closes HID raw device of provided handle.
 *
 * \param[in] hdev Handle to the HID raw device
 *
 *---------------------------------------------------------------------------
 *
 *  return 0 if successful, negative value for an error
 */
int
CloseDevice(int hdev);

/**
 * \ingroup API
 * Gets the raw name of the HID raw device.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] buf pointer to char to store device name (max length: 256)
 *
 * return 0 if successful, negative value for an error
 */
int
GetRawDeviceName(
    int   hdev,
    char* buf
);

/**
 * \ingroup API
 * Gets the board name (i.e. product name) of HID raw device.
 *
 * return char pointer to board name
 */
const char*
GetBoardName();

/**
 * \ingroup API
 * Gets status of power of modules present on the given HID device.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] module Module identity (1 to 3)
 *
 * \return 0 for power off, 1 for power on, -1 on error
 *
 */
int
GetModulePower(
    int           hdev,
    unsigned char module
);

/**
 * \ingroup API
 * Sets the power of provided module of HID device to on or off.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] module Module identity (1 to 3)
 * \param[in] powerOn Power status (0: power off, 1: power on)
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetModulePower(
    int           hdev,
    unsigned char module,
    unsigned char powerOn
);

/**
 * \ingroup API
 * Gets the status of SIM of provided modem of HID device.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] modem Modem identity (1 to 2)
 * \param[out] sim when SIM is present on modem then SIM number (1 to 10), otherwise 0
 *
 * \return 0 on success, -1 on error
 *
 */
int
GetModemSim(
    int            hdev,
    unsigned char  modem,
    unsigned char* sim
);

/**
 * \ingroup API
 * Sets the provided SIM for a modem of HID device.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] modem Modem identity (1 to 2)
 * \param[in] sim SIM card identity
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetModemSim(
    int           hdev,
    unsigned char modem,
    unsigned char sim
);

/**
 * \ingroup API
 * Gets the modem off time, which is common for all modems.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] offTime modem off time in ms
 *
 * \return 0 on success, -1 on error
 *
 */
int
GetModemOffTime(
    int           hdev,
    unsigned int* offTime
);

/**
 * \ingroup API
 * Sets the modem off time, which is common for all modems.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] offTime Modems off time in ms
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetModemOffTime(
    int          hdev,
    unsigned int offTime
);

/**
 * \ingroup API
 * Gets the configuration data (see \ref CONF_DATA_STRUCT) stored on non-volatile memory of HID
 * device.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] config_data pointer to configuration data structure
 *
 * \return 0 on success, -1 on error
 *
 */
int
GetConfigurationData(
    int               hdev,
    CONF_DATA_STRUCT* config_data
);

/**
 * \ingroup API
 * Sets the configuration data (see \ref CONF_DATA_STRUCT), which is then stored by HID device on
 * its non-volatile memory.
 *
 * \param[in] hdev Handle to the HID raw device
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetConfigurationData(int hdev);

/**
 * \ingroup API
 * Gets time pulse of the modem.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] time_pulse to store time pulse of modem
 * \return 0 success, -1 on error
 *
 */
int
GetTimePulse(
    int           hdev,
    unsigned int* time_pulse
);

/**
 * \ingroup API
 * Gets the status of provided LED.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] led LED identity (starts from 1)
 * \param[out] led_status pointer to LED_STATUS_STRUCT
 *
 * \return 0 on success, -1 on error
 *
 */
int
GetLedStatus(
    int                hdev,
    unsigned char      led,
    LED_STATUS_STRUCT* led_status
);

/**
 * \ingroup API
 * Sets the provided LED to on or off.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] led LED identity (starts from 1)
 * \param[in] powerOn Power status (0:off, 1:on)
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetLedOnOff(
    int           hdev,
    unsigned char led,
    unsigned char powerOn
);

/**
 * \ingroup API
 * Sets the provided LED to blinking with given period and duty cycle.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] led LED identity
 * \param[in] period Blinking period in ms
 * \param[in] dutyCycle Blinking duty cycle in %
 *
 * \return 0 on success, -1 on error
 *
 */
int
SetLedBlinking(
    int           hdev,
    unsigned char led,
    unsigned int  period,
    unsigned char dutyCycle
);

/**
 * \ingroup API
 * Gets the board temperature.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] temperature Temperature of the board in degree Celsius
 *
 * \return success if 0, error otherwise
 *
 */
char
GetBoardTemperature(
    int   hdev,
    char* temperature
);

/**
 * \ingroup API
 * Sets the given WWAN or GNSS signal of modem to enable or disable.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] modem Modem identity (1 to 2)
 * \param[in] signal 0 (WWAN), 1 (GNSS)
 * \param[in] disable 0 (disable), 1 (enable)
 *
 * \return success if 0, error otherwise
 *
 */
char
SetWWANSignal(
    int           hdev,
    unsigned char modem,
    unsigned char signal,
    unsigned char disable
);

/**
 * \ingroup API
 * Gets the status WWAN signal of both modems on the board.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[out] buf Buffer of at least size 4 to be filled with status of WWAN and GNSS signals of
 *                 each of the modem.
 * \param[in] buf_length Length of buf
 *
 * \return success if 0, error otherwise
 *
 */
char
GetWWANSignal(
    int            hdev,
    unsigned char* buf,
    unsigned char  buf_length
);

/**
 * \ingroup API
 * Resets the modem.
 *
 * \param[in] hdev Handle to the HID raw device
 * \param[in] modem Modem identity (1 to 2)
 *
 * \return success if 0, error otherwise
 *
 */
char
ResetModem(
    int           hdev,
    unsigned char modem
);

/**
 * \ingroup API
 * Get status of powergood signal of board.
 *
 * \param[in] hdev Handle to the HID raw device
 *
 * \return 0 if powergood status is NOk, 1 if powergood status ok, error otherwise
 *
 */
char
GetPowerGoodStatus(int hdev);

/**
 * \ingroup API
 * Sets provided HID device into bootloader mode for firmware update.
 *
 * \param[in] hdev Handle to the HID raw device
 *
 */
void
SetBootloaderMode(int hdev);

/**
 * @todo Function to get firmware version of the board (HID device) could not be
 * implemented. libudev is required to read system attribute bcdDevice.
 * Possibility to find device and its bcdDevice attribute from sys path shall
 * be researched.
 *
 * @todo Function to get geographical address of board (HID device) is not
 * available due to limitation of hidraw IOCTLs. IOTCL is not available
 * to get string descriptor at specific index. Libusb does have such
 * function. Possibility to get string descriptor at index shall be researched.
 *
 */

#endif /* HIDAPI_MINIMAL_13Y063_90_H */
