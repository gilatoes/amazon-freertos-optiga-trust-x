#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include "optiga/optiga_util.h"
#include "optiga/pal/pal_os_event.h"
#include "optiga/ifx_i2c/ifx_i2c_config.h"
#include "mbedtls/base64.h"
#include "trustm_chipinfo.h"

/* PKCS#11 includes. */
#include "aws_pkcs11.h"

/* Key provisioning includes. */
#include "aws_dev_mode_key_provisioning.h"

/* OPTIGA Trust M defines. */
#define mainTrustM_TASK_STACK_SIZE     		( configMINIMAL_STACK_SIZE * 8 )

char CLIENT_CERTIFICATE_PEM[1124];
uint32_t CLIENT_CERTIFICATE_LENGTH;

// Not used, but required for mbedTLS...
const char CLIENT_PRIVATE_KEY_PEM[] =
//"-----BEGIN EC PARAMETERS-----\n"
//"BggqhkjOPQMBBw==\n"
//"-----END EC PARAMETERS-----\n"
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIH84O6hf145Awun9lH6GbesjSmwJ5w1ODaYhp7xLZRA3oAoGCCqGSM49\n"
"AwEHoUQDQgAEb5ERgUCa0019j5JWdEiH0JyldJF9cv/MW2Lx94n9HVJ9FiQcooFi\n"
"KegulIrIuCLE1a7zsv8Izs/6gY9rxOdHHA==\n"
"-----END EC PRIVATE KEY-----\n";

/*
 * Length of device private key included from aws_clientcredential_keys.h .
 */
const uint32_t CLIENT_PRIVATE_KEY_LENGTH = sizeof( CLIENT_PRIVATE_KEY_PEM );

static TimerHandle_t xTrustMInitTimer;
SemaphoreHandle_t xTrustMSemaphoreHandle; /**< OPTIGA™ Trust M module semaphore. */
const TickType_t xTrustMSemaphoreWaitTicks = pdMS_TO_TICKS( 60000 );

extern optiga_lib_status_t trustm_OpenCrypto(void);
extern optiga_lib_status_t trustm_CloseCrypto(void);
extern optiga_lib_status_t  example_authenticate_chip(void);

extern void example_optiga_crypt_ecc_generate_keypair(void);
extern void example_optiga_crypt_ecdh(void);
extern void example_optiga_crypt_ecdsa_sign(void);
extern void example_optiga_crypt_ecdsa_verify(void);
extern void example_optiga_crypt_hash(void);
extern void example_optiga_crypt_random(void);
extern void example_optiga_crypt_rsa_decrypt_and_export(void);
extern void example_optiga_crypt_rsa_encrypt_message(void);
extern void example_optiga_crypt_rsa_encrypt_session(void);
extern void example_optiga_crypt_rsa_generate_keypair(void);
extern void example_optiga_crypt_rsa_sign(void);
extern void example_optiga_crypt_rsa_verify(void);
extern void example_optiga_crypt_tls_prf_sha256(void);
extern void example_optiga_util_protected_update(void);
extern void example_optiga_util_read_data(void);
extern void example_optiga_util_update_count(void);
extern void example_optiga_util_write_data(void);

void example_log_execution_status(const char_t* function, uint8_t status);
void example_log_function_name(const char_t* function);

/*************************************************************************
*  Global
*************************************************************************/
optiga_lib_status_t optiga_example_lib_status;
optiga_util_t * me_example;
/*************************************************************************
*  functions
*************************************************************************/
/**
 * Callback when optiga_example_util_ operation is completed asynchronously
 */

static void optiga_example_util_callback(void * context, optiga_lib_status_t return_status)
{
	optiga_example_lib_status = return_status;
	//printf("optiga_example_util_callback: optiga_lib_status=0x%x\r\n",optiga_example_lib_status);
}

void example_log_execution_status(const char_t* function, uint8_t status)
{
	printf("%s = %d\r\n", function, status);
}

void example_log_function_name(const char_t* function)
{
	printf("%s\r\n", function);
}

void trustm_open_app()
{
	uint16_t i;
    optiga_lib_status_t return_status;
    const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

    do
    {
        //Create an instance of optiga_util to open the application on OPTIGA.
        me_example = optiga_util_create(0, optiga_example_util_callback, NULL);
        if (NULL == me_example)
        {
            printf("vTrustMTaskCallbackHandler: Failed to create Trust M instance\r\n");
            break;
        }
		printf("vTrustMTaskCallbackHandler: Trust M instance created Ok.\r\n");

        /**
         * Open the application on OPTIGA which is a precondition to perform any other operations
         * using optiga_util_open_application
         */
        optiga_example_lib_status = OPTIGA_LIB_BUSY;
        return_status = optiga_util_open_application(me_example, 0);

        if (OPTIGA_LIB_SUCCESS != return_status)
        {
        	printf("vTrustMTaskCallbackHandler: Failed to open Trust M application\r\n");
            break;
        }


        //printf("waiting (max count: 50)");
        //Wait until the optiga_util_open_application is completed
		i=0;
        while (optiga_example_lib_status == OPTIGA_LIB_BUSY)
		{
			//printf(".");
			i++;
			vTaskDelay(xDelay);
			if (i == 50)
				break;
		}
        //printf("\n");
		//printf("count : %d \n",i);

        if (OPTIGA_LIB_SUCCESS != optiga_example_lib_status)
        {
			printf("vTrustMTaskCallbackHandler: Failed to open Trust M application. optiga_example_lib_status=0x%x\r\n", optiga_example_lib_status);
			return_status = optiga_example_lib_status;
            break;
        }

        printf("vTrustMTaskCallbackHandler： Open Trust M application Ok.\n");
    }while(FALSE);
}

