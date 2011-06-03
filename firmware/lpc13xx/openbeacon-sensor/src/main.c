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

uint32_t g_sysahbclkctrl;

#define MAINCLKSEL_IRC 0
#define MAINCLKSEL_SYSPLL_IN 1
#define MAINCLKSEL_WDT 2
#define MAINCLKSEL_SYSPLL_OUT 3

/* device UUID */
static uint16_t tag_id;
static TDeviceUID device_uuid;

/* set nRF24L01 broadcast mac */
static const unsigned char broadcast_mac[NRF_MAX_MAC_SIZE] = { 1, 2, 3, 2, 1 };

void
int2str (uint32_t value, uint8_t digits, uint8_t base, char* string)
{
  uint8_t digit;
  while(digits)
  {
    digit = value % base;
    value/= base;

    if(digit<=9)
	digit+='0';
    else
	digit='A'+(digit-0xA);

    string[--digits]=digit;
  }
}

int
main (void)
{
  volatile int i;
  /* wait on boot - debounce */
  for (i = 0; i < 2000000; i++);

  /* Initialize GPIO (sets up clock) */
  GPIOInit ();

  /* switch PMU to high pwoer mode*/
  LPC_IOCON->PIO0_5 = 1 << 8;
  GPIOSetDir (0, 5, 1);		//OUT
  GPIOSetValue (0, 5, 0);

  /* initialize power management */
  pmu_init ();

  /* NVIC is installed inside UARTInit file. */
  UARTInit (115200, 0);

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

  /* select UART_TXD */
//  LPC_IOCON->PIO1_7 = 1; FIXME
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

  /* Initialize OpenBeacon nRF24L01 interface */
  if (!nRFAPI_Init (81, broadcast_mac, sizeof (broadcast_mac), 0))
    for (;;)
      {
	GPIOSetValue (1, 2, 1);
	pmu_sleep_ms (100);
	GPIOSetValue (1, 2, 0);
	pmu_sleep_ms (400);
      }
  /* set tx power power to high */
  nRFCMD_Power (1);

  /* test */
  nRFAPI_PowerDown ();

  /* blink LED for 1s to show readyness */
  GPIOSetValue (1, 3, 1);
  pmu_sleep_ms (1000);
  GPIOSetValue (1, 3, 0);

  /* switch MAINCLKSEL to system PLL input */
  LPC_SYSCON->MAINCLKSEL = MAINCLKSEL_SYSPLL_IN;
  /* push clock change */
  LPC_SYSCON->MAINCLKUEN = 0;
  LPC_SYSCON->MAINCLKUEN = 1;
  while (!(LPC_SYSCON->MAINCLKUEN & 1));

  LPC_SYSCON->SSPCLKDIV = 0;
  LPC_SYSCON->UARTCLKDIV = 0;
  LPC_SYSCON->USBCLKDIV = 0;
  LPC_SYSCON->WDTCLKDIV = 0;
  LPC_SYSCON->CLKOUTDIV = 0;
  LPC_SYSCON->SYSTICKCLKDIV = 0;

  /* set system clock to 2MHz */
  LPC_SYSCON->SYSAHBCLKDIV = 6;
  LPC_SYSCON->SYSAHBCLKCTRL = EN_RAM | EN_GPIO | EN_FLASHARRAY | EN_IOCON;

  /* disable unused stuff */
  LPC_SYSCON->PDRUNCFG |= (IRCOUT_PD|IRC_PD|ADC_PD|WDTOSC_PD|SYSPLL_PD|USBPLL_PD|USBPAD_PD);
  /* save current power settings, power WDT on wake */
  LPC_SYSCON->PDAWAKECFG = LPC_SYSCON->PDRUNCFG;
   /* power sysem oscillator & BOD in deep sleep mode */
  LPC_SYSCON->PDSLEEPCFG = (~(SYSOSC_PD|BOD_PD)) & 0xFFF;

  /* switch to PMU low power mode */
  GPIOSetValue (0, 5, 1);
  /* go sleeping */
  LPC_PMU->PCON = (1 << 11) | (1 << 8);
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  LPC_SYSCON->SYSAHBCLKCTRL = EN_FLASHARRAY;
  __WFI ();
  LPC_SYSCON->SYSAHBCLKCTRL = EN_RAM | EN_GPIO | EN_FLASHARRAY | EN_IOCON;
  /* switch to PMU high power mode */
  GPIOSetValue (0, 5, 0);

  /* stop blinking */
  while (1)
    {
    GPIOSetValue (1, 2, 1);
    GPIOSetValue (1, 2, 0);
    GPIOSetValue (1, 1, 1);
    GPIOSetValue (1, 1, 0);
    }



  return 0;
}
