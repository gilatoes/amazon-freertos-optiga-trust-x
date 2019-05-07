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

#include "optiga/optiga_util.h"
#include "optiga/ifx_i2c/ifx_i2c_config.h"
#include "optiga/comms/optiga_comms.h"
#include "optiga/optiga_crypt.h"
#include "trustm_chipinfo.h"
#include "optiga/common/optiga_lib_types.h"

/*************************************************************************
*  Global
*************************************************************************/

extern void example_log_execution_status(const char_t* function, uint8_t status);
extern void example_log_function_name(const char_t* function);

/* Callback when optiga_util_xxxx operation is completed asynchronously
*/
static volatile optiga_lib_status_t optiga_lib_status;
//lint --e{818} suppress "argument "context" is not used in the sample provided"
static void optiga_util_callback(void * context, optiga_lib_status_t return_status)
{
   optiga_lib_status = return_status;
   if (NULL != context)
   {
       // callback to upper layer here
   }
}

/**********************************************************************
* example_optiga_util_read_uid(void)
**********************************************************************/
void example_optiga_util_read_uid(void)
{
    uint16_t offset, bytes_to_read;
    uint16_t optiga_oid;
    uint8_t read_data_buffer[27];
    optiga_util_t * me = NULL;
    uint8_t logging_status = 0;

    optiga_lib_status_t return_status;
    uint16_t i=0;
    utrustm_UID_t UID;

    example_log_function_name(__FUNCTION__);

    /* Block for 50ms. */
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

    do
    {
        /**
         * 1. Create OPTIGA Util Instance
         */
        me = optiga_util_create(0, optiga_util_callback, NULL);
        if (NULL == me)
        {
            break;
        }

        /**
         * 2. Read the device UID location 0xE0C2
         */
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

        logging_status = 1;
    } while(FALSE);

        if (me)
        {
            //Destroy the instance after the completion of usecase if not required.
            return_status = optiga_util_destroy(me);
        }



#if 1
		for (i=0;i<bytes_to_read;i++)
		{
			UID.b[i] = read_data_buffer[i];
		}

		printf("==================== Trust M Chip Info ===========================\n");
		printf("CIM Identifier             [bCimIdentifer]: 0x%.2x\n", UID.st.bCimIdentifer);
		printf("Platform Identifer   [bPlatformIdentifier]: 0x%.2x\n", UID.st.bPlatformIdentifier);
		printf("Model Identifer         [bModelIdentifier]: 0x%.2x\n", UID.st.bModelIdentifier);
		printf("ID of ROM mask                  [wROMCode]: 0x%.2x%.2x\n",
								UID.st.wROMCode[0],
								UID.st.wROMCode[1]);
		printf("Chip Type                    [rgbChipType]: 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n",
								UID.st.rgbChipType[0],
								UID.st.rgbChipType[1],
								UID.st.rgbChipType[2],
								UID.st.rgbChipType[3],
								UID.st.rgbChipType[4],
								UID.st.rgbChipType[5]);
		printf("Batch Number              [rgbBatchNumber]: 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n",
								UID.st.rgbBatchNumber[0],
								UID.st.rgbBatchNumber[1],
								UID.st.rgbBatchNumber[2],
								UID.st.rgbBatchNumber[3],
								UID.st.rgbBatchNumber[4],
								UID.st.rgbBatchNumber[5]);
		printf("X-coordinate              [wChipPositionX]: 0x%.2x%.2x\n",
								UID.st.wChipPositionX[0],
								UID.st.wChipPositionX[1]);
		printf("Y-coordinate              [wChipPositionY]: 0x%.2x%.2x\n",
								UID.st.wChipPositionY[0],
								UID.st.wChipPositionY[1]);
		printf("Firmware Identifier [dwFirmwareIdentifier]: 0x%.2x%.2x%.2x%.2x\n",
								UID.st.dwFirmwareIdentifier[0],
								UID.st.dwFirmwareIdentifier[1],
								UID.st.dwFirmwareIdentifier[2],
								UID.st.dwFirmwareIdentifier[3]);
		printf("Build Number                 [rgbESWBuild]: %.2x %.2x\n",
								UID.st.rgbESWBuild[0],
								UID.st.rgbESWBuild[1]);

		printf("\n");

		printf("Chip software build ");
		if ((UID.st.rgbESWBuild[0] == 0x05) && (UID.st.rgbESWBuild[1] == 0x10))
			printf("V1.0.510\n");
		else if ((UID.st.rgbESWBuild[0] == 0x07) && (UID.st.rgbESWBuild[1] == 0x15))
			printf("V1.1.715\n");
		else if ((UID.st.rgbESWBuild[0] == 0x10) && (UID.st.rgbESWBuild[1] == 0x48))
			printf("V1.2.1048\n");
		else if ((UID.st.rgbESWBuild[0] == 0x11) && (UID.st.rgbESWBuild[1] == 0x12))
			printf("V1.30.1112\n");
		else if ((UID.st.rgbESWBuild[0] == 0x11) && (UID.st.rgbESWBuild[1] == 0x18))
			printf("V1.40.1118\n");
		else
			printf("Unknown\n");
		printf("====================================================================\n");
#endif

        example_log_execution_status(__FUNCTION__,logging_status);

    return;
}
