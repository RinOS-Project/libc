/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - linux/auxvec.h
 * ELF Auxiliary Vector definitions
 *
 * The auxiliary vector is passed to the program by the kernel
 * and contains useful information about the system and process.
 */

#ifndef _LINUX_AUXVEC_H
#define _LINUX_AUXVEC_H

#include "../errno.h"
#include "../rin_account_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Auxiliary Vector Entry Types
 * These define the a_type field in Elf32_auxv_t / Elf64_auxv_t
 * ═══════════════════════════════════════════════════════════════*/

#define AT_NULL         0       /* End of vector */
#define AT_IGNORE       1       /* Entry should be ignored */
#define AT_EXECFD       2       /* File descriptor of program */
#define AT_PHDR         3       /* Program headers for program */
#define AT_PHENT        4       /* Size of program header entry */
#define AT_PHNUM        5       /* Number of program headers */
#define AT_PAGESZ       6       /* System page size */
#define AT_BASE         7       /* Base address of interpreter */
#define AT_FLAGS        8       /* Flags */
#define AT_ENTRY        9       /* Entry point of program */
#define AT_NOTELF       10      /* Program is not ELF */
#define AT_UID          11      /* Real uid */
#define AT_EUID         12      /* Effective uid */
#define AT_GID          13      /* Real gid */
#define AT_EGID         14      /* Effective gid */
#define AT_PLATFORM     15      /* String identifying CPU for optimizations */
#define AT_HWCAP        16      /* Machine-dependent hints about processor capabilities */
#define AT_CLKTCK       17      /* Frequency at which times() increments */
/* 18-19 reserved */
#define AT_FPUCW        18      /* Used FPU control word (i386 specific) */
#define AT_DCACHEBSIZE  19      /* Data cache block size */
#define AT_ICACHEBSIZE  20      /* Instruction cache block size */
#define AT_UCACHEBSIZE  21      /* Unified cache block size */
#define AT_IGNOREPPC    22      /* Entry should be ignored (PowerPC specific) */
#define AT_SECURE       23      /* Boolean, was exec setuid-like? */
#define AT_BASE_PLATFORM 24     /* String identifying real platform */
#define AT_RANDOM       25      /* Address of 16 random bytes */
#define AT_HWCAP2       26      /* Extension of AT_HWCAP */
/* 27-30 reserved */
#define AT_EXECFN       31      /* File name of executed program */
#define AT_SYSINFO      32      /* Entry point of system call in vDSO */
#define AT_SYSINFO_EHDR 33      /* ELF header of vDSO */
#define AT_L1I_CACHESHAPE   34  /* L1 instruction cache shape */
#define AT_L1D_CACHESHAPE   35  /* L1 data cache shape */
#define AT_L2_CACHESHAPE    36  /* L2 cache shape */
#define AT_L3_CACHESHAPE    37  /* L3 cache shape */
#define AT_L1I_CACHESIZE    40  /* L1 instruction cache size */
#define AT_L1I_CACHEGEOMETRY 41 /* L1 instruction cache geometry */
#define AT_L1D_CACHESIZE    42  /* L1 data cache size */
#define AT_L1D_CACHEGEOMETRY 43 /* L1 data cache geometry */
#define AT_L2_CACHESIZE     44  /* L2 cache size */
#define AT_L2_CACHEGEOMETRY 45  /* L2 cache geometry */
#define AT_L3_CACHESIZE     46  /* L3 cache size */
#define AT_L3_CACHEGEOMETRY 47  /* L3 cache geometry */
#define AT_MINSIGSTKSZ      51  /* Minimum stack size for signal handler */

/* ═══════════════════════════════════════════════════════════════
 * x86/x86_64 Hardware Capabilities (AT_HWCAP)
 * These correspond to CPUID feature flags
 * ═══════════════════════════════════════════════════════════════*/

