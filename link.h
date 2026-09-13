/*
 * RinOS libc - link.h
 * Dynamic linker structures (ELF)
 */

#ifndef _LINK_H
#define _LINK_H

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "sys/syscall.h"
#include "../../../src/shared/rin_dynlink_inventory_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * ElfW macro - selects 32/64 bit ELF types based on platform
 * ═══════════════════════════════════════════════════════════════*/

#if __SIZEOF_POINTER__ == 4
#define __ELF_NATIVE_CLASS 32
#else
#define __ELF_NATIVE_CLASS 64
#endif

#define __ELFW(type, bits) Elf##bits##_##type
#define _ELFW(type, bits) __ELFW(type, bits)
#define ElfW(type) _ELFW(type, __ELF_NATIVE_CLASS)

/* ═══════════════════════════════════════════════════════════════
 * ELF Magic numbers and identification
 * ═══════════════════════════════════════════════════════════════*/

/* e_ident[] indexes */
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_ABIVERSION   8
#define EI_PAD          9
#define EI_NIDENT       16

/* Magic number values */
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'
#define ELFMAG          "\177ELF"
#define SELFMAG         4

/* ELF classes */
#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2
#define ELFCLASSNUM     3

/* Data encoding */
#define ELFDATANONE     0
#define ELFDATA2LSB     1
#define ELFDATA2MSB     2
#define ELFDATANUM      3

/* ELF version */
#define EV_NONE         0
#define EV_CURRENT      1
#define EV_NUM          2

/* OS/ABI identification */
#define ELFOSABI_NONE       0
#define ELFOSABI_SYSV       0
#define ELFOSABI_LINUX      3

/* ═══════════════════════════════════════════════════════════════
 * ELF File types
 * ═══════════════════════════════════════════════════════════════*/
#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4
#define ET_NUM          5

/* ═══════════════════════════════════════════════════════════════
 * Program header types
 * ═══════════════════════════════════════════════════════════════*/
#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_TLS          7
#define PT_NUM          8
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK    0x6474e551
#define PT_GNU_RELRO    0x6474e552

/* Program header flags */
#define PF_X            (1 << 0)
#define PF_W            (1 << 1)
#define PF_R            (1 << 2)

/* ═══════════════════════════════════════════════════════════════
 * Section header special indexes
 * ═══════════════════════════════════════════════════════════════*/
#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_XINDEX      0xffff
#define SHN_HIRESERVE   0xffff

/* Section header types */
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_INIT_ARRAY  14
#define SHT_FINI_ARRAY  15
#define SHT_PREINIT_ARRAY 16
#define SHT_GNU_HASH    0x6ffffff6

/* ═══════════════════════════════════════════════════════════════
 * Dynamic section tags
 * ═══════════════════════════════════════════════════════════════*/
#define DT_NULL         0
#define DT_NEEDED       1
#define DT_PLTRELSZ     2
#define DT_PLTGOT       3
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_RELAENT      9
#define DT_STRSZ        10
#define DT_SYMENT       11
#define DT_INIT         12
#define DT_FINI         13
#define DT_SONAME       14
#define DT_RPATH        15
#define DT_SYMBOLIC     16
#define DT_REL          17
#define DT_RELSZ        18
#define DT_RELENT       19
#define DT_PLTREL       20
#define DT_DEBUG        21
#define DT_TEXTREL      22
#define DT_JMPREL       23
#define DT_BIND_NOW     24
#define DT_INIT_ARRAY   25
#define DT_FINI_ARRAY   26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH      29
#define DT_FLAGS        30
#define DT_ENCODING     32
#define DT_PREINIT_ARRAY    32
#define DT_PREINIT_ARRAYSZ  33
#define DT_NUM          34

/* DT_* entries with values within this range use the d_un.d_ptr field */
#define DT_VALRNGLO     0x6ffffd00
#define DT_VALRNGHI     0x6ffffdff
#define DT_ADDRRNGLO    0x6ffffe00
#define DT_ADDRRNGHI    0x6ffffeff
#define DT_VERSYM       0x6ffffff0
#define DT_RELACOUNT    0x6ffffff9
#define DT_RELCOUNT     0x6ffffffa
#define DT_FLAGS_1      0x6ffffffb
#define DT_VERDEF       0x6ffffffc
#define DT_VERDEFNUM    0x6ffffffd
#define DT_VERNEED      0x6ffffffe
#define DT_VERNEEDNUM   0x6fffffff

