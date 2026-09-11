/***************************************************************************//**
 * @file
 * @brief custom_commands.h
 * Definition of custom CPC commands for both the host and RCP firmware
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef CPC_COMMANDS_H_
#define CPC_COMMANDS_H_

enum CustCpcCommand {
  CPC_COMMAND_GET_CUST_VERSION=1,
  CPC_COMMAND_GET_SE_VERSION,
  CPC_COMMAND_GET_CTUNE_TOKEN,
  CPC_COMMAND_SET_CTUNE_TOKEN,
  CPC_COMMAND_GET_CTUNE_VALUE,
  CPC_COMMAND_SET_CTUNE_VALUE,
  CPC_COMMAND_TONE_START,
  CPC_COMMAND_TONE_STOP,
  CPC_COMMAND_GPIO_WRITE,
  CPC_COMMAND_ERASE_USERDATA_PAGE,
  CPC_COMMAND_GET_BTL_VERSION,
  CPC_COMMAND_GET_APP_PROPERTIES_VERSION
};

#endif /* CPC_COMMANDS_H_ */
