/*
 * Optional plugins
 *
 * Copyright (C) 2020, Alexander Boettcher <alexander.boettcher@genode-labs.com>
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

#pragma once

int smp_install_code();
int smp_main(unsigned const magic, void *multiboot);
int microcode_main(unsigned const magic, void *multiboot);
int intel_hwp_main(unsigned const magic, void *multiboot);

extern unsigned _ap;
extern unsigned _ap_code;
extern unsigned _ap_plugin;

enum PLUGIN { PLUGIN_MICROCODE = 1, PLUGIN_INTEL_HWP = 2 };

void flag_plugin_for_aps(enum PLUGIN const flag)
{
	unsigned const offset = (unsigned)&_ap_plugin - (unsigned)&_ap;
	unsigned * const plugins = (unsigned *)(_ap_code + offset);

	*plugins = *plugins | flag;
}
