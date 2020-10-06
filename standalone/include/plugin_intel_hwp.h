/* -*- Mode: C -*- */
/*
 * Intel hardware p-state (HWP) tweaking
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

#pragma once

#include <asm.h>

typedef uint64_t uint64;

#define ALWAYS_INLINE __attribute__((always_inline)) inline

struct Cpu
{
    ALWAYS_INLINE
    static void cpuid(unsigned const idx, unsigned &a, unsigned &b, unsigned &c, unsigned &d)
    {
        a = idx;
        b = c = d = 0;
        ::cpuid(&a, &b, &c, &d);
    }
};

struct Cpu_msr
{
    enum Register
    {
        IA32_POWER_CTL          = 0x1fc,
        IA32_ENERGY_PERF_BIAS   = 0x1b0,
        MSR_PM_ENABLE           = 0x770,
        MSR_HWP_INTERRUPT       = 0x773,
        MSR_HWP_REQUEST         = 0x774,
    };

    template <typename T>
    ALWAYS_INLINE
    static T read (Register msr)
    {
        return msr_read(msr);
    }

    template <typename T>
    ALWAYS_INLINE
    static void write (Register msr, T val)
    {
        msr_write(msr, val);
    }

    ALWAYS_INLINE
    static void hwp_notification_irqs(bool on)
    {
        write<uint64>(MSR_HWP_INTERRUPT, on ? 1 : 0);
    }

    ALWAYS_INLINE
    static void hardware_pstates(bool on)
    {
        write<uint64>(MSR_PM_ENABLE, on ? 1 : 0);
    }

    ALWAYS_INLINE
    static void energy_efficiency_optimization(bool on)
    {
        enum { DEEO_SHIFT = 20 };
        enum { DEEO_MASK = 0x1 };
        uint64 val = read<uint64>(IA32_POWER_CTL);
        val &= ~(static_cast<uint64>(DEEO_MASK) << DEEO_SHIFT);
        val |= (static_cast<uint64>(on ? 1 : 0) & DEEO_MASK) << DEEO_SHIFT;
        write<uint64>(IA32_POWER_CTL, val);
    }

    enum class Hwp_epp
    {
        PERFORMANCE  = 0,
        BALANCED     = 127,
        POWER_SAVING = 255,
    };

    ALWAYS_INLINE
    static void hwp_energy_perf_pref(Hwp_epp epp)
    {
        enum { EPP_SHIFT = 24 };
        enum { EPP_MASK = 0xff };
        uint64 val = read<uint64>(MSR_HWP_REQUEST);
        val &= ~(static_cast<uint64>(EPP_MASK) << EPP_SHIFT);
        val |= (static_cast<uint64>(epp) & EPP_MASK) << EPP_SHIFT;
        write<uint64>(MSR_HWP_REQUEST, val);
    }

    enum class Hwp_epb
    {
        PERFORMANCE  = 0,
        BALANCED     = 7,
        POWER_SAVING = 15,
    };

    ALWAYS_INLINE
    static void hwp_energy_perf_bias(Hwp_epb epb)
    {
        enum { EPB_SHIFT = 0 };
        enum { EPB_MASK = 0xf };
        uint64 val = read<uint64>(IA32_ENERGY_PERF_BIAS);
        val &= ~(static_cast<uint64>(EPB_MASK) << EPB_SHIFT);
        val |= (static_cast<uint64>(epb) & EPB_MASK) << EPB_SHIFT;
        write<uint64>(IA32_ENERGY_PERF_BIAS, val);
    }
};

struct Cpuid
{
    enum { MAX_LEAF_IDX = 8 };

    unsigned eax[MAX_LEAF_IDX];
    unsigned ebx[MAX_LEAF_IDX];
    unsigned ecx[MAX_LEAF_IDX];
    unsigned edx[MAX_LEAF_IDX];

    ALWAYS_INLINE
    void init_leaf(unsigned idx) {
        Cpu::cpuid (idx, eax[idx], ebx[idx], ecx[idx], edx[idx]);
    }

    ALWAYS_INLINE
    Cpuid() {
        Cpu::cpuid (0, eax[0], ebx[0], ecx[0], edx[0]);
        for (unsigned idx = 1; idx <= eax[0] && idx < MAX_LEAF_IDX; idx++) {
            init_leaf(idx);
        }
    }

    enum class Vendor {
        INTEL,
        UNKNOWN,
    };

    enum { VENDOR_STRING_LENGTH = 12 };

    ALWAYS_INLINE
    Vendor vendor() const
    {
        char intel[VENDOR_STRING_LENGTH] { 'G', 'e', 'n', 'u', 'i', 'n', 'e', 'I', 'n', 't', 'e', 'l' };
        unsigned idx { 0 };

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(ebx[0] >> shift);
            if (intel[idx] != str)
                return Vendor::UNKNOWN;
        }

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(edx[0] >> shift);
            if (intel[idx] != str)
                return Vendor::UNKNOWN;
        }

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(ecx[0] >> shift);
            if (intel[idx] != str)
               return Vendor::UNKNOWN;
        }

        return Vendor::INTEL;
    }

    using Family_id = unsigned;
    enum { FAMILY_ID_UNKNOWN = ~static_cast<unsigned>(0) };

    ALWAYS_INLINE
    Family_id family_id() const
    {
        if (eax[0] < 1) {
            return FAMILY_ID_UNKNOWN;
        }
        enum { FAMILY_ID_SHIFT = 8 };
        enum { FAMILY_ID_MASK = 0xf };
        enum { EXT_FAMILY_ID_SHIFT = 20 };
        enum { EXT_FAMILY_ID_MASK = 0xff };
        Family_id family_id {
            (eax[1] >> FAMILY_ID_SHIFT) & FAMILY_ID_MASK };

        if (family_id == 15) {
            family_id += (eax[1] >> EXT_FAMILY_ID_SHIFT) & EXT_FAMILY_ID_MASK;
        }
        return family_id;
    }

    enum class Model {
        KABY_LAKE_DESKTOP,
        UNKNOWN,
    };

    ALWAYS_INLINE
    Model model() const
    {
        if (eax[0] < 1) {
            return Model::UNKNOWN;
        }
        enum { MODEL_ID_SHIFT = 4 };
        enum { MODEL_ID_MASK = 0xf };
        enum { EXT_MODEL_ID_SHIFT = 16 };
        enum { EXT_MODEL_ID_MASK = 0xf };
        unsigned const fam_id { family_id() };
        unsigned model_id { (eax[1] >> MODEL_ID_SHIFT) & MODEL_ID_MASK };
        if (fam_id == 6 ||
            fam_id == 15)
        {
            model_id +=
                ((eax[1] >> EXT_MODEL_ID_SHIFT) & EXT_MODEL_ID_MASK) << 4;
        }
        switch (model_id) {
        case 0x9e: return Model::KABY_LAKE_DESKTOP;
        default:   return Model::UNKNOWN;
        }
    }

    ALWAYS_INLINE
    bool hwp() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 7) & 1) == 1;
    }

    ALWAYS_INLINE
    bool hwp_notification() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 8) & 1) == 1;
    }

    ALWAYS_INLINE
    bool hwp_energy_perf_pref() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 10) & 1) == 1;
    }

    ALWAYS_INLINE
    bool hardware_coordination_feedback_cap() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((ecx[6] >> 0) & 1) == 1;
    }

    ALWAYS_INLINE
    bool hwp_energy_perf_bias() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((ecx[6] >> 3) & 1) == 1;
    }
};

ALWAYS_INLINE
static void configure_hardware_pstates()
{
    Cpuid const cpuid { };

    serial_send('h');
    serial_send('w');
    serial_send('p');
    serial_send(' ');
    serial_send('c');
    serial_send('o');
    serial_send('n');
    serial_send('f');
    serial_send('i');
    serial_send('g');
    serial_send(':');

    serial_send(' ');
    serial_send('e');
    serial_send('e');
    serial_send('o');
    serial_send('=');

    if (cpuid.vendor() == Cpuid::Vendor::INTEL &&
        cpuid.family_id() == 6 &&
        cpuid.model() == Cpuid::Model::KABY_LAKE_DESKTOP &&
        cpuid.hardware_coordination_feedback_cap())
    {
        Cpu_msr::energy_efficiency_optimization(false);
        serial_send('0');

    } else {

        serial_send('n');
        serial_send('a');
    }

    serial_send(' ');
    serial_send('i');
    serial_send('r');
    serial_send('q');
    serial_send('=');

    if (cpuid.hwp() && cpuid.hwp_notification()) {

        Cpu_msr::hwp_notification_irqs(false);
        serial_send('0');

    } else {

        serial_send('n');
        serial_send('a');
    }

    serial_send(' ');
    serial_send('h');
    serial_send('w');
    serial_send('p');
    serial_send('=');

    if (cpuid.hwp()) {

        Cpu_msr::hardware_pstates(true);
        serial_send('1');

    } else {

        serial_send('n');
        serial_send('a');
    }

    serial_send(' ');
    serial_send('e');
    serial_send('p');
    serial_send('p');
    serial_send('=');

    if (cpuid.hwp() && cpuid.hwp_energy_perf_pref()) {

        Cpu_msr::hwp_energy_perf_pref(Cpu_msr::Hwp_epp::PERFORMANCE);
        serial_send('0');

    } else {

        serial_send('n');
        serial_send('a');
    }

    serial_send(' ');
    serial_send('e');
    serial_send('p');
    serial_send('b');
    serial_send('=');

    if (cpuid.hwp() && cpuid.hwp_energy_perf_bias()) {

        Cpu_msr::hwp_energy_perf_bias(Cpu_msr::Hwp_epb::PERFORMANCE);
        serial_send('0');

    } else {

        serial_send('n');
        serial_send('a');
    }

    serial_send('\n');
}
