/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"

#include "chprintf.h"
#include "shellcfg.h"
#include "shell.h"

#include "usbcfg.h"
#include "spinor.h"

#define SPI_TIMEOUT MS2ST(3000)

extern const char *gitversion;

static uint8_t done_state = 0;
/* Triggered when done goes low, white LED stops flashing */
static void extcb1(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  done_state = 1;
}

static const EXTConfig extcfg = {
  {
   {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART, extcb1, PORTA, 1}
  }
};

extern void programDumbRleFile(void);

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

#define stream (BaseSequentialStream *)&SDU1

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  // IOPORT1 = PORTA, IOPORT2 = PORTB, etc...
  palClearPad(IOPORT1, 4);    // white LED, active low
  palClearPad(IOPORT3, 3);    // MCU_F_MODE, set to 0, specifies SPI mode. Clear to 0 for SPI.

  /* Connect the FPGA to the SPINOR */
  fpgaConnect();

  /*
   * Activates the EXT driver 1.
   */
  extStart(&EXTD1, &extcfg);

  shellInit();

  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1000);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  /*
   * Normal main() thread activity, spawning shells.
   */
  while (true) {
    if (SDU1.config->usbp->state == USB_ACTIVE) {
      /* Wait for the user to send a keystroke */
      {
        uint8_t dummy;
        streamRead(stream, &dummy, 1);
      }

      chprintf(stream, SHELL_NEWLINE_STR SHELL_NEWLINE_STR);
      chprintf(stream, "NeTVCR bootloader.  Based on build %s"SHELL_NEWLINE_STR,
               gitversion);
      chprintf(stream, "Core free memory : %d bytes"SHELL_NEWLINE_STR,
               chCoreGetStatusX());

      thread_t *shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE,
                                              "shell", NORMALPRIO + 1,
                                              shellThread, (void *)&shell_cfg);
      chThdWait(shelltp);               /* Waiting termination.             */
    }
    chThdSleepMilliseconds(1000);
  }
}

/* TAR settings for n bits at SYSCLK / 4 */
#define KINETIS_SPI_TAR_SYSCLK_DIV_4(n)\
      SPIx_CTARn_FMSZ((n) - 1) | \
    SPIx_CTARn_CPOL | \
    SPIx_CTARn_CPHA | \
    SPIx_CTARn_DBR | \
    SPIx_CTARn_PBR(0) | \
    SPIx_CTARn_BR(0x1) | \
    SPIx_CTARn_CSSCK(0x1) | \
    SPIx_CTARn_ASC(0x1) | \
    SPIx_CTARn_DT(0x1)
#define KINETIS_SPI_TAR_8BIT_NOT_AS_FAST   KINETIS_SPI_TAR_SYSCLK_DIV_4(8)

int spiConfigure(SPIDriver *spip) {
  static const SPIConfig spinor_config = {
    NULL,
    /* HW dependent part.*/
    GPIOC,
    4,
    KINETIS_SPI_TAR_8BIT_NOT_AS_FAST
  };

  // FPGA_DRIVE, send it low, switching bus from FPGA to us
  palSetPadMode(IOPORT2, 0, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(IOPORT2, 0);

#warning "Figure out why we need this, and why PCS doesn't work."
  palSetPadMode(IOPORT3, 4, PAL_MODE_OUTPUT_PUSHPULL);
//  palSetPadMode(IOPORT3, 4, PAL_MODE_ALTERNATIVE_2);
  palSetPadMode(IOPORT3, 5, PAL_MODE_ALTERNATIVE_2);
  palSetPadMode(IOPORT3, 6, PAL_MODE_ALTERNATIVE_2);
  palSetPadMode(IOPORT3, 7, PAL_MODE_ALTERNATIVE_2);
  spiStart(spip, &spinor_config);

  /*
   * This seems to be required to get communication to work.
   * Send a dummy byte down the wire, since the first packet is ignored.
   */
#warning "Figure out why this dummy byte is needed"
  spiSelect(spip);
  uint8_t dummy = 0xff;
  spiSend(spip, 1, &dummy);
  spiUnselect(spip);

  spinorEnableWrite(spip);
  uint8_t seq[4] = {0x01, 0x00, 0x00, 0x00};
  spiSelect(spip);
  spiSend(spip, 2, seq);
  spiUnselect(spip);

  int start_time = chVTGetSystemTimeX();
  int sleep_msecs = 1;
  while (spinorGetStatus(spip) & 0x01) {
    if (chVTTimeElapsedSinceX(start_time) > SPI_TIMEOUT)
      return MSG_TIMEOUT;
    chThdSleepMilliseconds(sleep_msecs += 5);
  }

  return 0;
}

int spiDeconfigure(SPIDriver *spip) {
  spiStop(spip);
  palSetPadMode(IOPORT3, 4, PAL_MODE_INPUT);
  palSetPadMode(IOPORT3, 5, PAL_MODE_INPUT);
  palSetPadMode(IOPORT3, 6, PAL_MODE_INPUT);
  palSetPadMode(IOPORT3, 7, PAL_MODE_INPUT);

  palSetPadMode(IOPORT2, 0, PAL_MODE_INPUT); // FPGA_DRIVE, let it float up

  return 0;
}