void trustm_close_app()
{
	 optiga_lib_status_t return_status;
	do{

		optiga_example_lib_status = OPTIGA_LIB_BUSY;
		return_status = optiga_util_close_application(me_example, 0);

		if (OPTIGA_LIB_SUCCESS != return_status)
		{
			printf("vTrustMTaskCallbackHandler: Failed to close Trust M.\r\n");
			break;
		}

		while (optiga_example_lib_status == OPTIGA_LIB_BUSY)
		{}

		if (OPTIGA_LIB_SUCCESS != optiga_example_lib_status)
		{
			//optiga util close application failed
			printf("vTrustMTaskCallbackHandler: Fail to close Trust M application.\r\n");
			return_status = optiga_example_lib_status;
			break;
		}

		printf("vTrustMTaskCallbackHandler: Trust M closed Ok.\r\n");

	}while(FALSE);

	// destroy me_example instances
	if (me_example != NULL)
		optiga_util_destroy(me_example);
}

void vTrustMInitCallback( TimerHandle_t xTimer )
{
	xSemaphoreGive(xTrustMSemaphoreHandle);
}

void vTrustMTaskCallbackHandler( void * pvParameters )
{
	optiga_lib_status_t status = OPTIGA_DEVICE_ERROR;

	if ( xSemaphoreTake(xTrustMSemaphoreHandle, xTrustMSemaphoreWaitTicks) == pdTRUE )
	{
	    do
	    {
	    	pal_os_event_init();
	    }while(FALSE);

#if 0
	    utrustm_UID_t UID;
	    optiga_lib_status_t return_status = trustm_Open();
	    if (return_status != OPTIGA_LIB_SUCCESS)
	    {
	    	printf("vTrustMTaskCallbackHandler: error opening Trust M.");
	    }
	    else
	    {
			return_status = trustm_readUID(&UID);

			if (return_status != OPTIGA_LIB_SUCCESS)
			{
				printf("vTrustMTaskCallbackHandler: readUID [0xE0C2] failed\n");
			}
			else
			{
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
				}
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

				trustm_Close();
	    }
#endif

#if 0
	    optiga_lib_status_t return_status = trustm_OpenCrypto();
	    if (return_status != OPTIGA_LIB_SUCCESS)
	    {
	    	printf("vTrustMTaskCallbackHandler: error opening Trust M crypto instance.");
	    }
	    else
	    {
	    	 example_authenticate_chip();
	    	 trustm_CloseCrypto();
	    }
#endif

	    trustm_open_app();
	    printf("vTrustMTaskCallbackHandler: Start Examples test...\r\n");

	    /*
	    example_optiga_crypt_ecc_generate_keypair();
	    example_optiga_crypt_ecdsa_sign();
	    example_optiga_crypt_ecdsa_verify();
	    example_optiga_crypt_hash();
	    example_optiga_crypt_random();
	    example_optiga_crypt_rsa_encrypt_message();
	    example_optiga_crypt_rsa_generate_keypair();
	    example_optiga_crypt_rsa_sign();
	    example_optiga_crypt_rsa_verify();
	    example_optiga_util_protected_update();
	    example_optiga_util_read_data();
	    example_optiga_util_update_count();
	    example_optiga_util_write_data();
        */
	    example_optiga_crypt_tls_prf_sha256();
	    example_optiga_crypt_ecdh();
	    example_optiga_crypt_rsa_encrypt_session();
	    example_optiga_crypt_rsa_decrypt_and_export();

	    printf("vTrustMTaskCallbackHandler: Examples completed.\r\n");
		trustm_close_app();


	    ProvisioningParams_t xParams;

	    xParams.ulClientPrivateKeyType = CKK_EC;
	    xParams.pcClientPrivateKey = ( uint8_t * ) CLIENT_PRIVATE_KEY_PEM;
	    xParams.ulClientPrivateKeyLength = CLIENT_PRIVATE_KEY_LENGTH;
	    xParams.pcClientCertificate = ( uint8_t * ) CLIENT_CERTIFICATE_PEM;
	    xParams.ulClientCertificateLength = CLIENT_CERTIFICATE_LENGTH;

	    vAlternateKeyProvisioning( &xParams );

        DEMO_RUNNER_RunDemos();
	}

	vTaskDelete(NULL);
}

void OPTIGA_TRUST_M_Init(void)
{
	/* Create the handler for the callbacks. */
	xTaskCreate( vTrustMTaskCallbackHandler,       /* Function that implements the task. */
				"TrstXHndlr",          /* Text name for the task. */
				configMINIMAL_STACK_SIZE*8,      /* Stack size in words, not bytes. */
				NULL,    /* Parameter passed into the task. */
				tskIDLE_PRIORITY,/* Priority at which the task is created. */
				NULL );      /* Used to pass out the created task's handle. */

    xTrustMSemaphoreHandle = xSemaphoreCreateBinary();

	xTrustMInitTimer = xTimerCreate("TrustM_init_timer",        /* Just a text name, not used by the kernel. */
									pdMS_TO_TICKS(1),    /* The timer period in ticks. */
									pdFALSE,         /* The timers will auto-reload themselves when they expire. */
									( void * )NULL,   /* Assign each timer a unique id equal to its array index. */
									vTrustMInitCallback  /* Each timer calls the same callback when it expires. */
									);
	if( xTrustMInitTimer == NULL )
	{
		// The timer was not created.
	}
	else
	{
		// Start the timer.  No block time is specified, and even if one was
		// it would be ignored because the scheduler has not yet been
		// started.
		if( xTimerStart( xTrustMInitTimer, 0 ) != pdPASS )
		{
		    // The timer could not be set into the Active state.
		}
	}
}

