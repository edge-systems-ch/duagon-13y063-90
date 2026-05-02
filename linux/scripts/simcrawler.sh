#!/usr/bin/env bash

#===================================================================================================
# Constant definitions
#===================================================================================================
# Tool specific constants
WIRELESS_INTERFACE_CFG_TOOL="13Y063-90"

# Option parsing
THIS_SCRIPT=$(basename "$0")
LONGOPTS=out:,geo:,single,help
OPTIONS=o:g:svh

#===================================================================================================
# Variable definitions
#===================================================================================================
output_file=""
write_output=false
geo_addr=""
single_modem=false
sim_max=0

#===================================================================================================
# Function definitions
#===================================================================================================
function print_usage()
{
    cat << EOM
Usage: $THIS_SCRIPT [OPTION]

Options:
    -o, --out <FILE>    output file to write csv data to
    -g, --geo <ADDR>    geo address of device to scan
    -s, --single        use only a single modem for crawling
    -h, --help          print help text
EOM
}

function print_help()
{
    print_usage
    cat << EOM

Description:
Get information on installed sim cards per device.
It will power on two modem slots at a time and read the information through modem manager.
Note:
Depending on the modems installed, changing the sim cards can take some time.
This tool requires mmcli and 13y063-90.

Examples:
$THIS_SCRIPT -o output.csv
EOM
}

function get_equipment_ids_on_bus()
{
    local target_bus_nr=$1
    equipment_id_1="N/A"
    equipment_id_2="N/A"

    for id in "${!modem_array[@]}"; do
        value=${modem_array[$id]}

        bus_nr=$(echo "$value" | cut -d- -f1)
        dev_nr=$(echo "$value" | cut -d- -f2)

         # Find modems on given USB bus and get the IDs
        if [[ "$bus_nr" == "$target_bus_nr" ]]; then

            if [[ "$dev_nr" == "1" ]]; then
                equipment_id_1=$id
            elif [[ "$dev_nr" == "2" ]]; then
                equipment_id_2=$id
            else
                echo "Something is wrong - more than 2 modems on USB bus $bus_nr!"
                exit 1
            fi
        fi
    done
}

function get_modem_info()
{
    # Enumeration / paths of modems change when they get reconnected
    modem_list=$(sudo mmcli -L | awk '{print $1;}' | tr " " "\n")

    # Find modems using their equipment id and store output of mmcli in slot[1-2]
    for modem in $modem_list
    do
        id=$(mmcli -m "$modem" | grep -oP 'equipment id:\s+\K(.+)')
        if [[ "$id" == "$equipment_id_1" ]]; then
            mmcli_out_slot1=$(mmcli -m "$modem")
        elif [[ "$id" == "$equipment_id_2" ]]; then
            mmcli_out_slot2=$(mmcli -m "$modem")
        fi
    done
}

function wait_for_modem() {
    local prev_modem_cnt
    local timeout_duration=60 # seconds

    prev_modem_cnt=$(mmcli -L | grep -c Modem/)

    # Start polling loop
    start_time=$(date +%s)
    while true; do

        cur_modem_cnt=$(mmcli -L | grep -c Modem/)

        # Timeout reached
        if [ $(( $(date +%s) - start_time )) -ge $timeout_duration ]; then
            echo "Timeout - no modems appeared"
            exit 1
        fi

        # Check if the output is longer than before (new modem)
        if [ "$cur_modem_cnt" -gt $((prev_modem_cnt)) ]; then
            break
        fi

        # Update previous line count and wait for a moment before the next check
        prev_modem_cnt=$cur_modem_cnt
        sleep 1
    done
}

#===================================================================================================
# The main function starts here.
#===================================================================================================

# Parse command line options
PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
if [[ $? -ne 0 ]]; then
    exit 1
fi

# Read getopt output this way to handle the quoting right
eval set -- "$PARSED"

# Handle all options
while true; do
    case "$1" in
        -h|--help)
            print_help
            exit 0
            ;;
        -o|--out)
            shift; # The arg is next in position args
            output_file=$1
            write_output=true
            shift
            ;;
        -g|--geo)
            shift; # The arg is next in position args
            geo_addr=$1
            shift
            ;;
        -s|--single)
            single_modem=true
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            print_usage
            exit 1
            ;;
    esac
done


# Check prerequisites
#===================================================================================================

if ! command -v mmcli &> /dev/null
then
    echo "'mmcli' could not be found. Make sure ModemManager is installed."
    exit 1
fi

if ! command -v $WIRELESS_INTERFACE_CFG_TOOL &> /dev/null
then
    echo "13y063-90 could not be found. Make sure the path is set correctly."
    exit 1
fi

if [ -z "$geo_addr" ]
then
    echo "No geo address set. Use -g option."
    exit 1
fi


# Setup
#===================================================================================================

hardware=$(sudo $WIRELESS_INTERFACE_CFG_TOOL -l | grep -oP 'Using tool for \K(\w+)')
if ! sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" &> /dev/null
then
    echo "$hardware at geographical address $geo_addr not available!"
    exit 1
fi

if [[ "$hardware" == "ME10" ]]; then
    sim_max=6
else
    sim_max=10
fi

declare -A modem_array

