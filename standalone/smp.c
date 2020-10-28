/* -*- Mode: C -*- */
/*
 * Support to wake up all CPUs - solely for Intel at moment
 *
 * Copyright (C) 2018-2020, Alexander Boettcher <alexander.boettcher@genode-labs.com>
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

#include <mbi.h>
#include <mbi2.h>
#include <util.h>
#include <cpuid.h>
#include <acpi.h>

static unsigned cpus_detected       = 0;
static unsigned hyperthread_per_cpu = 0;
static unsigned cpus_wait_for       = 0;

extern unsigned _ap;
extern unsigned _ap_data;
extern unsigned _ap_code;

enum {
  MSR_PLATFORM_ID    = 0x17,
  MSR_UPDATE_MC      = 0x79,
  MSR_SIGNATURE_ID   = 0x8b,
};

static
bool hyperthreading()
{
  unsigned eax = 1, ebx = 0, edx = 0, ecx = 0;
  cpuid(&eax, &ebx, &ecx, &edx);
  return edx & (1U << 28);
}

static
bool intel_cpu(unsigned *logical_cpu_count)
{
  unsigned eax = 0x0, ebx = 0, edx = 0, ecx = 0;
  cpuid(&eax, &ebx, &ecx, &edx);

  unsigned const max_cpuid_eax = eax;

  const char * intel = "GenuineIntel";

  if (memcmp(intel, &ebx, 4) || memcmp(intel + 4, &edx, 4) ||
      memcmp(intel + 8, &ecx, 4))
    return false;

  if (max_cpuid_eax >= 0xb) {
    eax = 0xb, ebx = 0, edx = 0, ecx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);

    *logical_cpu_count = ebx;
    return true;
  }

  if (max_cpuid_eax >= 0x4) {
    eax = 4, ebx = 0, edx = 0, ecx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);

    unsigned cores_per_package = ((eax >> 26) & 0x3f) + 1;

    eax = 1, ebx = 0, edx = 0, ecx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);

    unsigned threads_per_package = ebx >> 16 & 0xff;

    if (threads_per_package == cores_per_package)
      *logical_cpu_count = 1;
  }
  return true;
}

static
void send_ipi(uintptr_t lapic_base, uint32_t const target_cpu,
              uint32_t vector, unsigned type, unsigned shorthand)
{
    uint32_t * const lapic_icr_high = (uint32_t * const)(lapic_base + 0x310);
    uint32_t * const lapic_icr_low  = (uint32_t * const)(lapic_base + 0x300);

    /* wait until ready */
    while (*lapic_icr_low & (1 << 12)) /* delivery status bit check */
        asm volatile ("pause":::"memory");

    uint32_t icr_low = (vector & 0xff)     |     /* bit 0-7 */
                       ((type & 0x7) << 8) |     /* bit 8-11  - ipi type */
                       (1U << 14)          |     /* bit 14    - level assert */
                       ((shorthand & 3U) << 18); /* bit 18-19 */

    /* first write high, then low register */
    *lapic_icr_high = target_cpu << 24; /* apic cpu id */
    asm volatile ("":::"memory");
    *lapic_icr_low = icr_low;
    asm volatile ("":::"memory");
}

static
void apic_entry(struct apic_madt const * apic)
{
  enum { CPU_ENABLED = 1 };
  if (apic->type != APIC_MADT_LAPIC_TYPE || !(apic->flags & CPU_ENABLED))
    return;

  cpus_detected ++;

  uint32_t const my_apic_id = *(uint32_t *)(APIC_DEFAULT_PHYS_BASE + 0x020);
  if (my_apic_id == apic->id2)
    return;

  if (hyperthreading()) {
    if (!hyperthread_per_cpu)
      /* detection went wrong */
      return;

    bool const hyperthread = apic->id2 & (hyperthread_per_cpu - 1);
    if (hyperthread)
      /* we solely patch the first (0th) hyper thread */
      return;
  }

  /* count cores we started and have to wait for */
  cpus_wait_for ++;

  unsigned const apic_cpu_id = apic->id2;
  enum Shorthand { IPI_DIRECT = 0 };
  enum Mode { IPI_INIT = 5, IPI_SIPI = 6 };

  send_ipi(APIC_DEFAULT_PHYS_BASE, apic_cpu_id, 0 /* unused */, IPI_INIT, IPI_DIRECT);
  /* wait 10 ms - debates on whether required or not on modern machines */
  send_ipi(APIC_DEFAULT_PHYS_BASE, apic_cpu_id, _ap_code >> 12, IPI_SIPI, IPI_DIRECT);
  /* wait 200us - debates ... */
  send_ipi(APIC_DEFAULT_PHYS_BASE, apic_cpu_id, _ap_code >> 12, IPI_SIPI, IPI_DIRECT);
}

static
void xsdt_rsdt_entry(uint64_t entry)
{
  if (entry >= (1ULL << 32))
    /* we have only low 4G accessible ... */
    return;

  struct acpi_table * table = (struct acpi_table *)(uintptr_t)entry;
  for_each_apic_struct(table, apic_entry);
}

static
void wake_on_thread_per_core(struct rsdp const * const rsdp)
{
  unsigned const ap_data_offset   = (unsigned)&_ap_data - (unsigned)&_ap;;
  unsigned * const ap_cpus_booted = (unsigned *)(_ap_code + ap_data_offset);

  /* weak sanity check to cover the case we write into the startup code */
  if (*ap_cpus_booted & 0x7fffffffU) {
    printf("AP bootstrap page in unexpected state. Bye\n");
    return;
  }

  *ap_cpus_booted = 0;

  if (!intel_cpu(&hyperthread_per_cpu))
    return;

  /* detect number of unique cores to patch (without second++ hyperthread) */
  if (rsdp) {
    if (rsdp->xsdt && rsdp->xsdt < (1ULL << 32)) {
      struct acpi_table * xsdt = (struct acpi_table *)(uintptr_t)rsdp->xsdt;
      for_each_xsdt_entry(xsdt, xsdt_rsdt_entry);
    }

    /* if XSDT way failed - try RSDT if available */
    if (!cpus_detected && rsdp->rsdt) {
      struct acpi_table * rsdt = (struct acpi_table *)rsdp->rsdt;
      for_each_rsdt_entry(rsdt, xsdt_rsdt_entry);
    }
  }

  if (!cpus_wait_for)
    return;

  /* wait for patched AP CPUs */
  while (*ap_cpus_booted < cpus_wait_for)
    asm volatile("pause":::"memory");
}

int smp_main(uint32_t const magic, void *multiboot)
{
	struct rsdp * rsdp = 0;

	if (magic == MBI_MAGIC) {
		/* grep for the ACPI RSDP pointer */
		rsdp = acpi_get_rsdp();
	} else
	if (magic == MBI2_MAGIC) {
		for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
			struct mbi2_module * module = (struct mbi2_module *)(i + 1);

			if (i->type == MBI2_TAG_RSDP_V1 || i->type == MBI2_TAG_RSDP_V2)
				rsdp = (struct rsdp *)module;
		}
	}

	if (!rsdp)
		return 1;    

	wake_on_thread_per_core(rsdp);

	return 0;
}
