#!/usr/bin/env bash

# Exit on error. Append "|| true" if you expect an error.
#set -o errexit
# Exit on error inside any functions or subshells.
#set -o errtrace
# Turn on traces, useful while debugging but commented out by default
#set -o xtrace

########################################
#Global constants
########################################
# Customer specific constants
MINIMAL_VERSION=false
LOGFILE="cell_modem_power_ctrl.log"

# Tool specific constants
WIRELESS_INTERFACE_CFG_TOOL="13Y063-90"
# The 13Y063-90_minimal tool cannot read the firmware version, geographical address and serial number of the board
WIRELESS_INTERFACE_CFG_TOOL_MINIMAL="13Y063-90_minimal"

# Modem specific constants
MODEM_SHUTDOWN_TIME_MS="3000"

# Modules on carrier
MODULE_M2_1="1"
MODULE_M2_2="2"

# Misc
POWER_ON="1"
POWER_OFF="0"

#FUNCTIONS
################################################################################

function usage() {
    echo ""
    echo "This script controls the power supply for all mounted cellular modems in a duagon carrier board like F229, ME10 or G239: "
    echo ""
    echo "usage: ./cell_modem_power_ctrl --power off --logdir /var/log/"
    echo "   "
    echo "   "
    echo "  -p | --power            : set the power state for all connected cell modems. Possible values are: <on> or <off>"
    echo "  -l | --logdir           : Folder where logfiles are stored (default: /var/log)"
    echo "  -h | --help             : This message"
}

function parse_args() {
    # named args
    while [[ "$1" != "" ]]; do
        case "$1" in
        -l | --logdir)
            LOG_PATH="$2"
            shift
            ;;
        -p | --power)
            POWER="$2"
            shift
            ;;
        -h | --help)
            usage
            exit
            ;; # quit and show usage
        *) ;;
        esac
        shift # move to next kv pair
    done

    # set defaults
    if [[ -z "${LOG_PATH}" ]]; then
        LOG_PATH="/var/log/"
    fi

    if [[ -z "${POWER}" ]]; then
        echo "missing argument for the --power! Exit.."
        exit 1
    fi
}

function prep_log_files() {
    #Define Logile and Output
    OUTPUT="${LOG_PATH}/${LOGFILE}"
    CHECK="test -e ${OUTPUT}"

    if $CHECK; then
        rm -rf ${OUTPUT}
        touch ${OUTPUT}
    else
        touch ${OUTPUT}
    fi
}

function update_time() {
    TIMESTAMP=$(date +"%Y-%m-%d_%H:%M:%S")
}

function log() {
    local arg_log_type="$1"
    local arg_log_msg="$2"

    if [[ ${arg_log_msg} == "" ]]; then
        echo "" | tee -a ${OUTPUT}
    else
        update_time
        echo "[${TIMESTAMP}] ${arg_log_type}: ${arg_log_msg}" | tee -a ${OUTPUT}
    fi
}

function get_modem_smb_addr() {
    ${WIRELESS_INTERFACE_CFG_TOOL} -l | grep -o 'Geographical Address.\{6\}' | awk '{print $3}'
    if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
        log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
        exit 1
    fi
}

function num_of_modems() {
    wc -w <<<$(get_modem_smb_addr)
}

