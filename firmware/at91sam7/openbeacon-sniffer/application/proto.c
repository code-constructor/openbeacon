/***************************************************************
 *
 * OpenBeacon.org - OpenBeacon link layer protocol
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
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
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <string.h>
#include <board.h>
#include <beacontypes.h>
#include <USB-CDC.h>
#include "led.h"
#include "xxtea.h"
#include "proto.h"
#include "nRF24L01/nRF_HW.h"
#include "nRF24L01/nRF_CMD.h"
#include "nRF24L01/nRF_API.h"

const unsigned char broadcast_mac[NRF_MAX_MAC_SIZE] = { 1, 2, 3, 2, 1 };
TBeaconEnvelope g_Beacon;

/**********************************************************************/
#define SHUFFLE(a,b)    tmp=g_Beacon.datab[a];\
                        g_Beacon.datab[a]=g_Beacon.datab[b];\
                        g_Beacon.datab[b]=tmp;

/**********************************************************************/
void
shuffle_tx_byteorder (void)
{
  unsigned char tmp;

  SHUFFLE (0 + 0, 3 + 0);
  SHUFFLE (1 + 0, 2 + 0);
  SHUFFLE (0 + 4, 3 + 4);
  SHUFFLE (1 + 4, 2 + 4);
  SHUFFLE (0 + 8, 3 + 8);
  SHUFFLE (1 + 8, 2 + 8);
  SHUFFLE (0 + 12, 3 + 12);
  SHUFFLE (1 + 12, 2 + 12);
}

static inline s_int8_t
PtInitNRF (void)
{
  if (!nRFAPI_Init
      (DEFAULT_CHANNEL, broadcast_mac, sizeof (broadcast_mac),
       ENABLED_NRF_FEATURES))
    return 0;

  nRFAPI_SetPipeSizeRX (0, 16);
  nRFAPI_SetTxPower (3);
  nRFAPI_SetRxMode (1);
  nRFCMD_CE (1);

  return 1;
}

static inline unsigned short
swapshort (unsigned short src)
{
  return (src >> 8) | (src << 8);
}

static inline unsigned long
swaplong (unsigned long src)
{
  return (src >> 24) | (src << 24) | ((src >> 8) & 0x0000FF00) | ((src << 8) &
								  0x00FF0000);
}

static inline short
crc16 (const unsigned char *buffer, int size)
{
  unsigned short crc = 0xFFFF;

  if (buffer && size)
    while (size--)
      {
	crc = (crc >> 8) | (crc << 8);
	crc ^= *buffer++;
	crc ^= ((unsigned char) crc) >> 4;
	crc ^= crc << 12;
	crc ^= (crc & 0xFF) << 5;
      }

  return crc;
}

static inline void
DumpUIntToUSB (unsigned int data)
{
  int i = 0;
  unsigned char buffer[10], *p = &buffer[sizeof (buffer)];

  do
    {
      *--p = '0' + (unsigned char) (data % 10);
      data /= 10;
      i++;
    }
  while (data);

  while (i--)
    vUSBSendByte (*p++);
}

static inline void
DumpStringToUSB (char *text)
{
  unsigned char data;

  if (text)
    while ((data = *text++) != 0)
      vUSBSendByte (data);
}

void
vnRFtaskRx (void *parameter)
{
  (void) parameter;

  if (!PtInitNRF ())
    return;

  for (;;)
    {
      if (nRFCMD_WaitRx (10))
	{
	  vLedSetRed (1);

	  do
	    {
	      // read packet from nRF chip
	      nRFCMD_RegReadBuf (RD_RX_PLOAD, g_Beacon.datab,
				 sizeof (g_Beacon));

	      // adjust byte order and decode
	      shuffle_tx_byteorder ();
	      xxtea_decode ();
	      shuffle_tx_byteorder ();

	      //select protocol
	      switch(g_Beacon.pos.hdr.proto){
	      	  case RFBPROTO_BEACONPOSITIONTRACKER:
	      		showInformationFromPosTracker();
	      		break;
	      	  case RFBPROTO_BEACONTRACKER:
	      		//showInformationFromTracker();
	      		break;
	      	  case RFBPROTO_BEACONCOLLECTEDFORWARDER:
	      		showInformationFromForwarder();
	      		break;
	      	  default:
	      		DumpStringToUSB ("cant read Protocol : ");
	      		DumpUIntToUSB (g_Beacon.pos.hdr.proto);
	      		DumpStringToUSB ("\n\r ");
	      		break;
	      }

	    }
	  while ((nRFAPI_GetFifoStatus () & FIFO_RX_EMPTY) == 0);

	  vLedSetRed (0);
	  nRFAPI_GetFifoStatus ();
	}
      nRFAPI_ClearIRQ (MASK_IRQ_FLAGS);
    }
}

