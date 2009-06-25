/*
 * \brief   ASM inline helper routines.
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#pragma once

#include <stdint.h>

void jmp_multiboot(const void * mbi, uint32_t entry) __attribute__((noreturn));

static inline uint32_t
ntohl(uint32_t v)
{
  asm volatile("bswap %1": "+r"(v));
  return v;
}


static inline uint64_t
rdtsc(void)
{
  uint64_t  res;
  // =A does not work in 64 bit mode
  asm volatile ("rdtsc" : "=A"(res));
  return res;
}

static inline uint8_t
inb(const uint16_t port)
{
  uint8_t res;
  asm volatile("inb %1, %0" : "=a"(res): "Nd"(port));
  return res;
}

static inline uint16_t
inw(uint16_t port)
{
  uint16_t res;
  asm volatile("inw  %1, %0" : "=a"(res) : "Nd"(port));
  return res;
}

static inline uint32_t
inl(const uint16_t port)
{
  uint32_t res;
  asm volatile("in %1, %0" : "=a"(res): "Nd"(port));
  return res;
}

static inline void
outb(const uint16_t port, uint8_t value)
{
  asm volatile("outb %0,%1" :: "a"(value),"Nd"(port));
}

/* TODO Wrong order (w/regard to the Linux source) */
static inline void
outw(const uint16_t port, uint16_t value)
{
  asm volatile("outw %0,%1" :: "a"(value),"Nd"(port));
}

static inline void
outl(const uint16_t port, uint32_t value)
{
  asm volatile("outl %0,%1" :: "a"(value),"Nd"(port));
}


static inline uint32_t
bsr(uint32_t value)
{
  uint32_t res;
  asm volatile("bsr %1,%0" : "=r"(res): "r"(value));
  return res;
}


static inline uint32_t
bsf(uint32_t value)
{
  uint32_t res;
  asm volatile("bsf %1,%0" : "=r"(res): "r"(value));
  return res;
}

static inline void
cli_halt(void)
{
  asm volatile("cli ; hlt");
}

/* EOF */