/** @file
  Serial I/O Port library functions with no library constructor/destructor

  Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2012 - 2016, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>

#include <Library/PcdLib.h>
#include <Library/SerialPortLib.h>
#include <Library/S5P6818Lib.h>

#include <nx_s5p6818.h>
#include <nx_clkpwr.h>
#include <nx_clkgen.h>
#include <nx_rstcon.h>
#include <nx_uart.h>
#include <nx_gpio.h>

#define UART_CLKGEN_FREQ 100000000  // 100MHz
static volatile UINT32 *serial = (void *)PHY_BASEADDR_UART0_MODULE;

/** Initialise the serial device hardware with default settings.

  @retval RETURN_SUCCESS            The serial device was initialised.
  @retval RETURN_INVALID_PARAMETER  One or more of the default settings
                                    has an unsupported value.
 **/
RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  UINT64              BaudRate;
  UINT32              ReceiveFifoDepth;
  EFI_PARITY_TYPE     Parity;
  UINT8               DataBits;
  EFI_STOP_BITS_TYPE  StopBits;
  volatile struct nx_rstcon_registerset *rst = (void *)PHY_BASEADDR_RSTCON_MODULE;
  volatile struct nx_clkgen_registerset *clk = (void *)PHY_BASEADDR_CLKGEN22_MODULE;
  volatile struct nx_gpio_registerset *gpio = (void *)PHY_BASEADDR_GPIOD_MODULE;
  UINTN divisor;

  BaudRate = FixedPcdGet64 (PcdUartDefaultBaudRate);
  ReceiveFifoDepth = 0;         // Use default FIFO depth
  Parity = (EFI_PARITY_TYPE)FixedPcdGet8 (PcdUartDefaultParity);
  DataBits = FixedPcdGet8 (PcdUartDefaultDataBits);
  StopBits = (EFI_STOP_BITS_TYPE) FixedPcdGet8 (PcdUartDefaultStopBits);

  // Reset Serial
  rst->regrst[RESETINDEX_OF_UART0_MODULE_nUARTRST >> 5] &= ~(1 << (RESETINDEX_OF_UART0_MODULE_nUARTRST & 0x1F)); // Clear bit
  rst->regrst[RESETINDEX_OF_UART0_MODULE_nUARTRST >> 5] |=  (1 << (RESETINDEX_OF_UART0_MODULE_nUARTRST & 0x1F)); // Set bit

  // Sets Serial clock source
  clk->clkgen[0] = (clk->clkgen[0] & ~(0x7 << 2)) | (2 << 2); // Set Pll source 2
  // Set Serial Clock frequency
  divisor = (UINTN)(((double)S5P6818_GetPLLClock(2)/UART_CLKGEN_FREQ) + 0.5);
  clk->clkgen[0] = (clk->clkgen[0] & ~(0xFF << 5)) | ((divisor - 1) << 5);

  // Set Serial params
  serial[UARTRUCON] |= 0x3 << 8;    // Rx/Tx Interrupt Mode (Level Mode)
  serial[UARTRUCON] = (serial[UARTRUCON] & ~0xF) | 0x5; // Set Rx/Tx use Interrupt

  serial[UARTDLCON] &= ~(0x7 << 3); // Clear Parity bit fields
  switch(Parity){   // Set Parity bits
  case EvenParity:
    serial[UARTDLCON] |= 5 << 3;
    break;
  case OddParity:
    serial[UARTDLCON] |= 4 << 3;
    break;
  case NoParity:
  default:
    serial[UARTDLCON] |= 0 << 3;
    break;
  }
  // Stop bits
  switch(StopBits){
  case TwoStopBits:
    serial[UARTDLCON] |= 1 << 2;
    break;
  case OneStopBit:
  default:
    serial[UARTDLCON] &= ~(1 << 2);
  }
  // Data Length
  serial[UARTDLCON] &= ~0x3;
  switch(DataBits){
  case 5:
    serial[UARTDLCON] |= 0;
    break;
  case 6:
    serial[UARTDLCON] |= 1;
    break;
  case 7:
    serial[UARTDLCON] |= 2;
    break;
  case 8:
  default:
    serial[UARTDLCON] |= 3;
  }
  // Set baudrate divisor
  divisor = (UINTN)((((double)S5P6818_GetPLLClock(2) / (divisor + 1)) / BaudRate) + 0.5);
  serial[UARTBRDR] = divisor >> 4;
  serial[UARTFRACVAL] = divisor & 0xF;
  // Set FIFOs
  serial[UARTEFCON] |= 0x3 << 1;
  while(serial[UARTEFCON] & (0x3 << 1));    // Wait reset complete
  // Enable FIFO
  serial[UARTEFCON] |= 0x1;
  // Set GPIO AltFn
  gpio->gpioxaltfn[0] = (gpio->gpioxaltfn[0] & ~(0x3 << 0x1C)) | (0x1 << 0x1C);
  gpio->gpioxaltfn[1] = (gpio->gpioxaltfn[1] & ~(0x3 << 0x4)) | (0x1 << 0x4);
  return EFI_SUCCESS;
}

