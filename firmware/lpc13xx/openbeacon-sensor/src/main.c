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
#include "iap.h"
#include "spi.h"
#include "nRF_API.h"
#include "nRF_CMD.h"
#include "openbeacon-proto.h"

uint32_t g_sysahbclkctrl;

#define MAINCLKSEL_IRC 0
#define MAINCLKSEL_SYSPLL_IN 1
#define MAINCLKSEL_WDT 2
#define MAINCLKSEL_SYSPLL_OUT 3

/* device UUID */
static uint16_t tag_id;
static TDeviceUID device_uuid;

#if 0
/* OpenBeacon packet */
static TBeaconEnvelope g_Beacon;

/* Default TEA encryption key of the tag - MUST CHANGE ! */
static const uint32_t xxtea_key[4] = { 0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF };

/* set nRF24L01 broadcast mac */
static const unsigned char broadcast_mac[NRF_MAX_MAC_SIZE] = { 1, 2, 3, 2, 1 };

static void
nRF_tx (uint8_t power)
{
  /* update crc */
  g_Beacon.pkt.crc = htons (crc16 (g_Beacon.byte, sizeof (g_Beacon)
				   - sizeof (uint16_t)));
  /* encrypt data */
  xxtea_encode (g_Beacon.block, XXTEA_BLOCK_COUNT, xxtea_key);

  pin_led (GPIO_LED0);

  /* update power pin */
  nRFCMD_Power (power & 0x4);

  /* disable RX mode */
  nRFCMD_CE (0);
  vTaskDelay (5 / portTICK_RATE_MS);

  /* switch to TX mode */
  nRFAPI_SetRxMode (0);

  /* set TX power */
  nRFAPI_SetTxPower (power & 0x3);

  /* upload data to nRF24L01 */
  nRFAPI_TX (g_Beacon.byte, sizeof (g_Beacon));

  /* transmit data */
  nRFCMD_CE (1);

  delay (2000);

  /* switch to RX mode again */
  nRFAPI_SetRxMode (1);

  pin_led (GPIO_LEDS_OFF);

  if (power & 0x4)
    nRFCMD_Power (0);
}
#endif

void
nrf_off (void)
{
  /* disable RX mode */
  nRFCMD_CE (0);
//  vTaskDelay (5 / portTICK_RATE_MS);
//FIXME

  /* switch to TX mode */
  nRFAPI_SetRxMode (0);

  /* powering down */
  nRFAPI_PowerDown ();
}

void
WAKEUP_IRQHandlerPIO0_8 (void)
{
  /* set sytem clock divider back to full speed */
  LPC_SYSCON->SYSAHBCLKDIV = 1;
  /* switch to IRC oscillator */
  LPC_SYSCON->MAINCLKSEL = MAINCLKSEL_IRC;
  /* push clock change */
  LPC_SYSCON->MAINCLKUEN = 0;
  LPC_SYSCON->MAINCLKUEN = 1;
  /* wait for clock change to be finished */
  while (!(LPC_SYSCON->MAINCLKUEN & 1));
  /* power down watchdog oscillator */
  LPC_SYSCON->PDRUNCFG |= WDTOSC_PD;

  /* re-trigger match output */
  LPC_TMR16B0->EMR &= ~1;
  /* reset wakeup logic */
  LPC_SYSCON->STARTRSRP0CLR = STARTxPRP0_PIO0_8;
  /* disable deep sleep */
  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
  /* enable previous clock settings  */
  LPC_SYSCON->SYSAHBCLKCTRL = g_sysahbclkctrl;

  /* vodoo -NOP */
  __NOP ();
}

void
pmu_init (void)
{
  /* set sytem clock divider to full speed */
  LPC_SYSCON->SYSAHBCLKDIV = 1;
  /* reset 16B0 timer */
  LPC_TMR16B0->TCR = 2;
  /* Turn on the watchdog oscillator */
  LPC_SYSCON->WDTOSCCTRL = 0x3F;
  /* enable IRQ routine for PIO0_8 */
  NVIC_EnableIRQ (WAKEUP_PIO0_8_IRQn);
  /* initialize start logic for PIO0_8 */
  LPC_SYSCON->STARTAPRP0 |= STARTxPRP0_PIO0_8;
  LPC_SYSCON->STARTRSRP0CLR = STARTxPRP0_PIO0_8;
  LPC_SYSCON->STARTERP0 |= STARTxPRP0_PIO0_8;
}

