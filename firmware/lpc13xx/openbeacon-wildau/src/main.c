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
#include "3d_acceleration.h"
#include "sound.h"
#include "pmu.h"
#include "iap.h"
#include "spi.h"
#include "nRF_API.h"
#include "nRF_CMD.h"
#include "xxtea.h"
#include "openbeacon-proto.h"

/* device UUID */
static uint16_t tag_id;
static TDeviceUID device_uuid;
/* random seed */
static uint32_t random_seed;
static uint8_t sendAwayFlag;
static uint16_t counter;

#define TX_STRENGTH_OFFSET 2

#define MAINCLKSEL_IRC 0
#define MAINCLKSEL_SYSPLL_IN 1
#define MAINCLKSEL_WDT 2
#define MAINCLKSEL_SYSPLL_OUT 3


/* Default TEA encryption key of the tag - MUST CHANGE ! */
static const uint32_t xxtea_key[4] = {
  0x00112233,
  0x44556677,
  0x8899AABB,
  0xCCDDEEFF
};

/* set nRF24L01 broadcast mac */
static const unsigned char broadcast_mac[NRF_MAX_MAC_SIZE] = {
  1, 2, 3, 2, 1
};

/* OpenBeacon packet */
static TBeaconEnvelope g_Beacon;

/* CollectedForwarder Array */
static TBeaconCollectedForwarder g_collection[COLLECTION_SIZE];

/* static var for calculation of signal strength count */
static uint32_t curSignal;

/*
 * add signalstrenght to uint16_t signals
 */