/**
  Write data to serial device.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Number of output bytes which are cached in Buffer.

  @retval 0                Write data failed.
  @retval !0               Actual number of bytes written to serial device.

**/
UINTN
EFIAPI
SerialPortWrite (
  IN UINT8     *Buffer,
  IN UINTN     NumberOfBytes
  )
{
  UINTN i;
  for(i = 0; i < NumberOfBytes; i++){
    while(((serial[UARTFSTATUS] >> 16) & 0x1FF) >= 0x100);    // wait until buffer not full
    serial[UARTTXH] = Buffer[i];
  }
  return i;
}

/**
  Read data from serial device and save the data in buffer.

  @param  Buffer           Point of data buffer which need to be written.
  @param  NumberOfBytes    Number of output bytes which are cached in Buffer.

  @retval 0                Read data failed.
  @retval !0               Actual number of bytes read from serial device.

**/
UINTN
EFIAPI
SerialPortRead (
  OUT UINT8     *Buffer,
  IN  UINTN     NumberOfBytes
)
{
  UINTN i;
  for(i = 0; i < NumberOfBytes; i++){
    while((serial[UARTFSTATUS] & 0x1FF) == 0);  // wait until data come
    Buffer[i] = (UINT8)serial[UARTRXH];
  }
  return i;
}

/**
  Check to see if any data is available to be read from the debug device.

  @retval TRUE       At least one byte of data is available to be read
  @retval FALSE      No data is available to be read

**/
BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  return serial[UARTFSTATUS] & 0x1FF ? TRUE : FALSE;
}
/**
  Set new attributes to PL011.

  @param  BaudRate                The baud rate of the serial device. If the
                                  baud rate is not supported, the speed will
                                  be reduced down to the nearest supported one
                                  and the variable's value will be updated
                                  accordingly.
  @param  ReceiveFifoDepth        The number of characters the device will
                                  buffer on input. If the specified value is
                                  not supported, the variable's value will
                                  be reduced down to the nearest supported one.
  @param  Timeout                 If applicable, the number of microseconds the
                                  device will wait before timing out a Read or
                                  a Write operation.
  @param  Parity                  If applicable, this is the EFI_PARITY_TYPE
                                  that is computed or checked as each character
                                  is transmitted or received. If the device
                                  does not support parity, the value is the
                                  default parity value.
  @param  DataBits                The number of data bits in each character
  @param  StopBits                If applicable, the EFI_STOP_BITS_TYPE number
                                  of stop bits per character. If the device
                                  does not support stop bits, the value is the
                                  default stop bit value.

  @retval EFI_SUCCESS             All attributes were set correctly.
  @retval EFI_INVALID_PARAMETERS  One or more attributes has an unsupported
                                  value.

**/
RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  return RETURN_UNSUPPORTED;
}

/**

  Assert or deassert the control signals on a serial port.
  The following control signals are set according their bit settings :
  . Request to Send
  . Data Terminal Ready

  @param[in]  Control  The following bits are taken into account :
                       . EFI_SERIAL_REQUEST_TO_SEND : assert/deassert the
                         "Request To Send" control signal if this bit is
                         equal to one/zero.
                       . EFI_SERIAL_DATA_TERMINAL_READY : assert/deassert
                         the "Data Terminal Ready" control signal if this
                         bit is equal to one/zero.
                       . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : enable/disable
                         the hardware loopback if this bit is equal to
                         one/zero.
                       . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : not supported.
                       . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : enable/
                         disable the hardware flow control based on CTS (Clear
                         To Send) and RTS (Ready To Send) control signals.

  @retval  RETURN_SUCCESS      The new control bits were set on the device.
  @retval  RETURN_UNSUPPORTED  The device does not support this operation.

**/
RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  return RETURN_UNSUPPORTED;
}

/**

  Retrieve the status of the control bits on a serial device.

  @param[out]  Control  Status of the control bits on a serial device :

                        . EFI_SERIAL_DATA_CLEAR_TO_SEND,
                          EFI_SERIAL_DATA_SET_READY,
                          EFI_SERIAL_RING_INDICATE,
                          EFI_SERIAL_CARRIER_DETECT,
                          EFI_SERIAL_REQUEST_TO_SEND,
                          EFI_SERIAL_DATA_TERMINAL_READY
                          are all related to the DTE (Data Terminal Equipment)
                          and DCE (Data Communication Equipment) modes of
                          operation of the serial device.
                        . EFI_SERIAL_INPUT_BUFFER_EMPTY : equal to one if the
                          receive buffer is empty, 0 otherwise.
                        . EFI_SERIAL_OUTPUT_BUFFER_EMPTY : equal to one if the
                          transmit buffer is empty, 0 otherwise.
                        . EFI_SERIAL_HARDWARE_LOOPBACK_ENABLE : equal to one if
                          the hardware loopback is enabled (the output feeds
                          the receive buffer), 0 otherwise.
                        . EFI_SERIAL_SOFTWARE_LOOPBACK_ENABLE : equal to one
                          if a loopback is accomplished by software, else 0.
                        . EFI_SERIAL_HARDWARE_FLOW_CONTROL_ENABLE : equal to
                          one if the hardware flow control based on CTS (Clear
                          To Send) and RTS (Ready To Send) control signals is
                          enabled, 0 otherwise.

  @retval RETURN_SUCCESS  The control bits were read from the device.

**/
RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  return RETURN_UNSUPPORTED;
}
