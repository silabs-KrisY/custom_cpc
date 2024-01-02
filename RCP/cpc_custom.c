/***************************************************************************//**
 * @file
 * @brief cpc_custom.c
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

#include <cpc_custom.h>
#include "cpc_commands.h"
#include "sl_cpc.h"
#include <stdint.h>
#include <stdio.h>
#include "em_gpio.h"
#include "task.h"
#include "rail_ieee802154.h"
#include "rail.h"
#include "sl_se_manager_util.h" // include for SE manager API

#define SWODEBUG 1

#define debug_print(...) \
            do { if (SWODEBUG) printf(__VA_ARGS__); } while (0)

// Fetch CTUNE value from USERDATA page as a manufacturing token
#define USERDATA_CTUNE_OFFSET  0x100 //EFR32xG21 specific
#define MFG_CTUNE_ADDR (USERDATA_BASE + USERDATA_CTUNE_OFFSET)
#define MFG_CTUNE_VAL  (*((uint16_t *) (MFG_CTUNE_ADDR)))

static sl_cpc_endpoint_handle_t custom_endpoint_handle;
typedef enum {
  CPC_ENDPOINT_CLOSED,
  CPC_ENDPOINT_OPEN,
  CPC_ENDPOINT_OPEN_WAIT_FOR_CONNECTION,
  CPC_ENDPOINT_CONNECTED,
  CPC_ENDPOINT_DISCONNECTED,
} cpc_endpoint_status_t;

static cpc_endpoint_status_t endpoint_status = CPC_ENDPOINT_CLOSED;
static volatile bool send_button_pressed = false;
extern RAIL_Handle_t emPhyRailHandle;

// 32-bit customer version (can be overridden global compiler define)
#ifndef CUSTOMER_VERSION
#define CUSTOMER_VERSION 0x12345678
#endif
const uint32_t customer_version = CUSTOMER_VERSION;

#define DEFAULT_802154_CH 11 //default channel 11

// Context for SE command(s)
sl_se_command_context_t cmd_ctx;

uint8_t* tx_ptr=NULL; // ptr for TX

static void process_command(uint8_t *commandData, uint16_t size){
  RAIL_Status_t rail_status;
  sl_status_t slstatus=SL_STATUS_OK;
  uint32_t ctune_val=0u;
  uint32_t se_version;

  //int16_t powerDDbm;
  //uint16_t channel;
  //uint8_t cpc_buf[256];
  uint8_t transmit_len;
  EFM_ASSERT(tx_ptr == NULL); //don't overwrite previous buffer - shouldn't happen?
  // TODO: check size?

  switch ( commandData[0] ){
    case CPC_COMMAND_GET_CUST_VERSION:
      debug_print("Cmd received: CPC_COMMAND_GET_CUST_VERSION\r\n");
      tx_ptr = pvPortMalloc(sizeof(customer_version));
      memcpy(tx_ptr,&customer_version,sizeof(customer_version));
      transmit_len = sizeof(customer_version);
      break;

    case CPC_COMMAND_GET_SE_VERSION:
      debug_print("Cmd received: CPC_COMMAND_GET_SE_VERSION\r\n");
      slstatus = sl_se_get_se_version(&cmd_ctx, &se_version);
      debug_print("sl_se_get_se_version status 0x%lx\r\n", slstatus);
      tx_ptr = pvPortMalloc(sizeof(se_version));
      memcpy(tx_ptr,&se_version,sizeof(se_version));
      transmit_len = sizeof(se_version);
      break;

    case CPC_COMMAND_GET_CTUNE_TOKEN:
      debug_print("Cmd received: CPC_COMMAND_GET_CTUNE_TOKEN\r\n");
      tx_ptr = pvPortMalloc(sizeof(MFG_CTUNE_VAL));
      memcpy(tx_ptr,&MFG_CTUNE_VAL,sizeof(MFG_CTUNE_VAL));
      transmit_len = sizeof(MFG_CTUNE_VAL);
      break;

    case CPC_COMMAND_SET_CTUNE_TOKEN:
      debug_print("Cmd received: CPC_COMMAND_SET_CTUNE_TOKEN\r\n");
#if SWODEBUG
      printf("commandData[1] = 0x%x, commandData[2]= 0x%x\r\n", commandData[1], commandData[2]);
#endif
      // copy received data into lsb as uint_16
      memcpy(&ctune_val, &commandData[1], sizeof(uint16_t));
      // ctune is lower 16-bits, upper 16-bits are all 0xffff
      ctune_val = (ctune_val & 0x0000ffff) | 0xffff0000;
      debug_print("writing ctune token 0x%lx\r\n", ctune_val);
      slstatus = sl_se_write_user_data(&cmd_ctx, USERDATA_CTUNE_OFFSET, &ctune_val, 4);
      debug_print("sl_se_write_user_data status 0x%lx\r\n", slstatus);
      tx_ptr = pvPortMalloc(sizeof(uint16_t)); // two byte status reply
      memcpy(tx_ptr, &slstatus, sizeof(uint16_t)); //copy lower two bytes of slstatus
      transmit_len = sizeof(uint16_t);
      break;

    case CPC_COMMAND_GET_CTUNE_VALUE:
      debug_print("Cmd received: CPC_COMMAND_GET_CTUNE_VALUE\r\n");
      ctune_val = (uint16_t) RAIL_GetTune(emPhyRailHandle);
      debug_print("RAIL_GetTune returned 0x%lx",ctune_val);
      tx_ptr = pvPortMalloc(sizeof(uint16_t)); // return ctune_val as uint16_t
      memcpy(tx_ptr,&ctune_val,sizeof(uint16_t));
      transmit_len = sizeof(uint16_t);
      break;

    case CPC_COMMAND_SET_CTUNE_VALUE:
     debug_print("Cmd received: CPC_COMMAND_SET_CTUNE_VALUE\r\n");
#if SWODEBUG
     printf("commandData[1] = 0x%x, commandData[2]= 0x%x\r\n", commandData[1], commandData[2]);
#endif
     // copy received data into lsb as uint_16
     memcpy(&ctune_val, &commandData[1], sizeof(uint16_t));
     debug_print("writing ctune value 0x%lx\r\n", ctune_val);
     rail_status = RAIL_SetTune(emPhyRailHandle,ctune_val);
     debug_print("RAIL_SetTune 0x%x\r\n", rail_status);
     tx_ptr = pvPortMalloc(sizeof(rail_status)); // rail_status reply
     memcpy(tx_ptr, &rail_status, sizeof(rail_status)); //copy rail_status
     transmit_len = sizeof(rail_status);
     break;

    case CPC_COMMAND_GPIO_WRITE:
      // write a received value to GPIO(s)
      debug_print("Cmd received: CPC_COMMAND_GPIO_WRITE\r\n");
      debug_print("gpio write value %d\r\n", commandData[1]);
      GPIO_PinModeSet(gpioPortD, 2, gpioModePushPull, commandData[1]);
      // return default status (SL_STATUS_OK)
      tx_ptr = pvPortMalloc(sizeof(uint16_t));
      memcpy(tx_ptr, &slstatus, sizeof(uint16_t)); //copy lower two bytes of slstatus
      transmit_len = sizeof(uint16_t);
      break;

    case CPC_COMMAND_TONE_START:
      debug_print("Cmd received: CPC_COMMAND_TOME_START\r\n");
      // start CW stream on specified channel
      //TODO: read channel here
      rail_status = RAIL_StartTxStream(emPhyRailHandle, DEFAULT_802154_CH, RAIL_STREAM_CARRIER_WAVE);
      debug_print("RAIL_StartTxStream(), status=0x%x\r\n",rail_status);
      tx_ptr = pvPortMalloc(sizeof(rail_status)); // rail_status reply
      memcpy(tx_ptr, &rail_status, sizeof(rail_status)); //copy 1B rail_status
      transmit_len = sizeof(rail_status);
      break;

    case CPC_COMMAND_TONE_STOP:
      debug_print("Cmd received: CPC_COMMAND_TONE_STOP\r\n");
      // stop CW stream
      rail_status = RAIL_StopTxStream(emPhyRailHandle);
      debug_print("RAIL_StopTxStream(), status=0x%x\r\n",rail_status);
      tx_ptr = pvPortMalloc(sizeof(rail_status)); // rail_status reply
      memcpy(tx_ptr, &rail_status, sizeof(rail_status)); //copy 1B rail_status
      transmit_len = sizeof(rail_status);
      break;

    case CPC_COMMAND_ERASE_USERDATA_PAGE:
      debug_print("Cmd received: CPC_COMMAND_ERASE_USERDATA_PAGE\r\n");
      slstatus = sl_se_erase_user_data(&cmd_ctx);
      debug_print("sl_se_erase_user_data status 0x%lx\r\n", slstatus);
      tx_ptr = pvPortMalloc(sizeof(uint16_t)); // two byte status reply
      memcpy(tx_ptr, &slstatus, sizeof(uint16_t)); //copy lower two bytes of slstatus
      transmit_len = sizeof(uint16_t);
      break;

    default:
      debug_print("default case hit on cmd rcv\r\n");
      transmit_len = 0; //no tx reply
      break;
  }
  if (transmit_len > 0) {
    slstatus = sl_cpc_write(&custom_endpoint_handle,
                              tx_ptr,
                              transmit_len,
                              0,
                              NULL); //no flag, no write complete arg
    debug_print("sl_cpc_write status=0x%lx\r\n", slstatus);
  }
}

static void cpc_write_complete(sl_cpc_user_endpoint_id_t endpoint_id, void *buffer, void *arg, sl_status_t status){
  (void)endpoint_id;
  (void)buffer;
  (void)arg;
  //(void)status;
  printf("Write complete, status=0x%x\r\n", (unsigned int) status);
  if (status == 0) {
    debug_print("successfully completed write\r\n");
    if (tx_ptr) {
        vPortFree(tx_ptr); // free the tx buffer
        tx_ptr = NULL;
    }
  }
  //error handling would go here
}

static void cpc_read_command(uint8_t endpoint_id, void *arg)

{
  (void)endpoint_id;
  (void)arg;
  sl_status_t status;
  uint8_t *read_array;
  uint16_t size;

  status = sl_cpc_read(&custom_endpoint_handle,
                       (void **)&read_array,
                       &size,
                       0, // Timeout : relevent only when using a kernel with blocking
                       0); // flags : relevent only when using a kernel to specify a non-blocking operation (polling).

  if (status != SL_STATUS_OK) {
    // log and ignore error
    debug_print("sl_cpc_read status 0x%lx\r\n", status);
  } else {
#if SWODEBUG
    printf("read status OK, command size=%d\r\n",size);
    for(uint8_t i=0;i<size;i++) {
        printf("data[%d]=0x%x ",i,read_array[i]);
    }
    printf("\r\n");
#endif
    process_command(read_array,size);
    sl_cpc_free_rx_buffer(read_array);
  }

}

static void cpc_error_cb(uint8_t endpoint_id, void *arg)
{
  (void)endpoint_id;
  (void)arg;
  uint8_t state = sl_cpc_get_endpoint_state(&custom_endpoint_handle);
  debug_print("cpc_error_cb, ep id=%d, state = %d\r\n", endpoint_id, state);
  if (state == SL_CPC_STATE_ERROR_DESTINATION_UNREACHABLE) {
    // This error is thrown on disconnect. Use this to change endpoint state
    sl_status_t status = sl_cpc_close_endpoint(&custom_endpoint_handle);
    EFM_ASSERT(status == SL_STATUS_OK);
    endpoint_status = CPC_ENDPOINT_DISCONNECTED;
  }
}

#if defined(SL_CATALOG_CPC_SECURITY_PRESENT)
uint64_t sl_cpc_security_on_unbind_request(bool is_link_encrypted)
{
  (void)is_link_encrypted;
  return SL_CPC_SECURITY_OK_TO_UNBIND;
}
#endif

void cpc_connect_command(uint8_t endpoint_id, void *arg)
{
  // This callback tells us we are connected
  (void)arg;
  if (SL_CPC_ENDPOINT_USER_ID_0 == endpoint_id) {
      debug_print("user ep connected\r\n");
      endpoint_status = CPC_ENDPOINT_CONNECTED;
  }
}

static cpc_endpoint_status_t connect(){
  sl_status_t status;
  uint8_t window_size = 1;
  uint8_t flags = 0;

  status = sl_cpc_open_user_endpoint(&custom_endpoint_handle,
                                         SL_CPC_ENDPOINT_USER_ID_0,
                                         flags,
                                         window_size);

  if (status != SL_STATUS_OK && status != SL_STATUS_ALREADY_EXISTS )
       return CPC_ENDPOINT_CLOSED;

  status = sl_cpc_set_endpoint_option(&custom_endpoint_handle,
                                      SL_CPC_ENDPOINT_ON_IFRAME_WRITE_COMPLETED,
                                      (void *)cpc_write_complete);
  if (status != SL_STATUS_OK)
     return CPC_ENDPOINT_CLOSED;

  status = sl_cpc_set_endpoint_option(&custom_endpoint_handle,
                                      SL_CPC_ENDPOINT_ON_IFRAME_RECEIVE,
                                      (void *)cpc_read_command);
  if (status != SL_STATUS_OK)
       return CPC_ENDPOINT_CLOSED;

  status = sl_cpc_set_endpoint_option(&custom_endpoint_handle,
                                      SL_CPC_ENDPOINT_ON_ERROR,
                                      (void*)cpc_error_cb);
  if (status != SL_STATUS_OK)
       return CPC_ENDPOINT_CLOSED;

  status = sl_cpc_set_endpoint_option(&custom_endpoint_handle,
                                      SL_CPC_ENDPOINT_ON_CONNECT,
                                      (void*)cpc_connect_command);
  if (status != SL_STATUS_OK)
         return CPC_ENDPOINT_CLOSED;

  return CPC_ENDPOINT_OPEN;

}

// Check & Update endpoint status
void cpc_test_endpoint_status(){
  if ( endpoint_status == CPC_ENDPOINT_DISCONNECTED && sl_cpc_get_endpoint_state(&custom_endpoint_handle) == SL_CPC_STATE_FREED){
      endpoint_status = CPC_ENDPOINT_CLOSED;
  }
  // If closed, open endpoint
  if ( endpoint_status == CPC_ENDPOINT_CLOSED ){
      debug_print("ep closed, connecting...\r\n");
      endpoint_status = connect();
  }
}

#if defined(SL_CATALOG_KERNEL_PRESENT)
void cpc_custom_task(void *pvParameter) {
  (void)pvParameter;

  // Check endpoint state and connect
    cpc_test_endpoint_status();

    // Any other cpc init tasks go here
    while (1) {
        cpc_custom_process_action();
    }
}
#endif

void cpc_custom_init(){
  debug_print("cpc_custom_init\r\n");

// If FreeRTOS, create polling task
#if defined(SL_CATALOG_KERNEL_PRESENT)
  xTaskCreate(cpc_custom_task,
              "cpc_custom_task",
              configMINIMAL_STACK_SIZE,
                  NULL,
                  1,
                  NULL);
#endif
}

void cpc_custom_process_action(){
  // This is a polled function, called either from the super loop or
  // as a FreeRTOS task

  // Verify that endpoint is connected
  cpc_test_endpoint_status();

}
