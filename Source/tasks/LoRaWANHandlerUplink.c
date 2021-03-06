/*
* loraWANHandler.c
*
* Created: 12/04/2019 10:09:05
*  Author: IHA
*/
#include "../Source/headers/LoRaWANHandlerUplink.h"


// keys used by the network - provided by ERL  
#define LORA_appEUI "1AB7F2972CC78C9A"
#define LORA_appKEY "6C7EF7F5BC5266D1FAEE88AF7EA9BABD"


lora_driver_payload_t payload;


static void _lora_setup(void)
{
	char _out_buf[20];
	
	lora_driver_returnCode_t rc;
	
	status_leds_slowBlink(led_ST2); //Led the green led blink slowly while we are setting up LoRa

	// Factory reset the transceiver
	printf("FactoryReset >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_rn2483FactoryReset()));
	
	// Configure to EU868 LoRaWAN standards
	printf("Configure to EU868 >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_configureToEu868()));

	// Get the transceivers HW EUI
	rc = lora_driver_getRn2483Hweui(_out_buf);
	
	printf("Get HWEUI >%s<: %s\n",lora_driver_mapReturnCodeToText(rc), _out_buf);

	// Set the HWEUI as DevEUI in the LoRaWAN software stack in the transceiver
	printf("Set DevEUI: %s >%s<\n", _out_buf, lora_driver_mapReturnCodeToText(lora_driver_setDeviceIdentifier(_out_buf)));

	// Set Over The Air Activation parameters to be ready to join the LoRaWAN
	printf("Set OTAA Identity appEUI:%s appKEY:%s devEUI:%s >%s<\n", LORA_appEUI, LORA_appKEY, _out_buf, lora_driver_mapReturnCodeToText(lora_driver_setOtaaIdentity(LORA_appEUI,LORA_appKEY,_out_buf)));

	// Save all the MAC settings in the transceiver
	printf("Save mac >%s<\n",lora_driver_mapReturnCodeToText(lora_driver_saveMac()));

	// Enable Adaptive Data Rate
	printf("Set Adaptive Data Rate: ON >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_setAdaptiveDataRate(LORA_ON)));

	// Set receiver window1 delay to 500 ms - this is needed if down-link messages will be used
	printf("Set Receiver Delay: %d ms >%s<\n", 500, lora_driver_mapReturnCodeToText(lora_driver_setReceiveDelay(500)));

	// Join the LoRaWAN
	uint8_t maxJoinTriesLeft = 100;
	
	do {
		rc = lora_driver_join(LORA_OTAA);
				
		printf("Join Network TriesLeft:%d >%s<\n", maxJoinTriesLeft, lora_driver_mapReturnCodeToText(rc));

		if ( rc != LORA_ACCEPTED)
		{
			// Make the red led pulse to tell something went wrong
			status_leds_longPuls(led_ST1); 
			// Waits 5 sec and tries again
			vTaskDelay(pdMS_TO_TICKS(5000UL));
		}
		else
		{
			break;
		}
	} while (--maxJoinTriesLeft);

	if (rc == LORA_ACCEPTED)
	{
		// Connected to LoRaWAN :-)
		// Make the green led steady
		status_leds_ledOn(led_ST2);
	}
	else
	{
		// Something went wrong and turns off the green led
		status_leds_ledOff(led_ST2);
		// Make the red led blink fast to tell something went wrong
		status_leds_fastBlink(led_ST1);

		while (1)
		{
			taskYIELD();
		}
	}
}

/*-----------------------------------------------------------*/
void lora_handler_uplink_task( void *pvParameters )
{
	// Hardware reset of LoRaWAN transceiver
	lora_driver_resetRn2483(1);
	vTaskDelay(2);
	lora_driver_resetRn2483(0);
	//time to wakeup
	vTaskDelay(150);

	lora_driver_flushBuffers(); // get rid of first version string from module after reset!

	_lora_setup();



	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(300000 ms); // Uploads a message every 5 minutes (300000 ms)
	xLastWakeTime = xTaskGetTickCount();
	
	for(;;)
	{
		
		EventBits_t temp;
	
		temp=xMessageBufferReceive(uplinkMessageBuffer,(void*)&payload,sizeof(payload),portMAX_DELAY);
	
		xTaskDelayUntil( &xLastWakeTime, xFrequency );
		
		if( temp > 0 )
		{
			status_leds_shortPuls(led_ST4); 
			printf("Uploaded Message ------------------>%s<\n", lora_driver_mapReturnCodeToText(lora_driver_sendUploadMessage(false, &payload)));
		}
		vTaskDelay(100);  
	}
}
// create task for the uplink handler 
void lora_uplink_handler_create(UBaseType_t lora_handler_task_priority)
{
	xTaskCreate(
	lora_handler_uplink_task,
	"LRHandUplink"  // A name for humans
	, configMINIMAL_STACK_SIZE  
	, NULL
	, lora_handler_task_priority  // Priority setter.
	, NULL );
}