static
void incrementStrenghtForIndex(int index, uint16_t strength){
	(void) curSignal;
	//get signalcount
	curSignal = (g_collection[index].signals >> (4 * strength)) & 0x0000000F;

	//increment signal
	if(curSignal < 15)	
	   curSignal = curSignal + 1;

	//check if signalincrement not bigger than 15
	curSignal = (curSignal & 0x0000000F) << (4 * strength);

	//push the signalstrenght 0-3 away and add signals to struct
	g_collection[index].signals = (((g_collection[index].signals & (0xFFFFFFFF ^ (0x0000000F << (4 * strength)))) | curSignal));

	/*curSignal = g_collection[index].signals;
    	//check if signalstrenght bigger then 16 (1111)
    	if(((curSignal >> (4 * strength)) & 0x0000000F) ^ 0x0000000F){
    		//increment signal-counter
    		g_collection[index].signals = g_collection[index].signals + (1 << (4 * strength));
    	}*/
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
			g_collection[i].oid = 43;
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


//static void
//nRF_tx (uint8_t power)
//{

  /* encrypt data */
//  xxtea_encode (g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

  /* set TX power */
//  nRFAPI_SetTxPower (power & 0x3);

  /* upload data to nRF24L01 */
//  nRFAPI_TX ((uint8_t *) & g_Beacon, sizeof (g_Beacon));

  /* transmit data */
//  nRFCMD_CE (1);

  /* wait for packet to be transmitted */
//  pmu_sleep_ms (2);

  /* transmit data */
//  nRFCMD_CE (0);
//}

static void nRF_tx(uint8_t power)
{
	/* update crc */
	g_Beacon.forwarder.crc = htons(crc16(g_Beacon.byte, sizeof(g_Beacon)
			- sizeof(uint16_t)));
	/* encrypt data */
	xxtea_encode(g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

	GPIOSetValue (1, 2, 1);

	/* update power pin */
	nRFCMD_Power(power & 0x4);

	/* disable RX mode */
	nRFCMD_CE(0);
	pmu_sleep_ms (2);

	/* switch to TX mode */
	nRFAPI_SetRxMode(0);

	/* set TX power */
	nRFAPI_SetTxPower(power & 0x3);

	/* set TX Channel */
	nRFAPI_SetChannel(CONFIG_FORWARDER_CHANNEL);

	/* upload data to nRF24L01 */
	nRFAPI_TX(g_Beacon.byte, sizeof(g_Beacon));

	/* transmit data */
	nRFCMD_CE(1);

	/* wait until packet is transmitted */
	pmu_sleep_ms (2);

	/* set RX Channel back to default */
	nRFAPI_SetChannel(CONFIG_REFTAG_CHANNEL);

	/* switch to RX mode again */
	nRFAPI_SetRxMode(1);

	GPIOSetValue (1, 2, 0);

	if (power & 0x4)
		nRFCMD_Power(0);
}

void
nrf_off (void)
{
  /* disable RX mode */
  nRFCMD_CE (0);

  /* wait till RX is done */
  pmu_sleep_ms (5);

  /* switch to TX mode */
  nRFAPI_SetRxMode (0);
}

static uint32_t
rnd (uint32_t range)
{
  static uint32_t v1 = 0x52f7d319;
  static uint32_t v2 = 0x6e28014a;

  /* reseed random with timer */
  random_seed += LPC_TMR32B0->TC;

  /* MWC generator, period length 1014595583 */
  return ((((v1 = 36969 * (v1 & 0xffff) + (v1 >> 16)) << 16) ^
	   (v2 = 30963 * (v2 & 0xffff) + (v2 >> 16))) ^ random_seed) % range;
}

int
main (void)
{
  uint32_t SSPdiv;
  uint16_t crc, oid_last_seen;
  uint8_t status, seen_low, seen_high;
  volatile int t;
  int i;

  /* wait on boot - debounce */
  for (t = 0; t < 2000000; t++);

  /* Initialize GPIO (sets up clock) */
  GPIOInit ();

  /* initialize power management */
  pmu_init ();

  LPC_IOCON->PIO2_0 = 0;
  GPIOSetDir (2, 0, 1);		//OUT
  GPIOSetValue (2, 0, 0);

  LPC_IOCON->RESET_PIO0_0 = 0;
  GPIOSetDir (0, 0, 0);		//IN

  LPC_IOCON->PIO0_1 = 0;
  GPIOSetDir (0, 1, 0);		//IN

  LPC_IOCON->PIO1_8 = 0;
  GPIOSetDir (1, 8, 1);		//OUT
  GPIOSetValue (1, 8, 1);

  LPC_IOCON->PIO0_2 = 0;
  GPIOSetDir (0, 2, 1);		//OUT
  GPIOSetValue (0, 2, 0);

  LPC_IOCON->PIO0_3 = 0;
  GPIOSetDir (0, 3, 0);		//IN

  LPC_IOCON->PIO0_4 = 1 << 8;
  GPIOSetDir (0, 4, 1);		//OUT
  GPIOSetValue (0, 4, 1);

  LPC_IOCON->PIO1_9 = 0;	//FIXME
  GPIOSetDir (1, 9, 1);		//OUT
  GPIOSetValue (1, 9, 0);

  LPC_IOCON->PIO0_6 = 0;
  GPIOSetDir (0, 6, 1);		//OUT
  GPIOSetValue (0, 6, 1);

  LPC_IOCON->PIO0_7 = 0;
  GPIOSetDir (0, 7, 1);		//OUT
  GPIOSetValue (0, 7, 0);

  LPC_IOCON->PIO1_7 = 0;
  GPIOSetDir (1, 7, 1);		//OUT
  GPIOSetValue (1, 7, 0);

  LPC_IOCON->PIO1_6 = 0;
  GPIOSetDir (1, 6, 1);		//OUT
  GPIOSetValue (1, 6, 0);

  LPC_IOCON->PIO1_5 = 0;
  GPIOSetDir (1, 5, 1);		//OUT
  GPIOSetValue (1, 5, 0);

  LPC_IOCON->PIO3_2 = 0;	// FIXME
  GPIOSetDir (3, 2, 1);		//OUT
  GPIOSetValue (3, 2, 1);

  LPC_IOCON->PIO1_11 = 0x80;	//FIXME
  GPIOSetDir (1, 11, 1);	// OUT
  GPIOSetValue (1, 11, 0);

  LPC_IOCON->PIO1_4 = 0x80;
  GPIOSetDir (1, 4, 0);		// IN

  LPC_IOCON->ARM_SWDIO_PIO1_3 = 0x81;
  GPIOSetDir (1, 3, 1);		// OUT
  GPIOSetValue (1, 3, 0);

  LPC_IOCON->JTAG_nTRST_PIO1_2 = 0x81;
  GPIOSetDir (1, 2, 1);		// OUT
  GPIOSetValue (1, 2, 0);

  LPC_IOCON->JTAG_TDO_PIO1_1 = 0x81;
  GPIOSetDir (1, 1, 1);		// OUT
  GPIOSetValue (1, 1, 0);

  LPC_IOCON->JTAG_TMS_PIO1_0 = 0x81;
  GPIOSetDir (1, 0, 1);		// OUT
  GPIOSetValue (1, 0, 0);

  LPC_IOCON->JTAG_TDI_PIO0_11 = 0x81;
  GPIOSetDir (0, 11, 1);	// OUT
  GPIOSetValue (0, 11, 0);

  LPC_IOCON->PIO1_10 = 0x80;
  GPIOSetDir (1, 10, 1);	// OUT
  GPIOSetValue (1, 10, 1);

  LPC_IOCON->JTAG_TCK_PIO0_10 = 0x81;
  GPIOSetDir (0, 10, 1);	// OUT
  GPIOSetValue (0, 10, 0);

  LPC_IOCON->PIO0_9 = 0;
  GPIOSetDir (0, 9, 1);		// OUT
  GPIOSetValue (0, 9, 0);

  /* select MISO function for PIO0_8 */
  LPC_IOCON->PIO0_8 = 1;

  /* initialize SPI */
  spi_init ();

  /* Init 3D acceleration sensor */
  acc_init (0);

  /* read device UUID */
  bzero (&device_uuid, sizeof (device_uuid));
  iap_read_uid (&device_uuid);
  tag_id = crc16 ((uint8_t *) & device_uuid, sizeof (device_uuid));
  random_seed =
    device_uuid[0] ^ device_uuid[1] ^ device_uuid[2] ^ device_uuid[3];

  /* Initialize OpenBeacon nRF24L01 interface */
  if (!nRFAPI_Init (81, broadcast_mac, sizeof (broadcast_mac), 0))
    for (;;)
      {
	GPIOSetValue (1, 2, 1);
	pmu_sleep_ms (500);
	GPIOSetValue (1, 2, 0);
	pmu_sleep_ms (500);
      }
  /*set nrf channel*/
  nRFAPI_SetChannel(CONFIG_REFTAG_CHANNEL);
  /* set tx power power to high */
  nRFCMD_Power (1);
  /* switch to RX mode */
  nRFAPI_SetRxMode(1);
  /* transmit data */
  nRFCMD_CE(1);
  /* blink LED for 1s to show readyness */
  GPIOSetValue (1, 2, 1);
  pmu_sleep_ms (1000);
  GPIOSetValue (1, 2, 0);

  //set send away flag
  sendAwayFlag = 0;

  /* disable unused jobs */
  SSPdiv = LPC_SYSCON->SSPCLKDIV;
  i = 0;
  counter = 0;
  seen_low = seen_high = 0;
  oid_last_seen = 0;
  while (1)
    {
      sendAwayFlag = 0;
      counter = 0;
      /* transmit every 50-150ms */
      pmu_sleep_ms (50 + rnd (100));

      /* getting SPI back up again */
      LPC_SYSCON->SSPCLKDIV = SSPdiv;
      if (nRFCMD_IRQ ()){	
	do{
	   // read packet from nRF chip
	   nRFCMD_RegReadBuf (RD_RX_PLOAD, g_Beacon.byte, sizeof (g_Beacon));
		
	   // adjust byte order and decode
	   xxtea_decode (g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

	   // verify the CRC checksum
	   crc = crc16 (g_Beacon.byte, sizeof (g_Beacon) - sizeof (uint16_t));
	
	   if ((ntohs (g_Beacon.forwarder.crc) == crc) &&
		      (g_Beacon.forwarder.hdr.proto == RFBPROTO_BEACONPOSITIONTRACKER))
	   {
	      //add informations of the received package to data structure
	      collectForwardData();
	      //check if flag is set
	      if(sendAwayFlag != 1)
      	      	sendAwayFlag = 1;
	      /* increment counter */
	      counter++;
 	      /* fire up LED to indicate rx */
	      GPIOSetValue (1, 1, 1);
	      /* light LED for 2ms */
	      pmu_sleep_ms (2);
	      /* turn LED off */
	      GPIOSetValue (1, 1, 0);
	   }
	   status = nRFAPI_GetFifoStatus ();
	 }
	 while ((status & FIFO_RX_EMPTY) == 0 || counter < MAX_PACKET_RECEIVE_COUNT_BEFORE_SEND);

	 if((status & FIFO_RX_EMPTY) != 0)
	    counter = MAX_PACKET_RECEIVE_COUNT_BEFORE_SEND;
       }
	  nRFAPI_ClearIRQ (MASK_IRQ_FLAGS);

	
	if(sendAwayFlag > 0 && counter >= MAX_PACKET_RECEIVE_COUNT_BEFORE_SEND ){
	  for(int i = 0; i < COLLECTION_SIZE; i++){
	   /* setup tracking packet */
	   bzero(&g_Beacon, sizeof(g_Beacon));
	   //send away if the dataset not empty
	   for(i = 0; i < COLLECTION_SIZE; i++){
	   	if(g_collection[i].tagId != 0){
			g_collection[i].signals = htonl(g_collection[i].signals);
			g_Beacon.forwarder = g_collection[i];
			g_Beacon.forwarder.hdr.proto = RFBPROTO_BEACONCOLLECTEDFORWARDER;
			g_Beacon.forwarder.hdr.size = sizeof(g_Beacon);

			/* send away packet */
	 		/* transmit packet */
	  		nRF_tx (7);

			g_collection[i].tagId = 0;
			g_collection[i].signals = 0;
		}
	    }
           }
	    //i = 0;
         }

      /* powering down */
      //nRFAPI_PowerDown ();
      LPC_SYSCON->SSPCLKDIV = 0x00;
    }
  return 0;
}