#define DT_GNU_HASH     0x6ffffef5

/* ═══════════════════════════════════════════════════════════════
 * Symbol table binding and types
 * ═══════════════════════════════════════════════════════════════*/
#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2
#define STB_NUM         3

#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3
#define STT_FILE        4
#define STT_COMMON      5
#define STT_TLS         6
#define STT_NUM         7

#define STV_DEFAULT     0
#define STV_INTERNAL    1
#define STV_HIDDEN      2
#define STV_PROTECTED   3

/* ═══════════════════════════════════════════════════════════════
 * Relocation types (i386)
 * ═══════════════════════════════════════════════════════════════*/
#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC     10
#define R_386_32PLT     11
#define R_386_TLS_TPOFF 14
#define R_386_TLS_IE    15
#define R_386_TLS_GOTIE 16
#define R_386_TLS_LE    17
#define R_386_TLS_GD    18
#define R_386_TLS_LDM   19

/* ═══════════════════════════════════════════════════════════════
 * Version definition flags
 * ═══════════════════════════════════════════════════════════════*/
#define VER_FLG_BASE    0x1
#define VER_FLG_WEAK    0x2

#define VER_NDX_LOCAL   0
#define VER_NDX_GLOBAL  1

/* ═══════════════════════════════════════════════════════════════
 * ELF types for 32-bit
 * ═══════════════════════════════════════════════════════════════*/
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint16_t Elf32_Section;
typedef uint16_t Elf32_Versym;

/* Auxiliary vector entry */
typedef struct {
    uint32_t a_type;
    union {
        uint32_t a_val;
    } a_un;
} Elf32_auxv_t;

/* ═══════════════════════════════════════════════════════════════
 * ELF types for 64-bit (for completeness)
 * ═══════════════════════════════════════════════════════════════*/
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint16_t Elf64_Section;
typedef uint16_t Elf64_Versym;

/* Auxiliary vector entry */
typedef struct {
    uint64_t a_type;
    union {
        uint64_t a_val;
    } a_un;
} Elf64_auxv_t;

/* ELF Header */
typedef struct {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} Elf32_Ehdr;

/* Program Header */
typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

/* Dynamic section entry */
typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
} Elf32_Dyn;

/* Symbol table entry */
typedef struct {
    Elf32_Word    st_name;
    Elf32_Addr    st_value;
    Elf32_Word    st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half    st_shndx;
} Elf32_Sym;

/* Relocation entry (without addend) */
typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

/* Relocation entry (with addend) */
typedef struct {
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
    Elf32_Sword r_addend;
} Elf32_Rela;

