/***************************************************************
 *
 * OpenBeacon.org - main file for OpenBeacon USB II Bluetooth
 *
 * Copyright 2010 Milosch Meriac <meriac@openbeacon.de>
 *
 ***************************************************************

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#include <openbeacon.h>
#include "pin.h"
#include "hid.h"
#include "spi.h"
#include "iap.h"
#include "pmu.h"
#include "crc16.h"
#include "xxtea.h"
#include "bluetooth.h"
#include "3d_acceleration.h"
#include "storage.h"
#include "nRF_API.h"
#include "nRF_CMD.h"
#include "openbeacon-proto.h"

/* OpenBeacon packet */
static TBeaconEnvelope g_Beacon;

/* ForwardCollection array */
static TBeaconCollectedForwarder g_collection[COLLECTION_SIZE];

/* Default TEA encryption key of the tag - MUST CHANGE ! */
static const uint32_t xxtea_key[4] =
{ 0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF };

/* set nRF24L01 broadcast mac */
static const unsigned char broadcast_mac[NRF_MAX_MAC_SIZE] =
{ 1, 2, 3, 2, 1 };

/* device UUID */
static uint16_t tag_id;
static TDeviceUID device_uuid;

#if (USB_HID_IN_REPORT_SIZE>0)||(USB_HID_OUT_REPORT_SIZE>0)
static uint8_t hid_buffer[USB_HID_IN_REPORT_SIZE];

void
GetInReport (uint8_t * src, uint32_t length)
{
	(void) src;
	(void) length;

	if (length > USB_HID_IN_REPORT_SIZE)
	length = USB_HID_IN_REPORT_SIZE;

	memcpy (src, hid_buffer, length);
}

void
SetOutReport (uint8_t * dst, uint32_t length)
{
	(void) dst;
	(void) length;
}
#endif

static void show_version(void)
{
	debug_printf(" * Tag ID: %i\n", (uint16_t) device_uuid[3]);
	debug_printf(" * Device UID: %08X:%08X:%08X:%08X\n", device_uuid[0],
			device_uuid[1], device_uuid[2], device_uuid[3]);
	debug_printf(" * free heap memory: %i bytes\n",xPortGetFreeHeapSize());
}

void main_menue(uint8_t cmd)
{
	/* ignore non-printable characters */
	if (cmd <= ' ')
		return;
	/* show key pressed */
	debug_printf("%c\n", cmd);
	/* map lower case to upper case */
	if (cmd > 'a' && cmd < 'z')
		cmd -= ('a' - 'A');

	switch (cmd)
	{
	case '?':
	case 'H':
		debug_printf("\n"
			" *****************************************************\n"
			" * OpenBeacon USB II - Bluetooth Console             *\n"
			" * (C) 2010 Milosch Meriac <meriac@openbeacon.de>    *\n"
			" *****************************************************\n"
			" * H,?          - this help screen\n"
#ifdef MENUE_ALLOW_ISP_REBOOT
				" * P            - invoke ISP programming mode\n"
#endif
				" * S            - show device status\n"
				" * R            - OpenBeacon nRF24L01 register dump\n"
				" *****************************************************\n"
				"\n");
		break;
#ifdef MENUE_ALLOW_ISP_REBOOT
		case 'P':
		debug_printf ("\nRebooting...");
		iap_invoke_isp ();
		break;
#endif
	case 'R':
		nRFCMD_RegisterDump();
		break;
	case 'S':
		debug_printf("\n"
			" *****************************************************\n"
			" * OpenBeacon Status Information                     *\n"
			" *****************************************************\n");
		show_version();
		spi_status();
		nRFCMD_Status();
		acc_status();
		//      pmu_status ();
#if (DISK_SIZE>0)
		storage_status();
#endif
		nRFCMD_Status();
		debug_printf(" *****************************************************\n"
			"\n");
		break;
	default:
		debug_printf("Unknown command '%c' - please press 'H' for help \n", cmd);
	}
	debug_printf("\n# ");
}

/*
 * add signalstrenght to uint16_t signals
 */
