/* -*- Mode: C -*- */
/*
 * Intel microcode update support
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

static unsigned hyperthread_per_cpu = 0;

extern unsigned _ap;
extern unsigned _ap_data;
extern unsigned _ap_code;

enum {
  MSR_PLATFORM_ID    = 0x17,
  MSR_UPDATE_MC      = 0x79,
  MSR_SIGNATURE_ID   = 0x8b,
};

struct cpuid_eax {
  unsigned stepping:4;
  unsigned model:4;
  unsigned family:4;
  unsigned type:2;
  unsigned res0:2;
  unsigned model_ext:4;
  unsigned family_ext:8;
  unsigned res1:4;
} __attribute__((packed));

struct microcode
{
  uint32_t version;
  uint32_t revision;
  uint32_t date;
  struct cpuid_eax cpuid;
  uint32_t checksum;
  uint32_t loader_revision;
  uint32_t pflags;
  uint32_t data_size;
  uint32_t total_size;
  uint32_t reserved[3];
  uint8_t  data[];
} __attribute__((packed));

static
uint64_t signature_info(unsigned *eax)
{
  msr_write(MSR_SIGNATURE_ID, 0);
  unsigned ebx = 0, edx = 0, ecx = 0;
  *eax = 1;
  cpuid(eax, &ebx, &ecx, &edx);
  return msr_read(MSR_SIGNATURE_ID);
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
void apply_microcode(struct microcode const * const microcode)
{
  unsigned const ap_data_offset   = (unsigned)&_ap_data - (unsigned)&_ap;;
  unsigned * const ap_mc_memory   = (unsigned *)(_ap_code + ap_data_offset + 4);

  /* apply microcode if available and Intel CPU */
  if (!microcode || !intel_cpu(&hyperthread_per_cpu))
    return;

  /* weak sanity check to cover the case we write into the startup code */
  if (*ap_mc_memory & 0x7fffffffU) {
    printf("AP bootstrap page in unexpected state. Bye\n");
    return;
  }

  *ap_mc_memory   = (unsigned)microcode->data;

  uint64_t platform_id  = (msr_read(MSR_PLATFORM_ID) >> 50) & 0x7;
  bool platform_match   = (1U << platform_id) & microcode->pflags;

  printf("micro.code module detected\n - version=%u revision=%x date=%x\n"
         " - data location BSP %x\n",
         microcode->version, microcode->revision, microcode->date,
         microcode->data);

  unsigned eax             = 0;
  uint64_t sign_id         = signature_info(&eax);
  struct cpuid_eax * cpuid = (struct cpuid_eax *)&eax;

  bool const match = (microcode->cpuid.family_ext == cpuid->family_ext) &&
                     (microcode->cpuid.family     == cpuid->family)     &&
                     (microcode->cpuid.model_ext  == cpuid->model_ext)  &&
                     (microcode->cpuid.model      == cpuid->model)      &&
                     (microcode->cpuid.stepping   == cpuid->stepping)   &&
                     (microcode->revision         >  (sign_id >> 32)    &&
                     platform_match);

  printf(" - targets %2x:%2x:%2x [%2x] -> %2x:%2x:%2x [%2llx] - %s%s\n",
         (microcode->cpuid.family_ext << 4) | microcode->cpuid.family,
         (microcode->cpuid.model_ext << 4) | microcode->cpuid.model,
         microcode->cpuid.stepping, microcode->revision,
         (cpuid->family_ext << 4) | cpuid->family,
         (cpuid->model_ext << 4) | cpuid->model, cpuid->stepping,
         sign_id >> 32,
         platform_match ? "" : "platform id mismatch,",
         match ? "match" : " cpuid mismatch - no patching");

  /* XXX we may need to check per core, since IDs may differ, e.g. servers */
  if (!match)
    return;

  /* apply microcode update to this CPU -  bootstrap processor (BSP) */
  msr_write(MSR_UPDATE_MC, (uintptr_t)microcode->data);

  /* re-read after update */
  sign_id = signature_info(&eax);

  printf(" - patched BSP           -> %2x:%2x:%2x [%2lx]\n",
         (cpuid->family_ext << 4) | cpuid->family,
         (cpuid->model_ext << 4) | cpuid->model, cpuid->stepping,
         (sign_id >> 32));
}

int microcode_main(uint32_t const magic, void *multiboot)
{
  char   const mc_rom []       = "micro.code";
  size_t const mc_rom_size     = sizeof(mc_rom) - 1;
  struct microcode * microcode = 0;

  if (magic == MBI_MAGIC) {
    struct mbi * const mbi = (struct mbi *)multiboot;
    struct module *m  = (struct module *) mbi->mods_addr;

    for (unsigned i=0; i < mbi->mods_count; i++) {
      struct module * module = (struct module *)(m + i);
      if (!module->string)
        continue;

      if (memcmp((void *)(module->string), mc_rom, mc_rom_size)) {
        size_t len = strlen((char *)module->string);
        if (len <= 10)
          continue;
        if (memcmp((void *)(module->string + len - mc_rom_size), mc_rom, mc_rom_size))
          continue;
      }

      microcode = (struct microcode *)(module->mod_start);

      /* skip module for kernel or next bootloader */
      mbi->mods_count--;
      if (mbi->mods_count) {
        if (i == 0) {
          mbi->mods_addr += sizeof(*module);
          mbi->cmdline    = module->string;
        } else
          memcpy(module, module + 1, (mbi->mods_count - i) * sizeof(*module));
      }
      break;
    }
  } else if (magic == MBI2_MAGIC) {
    for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
      struct mbi2_module * module = (struct mbi2_module *)(i + 1);

      if (microcode || i->type != MBI2_TAG_MODULE)
        continue;

      if (memcmp(module->string, mc_rom, mc_rom_size))
        continue;

      enum { MBI2_TAG_INVALID = 0xbad }; /* XXX */
      /* avoid booting this module */
      i->type = MBI2_TAG_INVALID;

      microcode = (struct microcode *)module->mod_start;
    }
  } else {
    printf("Unknown multiboot magic value. Bye.\n");
    return 1;
  }

  apply_microcode(microcode);

  return 0;
}