/* EDX features from CPUID(1) */
#define HWCAP_X86_FPU           (1 << 0)    /* x87 FPU on chip */
#define HWCAP_X86_VME           (1 << 1)    /* Virtual 8086 mode */
#define HWCAP_X86_DE            (1 << 2)    /* Debugging extensions */
#define HWCAP_X86_PSE           (1 << 3)    /* Page size extensions */
#define HWCAP_X86_TSC           (1 << 4)    /* Time stamp counter */
#define HWCAP_X86_MSR           (1 << 5)    /* Model-specific registers */
#define HWCAP_X86_PAE           (1 << 6)    /* Physical address extension */
#define HWCAP_X86_MCE           (1 << 7)    /* Machine check exception */
#define HWCAP_X86_CX8           (1 << 8)    /* CMPXCHG8B instruction */
#define HWCAP_X86_APIC          (1 << 9)    /* On-chip APIC */
#define HWCAP_X86_SEP           (1 << 11)   /* SYSENTER/SYSEXIT */
#define HWCAP_X86_MTRR          (1 << 12)   /* Memory type range registers */
#define HWCAP_X86_PGE           (1 << 13)   /* Page global enable */
#define HWCAP_X86_MCA           (1 << 14)   /* Machine check architecture */
#define HWCAP_X86_CMOV          (1 << 15)   /* Conditional move instruction */
#define HWCAP_X86_PAT           (1 << 16)   /* Page attribute table */
#define HWCAP_X86_PSE36         (1 << 17)   /* 36-bit page size extension */
#define HWCAP_X86_PSN           (1 << 18)   /* Processor serial number */
#define HWCAP_X86_CLFLUSH       (1 << 19)   /* CLFLUSH instruction */
#define HWCAP_X86_DS            (1 << 21)   /* Debug store */
#define HWCAP_X86_ACPI          (1 << 22)   /* ACPI thermal control */
#define HWCAP_X86_MMX           (1 << 23)   /* MMX instruction set */
#define HWCAP_X86_FXSR          (1 << 24)   /* FXSAVE/FXRSTOR instructions */
#define HWCAP_X86_SSE           (1 << 25)   /* SSE instructions */
#define HWCAP_X86_SSE2          (1 << 26)   /* SSE2 instructions */
#define HWCAP_X86_SS            (1 << 27)   /* Self-snoop */
#define HWCAP_X86_HTT           (1 << 28)   /* Hyper-Threading Technology */
#define HWCAP_X86_TM            (1 << 29)   /* Thermal monitor */
#define HWCAP_X86_IA64          (1 << 30)   /* IA64 processor emulating x86 */
#define HWCAP_X86_PBE           (1 << 31)   /* Pending break enable */

/* ECX features from CPUID(1) - mapped to different positions */
#define HWCAP_X86_SSE3          0x00000001  /* SSE3 instructions */
#define HWCAP_X86_PCLMULDQ      0x00000002  /* PCLMULQDQ instruction */
#define HWCAP_X86_DTES64        0x00000004  /* 64-bit debug store */
#define HWCAP_X86_MONITOR       0x00000008  /* MONITOR/MWAIT instructions */
#define HWCAP_X86_DSCPL         0x00000010  /* CPL qualified debug store */
#define HWCAP_X86_VMX           0x00000020  /* Virtual machine extensions */
#define HWCAP_X86_SMX           0x00000040  /* Safer mode extensions */
#define HWCAP_X86_EST           0x00000080  /* Enhanced SpeedStep */
#define HWCAP_X86_TM2           0x00000100  /* Thermal Monitor 2 */
#define HWCAP_X86_SSSE3         0x00000200  /* SSSE3 instructions */
#define HWCAP_X86_CID           0x00000400  /* Context ID */
#define HWCAP_X86_SDBG          0x00000800  /* Silicon debug */
#define HWCAP_X86_FMA           0x00001000  /* FMA3 instructions */
#define HWCAP_X86_CX16          0x00002000  /* CMPXCHG16B instruction */
#define HWCAP_X86_XTPR          0x00004000  /* xTPR update control */
#define HWCAP_X86_PDCM          0x00008000  /* Perf/debug capability MSR */
#define HWCAP_X86_PCID          0x00020000  /* Process context identifiers */
#define HWCAP_X86_DCA           0x00040000  /* Direct cache access */
#define HWCAP_X86_SSE4_1        0x00080000  /* SSE4.1 instructions */
#define HWCAP_X86_SSE4_2        0x00100000  /* SSE4.2 instructions */
#define HWCAP_X86_X2APIC        0x00200000  /* x2APIC */
#define HWCAP_X86_MOVBE         0x00400000  /* MOVBE instruction */
#define HWCAP_X86_POPCNT        0x00800000  /* POPCNT instruction */
#define HWCAP_X86_TSC_DEADLINE  0x01000000  /* TSC deadline timer */
#define HWCAP_X86_AES           0x02000000  /* AES instructions */
#define HWCAP_X86_XSAVE         0x04000000  /* XSAVE/XRSTOR instructions */
#define HWCAP_X86_OSXSAVE       0x08000000  /* XSAVE enabled by OS */
#define HWCAP_X86_AVX           0x10000000  /* AVX instructions */
#define HWCAP_X86_F16C          0x20000000  /* F16C instructions */
#define HWCAP_X86_RDRAND        0x40000000  /* RDRAND instruction */
#define HWCAP_X86_HYPERVISOR    0x80000000  /* Running in a hypervisor */

/* ═══════════════════════════════════════════════════════════════
 * x86/x86_64 Hardware Capabilities 2 (AT_HWCAP2)
 * Extended features from CPUID(7)
 * ═══════════════════════════════════════════════════════════════*/

