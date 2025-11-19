# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2025-11-19
### Added
- added code to RCP firmware to use SE for xg21 userdata write and MSC API for non-xg21 userdata write

## Changed
- minor changes to RCP code to remove warnings

## [0.2.0] - 2024-02-04
### Added
- user friendly error string on cpc_init
- command to get bootloader version running on RCP target
- command to get the app version from the Application_Properties_t struct of the RCP application

## [0.1.1] - 2024-01-16
### Changed 
- Adds detail to the documentation for components required to enable SWO debug
- Firmware adds the ability to run both on FreeRTOS (rcp-uart-802154-blehci) and bare metal (rcp-uart-802154) RCP firmware implementations
- Added some additional debug printing to firmware

### Added
- Changelog file

## [0.1.0] - 2024-01-02

Initial release
