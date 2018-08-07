/* -*- Mode: C -*- */
/*
 * ACPI definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2009-2012, Bernhard Kauer <kauer@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2018, Alexander Boettcher <alexander.boettcher@genode-labs.com>
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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct rsdp {
  char signature[8];
  uint8_t checksum;
  char oem[6];
  uint8_t rev;
  uint32_t rsdt;
  uint32_t size;
  uint64_t xsdt;
  uint8_t ext_checksum;
  char _res[3];
} __attribute__((packed));

struct acpi_table {
  char signature[4];
  uint32_t size;
  uint8_t  rev;
  uint8_t  checksum;
  char oemid[6];
  char oemtabid[8];
  uint32_t oemrev;
  char creator[4];
  uint32_t crev;
} __attribute__((packed));

struct device_scope {
  uint8_t type;
  uint8_t size;
  uint16_t _res;
  uint8_t enum_id;
  uint8_t start_bus;
  /* XXX Hardcode PCI device scope: path = (device, function) */
  uint8_t path[2];
  //uint8_t path[];
} __attribute__((packed));

enum {
  TYPE_DMAR          = 0,
  TYPE_RMRR          = 1,
  SCOPE_PCI_ENDPOINT = 1,
};

struct dmar_entry {
  uint16_t type;
  uint16_t size;

  union {
    struct {
      uint32_t _res;
      uint64_t phys;
    } dmar;
    /* If we include more than RMRRs here, we need to fix the DMAR
       duplication code in zapp.c */
    struct rmrr {
      uint16_t _res;
      uint16_t segment;
      uint64_t base;
      uint64_t limit;
      struct device_scope first_scope;
    } rmrr;
  };
} __attribute__((packed));

struct dmar {
  struct acpi_table generic;
  uint8_t host_addr_width;
  uint8_t flags;
  char _res[10];
  struct dmar_entry first_entry;
};


enum { APIC_MADT_LAPIC_TYPE = 0 };
struct apic_madt
{
	uint8_t type;
	uint8_t length;
	uint8_t id1;
	uint8_t id2;
	uint32_t flags;
} __attribute__((packed));

char acpi_checksum(const char *table, size_t count);
void acpi_fix_checksum(struct acpi_table *tab);

struct rsdp *acpi_get_rsdp(void);
struct acpi_table **acpi_get_table_ptr(struct acpi_table *rsdt, const char signature[4]);

static inline struct dmar_entry *acpi_dmar_next(struct dmar_entry *cur)
{ return (struct dmar_entry *)((char *)cur + cur->size); }

static inline bool acpi_in_table(struct acpi_table *tab, const void *p)
{ return ((uintptr_t)tab + tab->size) > (uintptr_t)p; }

typedef void *(*memory_alloc_t)(size_t len, unsigned align);

struct acpi_table *acpi_dup_table(struct acpi_table *rsdt, const char signature[4],
				  memory_alloc_t alloc);

static inline
void for_each_rsdt_entry(struct acpi_table *rsdt, void (*fn)(uint64_t))
{
	if (rsdt->signature[0] != 'R' || rsdt->signature[1] != 'S' ||
	    rsdt->signature[2] != 'D' || rsdt->signature[3] != 'T')
		return;

	typedef uint32_t entry_t;

	unsigned const table_size  = rsdt->size;
	unsigned const entry_count = (table_size - sizeof(*rsdt)) / sizeof(entry_t);

	entry_t * entries = (entry_t *)(rsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}

static inline
void for_each_xsdt_entry(struct acpi_table *xsdt, void (*fn)(uint64_t))
{
	if (xsdt->signature[0] != 'X' || xsdt->signature[1] != 'S' ||
	    xsdt->signature[2] != 'D' || xsdt->signature[3] != 'T')
		return;

	typedef uint64_t entry_t;

	unsigned const table_size  = xsdt->size;
	unsigned const entry_count = (table_size - sizeof(*xsdt)) / sizeof(entry_t);

	entry_t * entries = (entry_t *)(xsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}

static inline
struct apic_madt const *acpi_madt_next(struct apic_madt const * const c)
{
	return (struct apic_madt *)((uint8_t *)c + c->length);
}

static inline
void for_each_apic_struct(struct acpi_table *apic_madt, void (*fn)(struct apic_madt const *))
{
	if (apic_madt->signature[0] != 'A' || apic_madt->signature[1] != 'P' ||
	    apic_madt->signature[2] != 'I' || apic_madt->signature[3] != 'C')
		return;

	struct apic_madt const * const first = (struct apic_madt *)(&apic_madt->crev + 3);
	struct apic_madt const * const last  = (struct apic_madt *)((uint8_t *)(apic_madt->signature) + apic_madt->size);

	for (struct apic_madt const * e = first; e < last ; e = acpi_madt_next(e))
		fn(e);
}
/* EOF */
