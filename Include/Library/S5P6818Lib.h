#ifndef _S5P6818_LIB_H_
#define _S5P6818_LIB_H_

#include <Uefi/UefiBaseType.h>

// Get PLL Clock
UINT64
EFIAPI 
S5P6818_GetPLLClock(
    IN UINTN PLLSrc
    );

#endif
