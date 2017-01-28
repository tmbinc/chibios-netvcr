#include <string.h>

#include "hal.h"

#include "fpga.h"

#include "shellcfg.h"
#include "chprintf.h"

void fpgaCommand(BaseSequentialStream *chp, int argc, char *argv[])
{

  if (argc <= 0) {
    chprintf(chp, "Usage: fpga [verb]:"SHELL_NEWLINE_STR);
    chprintf(chp, "    cycle        Power cycle the FPGA"SHELL_NEWLINE_STR);
    chprintf(chp, "    reset        Put the FPGA into reset"SHELL_NEWLINE_STR);
    chprintf(chp, "    unreset      Bring the FPGA out of reset"SHELL_NEWLINE_STR);
    chprintf(chp, "    connect      Connect the FPGA to the SPINOR"SHELL_NEWLINE_STR);
    chprintf(chp, "    disconnect   Disconnect the FPGA to the SPINOR"SHELL_NEWLINE_STR);
    chprintf(chp, "    status       Show status of the FPGA"SHELL_NEWLINE_STR);
    chprintf(chp, "    xvc          Enter XVC mode"SHELL_NEWLINE_STR);
    return;
  }

  if (!strcasecmp(argv[0], "cycle")) {
    chprintf(chp, "Power-cycling FPGA: ");

    if (fpgaReset()) {
      chprintf(chp, "Unable to reset"SHELL_NEWLINE_STR);
      return;
    }

    /* Let the FPGA think about things for a while, in reset */
    chThdSleepMilliseconds(10);

    if (fpgaUnreset()) {
      chprintf(chp, "Unable to un-reset"SHELL_NEWLINE_STR);
      return;
    }

    if (fpgaWaitUntilProgrammed(MS2ST(8000))) {
      chprintf(chp, "FPGA never programmed"SHELL_NEWLINE_STR);
      return;
    }

    chprintf(chp, "Ok"SHELL_NEWLINE_STR);
  }

  else if (!strcasecmp(argv[0], "connect")) {
    chprintf(chp, "Connecting FPGA: ");
    if (fpgaConnect())
      chprintf(chp, "Error"SHELL_NEWLINE_STR);
    else
      chprintf(chp, "Ok"SHELL_NEWLINE_STR);
  }

  else if (!strcasecmp(argv[0], "disconnect")) {
    chprintf(chp, "Disconnecting FPGA: ");
    if (fpgaDisconnect())
      chprintf(chp, "Error"SHELL_NEWLINE_STR);
    else
      chprintf(chp, "Ok"SHELL_NEWLINE_STR);
  }

  else if (!strcasecmp(argv[0], "reset")) {
    chprintf(chp, "Putting FPGA into reset: ");
    if (fpgaReset())
      chprintf(chp, "Error"SHELL_NEWLINE_STR);
    else
      chprintf(chp, "Ok"SHELL_NEWLINE_STR);
  }

  else if (!strcasecmp(argv[0], "unreset")) {
    chprintf(chp, "Bringing FPGA out of reset: ");
    if (fpgaUnreset())
      chprintf(chp, "Error"SHELL_NEWLINE_STR);
    else
      chprintf(chp, "Ok"SHELL_NEWLINE_STR);
  }

  else if (!strcasecmp(argv[0], "status")) {
    chprintf(chp, "FPGA programmed: %s"SHELL_NEWLINE_STR,
                                  fpgaProgrammed()?"Yes":"No");
  }

  else if (!strcasecmp(argv[0], "xvc")) {
    chprintf(chp, "XVC MODE ENTERED"SHELL_NEWLINE_STR);

    fpgaJtagAcquire();

    // this is a variant of the xvc interface; but for
    // memory reasons, tms/tdi are interleaved on byte-level.

    while (1)
    {
      uint8_t cmd[6];
      uint32_t length = 0;
      unsigned int i;

      for (i = 0; i < sizeof(cmd); ++i)
        cmd[i] = streamGet(chp);

      if (memcmp(cmd, "shift:", 6))
      {
        chprintf(chp, "unknown command"SHELL_NEWLINE_STR);
        break;
      }

      length  = streamGet(chp) << 0;
      length |= streamGet(chp) << 8;
      length |= streamGet(chp) << 16;
      length |= streamGet(chp) << 24;

      if (!length)
        continue;

      for (;;)
      {
        uint8_t tms = streamGet(chp);
        uint8_t tdi = streamGet(chp);
        uint8_t tdo = fpgaJtagShift(tms, tdi, (length > 7) ? 8 : length);
        streamPut(chp, tdo);
        if (length <= 8)
          break;
        length -= 8;
      }
    }

    fpgaJtagRelease();
  }

  else {
    chprintf(chp, "Unrecognized command: %s"SHELL_NEWLINE_STR, argv[0]);
  }
}
