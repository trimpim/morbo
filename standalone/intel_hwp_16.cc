/*
 * Intel hardware p-state (HWP) tweaking called on AP in 16bit mode
 *
 * Copyright (C) 2020, Martin Stein <martin.stein@genode-labs.com>
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <asm.h>

#if 0
__attribute__((always_inline)) inline
void serial_send(char const x)
{
  enum
  {
       LSR = 5,
       LSR_TMIT_HOLD_EMPTY = 1u << 5
  };

  unsigned uart_addr = 1 ? 0x1808 /* X201 */ : 0x3f8 /* Qemu && Macho */;
  while (!(inb (uart_addr + LSR) & LSR_TMIT_HOLD_EMPTY)) {
    asm volatile ("pause");
  }

  outb(uart_addr, x);
}
#else
__attribute__((always_inline)) inline
void serial_send(char const x)
{ }
#endif

#include <plugin_intel_hwp.h>

extern "C" void intel_hwp_16()
{
  configure_hardware_pstates();
}