echo "* Power cycle both modem slots"
# Power off both modem slots
sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -p 1 0 &> /dev/null
sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -p 2 0 &> /dev/null

sleep 2

# Power on both modem slots
sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -p 1 1 &> /dev/null
sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -p 2 1 &> /dev/null

# Wait until at least one modem is detected by ModemManager
wait_for_modem

echo "Modem(s) signaled power-up"

# Get latest modem added
modem_added=$(sudo mmcli -L | grep Modem/ |tail -1 | awk '{print $1;}')

# Get the USB bus the modem is attached to
modem_bus=$(mmcli -m "$modem_added" | grep -oP 'device: /sys/devices/pci.+/usb\d+/\K(\d+)')

# Store paths to all available modems
modem_list=$(sudo mmcli -L | awk '{print $1;}' | tr " " "\n")

if [ -z "${#modem_list[@]}" ]
then
    echo "No modems found."
    exit 1
fi

# Store modem equipment ids (IMEI) in array to identify later
for modem in $modem_list
do
    modem_info=$(mmcli -m "$modem")
    id=$(echo "$modem_info" | grep -oP 'equipment id:\s+\K(.+)')

    # Get USB bus and device number in form of "BUS-DEVICE"
    usb_bus=$(echo "$modem_info" | grep -oP 'device: /sys/devices/pci.+/usb\d+/\K(\d+-\d)')

    # Store equipment id (IMEI) and its USB information
    modem_array+=(["$id"]="$usb_bus")
done

# Get unique bus numbers - each physical G239 / ME10 / F229 device has it's own bus number
IFS=" " read -r -a sorted_unique_usb_bus_nrs <<< "$(echo "${modem_array[@]}" | tr ' ' '\n' | cut -d- -f1 | sort -u | tr '\n' ' ')"

echo "Found modems on USB bus #:" "${sorted_unique_usb_bus_nrs[@]}"
echo "Using modems on USB bus #$modem_bus corresponding to geoaddress $geo_addr"


# Holds the current modem info from mmcli
mmcli_out_slot1=""
mmcli_out_slot2=""


# Start crawling
#===================================================================================================

get_equipment_ids_on_bus "$modem_bus"
#echo "modem ids on bus $usb_bus: $equipment_id_1 $equipment_id_2"

if [[ "$equipment_id_2" == "N/A" ]]; then 
    echo "Only one modem found - using single mode"
    single_modem=true
fi

echo "* Start crawling (this may take a while).."

# If output to file - write CSV header
if "$write_output"; then
    printf "Modem Slot;Model;IMEI;SIM Slot;Phone Number" > "$output_file"
fi

printf  "\nModem Slot | Modem Model             | IMEI          | SIM Slot | Phone Number  \n"
printf -- "-----------+-------------------------+---------------+----------+---------------\n"

# Iterate over SIMs depending on the modem count
if $single_modem; then
    sim_step=1
    # Assign no SIM card to second modem
    sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -s 2 0 &> /dev/null
else
    sim_step=2
fi

for sim_num in $(seq 1 $sim_step $sim_max)
do 
    # Switching SIM on modem 1
    sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -s 1 "$sim_num" &> /dev/null

    # Switching SIM on modem 2
    if ! $single_modem; then
        ((sim_num++))
        sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -s 2 "$sim_num" &> /dev/null
    fi
    sleep 2

    # Wait for modem manager to detect modem
    wait_for_modem # times out after 60s

    get_modem_info # updates mmcli_out_slot*

    # Determine IMEI of wireless modules 
    # Note: IMEI is the same as equipment_id reported by ModemManager
    imei1=$equipment_id_1

    # Determine phone numbers assigned to each IMEI
    phone_number1=$(echo "$mmcli_out_slot1" | grep -oP 'own:\s+\K(.+)')

    # Determine which sim slot is in use using the duagon wireless tool 13Y063-90
    simslot1=$(sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -s | grep -oP '(?<=Modem #1 uses SIM card #)\d+')

    # Get model numbers
    model1=$(echo "$mmcli_out_slot1" | grep -oP 'model:\s+\K(.+)')

    printf "1          |%-25s|%-15s|%-10s|%-15s\n" "$model1" "$imei1" "$simslot1" "$phone_number1"

    if  $write_output; then
        # Append results to CSV
        printf "1;%s;%s;%s;%s;\n" "$model1" "$imei1" "$simslot1" "$phone_number1" >> "$output_file"
    fi

    # Repeat steps for modem 2
    if ! $single_modem; then
        imei2=$equipment_id_2
        phone_number2=$(echo "$mmcli_out_slot2" | grep -oP 'own:\s+\K(.+)')
        simslot2=$(sudo $WIRELESS_INTERFACE_CFG_TOOL -g "$geo_addr" -s | grep -oP '(?<=Modem #2 uses SIM card #)\d+')
        model2=$(echo "$mmcli_out_slot2" | grep -oP 'model:\s+\K(.+)')

        printf "2          |%-25s|%-15s|%-10s|%-15s\n" "$model2" "$imei2" "$simslot2" "$phone_number2"

        if  $write_output; then
            printf "2;%s;%s;%s;%s;\n" "$model2" "$imei2" "$simslot2" "$phone_number2" >> "$output_file"
        fi
    fi
done
