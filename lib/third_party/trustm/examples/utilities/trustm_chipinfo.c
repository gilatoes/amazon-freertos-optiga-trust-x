/**
* MIT License
*
* Copyright (c) 2019 Infineon Technologies AG
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//#include "optiga/ifx_i2c/ifx_i2c_config.h"
#include "optiga/optiga_util.h"

#include "optiga/ifx_i2c/ifx_i2c_config.h"
#include "optiga/comms/optiga_comms.h"
#include "optiga/optiga_crypt.h"

#include "trustm_chipinfo.h"

#include "optiga/common/optiga_lib_types.h"

/*************************************************************************
*  Global
*************************************************************************/
static volatile optiga_lib_status_t optiga_lib_status;
static optiga_util_t * me;
uint16_t trustm_open_flag = 0;
/*************************************************************************
*  functions
*************************************************************************/
/**
 * Callback when optiga_util_xxxx operation is completed asynchronously
 */

static void optiga_util_callback(void * context, optiga_lib_status_t return_status)
{
    optiga_lib_status = return_status;
	//printf("optiga_util_callback: optiga_lib_status=0x%x\r\n",optiga_lib_status);
}

/**********************************************************************
* trustm_Open()
**********************************************************************/
optiga_lib_status_t trustm_Open(void)
{
	uint16_t i;
    optiga_lib_status_t return_status;
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

    //printf(">trustm_Open()\r\n");
    trustm_open_flag = 0;
    do
    {
        //Create an instance of optiga_util to open the application on OPTIGA.
        me = optiga_util_create(0, optiga_util_callback, NULL);
        if (NULL == me)
        {
            printf("trustm_Open(): Failed to create Trust M instance\r\n");
            break;
        }
		printf("trustm_Open(): Trust M instance created Ok.\r\n");

        /**
         * Open the application on OPTIGA which is a precondition to perform any other operations
         * using optiga_util_open_application
         */
        optiga_lib_status = OPTIGA_LIB_BUSY;
        return_status = optiga_util_open_application(me, 0);

        if (OPTIGA_LIB_SUCCESS != return_status)
        {
        	printf("trustm_Open(): Failed to open Trust M application\r\n");
            break;
        }


        //printf("waiting (max count: 50)");
        //Wait until the optiga_util_open_application is completed
		i=0;
        while (optiga_lib_status == OPTIGA_LIB_BUSY)
		{
			//printf(".");
			i++;
			vTaskDelay(xDelay);
			if (i == 50)
				break;
		}
        //printf("\n");
		//printf("count : %d \n",i);

        if (OPTIGA_LIB_SUCCESS != optiga_lib_status)
        {
			printf("trustm_Open(): Failed to open Trust M application. optiga_lib_status=0x%x\r\n",optiga_lib_status);
			return_status = optiga_lib_status;
            break;
        }

        trustm_open_flag = 1;
        printf("trustm_Open()： Open Trust M application Ok.\n");
    }while(FALSE);

	//printf("<trustm_Open()\r\n");
	return return_status;
}

/**********************************************************************
* trustX_Close()
**********************************************************************/
optiga_lib_status_t trustm_Close(void)
{
    optiga_lib_status_t return_status;

    //printf(">trustm_Close()");

	do{
		if (trustm_open_flag != 1)
		{
			printf("trustm_Close: Trust M is not open\r\n");
			break;
		}

        optiga_lib_status = OPTIGA_LIB_BUSY;
        //++ty++ OPTIGA_COMMS_PROTECTION_MANAGE_CONTEXT(me_util, OPTIGA_COMMS_SESSION_CONTEXT_NONE);
        return_status = optiga_util_close_application(me, 0);

        if (OPTIGA_LIB_SUCCESS != return_status)
        {
        	printf("trustm_Close(): Failed to close Trust M.\r\n");
            break;
        }

        while (optiga_lib_status == OPTIGA_LIB_BUSY)
        {
            //Wait until the optiga_util_close_application is completed
			//printf("Waiting : optiga_util_close_application \n");
			//printf(".");
        }

        if (OPTIGA_LIB_SUCCESS != optiga_lib_status)
        {
            //optiga util close application failed
        	printf("Fail : optiga_util_close_application \n");
			return_status = optiga_lib_status;
            break;
        }

		trustm_open_flag = 0;
		printf("trustm_Close(): Trust M closed Ok.\r\n");

	}while(FALSE);

    // destroy util and crypt instances
    if (me != NULL)
		optiga_util_destroy(me);

#ifdef WORKAROUND
	pal_os_event_disarm();
#endif

	//printf("trustm_Close(): TrustM Closed.\n");
	//printf("<trustm_Close()");
	return return_status;
}
/**********************************************************************
* trustm_readUID()
**********************************************************************/
optiga_lib_status_t trustm_readUID(utrustm_UID_t *UID)
{
    uint16_t offset, bytes_to_read;
    uint16_t optiga_oid;
    uint8_t read_data_buffer[27];

    optiga_lib_status_t return_status;
    uint16_t i=0;

    /* Block for 50ms. */
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

    do
    {
        //Read device UID
        optiga_oid = 0xE0C2;
        offset = 0x00;
        bytes_to_read = sizeof(read_data_buffer);

        optiga_lib_status = OPTIGA_LIB_BUSY;
        return_status = optiga_util_read_data(me,
											  optiga_oid,
                                              offset,
                                              read_data_buffer,
                                              &bytes_to_read);

        if (OPTIGA_LIB_SUCCESS != return_status)
        {
			printf("trustm_readUID(): Failed to read data from OID\n");
            break;
        }

        while (OPTIGA_LIB_BUSY == optiga_lib_status)
        {
            //Wait until the optiga_util_read_data operation is completed
   			i++;
   			vTaskDelay(xDelay);
   			if (i == 50)
   				break;
        }

        if (OPTIGA_LIB_SUCCESS != optiga_lib_status)
        {
        	printf("trustm_readUID(): Failed to read data from OID.\n");
            break;
        }

		for (i=0;i<bytes_to_read;i++)
		{
			UID->b[i] = read_data_buffer[i];
		}

    } while(FALSE);

    return return_status;
}