static
void incrementStrenghtForIndex(int index, uint16_t strenth){
	uint32_t curSignal;

	//get signalcount
	curSignal = (g_collection[index].signals >> (4 * strenth)) & 0x0000000F;

	//increment signal
	curSignal = curSignal + 1;

	//check if signalincrement not bigger than 15
	curSignal = (curSignal & 0x0000000F) << (4 * strenth);

	//push the signalstrenght 0-3 away and add signals to struct
	g_collection[index].signals = ((g_collection[index].signals & (0xFFFFFFFF ^ (0x0000000F << (4 * strenth)))) | curSignal);
}

/*
 * collect the data of all received tags
 * and cache them
 */
static
void collectForwardData(void)
{
	int i;
	for(i = 0; i < COLLECTION_SIZE; i++){
		//update existing tag
		if(g_collection[i].tagId == g_Beacon.pos.oid){
			incrementStrenghtForIndex(i, (uint8_t)(g_Beacon.pos.strengthAndZ) >> 4);
			break;
		}

		//add new tag to collection
		if(g_collection[i].tagId == 0){
			g_collection[i].oid = 1;
			g_collection[i].x = g_Beacon.pos.x;
			g_collection[i].y = g_Beacon.pos.y;
			g_collection[i].tagId = g_Beacon.pos.oid;
			incrementStrenghtForIndex(i, (uint8_t)(g_Beacon.pos.strengthAndZ) >> 4);
			//incrementStrenghtForIndex(i, 6);
			g_collection[i].building = g_Beacon.pos.building;
			break;
		}
	}
}

