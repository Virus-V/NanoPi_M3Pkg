#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>

#include <nx_s5p68181.h>
#include <Library/S5P6818Lin.h>

// PLL configuration register
struct nx_clkpwr_registerset *clkpwr = (void *)PHY_BASEADDR_CLKPWR_MODULE;

#define PLL_GET_PDIV(s) ((clkpwr->pllsetreg[(s)]>>18) & 0x3F)
#define PLL_GET_MDIV(s) ((clkpwr->pllsetreg[(s)]>> 8) & 0x3FF)
#define PLL_GET_SDIV(s) ((clkpwr->pllsetreg[(s)]>> 0) & 0xFF)

UINT64
EFIAPI
S5P6818_GetPLLClock(
    IN UINTN PLLSrc
    )
{
    ASSERT(PLLSrc < 4);
    UINT64 OscFrequency = FixedPcdGet64(PcdOSCFrequency);
    return OscFrequency / PLL_GET_PDIV(PLLSrc) * PLL_GET_MDIV(PLLSrc) / (1 << PLL_GET_SDIV(PLLSrc));
            
}

