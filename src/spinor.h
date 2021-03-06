#ifndef __SPINOR_H__
#define __SPINOR_H__

struct SPIDriver;

uint8_t spinorGetStatus(SPIDriver *spip);
uint32_t spinorReadDeviceId(SPIDriver *spip);
uint32_t spinorReadElectronicSignature(SPIDriver *spip);
uint32_t spinorReadDeviceIdType(SPIDriver *spip, uint8_t id);
int spinorEnableWrite(SPIDriver *spip);
int spinorDisableWrite(SPIDriver *spip);
int spinorEraseChip(SPIDriver *spip);
int spinorErasePage(SPIDriver *spip, int page);
int spinorProgramPage(SPIDriver *spip, int page,
                      size_t count, const void *data);

/* These must be implemented for everything to work.*/
extern int spiConfigure(SPIDriver *spip);
extern int spiDeconfigure(SPIDriver *spip);

#endif /* __SPINOR_H__ */
