/***************************************************************
 *
 * OpenBeacon.org - Power Management
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
#include "pmu.h"

#define CPU_MODE_PMU_PORT 0
#define CPU_MODE_PMU_BIT 5

/* Watchdog oscillator control register setup */
#define FREQSEL 1
#define FCLKANA 500000.0
#define DIVSEL 0x1F
#define WDT_OSC_CLK (FCLKANA/(2.0*(1+DIVSEL)))

#define MAINCLKSEL_IRC_OSC 0
#define MAINCLKSEL_SYS_PLL 1
#define MAINCLKSEL_WDT_OSC 2
#define MAINCLKSEL_SYS_PLL_OUT 3

static const uint32_t pmu_reason_signature = 0xDEADBEEF;

void
deep_sleep_ms (uint32_t milliseconds)
{
  (void) milliseconds;

  /* back up current power states */
  LPC_SYSCON->PDAWAKECFG = LPC_SYSCON->PDRUNCFG;
  __asm volatile ("WFI");
}

void
pmu_sleep (void)
{
}

static inline void
pmu_mode (uint8_t mode)
{
  GPIOSetValue (CPU_MODE_PMU_PORT, CPU_MODE_PMU_BIT, mode);
}

void
pmu_off (uint32_t reason)
{
  pmu_mode (1);

  LPC_SYSCON->PDSLEEPCFG = 0xFFFFFFFF;

  /* store reason */
  LPC_PMU->GPREG0 = pmu_reason_signature;
  LPC_PMU->GPREG1 = reason;

  LPC_PMU->PCON = (1<<1)|(1<<11);
  SCB->SCR |= NVIC_LP_SLEEPDEEP;
  LPC_SYSCON->PDRUNCFG &= ~(EN_SYS|EN_ROM);
  __WFI ();
}

uint32_t
pmu_reason (void)
{
  return (LPC_PMU->GPREG0==pmu_reason_signature)?LPC_PMU->GPREG1:0;
}

void
pmu_init (void)
{
  /* Set to PMU high power mode by default */
  LPC_PMU->PCON = 0;
  LPC_IOCON->PIO0_5 = 0;
  GPIOSetDir (CPU_MODE_PMU_PORT, CPU_MODE_PMU_BIT, 1);

  pmu_mode (0);
}
