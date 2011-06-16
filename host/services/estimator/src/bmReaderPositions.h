/***************************************************************
 *
 * OpenBeacon.org - Reader Position Settings
 *
 * Copyright 2009 Milosch Meriac <meriac@bitmanufaktur.de>
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

#ifndef BMREADERPOSITIONS_H_
#define BMREADERPOSITIONS_H_

typedef struct
{
  uint32_t id, room, floor, group;
  double x, y;
} TReaderItem;

#define IPv4(a,b,c,d) ( ((uint32_t)a)<<24 | ((uint32_t)b)<<16 | ((uint32_t)c)<<8 | ((uint32_t)d)<<0 )

static const TReaderItem g_ReaderList[] = {
  // HTW: group 1
  {IPv4 (10, 1, 254, 100), 707, 7, 1, 15.5, 5.5},
  {IPv4 (10, 1, 254, 103), 707, 7, 1, 0.5, 0.5},
  {IPv4 (10, 1, 254, 104), 707, 7, 1, 0.5, 5.5},
  {IPv4 (10, 1, 254, 105), 707, 7, 1, 8.0, 3.0},
  {IPv4 (10, 1, 254, 106), 707, 7, 1, 15.5, 0.5},

  // FH Wildau: group=10
  {107, 1, 2, 10, 656, 538},
  {108, 1, 2, 10, 596, 538},
  {109, 1, 2, 10, 536, 538},
  {110, 1, 2, 10, 445, 538},
  {111, 1, 2, 10, 384, 538},
  {112, 13, 2, 10, 5, 5},
  {113, 13, 2, 10, 75, 5},
  {114, 13, 2, 10, 5, 91},
  {115, 13, 2, 10, 75, 91}
};

#define READER_COUNT (sizeof(g_ReaderList)/sizeof(g_ReaderList[0]))

#endif