/* Section Header */
typedef struct {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

/* Version definition */
typedef struct {
    Elf32_Half vd_version;
    Elf32_Half vd_flags;
    Elf32_Half vd_ndx;
    Elf32_Half vd_cnt;
    Elf32_Word vd_hash;
    Elf32_Word vd_aux;
    Elf32_Word vd_next;
} Elf32_Verdef;

/* Auxiliary version definition */
typedef struct {
    Elf32_Word vda_name;
    Elf32_Word vda_next;
} Elf32_Verdaux;

/* Version dependency */
typedef struct {
    Elf32_Half vn_version;
    Elf32_Half vn_cnt;
    Elf32_Word vn_file;
    Elf32_Word vn_aux;
    Elf32_Word vn_next;
} Elf32_Verneed;

/* Auxiliary version dependency */
typedef struct {
    Elf32_Word vna_hash;
    Elf32_Half vna_flags;
    Elf32_Half vna_other;
    Elf32_Word vna_name;
    Elf32_Word vna_next;
} Elf32_Vernaux;

/* ═══════════════════════════════════════════════════════════════
 * 64-bit ELF structures
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    unsigned char e_ident[16];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr  d_ptr;
    } d_un;
} Elf64_Dyn;

typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr  r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

typedef struct {
    Elf64_Addr   r_offset;
    Elf64_Xword  r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

typedef struct {
    Elf64_Word sh_name;
    Elf64_Word sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off  sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link;
    Elf64_Word sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
    Elf64_Half vd_version;
    Elf64_Half vd_flags;
    Elf64_Half vd_ndx;
    Elf64_Half vd_cnt;
    Elf64_Word vd_hash;
    Elf64_Word vd_aux;
    Elf64_Word vd_next;
} Elf64_Verdef;

typedef struct {
    Elf64_Word vda_name;
    Elf64_Word vda_next;
} Elf64_Verdaux;

typedef struct {
    Elf64_Half vn_version;
    Elf64_Half vn_cnt;
    Elf64_Word vn_file;
    Elf64_Word vn_aux;
    Elf64_Word vn_next;
} Elf64_Verneed;

typedef struct {
    Elf64_Word vna_hash;
    Elf64_Half vna_flags;
    Elf64_Half vna_other;
    Elf64_Word vna_name;
    Elf64_Word vna_next;
} Elf64_Vernaux;

/* ═══════════════════════════════════════════════════════════════
 * ELF macros
 * ═══════════════════════════════════════════════════════════════*/
#define ELF32_R_SYM(i)  ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

/* 64-bit ELF macros */
#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t) (((Elf64_Xword)(s) << 32) + (Elf64_Xword)(t))

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

/* Link map structure (for dynamic linker) */
struct link_map {
    ElfW(Addr) l_addr;          /* Base address */
    char*      l_name;          /* Object name */
    ElfW(Dyn)* l_ld;            /* Dynamic section */
    struct link_map* l_next;    /* Next in chain */
    struct link_map* l_prev;    /* Previous in chain */
};

/* r_debug structure (for debugger support) */
struct r_debug {
    int r_version;              /* Version number */
    struct link_map* r_map;     /* Link map head */
    ElfW(Addr) r_brk;           /* Breakpoint address */
    enum {
        RT_CONSISTENT,
        RT_ADD,
        RT_DELETE
    } r_state;
    ElfW(Addr) r_ldbase;        /* Loader base address */
};

/* dl_phdr_info structure (for dl_iterate_phdr) */
struct dl_phdr_info {
    ElfW(Addr)        dlpi_addr;    /* Base address */
    const char*       dlpi_name;    /* Object name */
    const ElfW(Phdr)* dlpi_phdr;    /* Program headers */
    ElfW(Half)        dlpi_phnum;   /* Number of program headers */

    /* Additional fields (glibc extension) */
    unsigned long long dlpi_adds;   /* Times objects added */
    unsigned long long dlpi_subs;   /* Times objects removed */
    size_t           dlpi_tls_modid;/* TLS module ID */
    void*            dlpi_tls_data; /* TLS data address */
};

/* Callback type for dl_iterate_phdr */
typedef int (*dl_iterate_phdr_callback)(struct dl_phdr_info* info,
                                        size_t size, void* data);

/* The RIN64 v3 process loader exposes a generation-consistent aggregate of
 * the launch graph and later dlopen() graphs.  RIN images have no ELF
 * program-header table, so those fields remain NULL/zero rather than being
 * synthesized. */
#ifndef _RIN_DYNLINK_SYSCALL2
#define RIN_DYNLINK_SYSCALL2_DEFAULT_HOOK 1
#define _RIN_DYNLINK_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

static inline int __rin_dl_inventory_error(intptr_t result) {
    uintptr_t error;
    if (result >= 0) return 0;
    error = (uintptr_t)0 - (uintptr_t)result;
    errno = error != 0u && error <= 4095u ? (int)error : EIO;
    return -1;
}

/* Fetch one immutable launch-graph record without exposing it.  Callers that
 * consume multiple records still have to verify a shared generation. */
static inline int __rin_dl_inventory_fetch(unsigned long index,
                                           RinDlInventoryRecordV1* record) {
    intptr_t result;
    unsigned long name_index;
    int legacy_rll;
    if (!record) {
        errno = EINVAL;
        return -1;
    }
    for (name_index = 0UL; name_index < sizeof(*record); ++name_index)
        ((unsigned char*)record)[name_index] = 0u;
    result = _RIN_DYNLINK_SYSCALL2(SYS_DLINVENTORY, (uintptr_t)index,
                                   (uintptr_t)record);
    if (__rin_dl_inventory_error(result) != 0) return -1;
    legacy_rll = record->flags == RIN_DLINVENTORY_V1_FLAG_LEGACY_RLL;
    if (legacy_rll && index == 0UL && record->record_count == 0u &&
        record->launch_generation != 0u &&
        record->name_length == 0u && record->load_address == 0u &&
        record->mapped_size == 0u && record->record_index == 0u &&
        record->add_count == 0u && record->sub_count == 0u &&
        record->program_headers == 0u &&
        record->program_header_count == 0u && record->tls_data == 0u &&
        record->tls_module_id == 0u) {
        /* Legacy RLL has no process image.  A zero-count sentinel is a
         * successful empty enumeration rather than an ENOENT failure. */
        return 1;
    }
    if (result != 0 ||
        record->abi_version != RIN_DLINVENTORY_V1_ABI_VERSION ||
        (!legacy_rll && record->record_count == 0u) ||
        (legacy_rll
             ? record->record_count > RIN_DLINVENTORY_V1_MAX_LAUNCH_RECORDS
             : record->record_count > RIN_DLINVENTORY_V1_MAX_RECORDS) ||
        record->record_index != index || record->launch_generation == 0u ||
        record->add_count != record->record_count || record->sub_count != 0u ||
        record->name_length == 0u ||
        record->name_length >= RIN_DLINVENTORY_V1_NAME_CAPACITY ||
        record->name[record->name_length] != '\0' ||
        record->load_address == 0u || record->mapped_size == 0u ||
        record->program_headers != 0u || record->program_header_count != 0u ||
        record->tls_data != 0u || record->tls_module_id != 0u ||
        (!legacy_rll &&
         (index == 0UL
              ? record->flags != RIN_DLINVENTORY_V1_FLAG_MAIN_IMAGE
              : record->flags != 0u))) {
        errno = EIO;
        return -1;
    }
    if (legacy_rll && record->flags != RIN_DLINVENTORY_V1_FLAG_LEGACY_RLL) {
        errno = EIO;
        return -1;
    }
    for (name_index = 0UL; name_index < record->name_length; ++name_index) {
        if (record->name[name_index] == '\0') {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

static inline int dl_iterate_phdr(dl_iterate_phdr_callback callback,
                                  void* data) {
    RinDlInventoryRecordV1 record;
    unsigned long index;
    unsigned long expected_count = 0UL;
    unsigned long long expected_generation = 0ULL;
    unsigned long long expected_adds = 0ULL;
    unsigned long long expected_subs = 0ULL;
    unsigned int expected_legacy = 0u;

    if (!callback) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0UL; ; ++index) {
        struct dl_phdr_info information;
        int callback_result;
        const int fetch_result = __rin_dl_inventory_fetch(index, &record);
        if (fetch_result == 1) return 0;
        if (fetch_result != 0) return -1;
        if (index == 0UL) {
            expected_count = record.record_count;
            expected_generation = record.launch_generation;
            expected_adds = record.add_count;
            expected_subs = record.sub_count;
            expected_legacy = record.flags ==
                RIN_DLINVENTORY_V1_FLAG_LEGACY_RLL;
        } else if (record.record_count != expected_count ||
                   record.launch_generation != expected_generation ||
                   record.add_count != expected_adds ||
                   record.sub_count != expected_subs ||
                   ((record.flags == RIN_DLINVENTORY_V1_FLAG_LEGACY_RLL) !=
                    (expected_legacy != 0u))) {
            errno = EIO;
            return -1;
        }
        if (index >= expected_count) {
            errno = EIO;
            return -1;
        }
        information.dlpi_addr = (ElfW(Addr))record.load_address;
        information.dlpi_name = record.name;
        information.dlpi_phdr = (const ElfW(Phdr)*)(uintptr_t)
            record.program_headers;
        information.dlpi_phnum = (ElfW(Half))record.program_header_count;
        information.dlpi_adds = record.add_count;
        information.dlpi_subs = record.sub_count;
        information.dlpi_tls_modid = (size_t)record.tls_module_id;
        information.dlpi_tls_data = (void*)(uintptr_t)record.tls_data;
        callback_result = callback(&information, sizeof(information), data);
        if (callback_result != 0) return callback_result;
        if (index + 1UL == expected_count) return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* _LINK_H */
