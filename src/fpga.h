#ifndef __FPGA_H__
#define __FPGA_H__

int fpgaProgrammed(void);
int fpgaConnect(void);
int fpgaDisconnect(void);
int fpgaReset(void);
int fpgaUnreset(void);
int fpgaWaitUntilProgrammed(uint32_t max_ticks);
void fpgaJtagAcquire(void);
void fpgaJtagRelease(void);
uint8_t fpgaJtagShift(uint8_t tms, uint8_t tdi, int bits);

#endif /* __FPGA_H__ */