/**
 * show information for RFBPROTO_BEACONPOSITIONTRACKER Protocol
 */
void
showInformationFromForwarder(void)
{
	u_int16_t crc;
		   // verify the crc checksum
		crc = crc16 (g_Beacon.datab,
				     sizeof (g_Beacon) - sizeof (g_Beacon.forwarder.crc));
		if ((swapshort (g_Beacon.forwarder.crc) == crc)){
			DumpStringToUSB ("Forwarder: oid:");
			DumpUIntToUSB (g_Beacon.forwarder.oid);
			DumpStringToUSB (", signals[0]:");
			DumpUIntToUSB (swapshort(g_Beacon.forwarder.signals) & 0x000F);
			DumpStringToUSB (", signals[1]:");
			DumpUIntToUSB ((swapshort(g_Beacon.forwarder.signals) >> 4) & 0x000F);
			DumpStringToUSB (", signals[2]:");
			DumpUIntToUSB ((swapshort(g_Beacon.forwarder.signals) >> 8) & 0x000F);
			DumpStringToUSB (", signals[3]:");
			DumpUIntToUSB ((swapshort(g_Beacon.forwarder.signals) >> 12) & 0x000F);
			DumpStringToUSB (", tagId:");
			DumpUIntToUSB (swapshort(g_Beacon.forwarder.tagId));
			DumpStringToUSB (", x:");
			DumpUIntToUSB (swapshort(g_Beacon.forwarder.x));
			DumpStringToUSB (", y:");
			DumpUIntToUSB (swapshort(g_Beacon.forwarder.y));
			DumpStringToUSB (", building:");
			DumpUIntToUSB (g_Beacon.forwarder.building);
			DumpStringToUSB ("\n\r");
		}
}

/**
 * show information for RFBPROTO_BEACONPOSITIONTRACKER Protocol
 */
void
showInformationFromPosTracker(void)
{
	u_int16_t crc;
	   // verify the crc checksum
	crc = crc16 (g_Beacon.datab,
			     sizeof (g_Beacon) - sizeof (g_Beacon.pkt.crc));
	if ((swapshort (g_Beacon.pkt.crc) == crc)){
		DumpStringToUSB ("PositonTracker: oid:");
		DumpUIntToUSB (swapshort (g_Beacon.pos.oid));
		DumpStringToUSB (", seq:");
		DumpUIntToUSB (swaplong(g_Beacon.pos.seq));
		DumpStringToUSB (", strength:");
		DumpUIntToUSB (g_Beacon.pos.strengthAndZ >> 4);
		DumpStringToUSB (", building:");
		DumpUIntToUSB (g_Beacon.pos.building);
		DumpStringToUSB (", x:");
		DumpUIntToUSB (swapshort(g_Beacon.pos.x));
		DumpStringToUSB (", y:");
		DumpUIntToUSB (swapshort(g_Beacon.pos.y));
		DumpStringToUSB (", z:");
		DumpUIntToUSB (g_Beacon.pos.strengthAndZ & 0x0F);
		DumpStringToUSB ("\n\r");
	}
}

/**
 * show information for RFBPROTO_BEACONTRACKER Protocol
 */
void
showInformationFromTracker(void)
{
	u_int16_t crc;
	   // verify the crc checksum
	crc = crc16 (g_Beacon.datab,
			     sizeof (g_Beacon) - sizeof (g_Beacon.pkt.crc));
	if ((swapshort (g_Beacon.pkt.crc) == crc)){
		DumpStringToUSB ("Tracker: ");
		DumpUIntToUSB (swaplong (g_Beacon.pkt.oid));
		DumpStringToUSB (",");
		DumpUIntToUSB (swaplong (g_Beacon.pkt.seq));
		DumpStringToUSB (",");
		DumpUIntToUSB (g_Beacon.pkt.strength);
		DumpStringToUSB (",");
		DumpUIntToUSB (g_Beacon.pkt.flags);
		DumpStringToUSB ("\n\r");
	}
}

void
vInitProtocolLayer (void)
{
  xTaskCreate (vnRFtaskRx, (signed portCHAR *) "nRF_Rx", TASK_NRF_STACK,
	       NULL, TASK_NRF_PRIORITY, NULL);
}