function cell_modem_power_ctrl() {

    if [[ $(num_of_modems) -eq 1 ]]; then
        if cell_modem_power_ctrl_minimal; then
            return 0
        else
            return 1
        fi
    else
        for modem_smb_addr in $(get_modem_smb_addr); do
            if [[ $POWER == "off" ]]; then
                PWR_CMD="${POWER_OFF}"
                log INFO "set specific modem shutdown time: ${MODEM_SHUTDOWN_TIME_MS}ms for SMB ADDR: $modem_smb_addr.."
                "${WIRELESS_INTERFACE_CFG_TOOL}" -g $modem_smb_addr -f "${MODEM_SHUTDOWN_TIME_MS}" | tee -a ${OUTPUT}
                if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
                    log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
                    exit 1
                fi
            elif [[ $POWER == "on" ]]; then
                PWR_CMD="${POWER_ON}"
            else
                log ERROR "No valid parameter! Valid values are <on> or <off>."
                return 1
            fi

            for module in "${MODULE_M2_1}" "${MODULE_M2_2}"; do
                log INFO ""
                log INFO "-----------------------------------------------------------------------"
                log INFO "Power ${POWER} module #$module on SMB-Address $modem_smb_addr.."
                log INFO "-----------------------------------------------------------------------"
                "${WIRELESS_INTERFACE_CFG_TOOL}" -g $modem_smb_addr -p $module "${PWR_CMD}" | tee -a ${OUTPUT}
                if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
                    log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
                    exit 1
                fi
            done
        done
    fi

    return 0
}

function cell_modem_power_ctrl_minimal() {
    # This function can be used without geographical address of the board
    if [[ $POWER == "off" ]]; then
        PWR_CMD="${POWER_OFF}"
        log INFO "set specific modem shutdown time: ${MODEM_SHUTDOWN_TIME_MS}ms.."
        "${WIRELESS_INTERFACE_CFG_TOOL}" -f "${MODEM_SHUTDOWN_TIME_MS}" | tee -a ${OUTPUT}
        if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
            log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
            exit 1
        fi
    elif [[ $POWER == "on" ]]; then
        PWR_CMD="${POWER_ON}"
    else
        log ERROR "No valid parameter! Valid values are <on> or <off>."
        return 1
    fi

    for module in "${MODULE_M2_1}" "${MODULE_M2_2}"; do
        log INFO ""
        log INFO "-----------------------------------------------------------------------"
        log INFO "Power ${POWER} module #$module.."
        log INFO "-----------------------------------------------------------------------"
        "${WIRELESS_INTERFACE_CFG_TOOL}" -p $module "${PWR_CMD}" | tee -a ${OUTPUT}
        if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
            log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
            exit 1
        fi
    done

    return 0
}

function wait_for_cell_modem_reaction() {
    wait_for_cell_modem_reaction_s=$((("${MODEM_SHUTDOWN_TIME_MS}" + 999) / 1000))
    sleep $wait_for_cell_modem_reaction_s
}

function show_module_power_states() {
    if [[ $(num_of_modems) -eq 1 ]]; then
        log INFO "Module power states:"
        "${WIRELESS_INTERFACE_CFG_TOOL}" -p | grep -m 2 'Module #' | tee -a ${OUTPUT} # <-m 2> hides powered module #3 (PCI Express Mini card) which doesn`t need to be shut down
        if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
            log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
            exit 1
        fi
    else
        for modem_smb_addr in $(get_modem_smb_addr); do
            log INFO ""
            log INFO "Module power states on SMB-Address <$modem_smb_addr>:"
            log INFO ""
            "${WIRELESS_INTERFACE_CFG_TOOL}" -g $modem_smb_addr -p | grep 'Module #' | tee -a ${OUTPUT}
            if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
                log ERROR ""${WIRELESS_INTERFACE_CFG_TOOL}" terminates due to an error!"
                exit 1
            fi
        done
    fi
}

#MAIN
################################################################################

parse_args "$@"
prep_log_files

log INFO ""
log INFO "-----------------------------------------------------------------------"
log INFO "Start power control for cellular modems carried on F229, ME10 or G239."
log INFO "-----------------------------------------------------------------------"
log INFO ""

if $MINIMAL_VERSION; then
    WIRELESS_INTERFACE_CFG_TOOL=${WIRELESS_INTERFACE_CFG_TOOL_MINIMAL}
    if ! cell_modem_power_ctrl_minimal; then
        log ERROR "cell_modem_power_ctrl_minimal() terminates due to an error!"
        exit 1
    fi
else
    if ! cell_modem_power_ctrl; then
        log ERROR "cell_modem_power_ctrl() terminates due to an error!"
        exit 1
    fi
fi

wait_for_cell_modem_reaction
show_module_power_states

exit 0