#define HWCAP2_X86_FSGSBASE     (1 << 0)    /* FSGSBASE instructions */
#define HWCAP2_X86_TSC_ADJUST   (1 << 1)    /* IA32_TSC_ADJUST MSR */
#define HWCAP2_X86_SGX          (1 << 2)    /* Software Guard Extensions */
#define HWCAP2_X86_BMI1         (1 << 3)    /* Bit manipulation group 1 */
#define HWCAP2_X86_HLE          (1 << 4)    /* Hardware Lock Elision */
#define HWCAP2_X86_AVX2         (1 << 5)    /* AVX2 instructions */
#define HWCAP2_X86_SMEP         (1 << 7)    /* Supervisor Mode Execution Prevention */
#define HWCAP2_X86_BMI2         (1 << 8)    /* Bit manipulation group 2 */
#define HWCAP2_X86_ERMS         (1 << 9)    /* Enhanced REP MOVSB/STOSB */
#define HWCAP2_X86_INVPCID      (1 << 10)   /* INVPCID instruction */
#define HWCAP2_X86_RTM          (1 << 11)   /* Restricted Transactional Memory */
#define HWCAP2_X86_PQM          (1 << 12)   /* Platform Quality of Service Monitoring */
#define HWCAP2_X86_MPX          (1 << 14)   /* Memory Protection Extensions */
#define HWCAP2_X86_PQE          (1 << 15)   /* Platform Quality of Service Enforcement */
#define HWCAP2_X86_AVX512F      (1 << 16)   /* AVX-512 Foundation */
#define HWCAP2_X86_AVX512DQ     (1 << 17)   /* AVX-512 Doubleword/Quadword */
#define HWCAP2_X86_RDSEED       (1 << 18)   /* RDSEED instruction */
#define HWCAP2_X86_ADX          (1 << 19)   /* Multi-precision add-carry */
#define HWCAP2_X86_SMAP         (1 << 20)   /* Supervisor Mode Access Prevention */
#define HWCAP2_X86_AVX512IFMA   (1 << 21)   /* AVX-512 Integer FMA */
#define HWCAP2_X86_CLFLUSHOPT   (1 << 23)   /* CLFLUSHOPT instruction */
#define HWCAP2_X86_CLWB         (1 << 24)   /* CLWB instruction */
#define HWCAP2_X86_INTEL_PT     (1 << 25)   /* Intel Processor Trace */
#define HWCAP2_X86_AVX512PF     (1 << 26)   /* AVX-512 Prefetch */
#define HWCAP2_X86_AVX512ER     (1 << 27)   /* AVX-512 Exponential/Reciprocal */
#define HWCAP2_X86_AVX512CD     (1 << 28)   /* AVX-512 Conflict Detection */
#define HWCAP2_X86_SHA          (1 << 29)   /* SHA extensions */
#define HWCAP2_X86_AVX512BW     (1 << 30)   /* AVX-512 Byte/Word */
#define HWCAP2_X86_AVX512VL     (1 << 31)   /* AVX-512 Vector Length Extensions */

/* ═══════════════════════════════════════════════════════════════
 * Auxiliary Vector Structure (for reference)
 * ═══════════════════════════════════════════════════════════════*/

/* Note: The actual structures are defined in elf.h */
#ifndef __ASSEMBLY__
typedef struct {
    unsigned long a_type;
    union {
        unsigned long a_val;
    } a_un;
} Elf32_auxv_t;

#ifdef __x86_64__
typedef struct {
    unsigned long a_type;
    union {
        unsigned long a_val;
    } a_un;
} Elf64_auxv_t;
#endif
#endif /* !__ASSEMBLY__ */

/* ═══════════════════════════════════════════════════════════════
 * RinOS Auxiliary Vector Access
 * ═══════════════════════════════════════════════════════════════*/

/* RinOS provides a minimal auxv for program initialization */
#ifndef __ASSEMBLY__

/* Get a value that RinOS can currently report without fabricating process
 * metadata. A zero return is disambiguated through errno, as with glibc. */
static inline unsigned long getauxval(unsigned long type) {
    switch (type) {
        case AT_PAGESZ:
            return 4096;
        case AT_CLKTCK:
            return 1000;  /* RinOS scheduler tick, also exposed by sysconf() */
        case AT_HWCAP:
            /* Conservative x86_64 architectural baseline. */
            return HWCAP_X86_FPU | HWCAP_X86_MMX | HWCAP_X86_SSE |
                   HWCAP_X86_SSE2 | HWCAP_X86_FXSR | HWCAP_X86_CMOV;
        case AT_HWCAP2:
            /* Do not claim optional instruction sets until the loader passes
             * kernel-validated CPU/XSAVE state in a real auxiliary vector. */
            return 0;
        case AT_UID:
        case AT_EUID:
        case AT_GID:
        case AT_EGID: {
            __rin_credentials_v1 credentials;
            int error = __rin_credentials_get(&credentials);
            if (error != 0) {
                errno = error;
                return 0;
            }
            if (type == AT_UID) return credentials.uid;
            if (type == AT_EUID) return credentials.effective_uid;
            if (type == AT_GID) return credentials.gid;
            return credentials.effective_gid;
        }
        case AT_SECURE:
            errno = ENOSYS;
            return 0;
        default:
            errno = ENOENT;
            return 0;
    }
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_AUXVEC_H */