void
pmu_sleep_ms (uint16_t ms)
{
  /* Turn off all other peripheral dividers FIXME save settings */
  LPC_SYSCON->SSPCLKDIV = 0;
  LPC_SYSCON->USBCLKDIV = 0;
  LPC_SYSCON->WDTCLKDIV = 0;
  LPC_SYSCON->SYSTICKCLKDIV = 0;

  g_sysahbclkctrl = LPC_SYSCON->SYSAHBCLKCTRL;
  LPC_SYSCON->SYSAHBCLKCTRL =
    EN_RAM | EN_GPIO | EN_CT16B0 | EN_FLASHARRAY | EN_IOCON;

  /* prepare 16B0 timer */
  LPC_TMR16B0->TCR = 2;
  LPC_TMR16B0->EMR = 2 << 4;
  LPC_TMR16B0->MR0 = ms;
  /* enable IRQ, reset and timer stop in MR0 match */
  LPC_TMR16B0->MCR = 7;

  /* prepare sleep */
  LPC_PMU->PCON = (1 << 11) | (1 << 8);
  SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;

  /* power up watchdog */
  LPC_SYSCON->PDRUNCFG &= ~WDTOSC_PD;
  /* save current power settings, power WDT on wake */
  LPC_SYSCON->PDAWAKECFG = LPC_SYSCON->PDRUNCFG;
  /* power watchdog oscillator in deep sleep mode */
  LPC_SYSCON->PDSLEEPCFG = (~WDTOSC_PD) & 0xFFF;
  /* switch MAINCLKSEL to Watchdog Oscillator */
  LPC_SYSCON->MAINCLKSEL = MAINCLKSEL_WDT;
  /* push clock change */
  LPC_SYSCON->MAINCLKUEN = 0;
  LPC_SYSCON->MAINCLKUEN = 1;
  /* set clock divider to roughly 1kHz */
  LPC_SYSCON->SYSAHBCLKDIV = 9;
  /* wait for clock change to be executed */
  while (!(LPC_SYSCON->MAINCLKUEN & 1));

  /* start timer */
  LPC_TMR16B0->TCR = 1;
  /* sleep */
  __WFI ();
}

int
main (void)
{
  volatile int i;
  /* wait on boot - debounce */
  for (i = 0; i < 2000000; i++);

  /* Initialize GPIO (sets up clock) */
  GPIOInit ();

  LPC_IOCON->PIO2_0 = 0;
  GPIOSetDir (2, 0, 1);		//OUT
  GPIOSetValue (2, 0, 0);

  LPC_IOCON->RESET_PIO0_0 = 0;
  GPIOSetDir (0, 0, 0);		//IN

  LPC_IOCON->PIO0_1 = 0;
  GPIOSetDir (0, 1, 0);		//IN

  LPC_IOCON->PIO1_8 = 0;
  GPIOSetDir (1, 8, 1);		//OUT
  GPIOSetValue (1, 8, 0);

  LPC_IOCON->PIO0_2 = 0;
  GPIOSetDir (0, 2, 1);		//OUT
  GPIOSetValue (0, 2, 0);

  LPC_IOCON->PIO0_3 = 0;
  GPIOSetDir (0, 3, 0);		//IN

  LPC_IOCON->PIO0_4 = 1 << 8;
  GPIOSetDir (0, 4, 1);		//OUT
  GPIOSetValue (0, 4, 1);

  LPC_IOCON->PIO0_5 = 1 << 8;
  GPIOSetDir (0, 5, 1);		//OUT
  GPIOSetValue (0, 5, 1);

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
  GPIOSetValue (3, 2, 0);

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

  LPC_IOCON->JTAG_TCK_PIO0_10 = 0x80;
  GPIOSetDir (0, 10, 1);	// OUT
  GPIOSetValue (0, 10, 0);

  LPC_IOCON->PIO0_9 = 0;
  GPIOSetDir (0, 9, 1);		// OUT
  GPIOSetValue (0, 9, 0);

  /* select CT16B0_MAT0 for PIO0_8 */
  LPC_IOCON->PIO0_8 = 2;
  GPIOSetDir (0, 8, 0);		// IN

  /* initialize SPI */
//  spi_init ();

#ifdef	SOUND_ENABLE
  /* Init Speaker Output */
  snd_init ();
#endif /*SOUND_ENABLE */

  /* Init 3D acceleration sensor */
//  acc_init (0);

  /* read device UUID */
  bzero (&device_uuid, sizeof (device_uuid));
  iap_read_uid (&device_uuid);
  tag_id = crc16 ((uint8_t *) & device_uuid, sizeof (device_uuid));

  pmu_init ();
  while (1)
    {
      GPIOSetValue (1, 3, 1);
      pmu_sleep_ms (100);
      GPIOSetValue (1, 3, 0);
      pmu_sleep_ms (4900);
    }

  return 0;
}