static
void nRF_tx(uint8_t power)
{
	/* update crc */
	g_Beacon.pkt.crc = htons(crc16(g_Beacon.byte, sizeof(g_Beacon)
			- sizeof(uint16_t)));
	/* encrypt data */
	xxtea_encode(g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

	pin_led(GPIO_LED0);

	/* update power pin */
	nRFCMD_Power(power & 0x4);

	/* disable RX mode */
	nRFCMD_CE(0);
	vTaskDelay(5 / portTICK_RATE_MS);

	/* switch to TX mode */
	nRFAPI_SetRxMode(0);

	/* set TX power */
	nRFAPI_SetTxPower(power & 0x3);

	/* upload data to nRF24L01 */
	nRFAPI_TX(g_Beacon.byte, sizeof(g_Beacon));

	/* transmit data */
	nRFCMD_CE(1);

	/* wait until packet is transmitted */
	vTaskDelay(2 / portTICK_RATE_MS);

	/* switch to RX mode again */
	nRFAPI_SetRxMode(1);

	pin_led(GPIO_LEDS_OFF);

	if (power & 0x4)
		nRFCMD_Power(0);
}

static
void nRF_Task(void *pvParameters)
{
	int t, active, i;
	uint8_t strength, status;
	uint16_t crc;
	uint32_t seq;
	portTickType LastUpdateTicks, Ticks;

	(void) pvParameters;

	/* Initialize OpenBeacon nRF24L01 interface */
	//first number equal frequence
	if (!nRFAPI_Init(77, broadcast_mac, sizeof(broadcast_mac), 0))
		/* bail out if can't initialize */
		for (;;)
		{
			pin_led(GPIO_LED0 | GPIO_LED1);
			vTaskDelay(500 / portTICK_RATE_MS);

			pin_led(GPIO_LEDS_OFF);
			vTaskDelay(500 / portTICK_RATE_MS);
		}

	/* blink as a sign of boot to detect crashes */
	for (t = 0; t < 20; t++)
	{
		pin_led(GPIO_LED0);
		vTaskDelay(50 / portTICK_RATE_MS);

		pin_led(GPIO_LEDS_OFF);
		vTaskDelay(50 / portTICK_RATE_MS);
	}

	nRFAPI_SetRxMode(1);
	nRFCMD_CE(1);

	LastUpdateTicks = xTaskGetTickCount();

	/* main loop */
	active = 0;
	seq = t = 0;
	UARTCount = 0;
	while (1)
	{
		/* turn off after button press */
		if (!pin_button0())
		{
			bt_init(0);
			acc_init(0);
			pin_mode_pmu(0);
			pmu_off(0);
		}

		if (nRFCMD_WaitRx(10 / portTICK_RATE_MS))
			do
			{
				// read packet from nRF chip
				nRFCMD_RegReadBuf(RD_RX_PLOAD, g_Beacon.byte, sizeof(g_Beacon));

				// adjust byte order and decode
				xxtea_decode(g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

				// verify the CRC checksum
				crc = crc16(g_Beacon.byte, sizeof(g_Beacon) - sizeof(uint16_t));

				if (ntohs (g_Beacon.pos.crc) == crc)
				{
					pin_led(GPIO_LED1);

					//identifie protocol
					switch (g_Beacon.pos.hdr.proto){
						case RFBPROTO_BEACONPOSITIONTRACKER:
							//add received data to collection array
							collectForwardData();
							if(active)
								debug_printf("%i:%i\n",ntohs(g_Beacon.pos.oid) ,((g_Beacon.pos.strengthAndZ) >> 4));
						break;

						default:
							break;
					}

					if (strength < 0xFF)
					{
						/* do something with the data */
					}
					pin_led(GPIO_LEDS_OFF);
				}
				status = nRFAPI_GetFifoStatus();
			} while ((status & FIFO_RX_EMPTY) == 0);

		nRFAPI_ClearIRQ(MASK_IRQ_FLAGS);

		// update regularly
		if (((Ticks = xTaskGetTickCount()) - LastUpdateTicks)
				> UPDATE_INTERVAL_MS)
		{
			LastUpdateTicks = Ticks;

			/* setup tracking packet */
			bzero(&g_Beacon, sizeof(g_Beacon));

			//send away if the dataset not empty
			for(i = 0; i < COLLECTION_SIZE; i++){
				if(g_collection[i].tagId != 0){
					g_collection[i].signals = htons(g_collection[i].signals);
					g_Beacon.forwarder = g_collection[i];
					g_Beacon.forwarder.hdr.proto = RFBPROTO_BEACONCOLLECTEDFORWARDER;
					g_Beacon.forwarder.hdr.size = sizeof(g_Beacon);

					/* send away packet */
					nRF_tx(7);

					g_collection[i].tagId = 0;
					g_collection[i].signals = 0;
				}
			}
		}

		if (UARTCount)
		{
			/* blink LED1 upon Bluetooth command */
			pin_led(GPIO_LED1);

			/* show help screen upon Bluetooth connect */
			if (!active)
			{
				active = 1;
				debug_printf("press 'H' for help...\n# ");
			}
			else
				/* execute menu command with last character received */
				main_menue(UARTBuffer[UARTCount - 1]);

			/* LED1 off again */
			pin_led(GPIO_LEDS_OFF);

			/* clear UART buffer */
			UARTCount = 0;
		}
	}
}

int main(void)
{
	volatile int i;
	/* wait on boot - debounce */
	for (i = 0; i < 2000000; i++)
		;

	/* init g_collection*/
	for(int i = 0; i < COLLECTION_SIZE; i++){
		g_collection[i].signals = 0;
		g_collection[i].tagId = 0;
	}

	/* initialize  pins */
	pin_init();
	/* Init SPI */
	spi_init();
	/* Init Storage */
#ifdef USB_DISK_SUPPORT
	storage_init();
#endif
	/* Init USB HID interface */
#if (USB_HID_IN_REPORT_SIZE>0)||(USB_HID_OUT_REPORT_SIZE>0)
	hid_init ();
#endif
	/* power management init */
	pmu_init();
	/* Init Bluetooth */
	bt_init(1);
	/* Init 3D acceleration sensor */
	acc_init(1);
	/* read device UUID */
	bzero(&device_uuid, sizeof(device_uuid));
	iap_read_uid(&device_uuid);
	tag_id = crc16((uint8_t*) &device_uuid, sizeof(device_uuid));

	xTaskCreate(nRF_Task, (const signed char*) "nRF", TASK_NRF_STACK_SIZE,
			NULL, TASK_NRF_PRIORITY, NULL);

	/* Start the tasks running. */
	vTaskStartScheduler();

	return 0;
}
