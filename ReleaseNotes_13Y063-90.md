# Release Notes 13Y063-90 2.02

## Included Components
13Y063-90  

## Release Description
Quality of life release - updated used libaries and added helper script 'simcrawler.sh' to automatically scan and print information of all installed simcards on a device.  


## Changelog

## [2.02] - 2025-07-08

### Added
- Helper script linux/scripts/simcrawler.sh
- Updated library cJSON to 1.7.18
- Updated library hidapi to 0.14.0

### Update Impacts
- None

## [2.01] - 2023-09-29

### Fixed
- All errors and warnings are outputted to stderr

### Added
- New option: -j for machine readable JSON output for 'get' commands
- Backward compatibility for legacy firmwares:
    - Detect hardware models correctly for firmwares < 2.00 
    - Workaround for power-set bug found in firmware < 2.03 (ME10, G239) and < 1.02 (F229)   
    (MAIN_PR008273 - Program ignores consecutively executed calls)

### Update Impacts
- None

## [2.00] - 2023-05-25

### Fixed
- Inconsistent option handling behavior (MAIN_PR008280 - Command line arguments not consistent)
- Setting the power with -p will now wait until the change is reported as applied by firmware (MAIN_PR008273 - Program ignores consecutively executed calls)

### Added
- Support of long options
- New option: --save-config replaces ambiguous -c -s (still supported for backwards compatibility)
- New option: -i to select card by index
- 13Y063-90_minimal: Added missing options -l and -i to match the non-minimal version

### Removed
- Interactive board selection as it is not script friendly - if several boards are in a system use explicit selection with -i, -s or -g

### Update Impacts
- **Tool v2.00 is not compatible with firmware versions < 2.03 (ME10, G239) and < 1.02 (F229)**
- Timing behavior changed: When setting power state with -p the tool will block until the modem signals the actual state change, or the configured modemOffTime elapsed. Especially when shutting down some modems require ~8 seconds until they can be safely powered off.


### Update Verification
When running the tools check for the version strings in the header to match:
`13Y063-90_02_02` and
`13Y063-90_minimal_02_00`


## How To Use, Dependencies & Build Instructions
See README.md
