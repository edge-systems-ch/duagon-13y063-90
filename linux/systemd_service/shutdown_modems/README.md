# Systemd Service to shut down all cell modems
This service shuts down all connected cell modems in the required manner during shutdown, halt or reboot
by using the 13Y063-90 Linux wireless interface configuration tool. 

## Requirements
- The 13Y063-90 or 13Y063-90_minimal was built and is located under /usr/bin/.

## How to install shutdown_modems service
1. Systemd service file:\
   Copy `shutdown_modems.service` to /usr/lib/systemd/system/

2. Bash script executed by shutdown_modems.service:\
   Copy `cell_modem_power_ctrl.sh` to /usr/bin/\
   *(The location /usr/bin for this script is defined in shutdown_modems.service.)*

3. Enable systemd service:\
   `~$ systemctl enable --now shutdown_modems`

The systemd service is started immediately (\-\-now) and will activate automatically at reboot, halt or shutdown. Journalctl will report a successful modem shutdown and the modems will be powered off.

## Configuration
- shutdown_modems.service:\
  Location path for the 13Y063-90 Linux wireless interface configuration tool, cell_modem_power_ctrl.sh script and log file.

- cell_modem_power_ctrl.sh:\
  If it is necessary to use the minimum version of the Linux wireless interface configuration tool, a constant needs to be adjusted as follows:\
  `MINIMAL_VERSION=true`
