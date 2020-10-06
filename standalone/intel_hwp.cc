/*
 * Intel hardware p-state (HWP) tweaking called on BSP
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

extern "C" void serial_send(int c);

#include <plugin_intel_hwp.h>

extern "C"
int intel_hwp_main(uint32_t const magic, void *multiboot)
{
  configure_hardware_pstates();
  return 0;
}
