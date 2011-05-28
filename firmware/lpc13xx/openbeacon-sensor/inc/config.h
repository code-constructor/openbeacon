/***************************************************************
 *
 * OpenBeacon.org - config file
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* Teshold for detecting 3D accelerometer movement */
#define ACC_TRESHOLD 2

/* SPI_CS(io_port, io_pin, CPSDVSR frequency, mode) */
#define SPI_CS_NRF   SPI_CS( 1,10, 5, SPI_CS_MODE_NORMAL  )	/*  9.6MHz */
#define SPI_CS_ACC3D SPI_CS( 0, 4, 6, SPI_CS_MODE_NORMAL  )	/*  8.0MHz */

#define NRF_MAX_MAC_SIZE 5

#endif/*__CONFIG_H__*/
