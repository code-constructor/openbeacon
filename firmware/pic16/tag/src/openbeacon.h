/***************************************************************
 *
 * OpenBeacon.org - OnAir protocol specification and definition
 *
 * Copyright 2006 Milosch Meriac <meriac@openbeacon.de>
 *
/***************************************************************

/*
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

#ifndef __OPENBEACON_H__
#define __OPENBEACON_H__

#define TEA_ENCRYPTION_BLOCK_COUNT 4

#define RFB_RFOPTIONS		0x0F

#define RFBPROTO_BEACONTRACKER	23
#define RFBPROTO_BEACONNODE	24
#define RFBPROTO_BEACONVOTESTATS	25
#define RFBPROTO_BEACONPOSITIONTRACKER	27

#define RFBFLAGS_ACK		0x01
#define RFBFLAGS_SENSOR		0x02
#define RFBFLAGS_VOTE_MASK      0x1C
#define RFBFLAGS_REQUEST_VOTE_TRANSMISSION  0x20

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;

typedef struct
{
  u_int8_t size, proto;
} TBeaconHeader;

typedef struct
{
  TBeaconHeader hdr;
  u_int8_t flags, strength;
  u_int32_t seq;
  u_int32_t oid;
  u_int16_t reserved;
  u_int16_t crc;
} TBeaconTracker;

//struct for iCampus-Wildau Project
typedef struct
{
	TBeaconHeader hdr;
	u_int8_t building;
	u_int8_t strengthAndZ; //first 4 bits the strength
	u_int16_t x;
	u_int16_t y;
	u_int16_t oid;
	u_int32_t seq;
	u_int16_t crc;
} TBeaconPositionTracker;

typedef union
{
  TBeaconPositionTracker pos;
  TBeaconTracker pkt;
  u_int32_t data[TEA_ENCRYPTION_BLOCK_COUNT];
  u_int8_t datab[TEA_ENCRYPTION_BLOCK_COUNT * sizeof (u_int32_t)];
}
TBeaconEnvelope;

#endif/*__OPENBEACON_H__*/
