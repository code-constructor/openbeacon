/***************************************************************
 *
 * OpenBeacon.org - GPIO declaration
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

/* IO definitions */
#define BUTTON0_PORT 1
#define BUTTON0_BIT 4
#define LED0_PORT 1
#define LED0_BIT 3

void
pin_led (uint8_t led)
{
  GPIOSetValue (LED0_PORT, LED0_BIT, (led > 0));
}

uint8_t
pin_button0 (void)
{
  return GPIOGetValue (BUTTON0_PORT, BUTTON0_BIT);
}

void
pin_init (void)
{
  /* Initialize GPIO (sets up clock) */
  GPIOInit ();

  /* switch ISP button pin to input */
  LPC_IOCON->PIO1_4 = 0x80;
  GPIOSetDir (BUTTON0_PORT, BUTTON0_BIT, 0);

  /* Set LED0 port pin to output */
  LPC_IOCON->ARM_SWDIO_PIO1_3 = 0x81;
  GPIOSetDir (LED0_PORT, LED0_BIT, 1);
  GPIOSetValue (LED0_PORT, LED0_BIT, 0);
}
