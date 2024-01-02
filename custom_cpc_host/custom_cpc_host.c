/***************************************************************************//**
 * @file
 * @brief custom_cpc_host.c
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
#include "sl_cpc.h"
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>
#include <stdio.h>
#include "string.h"
#include "cpc_commands.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef APP_VERSION_MAJOR
#define APP_VERSION_MAJOR 0
#endif
#ifndef APP_VERSION_MINOR
#define APP_VERSION_MINOR 1
#endif

#define ENABLE_TRACING false //if true, prints debug info to stderr
#define debug_print(...) \
            do { if (DEBUG) printf(__VA_ARGS__); } while (0)

#define OPTSTRING "hv"

static struct option long_options[] = {
     {"help",       no_argument,       0,  'h' },
     {"cust_version",     no_argument, 0,  'a' },
     {"se_version",      no_argument, 0,  'b' },
     {"get_ctune_token",  no_argument, 0,  'c' },
     {"set_ctune_token",     required_argument, 0,  'd' },
     {"get_ctune_value",      no_argument, 0,  'e' },
     {"set_ctune_value",  required_argument, 0, 'f' },
     {"tone_start",  no_argument, 0,  'g' },
     {"tone_stop",  no_argument, 0,  'i' },
     {"gpio_write",  required_argument, 0,  'j' },
     {"erase_userdata_page",  no_argument, 0,  'k' },
     {"version", no_argument, 0, 'v'},
     {0,           0,                 0,  0  }};

#define HELP_MESSAGE \
"./custom_cpc_host <arguments>\n"\
"                                   \n"\
"ARGUMENTS: \n"\
"--help        \n" \
"-h                         Prints help message.\n"\
"--cust_version             Returns 32-but customer version defined in the RCP firmware application (CUSTOMER_VERSION).\n"\
"--se_version               Returns the Secure Element version on the series 2 device running the RCP firmware. \n"\
"--get_ctune_token          Reads the CTUNE manufacturing token stored in userdata flash on the RCP target. This is a 16-bit value and will \n"\
"                             be FF FF when not already flashed/programmed \n"\
"--set_ctune_token <value>  Writes the CTUNE manufacturing token stored in userdata flash on the RCP target. Note that if the CTUNE \n"\
"                             manufacturing token is already written, this call will fail as the value can only be written if blank. \n"\
"--get_ctune_value          Reads the CTUNE register value currently set in firmware running on the RCP target. This is a 16-bit value\n"\
"--set_ctune_value <value>  Sets the CTUNE register value in firmware running on the RCP target. This is a 16-bit value\n"\
"                             NOTE: The radio needs to be in idle mode for this command to succeeed\n"\
"--gpio_write <value>       Writes the value of the GPIO pins(s) as determined by the RCP firmware. In the example firmware, \n"\
"                             value=\"1\" turns on the LED on the BRD4181B and value=\"0\" turns it off.\n"\
"--erase_userdata_page      Erase the page on the RCP device containing the manfacturing tokens, including the CTUNE manufacturing token. \n"\
"                             This allows a previously written CTUNE manufacturing token to be written to a new value.\n"\
"                             WARNING - any other values stored in the userdata page will also be erased, so use caution\n"\
"--tone_start               Enable a CW tone on the transmitter of the RCP at the 802.15.4 channel defined in the RCP firmware and at\n"\
"                             a default power level. \n"\
"--tone_stop                Disable the CW tone on the transmitter of the RCP.\n"\
"--version                  Prints the version of the host application.\n"\
"\n"\

#ifndef DEFAULT_CHANNEL
#define DEFAULT_CHANNEL 11
#endif
#ifndef DEFAULT_POWER_DDBM
#define DEFAULT_POWER_DDBM 0
#endif

#define TIMEOUT_SECONDS 5

#define TX_WINDOW_SIZE 1 //only 1 supported for now

static cpc_handle_t lib_handle;
static cpc_endpoint_t endpoint;

static void connectcpc(){
  uint8_t retry = 0;
  int ret;
  do {
    ret = cpc_init(&lib_handle, NULL, ENABLE_TRACING, NULL);
    if (ret == 0) {
      // speed up boot process if everything seems ok
      break;
    }
    nanosleep((const struct timespec[]){{ 0, 100000000L } }, NULL);
    retry++;
  } while ((ret != 0) && (retry < TIMEOUT_SECONDS));

  if (ret < 0) {
    fprintf(stderr,"cpc_init returned with %d\n", ret);
    exit(EXIT_FAILURE);
  }
  
  ret = cpc_open_endpoint(lib_handle,
                          &endpoint,
                          SL_CPC_ENDPOINT_USER_ID_0,
                          TX_WINDOW_SIZE);
  if (ret < 0) {
    fprintf(stderr,"cpc_open_endpoint returned with %d\n", ret);
    exit(EXIT_FAILURE);
  }
}

static void sendCmd(uint8_t *buffer, uint8_t len){
  cpc_write_endpoint(endpoint, buffer, (size_t) len, CPC_ENDPOINT_WRITE_FLAG_NONE);
}

// Read RCP data from CPC
static ssize_t getReply(uint8_t *buffer){
  uint8_t retry = 0;
  ssize_t size;

  do {
    size = cpc_read_endpoint(endpoint,
                            buffer,
                            SL_CPC_READ_MINIMUM_SIZE,
                            CPC_ENDPOINT_EVENT_FLAG_NON_BLOCKING);
    nanosleep((const struct timespec[]){{ 0, 100000000L } }, NULL);
    retry++;
    debug_print("cpc_read_endpoint attempt, size/status=%d, retry #%d\r\n", 
        size,retry);
  } while ((size <= 0) && (retry < TIMEOUT_SECONDS));

  return size;
}

static void disconnectcpc(){
  //TODO: wait for endpoint to close with timeout?
  int ret;
  uint8_t retry=0;
  cpc_endpoint_state_t state;
  debug_print("Closing endpoint...\r\n");
  ret = cpc_close_endpoint(&endpoint);
  if (ret != 0) {
    printf("cpc_close_endpoint error %d\r\n", ret);
  }
  debug_print("waiting for endpoint to close...\r\n");
  do {
    ret = cpc_get_endpoint_state(lib_handle, SL_CPC_ENDPOINT_USER_ID_0,&state);
    nanosleep((const struct timespec[]){{ 0, 100000000L } }, NULL);
    retry++;
  } while ((state != SL_CPC_STATE_CLOSED) && (retry <= TIMEOUT_SECONDS));
  debug_print("endpoint closed\r\n");
}


int main(int argc, char* argv[]) {
    int opt = 0;
    enum CustCpcCommand custCommand;
    uint16_t ctune_val;
    uint8_t cpc_tx_buf[SL_CPC_READ_MINIMUM_SIZE];
    uint8_t gpio_out_val;
    ssize_t len;

    if (argc < 2)
    {
      printf(HELP_MESSAGE);
      exit(0);
    }
    // Process command line options.
    while ((opt = getopt_long(argc, argv, OPTSTRING, long_options, NULL)) != -1) {
      switch (opt) {
        case 'h':
          printf(HELP_MESSAGE);
          exit(0);
        break;

        case 'a':
          custCommand = CPC_COMMAND_GET_CUST_VERSION;
          debug_print("CPC_COMMAND_GET_CUST_VERSION\r\n");
        break;

        case 'b':
          custCommand = CPC_COMMAND_GET_SE_VERSION;
        break;

        case 'c':
          custCommand = CPC_COMMAND_GET_CTUNE_TOKEN;
        break;

        case 'd':
          custCommand = CPC_COMMAND_SET_CTUNE_TOKEN;
          ctune_val = (uint16_t) strtoul(optarg,NULL,0);
          // TODO: check range?
        break;

        case 'e':
          custCommand = CPC_COMMAND_GET_CTUNE_VALUE;
        break;

        case 'f':
          custCommand = CPC_COMMAND_SET_CTUNE_VALUE;
          ctune_val = (uint16_t) strtoul(optarg,NULL,0);
          // TODO: check range?
        break;

        case 'g':
          custCommand = CPC_COMMAND_TONE_START;
        break;

        case 'i':
          custCommand = CPC_COMMAND_TONE_STOP;
        break;

        case 'j':
          custCommand = CPC_COMMAND_GPIO_WRITE;
          gpio_out_val = atoi(optarg);
        break;

        case 'k':
          custCommand = CPC_COMMAND_ERASE_USERDATA_PAGE;
        break;

        case 'v':
          //print app version info and exit
          printf("App Version: %d.%d\r\n", APP_VERSION_MAJOR, APP_VERSION_MINOR);
          exit(0);
          break;
          
        default:
        break;
      }
    }

    connectcpc();

    cpc_tx_buf[0] = (uint8_t) custCommand;
    switch (custCommand) {
      case CPC_COMMAND_GET_CUST_VERSION:
      case CPC_COMMAND_GET_SE_VERSION:
      case CPC_COMMAND_GET_CTUNE_TOKEN:
      case CPC_COMMAND_GET_CTUNE_VALUE:
      case CPC_COMMAND_TONE_START:
      case CPC_COMMAND_TONE_STOP:
      case CPC_COMMAND_ERASE_USERDATA_PAGE:
        /* For these, just send command and print result and exit*/
        debug_print("sending command 0x%x with no argument\r\n", custCommand);
        sendCmd(cpc_tx_buf,1);
      break;

      case CPC_COMMAND_GPIO_WRITE:
        debug_print("sending gpio command with argument %d\r\n", gpio_out_val);
        memcpy(&cpc_tx_buf[1],&gpio_out_val,1); //send gpio val
        sendCmd(cpc_tx_buf,2);
      break;

      case CPC_COMMAND_SET_CTUNE_TOKEN:
        debug_print("setting ctune token to 0x%x\r\n", ctune_val);
        memcpy(&cpc_tx_buf[1], &ctune_val, sizeof(uint16_t));
        sendCmd(cpc_tx_buf,3);
      break;

      case CPC_COMMAND_SET_CTUNE_VALUE:
        debug_print("setting ctune value to 0x%x\r\n", ctune_val);
        memcpy(&cpc_tx_buf[1], &ctune_val, sizeof(uint16_t));
        sendCmd(cpc_tx_buf,3);
      break;

      default:
        printf("No command!\r\n");
        printf(HELP_MESSAGE);
        exit(EXIT_FAILURE);
      break;
    }

    /* Always receive and print reply (may just be a status byte)*/
    len = getReply(cpc_tx_buf);
    printf("Reply to command 0x%x, len=%d: ",custCommand, len);
    if (len >= 0) {
      for (uint8_t i=0;i<len;i++) {
        printf("0x%x ", cpc_tx_buf[i]);
      }
      printf("\r\n");
    } else {
      printf("read timeout! cpc_read_endpoint last returned %s\r\n", strerror(-len));
    }

    disconnectcpc();
    exit(0);
}