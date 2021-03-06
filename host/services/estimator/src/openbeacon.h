/***************************************************************
 *
 * OpenBeacon.org - OnAir protocol specification and definition
 *
 * Copyright 2006 Milosch Meriac <meriac@openbeacon.de>
 *
 ***************************************************************/

/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation; version 3.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __OPENBEACON_H__
#define __OPENBEACON_H__

#define CONFIG_TRACKER_CHANNEL 81
#define CONFIG_PROX_CHANNEL 76

#define XXTEA_BLOCK_COUNT 4

#define RFBPROTO_READER_ANNOUNCE 22
#define RFBPROTO_READER_COMMAND  23
#define RFBPROTO_BEACONTRACKER   24
#define RFBPROTO_BEACONPOSTRACKER	27
#define RFBPROTO_BEACONFORWARDER	28
#define RFBPROTO_PROXTRACKER     42
#define RFBPROTO_PROXREPORT      69

#define PROX_MAX 4

#define RFBFLAGS_ACK			0x01
#define RFBFLAGS_SENSOR			0x02
#define RFBFLAGS_INFECTED		0x04

/* RFBPROTO_READER_COMMAND related opcodes */
#define READER_CMD_NOP			0x00
#define READER_CMD_RESET		0x01
#define READER_CMD_RESET_CONFIG		0x02
#define READER_CMD_RESET_FACTORY	0x03
#define READER_CMD_RESET_WIFI		0x04
#define READER_CMD_SET_OID		0x05
/* RFBPROTO_READER_COMMAND related results */
#define READ_RES__OK			0x00
#define READ_RES__DENIED		0x01
#define READ_RES__UNKNOWN_CMD		0xFF

#define PACKED  __attribute__((__packed__))

typedef struct
{
  u_int8_t size, proto;
} PACKED TBeaconHeader;

typedef struct
{
  u_int8_t strength;
  u_int16_t oid_last_seen;
  u_int16_t powerup_count;
  u_int8_t reserved;
  u_int32_t seq;
} PACKED TBeaconTracker;

typedef struct
{
  u_int16_t oid_prox[PROX_MAX];
  u_int16_t seq;
} PACKED TBeaconProx;

typedef struct
{
  u_int8_t opcode, res;
  u_int32_t data[2];
} PACKED TBeaconReaderCommand;

typedef struct
{
  u_int8_t opcode, strength;
  u_int32_t uptime, ip;
} PACKED TBeaconReaderAnnounce;

typedef struct
{
  TBeaconHeader hdr;
  u_int8_t oid; //own id
  u_int16_t tagID; //consists of oid an z-coordinate
  u_int32_t signal; //counted signals (4bits for every signal strength) strength 0 is on the right side
  u_int16_t x; //x coordinate of the tag
  u_int16_t y; //y coordinate of the tag
  u_int8_t building; //building-number where the tag is
  u_int16_t crc; //crc checksum
} PACKED TBeaconForwarder;


typedef struct
{
  TBeaconHeader hdr;
  u_int8_t building;
  u_int8_t strengthAndZ;
  u_int16_t x;
  u_int16_t y;
  u_int16_t oid;
  u_int32_t seq;
  u_int16_t crc;
} PACKED TBeaconPositionTracker;

typedef union
{
  TBeaconProx prox;
  TBeaconTracker tracker;
  TBeaconReaderCommand reader_command;
  TBeaconReaderAnnounce reader_announce;
} PACKED TBeaconPayload;

typedef struct
{
  u_int8_t proto;
  u_int16_t oid;
  u_int8_t flags;

  TBeaconPayload p;

  u_int16_t crc;
} PACKED TBeaconWrapper;

typedef union
{
  TBeaconForwarder forw;
  TBeaconWrapper pkt;
  TBeaconPositionTracker pos;
  u_int32_t block[XXTEA_BLOCK_COUNT];
  u_int8_t byte[XXTEA_BLOCK_COUNT * 4];
} PACKED TBeaconEnvelope;

#endif/*__OPENBEACON_H__*/
