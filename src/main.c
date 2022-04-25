/*
 * Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */
 /** @file
 *
 * Description: This is the source code for mfg_test app example in ModusToolbox.
 *
 * Related Document: See README.md
  *
 */

/* Header file includes */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "FreeRTOS.h"
#include "task.h"

#include "cy_wcm.h"
#include "cy_lwip.h"

#include "mfg_test_common_api.h"

/******************************************************************************
* Macros
******************************************************************************/

/* Task parameters for MfgTest app task. */
#define MFG_TEST_CLIENT_TASK_PRIORITY       (2)
#define MFG_TEST_CLIENT_TASK_STACK_SIZE     (1024 * 2)

#define WCM_INITIALIZED                  (1lu << 0)

/* Macro to check whether the result of an operation was successful.  When
 * it has failed, print the error message and return EXIT_FAILURE to the
 * calling function.
 */
#define CHECK_RESULT(result, init_mask, error_message...)   \
                     do                                     \
                     {                                      \
                         if ((int)result != EXIT_SUCCESS)   \
                         {                                  \
                             printf(error_message);         \
                             return;                        \
                         }                                  \
                     } while(0)

/* Macro to get the Wifi Mfg Tester application version.
 */
#define GET_WIFI_MFG_VER(str) #str
#define GET_WIFI_MFG_STRING(str) GET_WIFI_MFG_VER(str)
#define GET_WIFI_MFG_VER_STRING  GET_WIFI_MFG_STRING(WIFI_MFG_VER)

/******************************************************************************
* Function prototypes
*******************************************************************************/
cy_rslt_t cy_wcm_get_whd_interface(cy_wcm_interface_t interface_type, whd_interface_t *whd_iface);
static void mfg_test_client_task(void *pvParameters);

/******************************************************************************
* Global variables
******************************************************************************/
/* Enables RTOS-aware debugging. */
volatile int uxTopUsedPriority;

#define IOCTL_MED_LEN   (512)
static unsigned char buf[IOCTL_MED_LEN] = {0};

/*
 * Called first after the initialization of FreeRTOS. It basically connects to Wi-Fi and then starts the
 * task that waits for DHCP.  No tasks will actually run until this function returns.
 */
void vApplicationDaemonTaskStartupHook()
{
    printf("Mfg test Application.\r\n");

    /* Create the mfg_test client task. */
    xTaskCreate(mfg_test_client_task, "Mfg-test task", MFG_TEST_CLIENT_TASK_STACK_SIZE,
                NULL, MFG_TEST_CLIENT_TASK_PRIORITY, NULL);

}

/************************************************************************************
* Main initializes the hardware and low power support, and starts the task scheduler
*************************************************************************************/
int main()
{
    cy_rslt_t result ;

    /* Enables RTOS-aware debugging in OpenOCD */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    /* Initializes the board support package */
    result = cybsp_init() ;
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Dummy read to avoid warning for Release build */
    (void) result;

    /* Enables global interrupts */
    __enable_irq();

    /* Initializes retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Clears the screen */
    printf("\x1b[2J\x1b[;H");
    printf("=============================================\r\n");
    printf("FreeRTOS MfgTest Application\r\n") ;
    printf("FreeRTOS Version: %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("WiFi Mfg Tester : %s\r\n", GET_WIFI_MFG_VER_STRING);
    printf("=============================================\r\n");

    /* Starts the FreeRTOS scheduler */
    vTaskStartScheduler() ;

    /* Should never get here */
    CY_ASSERT(0) ;
}

/******************************************************************************
 * Function Name: mfg_test_client_task
 ******************************************************************************
 * Summary:
 *  Task for handling the initialization and connection of Wi-Fi and the MQTT Client.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void mfg_test_client_task(void *pvParameters)
{
    cy_wcm_config_t config = {.interface = CY_WCM_INTERFACE_TYPE_STA};
    cy_rslt_t result;
    whd_interface_t sta_interface = NULL;

    /* Avoid compiler warnings */
    (void)pvParameters;

    /* Set I/O to No Buffering */
    setvbuf( stdin, NULL, _IONBF, 0 );

    /* Set I/O to No Buffering */
    setvbuf( stdout, NULL, _IONBF, 0 );

    /* Configure the interface as a Wi-Fi STA (i.e. Client). */

    /* Initialize the Wi-Fi connection manager and return if the operation fails. */
    result = cy_wcm_init(&config);

    CHECK_RESULT(result, WCM_INITIALIZED, "\r\nWi-Fi Connection Manager initialization failed!\r\n");

    result = cy_wcm_get_whd_interface(CY_WCM_INTERFACE_TYPE_STA, &sta_interface);
    if (result != CY_RSLT_SUCCESS) {
        printf("\r\nWifi STA Interface Not found, Wi-Fi Connection Manager initialization failed!\r\n");
        return;
    }

    wl_set_sta_interface_handle(sta_interface);

    printf("\r\nWi-Fi Connection Manager initialized.\r\n");

    while (true)
    {
        memset(buf, 0, sizeof(buf));
        wl_remote_command_handler( buf);
    }

    /* Should never get here */
}

/* [] END OF FILE */
