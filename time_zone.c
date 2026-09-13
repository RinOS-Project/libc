/* SPDX-License-Identifier: MIT */

#include "errno.h"
#include "limits.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

#if !defined(RIN_USERSPACE)
#error "time_zone.c is a user-space libc runtime owner"
#endif

#define RIN_TZ_TEXT_MAX RIN_TIME_ZONE_PROVIDER_TEXT_MAX
#define RIN_TZ_NAME_MAX 31u

typedef enum {
    RIN_TIME_ZONE_RULE_MONTH_WEEKDAY,
    RIN_TIME_ZONE_RULE_JULIAN_NO_LEAP,
    RIN_TIME_ZONE_RULE_DAY_OF_YEAR
} RinTimeZoneRuleKind;

typedef enum {
    /* POSIX omits a suffix to mean the wall clock before this transition. */
    RIN_TIME_ZONE_RULE_TIME_WALL,
    RIN_TIME_ZONE_RULE_TIME_STANDARD,
    RIN_TIME_ZONE_RULE_TIME_UTC
} RinTimeZoneRuleTimeBasis;

typedef struct {
    RinTimeZoneRuleKind kind;
    unsigned int month;
    unsigned int week;
    unsigned int weekday;
    unsigned int day;
    int seconds;
    RinTimeZoneRuleTimeBasis time_basis;
} RinTimeZoneRule;

typedef struct {
    char standard_name[RIN_TZ_NAME_MAX + 1u];
    char daylight_name[RIN_TZ_NAME_MAX + 1u];
    long standard_offset;
    long daylight_offset;
    int has_daylight;
    RinTimeZoneRule start;
    RinTimeZoneRule end;
    int remote;
    char remote_id[RIN_TZ_TEXT_MAX + 1u];
} RinTimeZone;

typedef struct {
    const char* text;
    size_t offset;
} RinTimeZoneParser;

static RinTimeZone rin_time_zone = {
    "UTC", "UTC", 0, 0, 0, {0}, {0}, 0, ""
};
static volatile unsigned char rin_time_zone_lock;
static RinTimeZoneSystemProvider rin_time_zone_system_provider;
static void* rin_time_zone_system_provider_context;
static unsigned char rin_time_zone_system_provider_bound;
static char rin_time_zone_standard_name[RIN_TZ_NAME_MAX + 1u] = "UTC";
static char rin_time_zone_daylight_name[RIN_TZ_NAME_MAX + 1u] = "UTC";

/* Standard hosted `struct tm` has no tm_gmtoff/tm_zone fields.  Keep the
 * public layout unchanged and provide a small, allocation-free sidecar for
 * C++ locale facets.  The owner is intentionally bounded: exhaustion makes
 * a `%z`/`%Z` parse fail atomically instead of evicting metadata for another
 * live object. */
#define RIN_CXX_TM_TIMEZONE_SLOTS 32u
#define RIN_CXX_TM_TIMEZONE_HAS_OFFSET 1u
#define RIN_CXX_TM_TIMEZONE_HAS_NAME 2u
typedef struct RinCxxTmTimezoneSlot {
    const void* value;
    long offset;
    unsigned flags;
    char zone[64];
} RinCxxTmTimezoneSlot;
static RinCxxTmTimezoneSlot rin_cxx_tm_timezone_slots[
    RIN_CXX_TM_TIMEZONE_SLOTS];
static volatile unsigned char rin_cxx_tm_timezone_lock;

extern int rin_system_timezone_get(char* output, size_t capacity)
    __attribute__((weak));
extern int rin_system_timezone_offset(const char* zone,
                                      int64_t epoch_seconds,
                                      int* offset_minutes,
                                      int* in_dst) __attribute__((weak));
#if defined(__cplusplus)
#define RIN_TIME_ZONE_THREAD_LOCAL thread_local
#else
#define RIN_TIME_ZONE_THREAD_LOCAL _Thread_local
#endif
static RIN_TIME_ZONE_THREAD_LOCAL struct tm rin_time_zone_tm_buffer;
#undef RIN_TIME_ZONE_THREAD_LOCAL

char* tzname[2] = {
    rin_time_zone_standard_name,
    rin_time_zone_daylight_name
};
long timezone;
int daylight;

static void rin_time_zone_acquire(void) {
    while (__atomic_test_and_set(&rin_time_zone_lock, __ATOMIC_ACQUIRE)) {
    }
}

static void rin_time_zone_release(void) {
    __atomic_clear(&rin_time_zone_lock, __ATOMIC_RELEASE);
}

static void rin_cxx_tm_timezone_acquire(void) {
    while (__atomic_test_and_set(&rin_cxx_tm_timezone_lock, __ATOMIC_ACQUIRE)) {
    }
}

static void rin_cxx_tm_timezone_release(void) {
    __atomic_clear(&rin_cxx_tm_timezone_lock, __ATOMIC_RELEASE);
}

int rin_cxx_tm_timezone_get(const void* value, long* offset, char* zone,
                            size_t zone_capacity, unsigned* flags) {
    unsigned index;
    if (!value || !flags || (zone && zone_capacity == 0u)) return 0;
    *flags = 0u;
    if (offset) *offset = 0L;
    if (zone) zone[0] = '\0';
    rin_cxx_tm_timezone_acquire();
    for (index = 0u; index < RIN_CXX_TM_TIMEZONE_SLOTS; ++index) {
        RinCxxTmTimezoneSlot const* slot = &rin_cxx_tm_timezone_slots[index];
        size_t length = 0u;
        if (slot->value != value) continue;
        if (offset) *offset = slot->offset;
        *flags = slot->flags;
        while (length < sizeof(slot->zone) && slot->zone[length] != '\0')
            ++length;
        if (zone) {
            if (length + 1u > zone_capacity) {
                rin_cxx_tm_timezone_release();
                return 0;
            }
            memcpy(zone, slot->zone, length + 1u);
        }
        rin_cxx_tm_timezone_release();
        return 1;
    }
    rin_cxx_tm_timezone_release();
    return 0;
}

int rin_cxx_tm_timezone_set(const void* value, unsigned flags, long offset,
                            const char* zone) {
    unsigned index;
    unsigned free_index = RIN_CXX_TM_TIMEZONE_SLOTS;
    size_t length = 0u;
    if (!value || (flags & ~(RIN_CXX_TM_TIMEZONE_HAS_OFFSET |
                             RIN_CXX_TM_TIMEZONE_HAS_NAME)) != 0u ||
        ((flags & RIN_CXX_TM_TIMEZONE_HAS_OFFSET) != 0u &&
         (offset < -86340L || offset > 86340L || offset % 60L != 0L)) ||
        ((flags & RIN_CXX_TM_TIMEZONE_HAS_NAME) != 0u &&
         (!zone || zone[0] == '\0'))) return 0;
    if (zone) {
        while (length < 64u && zone[length] != '\0') {
            unsigned char current = (unsigned char)zone[length];
            if (current < 0x21u || current > 0x7eu) return 0;
            ++length;
        }
        if (length >= 64u) return 0;
    }
    rin_cxx_tm_timezone_acquire();
    for (index = 0u; index < RIN_CXX_TM_TIMEZONE_SLOTS; ++index) {
        if (rin_cxx_tm_timezone_slots[index].value == value) break;
        if (!rin_cxx_tm_timezone_slots[index].value &&
            free_index == RIN_CXX_TM_TIMEZONE_SLOTS) free_index = index;
    }
    if (index == RIN_CXX_TM_TIMEZONE_SLOTS) index = free_index;
    if (index == RIN_CXX_TM_TIMEZONE_SLOTS) {
        rin_cxx_tm_timezone_release();
        return 0;
    }
    rin_cxx_tm_timezone_slots[index].value = value;
    rin_cxx_tm_timezone_slots[index].offset = offset;
    rin_cxx_tm_timezone_slots[index].flags = flags;
    memset(rin_cxx_tm_timezone_slots[index].zone, 0,
           sizeof(rin_cxx_tm_timezone_slots[index].zone));
    if (zone && length != 0u)
        memcpy(rin_cxx_tm_timezone_slots[index].zone, zone, length);
    rin_cxx_tm_timezone_release();
    return 1;
}

static int rin_time_zone_ascii_alpha(char value) {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

static int rin_time_zone_digit(char value) {
    return value >= '0' && value <= '9';
}

static void rin_time_zone_copy(char* output, const char* input,
                               size_t capacity) {
    size_t index = 0u;
    while (index + 1u < capacity && input[index] != '\0') {
        output[index] = input[index];
        ++index;
    }
    output[index] = '\0';
}

static int rin_time_zone_snapshot_environment(char output[RIN_TZ_TEXT_MAX + 1u]) {
    const char* value = getenv("TZ");
    RinTimeZoneSystemProvider provider = NULL;
    void* provider_context = NULL;
    size_t length = 0u;
    for (size_t index = 0u; index <= RIN_TZ_TEXT_MAX; ++index)
        output[index] = '\0';
    if (!value) {
        rin_time_zone_acquire();
        if (rin_time_zone_system_provider_bound) {
            provider = rin_time_zone_system_provider;
            provider_context = rin_time_zone_system_provider_context;
        }
        rin_time_zone_release();
        if (provider) {
            if (!provider(output, RIN_TZ_TEXT_MAX + 1u, provider_context)) {
                errno = EINVAL;
                return 0;
            }
            while (length <= RIN_TZ_TEXT_MAX && output[length] != '\0') ++length;
            if (length > RIN_TZ_TEXT_MAX) {
                errno = EOVERFLOW;
                return 0;
            }
            return 1;
        }
        if (rin_system_timezone_get) {
            if (rin_system_timezone_get(output, RIN_TZ_TEXT_MAX + 1u)) {
                while (length <= RIN_TZ_TEXT_MAX && output[length] != '\0')
                    ++length;
                if (length > RIN_TZ_TEXT_MAX) {
                    errno = EOVERFLOW;
                    return 0;
                }
                return 1;
            }
            /* A missing or unreadable system owner is a UTC-safe fallback. */
            value = "UTC0";
        } else {
            value = "UTC0";
        }
    } else if (value[0] == '\0') {
        /* POSIX defines an explicitly empty TZ as UTC.  It must not silently
         * turn into the machine-wide provider merely because the provider is
         * installed. */
        value = "UTC0";
    }
    while (length <= RIN_TZ_TEXT_MAX && value[length] != '\0') ++length;
    if (length > RIN_TZ_TEXT_MAX) {
        errno = EOVERFLOW;
        return 0;
    }
    for (size_t index = 0u; index < length; ++index) output[index] = value[index];
    return 1;
}

int rin_time_zone_bind_system_provider(RinTimeZoneSystemProvider provider,
                                       void* context) {
    if (!provider) {
        errno = EINVAL;
        return -1;
    }
    rin_time_zone_acquire();
    if (rin_time_zone_system_provider_bound) {
        rin_time_zone_release();
        errno = EBUSY;
        return -1;
    }
    rin_time_zone_system_provider = provider;
    rin_time_zone_system_provider_context = context;
    rin_time_zone_system_provider_bound = 1u;
    rin_time_zone_release();
    return 0;
}

static int rin_time_zone_unsigned(RinTimeZoneParser* parser,
                                  unsigned int maximum,
                                  unsigned int* output) {
    unsigned int value = 0u;
    size_t start = parser->offset;
    while (rin_time_zone_digit(parser->text[parser->offset])) {
        unsigned int digit = (unsigned int)(parser->text[parser->offset] - '0');
        if (digit > maximum || value > (maximum - digit) / 10u) return 0;
        value = value * 10u + digit;
        ++parser->offset;
    }
    if (parser->offset == start || value > maximum) return 0;
    *output = value;
    return 1;
}

static int rin_time_zone_consume(RinTimeZoneParser* parser, char expected) {
    if (parser->text[parser->offset] != expected) return 0;
    ++parser->offset;
    return 1;
}

static int rin_time_zone_name(RinTimeZoneParser* parser, char* output) {
    size_t length = 0u;
    if (parser->text[parser->offset] == '<') {
        ++parser->offset;
        while (parser->text[parser->offset] != '\0' &&
               parser->text[parser->offset] != '>') {
            unsigned char value = (unsigned char)parser->text[parser->offset];
            if (value < 0x21u || value > 0x7eu || value == ',' ||
                length == RIN_TZ_NAME_MAX) return 0;
            output[length++] = (char)value;
            ++parser->offset;
        }
        if (parser->text[parser->offset] != '>') return 0;
        ++parser->offset;
    } else {
        while (rin_time_zone_ascii_alpha(parser->text[parser->offset])) {
            if (length == RIN_TZ_NAME_MAX) return 0;
            output[length++] = parser->text[parser->offset++];
        }
    }
    if (length < 3u) return 0;
    output[length] = '\0';
    return 1;
}

/* POSIX TZ offsets state how much must be added to local time to obtain UTC. */
static int rin_time_zone_offset(RinTimeZoneParser* parser, long* output) {
    int sign = 1;
    unsigned int hour = 0u;
    unsigned int minute = 0u;
    unsigned int second = 0u;
    long posix_seconds;
    if (parser->text[parser->offset] == '+' ||
        parser->text[parser->offset] == '-') {
        if (parser->text[parser->offset++] == '-') sign = -1;
    }
    if (!rin_time_zone_unsigned(parser, 23u, &hour)) return 0;
    if (parser->text[parser->offset] == ':') {
        ++parser->offset;
        if (!rin_time_zone_unsigned(parser, 59u, &minute)) return 0;
        if (parser->text[parser->offset] == ':') {
            ++parser->offset;
            if (!rin_time_zone_unsigned(parser, 59u, &second)) return 0;
        }
    }
    posix_seconds = (long)hour * 3600L + (long)minute * 60L + (long)second;
    *output = -(long)sign * posix_seconds;
    return 1;
}

static int rin_time_zone_rule(RinTimeZoneParser* parser,
                              RinTimeZoneRule* output) {
    RinTimeZoneRule candidate = {0};
    unsigned int month = 0u;
    unsigned int week = 0u;
    unsigned int weekday = 0u;
    unsigned int day = 0u;
    unsigned int hour = 2u;
    unsigned int minute = 0u;
    unsigned int second = 0u;
    int time_sign = 1;
    if (parser->text[parser->offset] == 'M') {
        if (!rin_time_zone_consume(parser, 'M') ||
            !rin_time_zone_unsigned(parser, 12u, &month) || month == 0u ||
            !rin_time_zone_consume(parser, '.') ||
            !rin_time_zone_unsigned(parser, 5u, &week) || week == 0u ||
            !rin_time_zone_consume(parser, '.') ||
            !rin_time_zone_unsigned(parser, 6u, &weekday)) return 0;
        candidate.kind = RIN_TIME_ZONE_RULE_MONTH_WEEKDAY;
        candidate.month = month;
        candidate.week = week;
        candidate.weekday = weekday;
    } else if (parser->text[parser->offset] == 'J') {
        if (!rin_time_zone_consume(parser, 'J') ||
            !rin_time_zone_unsigned(parser, 365u, &day) || day == 0u) return 0;
        candidate.kind = RIN_TIME_ZONE_RULE_JULIAN_NO_LEAP;
        candidate.day = day;
    } else {
        if (!rin_time_zone_unsigned(parser, 365u, &day)) return 0;
        candidate.kind = RIN_TIME_ZONE_RULE_DAY_OF_YEAR;
        candidate.day = day;
    }
    if (parser->text[parser->offset] == '/') {
        ++parser->offset;
        if (parser->text[parser->offset] == '+' ||
            parser->text[parser->offset] == '-') {
            if (parser->text[parser->offset] == '-') time_sign = -1;
            ++parser->offset;
        }
        if (!rin_time_zone_unsigned(parser, 167u, &hour)) return 0;
        if (parser->text[parser->offset] == ':') {
            ++parser->offset;
            if (!rin_time_zone_unsigned(parser, 59u, &minute)) return 0;
            if (parser->text[parser->offset] == ':') {
                ++parser->offset;
                if (!rin_time_zone_unsigned(parser, 59u, &second)) return 0;
            }
        }
        if (parser->text[parser->offset] == 'w') {
            ++parser->offset;
        } else if (parser->text[parser->offset] == 's') {
            candidate.time_basis = RIN_TIME_ZONE_RULE_TIME_STANDARD;
            ++parser->offset;
        } else if (parser->text[parser->offset] == 'u' ||
                   parser->text[parser->offset] == 'g' ||
                   parser->text[parser->offset] == 'z') {
            candidate.time_basis = RIN_TIME_ZONE_RULE_TIME_UTC;
            ++parser->offset;
        }
    }
    candidate.seconds = time_sign *
                        (int)(hour * 3600u + minute * 60u + second);
    *output = candidate;
    return 1;
}

static void rin_time_zone_default_rules(RinTimeZone* zone) {
    zone->start = (RinTimeZoneRule){
        RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 3u, 2u, 0u, 0u, 2 * 3600,
        RIN_TIME_ZONE_RULE_TIME_WALL
    };
    zone->end = (RinTimeZoneRule){
        RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 11u, 1u, 0u, 0u, 2 * 3600,
        RIN_TIME_ZONE_RULE_TIME_WALL
    };
}

static int rin_time_zone_text_equal(const char* left, const char* right) {
    size_t index = 0u;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

typedef struct {
    const char* name;
    long offset;
} RinTimeZoneAbbreviation;

static const RinTimeZoneAbbreviation rin_time_zone_abbreviations[] = {
    {"UTC", 0L},
    {"GMT", 0L},
    {"JST", 9L * 3600L},
    {"EST", -5L * 3600L},
    {"EDT", -4L * 3600L},
    {"CST", -6L * 3600L},
    {"CDT", -5L * 3600L},
    {"MST", -7L * 3600L},
    {"MDT", -6L * 3600L},
    {"PST", -8L * 3600L},
    {"PDT", -7L * 3600L},
    {"BST", 3600L},
    {"CET", 3600L},
    {"CEST", 2L * 3600L},
    {"EET", 2L * 3600L},
    {"EEST", 3L * 3600L},
    {"AEST", 10L * 3600L},
    {"AEDT", 11L * 3600L},
    {"NZST", 12L * 3600L},
    {"NZDT", 13L * 3600L},
};

/* `%Z` consumes only the immutable abbreviations emitted by the built-in
 * named-zone map. It deliberately does not infer a region or read TZ. */
int rin_time_zone_match_abbreviation(const char* input, size_t* consumed,
                                     long* offset, const char** zone) {
    size_t entry_index;
    if (!input || !consumed || !offset || !zone) return 0;
    for (entry_index = 0u;
         entry_index < sizeof(rin_time_zone_abbreviations) /
                       sizeof(rin_time_zone_abbreviations[0]);
         ++entry_index) {
        const RinTimeZoneAbbreviation* entry =
            &rin_time_zone_abbreviations[entry_index];
        size_t length = 0u;
        while (entry->name[length] != '\0' && input[length] == entry->name[length])
            ++length;
        if (entry->name[length] != '\0') continue;
        *consumed = length;
        *offset = entry->offset;
        *zone = entry->name;
        return 1;
    }
    return 0;
}

static void rin_time_zone_set_names(RinTimeZone* zone, const char* standard,
                                    const char* daylight_name) {
    rin_time_zone_copy(zone->standard_name, standard,
                       sizeof(zone->standard_name));
    rin_time_zone_copy(zone->daylight_name, daylight_name,
                       sizeof(zone->daylight_name));
}

static void rin_time_zone_europe_rules(RinTimeZone* zone) {
    zone->has_daylight = 1;
    zone->start = (RinTimeZoneRule){
        RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 3u, 5u, 0u, 0u, 2 * 3600,
        RIN_TIME_ZONE_RULE_TIME_WALL
    };
    zone->end = (RinTimeZoneRule){
        RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 10u, 5u, 0u, 0u, 3 * 3600,
        RIN_TIME_ZONE_RULE_TIME_WALL
    };
}

static void rin_time_zone_north_america(RinTimeZone* zone,
                                        const char* standard,
                                        const char* daylight_name,
                                        long standard_offset) {
    rin_time_zone_set_names(zone, standard, daylight_name);
    zone->standard_offset = standard_offset;
    zone->daylight_offset = standard_offset + 3600L;
    zone->has_daylight = 1;
    rin_time_zone_default_rules(zone);
}

static int rin_time_zone_named(const char* text, RinTimeZone* output) {
    RinTimeZone candidate = {0};
    if (rin_time_zone_text_equal(text, "UTC") ||
        rin_time_zone_text_equal(text, "Etc/UTC") ||
        rin_time_zone_text_equal(text, "Etc/GMT") ||
        rin_time_zone_text_equal(text, "GMT")) {
        rin_time_zone_set_names(&candidate, "UTC", "UTC");
    } else if (rin_time_zone_text_equal(text, "Asia/Tokyo")) {
        rin_time_zone_set_names(&candidate, "JST", "JST");
        candidate.standard_offset = 9L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "Asia/Seoul")) {
        rin_time_zone_set_names(&candidate, "KST", "KST");
        candidate.standard_offset = 9L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "Asia/Shanghai") ||
               rin_time_zone_text_equal(text, "Asia/Hong_Kong") ||
               rin_time_zone_text_equal(text, "Asia/Singapore")) {
        rin_time_zone_set_names(&candidate, "CST", "CST");
        candidate.standard_offset = 8L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "America/New_York")) {
        rin_time_zone_north_america(&candidate, "EST", "EDT",
                                    -5L * 3600L);
    } else if (rin_time_zone_text_equal(text, "America/Chicago")) {
        rin_time_zone_north_america(&candidate, "CST", "CDT",
                                    -6L * 3600L);
    } else if (rin_time_zone_text_equal(text, "America/Denver")) {
        rin_time_zone_north_america(&candidate, "MST", "MDT",
                                    -7L * 3600L);
    } else if (rin_time_zone_text_equal(text, "America/Los_Angeles")) {
        rin_time_zone_north_america(&candidate, "PST", "PDT",
                                    -8L * 3600L);
    } else if (rin_time_zone_text_equal(text, "America/Phoenix")) {
        rin_time_zone_set_names(&candidate, "MST", "MST");
        candidate.standard_offset = -7L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "Europe/London")) {
        rin_time_zone_set_names(&candidate, "GMT", "BST");
        candidate.daylight_offset = 3600L;
        candidate.has_daylight = 1;
        candidate.start = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 3u, 5u, 0u, 0u, 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
        candidate.end = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 10u, 5u, 0u, 0u, 2 * 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
    } else if (rin_time_zone_text_equal(text, "Europe/Paris") ||
               rin_time_zone_text_equal(text, "Europe/Berlin") ||
               rin_time_zone_text_equal(text, "Europe/Rome") ||
               rin_time_zone_text_equal(text, "Europe/Madrid")) {
        rin_time_zone_set_names(&candidate, "CET", "CEST");
        candidate.standard_offset = 3600L;
        candidate.daylight_offset = 2L * 3600L;
        rin_time_zone_europe_rules(&candidate);
    } else if (rin_time_zone_text_equal(text, "Europe/Helsinki")) {
        rin_time_zone_set_names(&candidate, "EET", "EEST");
        candidate.standard_offset = 2L * 3600L;
        candidate.daylight_offset = 3L * 3600L;
        rin_time_zone_europe_rules(&candidate);
    } else if (rin_time_zone_text_equal(text, "Europe/Moscow")) {
        rin_time_zone_set_names(&candidate, "MSK", "MSK");
        candidate.standard_offset = 3L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "Australia/Sydney") ||
               rin_time_zone_text_equal(text, "Australia/Melbourne")) {
        rin_time_zone_set_names(&candidate, "AEST", "AEDT");
        candidate.standard_offset = 10L * 3600L;
        candidate.daylight_offset = 11L * 3600L;
        candidate.has_daylight = 1;
        candidate.start = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 10u, 1u, 0u, 0u, 2 * 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
        candidate.end = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 4u, 1u, 0u, 0u, 3 * 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
    } else if (rin_time_zone_text_equal(text, "Australia/Perth")) {
        rin_time_zone_set_names(&candidate, "AWST", "AWST");
        candidate.standard_offset = 8L * 3600L;
        candidate.daylight_offset = candidate.standard_offset;
    } else if (rin_time_zone_text_equal(text, "Pacific/Auckland")) {
        rin_time_zone_set_names(&candidate, "NZST", "NZDT");
        candidate.standard_offset = 12L * 3600L;
        candidate.daylight_offset = 13L * 3600L;
        candidate.has_daylight = 1;
        candidate.start = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 9u, 5u, 0u, 0u, 2 * 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
        candidate.end = (RinTimeZoneRule){
            RIN_TIME_ZONE_RULE_MONTH_WEEKDAY, 4u, 1u, 0u, 0u, 3 * 3600,
            RIN_TIME_ZONE_RULE_TIME_WALL
        };
    } else {
        return 0;
    }
    *output = candidate;
    return 1;
}

int rin_time_zone_resolve_name(const char* input, size_t length, long* offset) {
    char token[RIN_TZ_NAME_MAX + 1u];
    RinTimeZone candidate = {0};
    size_t index;
    size_t consumed = 0u;
    const char* abbreviation = NULL;
    long abbreviation_offset = 0L;

    if (!input || !offset || length == 0u || length > RIN_TZ_NAME_MAX)
        return 0;
    for (index = 0u; index < length; ++index) {
        token[index] = input[index];
    }
    token[length] = '\0';
    if (rin_time_zone_match_abbreviation(token, &consumed,
                                         &abbreviation_offset,
                                         &abbreviation) &&
        consumed == length) {
        *offset = abbreviation_offset;
        return 1;
    }
    if (!rin_time_zone_named(token, &candidate)) return 0;
    *offset = candidate.standard_offset;
    return 1;
}

static int rin_time_zone_remote(const char* text, RinTimeZone* output) {
    RinTimeZone candidate = {0};
    int offset_minutes = 0;
    int in_dst = 0;
    int summer_offset = 0;
    int summer_dst = 0;
    int winter_offset = 0;
    int winter_dst = 0;
    int standard_minutes = 0;
    int have_standard = 0;
    size_t length = 0u;
    if (!rin_system_timezone_offset || !text || !output) return 0;
    while (length <= RIN_TZ_TEXT_MAX && text[length] != '\0') ++length;
    if (length == 0u || length > RIN_TZ_TEXT_MAX) return 0;
    if (!rin_system_timezone_offset(text, 0, &offset_minutes, &in_dst))
        return 0;
    /* Probe representative seasons so the legacy globals remain useful even
     * for a remote zone whose current instant is in standard time. */
    (void)rin_system_timezone_offset(text, 946684800LL, &winter_offset,
                                     &winter_dst); /* 2000-01-01 */
    (void)rin_system_timezone_offset(text, 962409600LL, &summer_offset,
                                     &summer_dst); /* 2000-07-01 */
    if (!winter_dst) {
        standard_minutes = winter_offset;
        have_standard = 1;
    } else if (!summer_dst) {
        standard_minutes = summer_offset;
        have_standard = 1;
    } else if (!in_dst) {
        standard_minutes = offset_minutes;
        have_standard = 1;
    }
    rin_time_zone_set_names(&candidate, "LOCAL", "LOCAL-DST");
    candidate.remote = 1;
    rin_time_zone_copy(candidate.remote_id, text, sizeof(candidate.remote_id));
    candidate.standard_offset = (long)(have_standard ? standard_minutes :
                                       offset_minutes) * 60L;
    candidate.daylight_offset = candidate.standard_offset;
    if (summer_dst) {
        candidate.has_daylight = 1;
        candidate.daylight_offset = (long)summer_offset * 60L;
    } else if (in_dst) {
        candidate.has_daylight = 1;
        candidate.daylight_offset = (long)offset_minutes * 60L;
    }
    *output = candidate;
    return 1;
}

static int rin_time_zone_parse(const char* text, RinTimeZone* output) {
    RinTimeZoneParser parser = {text, 0u};
    RinTimeZone candidate = {0};
    if (text[0] == ':') {
        if (rin_time_zone_named(text + 1, output)) return 1;
        if (rin_time_zone_remote(text + 1, output)) return 1;
        errno = EINVAL;
        return 0;
    }
    if (rin_time_zone_named(text, output)) return 1;
    if (rin_time_zone_remote(text, output)) return 1;
    if (!rin_time_zone_name(&parser, candidate.standard_name) ||
        !rin_time_zone_offset(&parser, &candidate.standard_offset)) {
        errno = EINVAL;
        return 0;
    }
    if (text[parser.offset] == '\0') {
        rin_time_zone_copy(candidate.daylight_name, candidate.standard_name,
                           sizeof(candidate.daylight_name));
        candidate.daylight_offset = candidate.standard_offset;
        *output = candidate;
        return 1;
    }
    if (!rin_time_zone_name(&parser, candidate.daylight_name)) {
        errno = EINVAL;
        return 0;
    }
    candidate.has_daylight = 1;
    candidate.daylight_offset = candidate.standard_offset + 3600L;
    if (text[parser.offset] != ',' && text[parser.offset] != '\0') {
        if (!rin_time_zone_offset(&parser, &candidate.daylight_offset)) {
            errno = EINVAL;
            return 0;
        }
    }
    if (text[parser.offset] == '\0') {
        rin_time_zone_default_rules(&candidate);
        *output = candidate;
        return 1;
    }
    if (!rin_time_zone_consume(&parser, ',') ||
        !rin_time_zone_rule(&parser, &candidate.start) ||
        !rin_time_zone_consume(&parser, ',') ||
        !rin_time_zone_rule(&parser, &candidate.end) ||
        text[parser.offset] != '\0' ||
        candidate.daylight_offset <= -86400L ||
        candidate.daylight_offset >= 86400L) {
        errno = EINVAL;
        return 0;
    }
    *output = candidate;
    return 1;
}

static int rin_time_zone_refresh(void) {
    char text[RIN_TZ_TEXT_MAX + 1u];
    RinTimeZone candidate;
    int saved_errno = errno;
    if (!rin_time_zone_snapshot_environment(text) ||
        !rin_time_zone_parse(text, &candidate)) return 0;
    rin_time_zone_acquire();
    rin_time_zone = candidate;
    rin_time_zone_copy(rin_time_zone_standard_name, candidate.standard_name,
                       sizeof(rin_time_zone_standard_name));
    rin_time_zone_copy(rin_time_zone_daylight_name, candidate.daylight_name,
                       sizeof(rin_time_zone_daylight_name));
    timezone = -candidate.standard_offset;
    daylight = candidate.has_daylight;
    rin_time_zone_release();
    errno = saved_errno;
    return 1;
}

void tzset(void) {
    (void)rin_time_zone_refresh();
}

static int64_t rin_time_floor_divide(int64_t value, int64_t divisor) {
    int64_t quotient = value / divisor;
    int64_t remainder = value % divisor;
    if (remainder < 0) --quotient;
    return quotient;
}

static int64_t rin_time_days_from_civil(int64_t year, unsigned int month,
                                        unsigned int day) {
    int64_t era;
    unsigned int year_of_era;
    unsigned int day_of_year;
    unsigned int day_of_era;
    year -= month <= 2u;
    era = rin_time_floor_divide(year, 400);
    year_of_era = (unsigned int)(year - era * 400);
    day_of_year = (153u * (month > 2u ? month - 3u : month + 9u) + 2u) /
                      5u +
                  day - 1u;
    day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u +
                 day_of_year;
    return era * 146097 + (int64_t)day_of_era - 719468;
}

static void rin_time_civil_from_days(int64_t days, int64_t* year,
                                     unsigned int* month,
                                     unsigned int* day) {
    int64_t era;
    unsigned int day_of_era;
    unsigned int year_of_era;
    int64_t candidate_year;
    unsigned int day_of_year;
    unsigned int month_prime;
    days += 719468;
    era = rin_time_floor_divide(days, 146097);
    day_of_era = (unsigned int)(days - era * 146097);
    year_of_era = (day_of_era - day_of_era / 1460u + day_of_era / 36524u -
                   day_of_era / 146096u) /
                  365u;
    candidate_year = (int64_t)year_of_era + era * 400;
    day_of_year = day_of_era -
                  (365u * year_of_era + year_of_era / 4u -
                   year_of_era / 100u);
    month_prime = (5u * day_of_year + 2u) / 153u;
    *day = day_of_year - (153u * month_prime + 2u) / 5u + 1u;
    *month = month_prime < 10u ? month_prime + 3u : month_prime - 9u;
    *year = candidate_year + (*month <= 2u);
}

static int rin_time_epoch_value(int64_t value, time_t* output) {
    time_t candidate = (time_t)value;
    if ((int64_t)candidate != value) {
        errno = EOVERFLOW;
        return 0;
    }
    *output = candidate;
    return 1;
}

static int rin_time_split(time_t epoch, long offset, int is_daylight,
                          const char* zone, struct tm* output) {
    int64_t value = (int64_t)epoch;
    int64_t days;
    int64_t seconds;
    int64_t year;
    unsigned int month;
    unsigned int day;
    int64_t year_start;
    if ((offset > 0 && value > INT64_MAX - offset) ||
        (offset < 0 && value < INT64_MIN - offset)) {
        errno = EOVERFLOW;
        return 0;
    }
    value += offset;
    days = rin_time_floor_divide(value, 86400);
    seconds = value - days * 86400;
    rin_time_civil_from_days(days, &year, &month, &day);
    if (year - 1900 < INT_MIN || year - 1900 > INT_MAX) {
        errno = EOVERFLOW;
        return 0;
    }
    year_start = rin_time_days_from_civil(year, 1u, 1u);
    output->tm_sec = (int)(seconds % 60);
    seconds /= 60;
    output->tm_min = (int)(seconds % 60);
    output->tm_hour = (int)(seconds / 60);
    output->tm_mday = (int)day;
    output->tm_mon = (int)month - 1;
    output->tm_year = (int)(year - 1900);
    output->tm_wday = (int)((days + 4) % 7);
    if (output->tm_wday < 0) output->tm_wday += 7;
    output->tm_yday = (int)(days - year_start);
    output->tm_isdst = is_daylight;
    output->tm_gmtoff = offset;
    output->tm_zone = zone;
    return 1;
}

static unsigned int rin_time_days_in_month(int64_t year, unsigned int month) {
    static const unsigned char lengths[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    unsigned int result = lengths[month - 1u];
    if (month == 2u && year % 4 == 0 &&
        (year % 100 != 0 || year % 400 == 0)) ++result;
    return result;
}

static int rin_time_is_leap_year(int64_t year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int rin_time_rule_local_seconds(int64_t year,
                                       const RinTimeZoneRule* rule,
                                       int64_t* output) {
    int64_t days;
    if (rule->kind == RIN_TIME_ZONE_RULE_MONTH_WEEKDAY) {
        int64_t first_day = rin_time_days_from_civil(year, rule->month, 1u);
        int first_weekday = (int)((first_day + 4) % 7);
        unsigned int day;
        if (first_weekday < 0) first_weekday += 7;
        day = 1u +
              (unsigned int)((int)rule->weekday - first_weekday + 7) % 7u +
              (rule->week - 1u) * 7u;
        if (day > rin_time_days_in_month(year, rule->month)) day -= 7u;
        days = rin_time_days_from_civil(year, rule->month, day);
    } else if (rule->kind == RIN_TIME_ZONE_RULE_JULIAN_NO_LEAP) {
        unsigned int day = rule->day - 1u;
        if (rin_time_is_leap_year(year) && day >= 59u) ++day;
        days = rin_time_days_from_civil(year, 1u, 1u) + (int64_t)day;
    } else if (rule->kind == RIN_TIME_ZONE_RULE_DAY_OF_YEAR) {
        days = rin_time_days_from_civil(year, 1u, 1u) + (int64_t)rule->day;
    } else {
        errno = EINVAL;
        return 0;
    }
    *output = days * 86400 + rule->seconds;
    return 1;
}

static int rin_time_zone_transition_offset(const RinTimeZone* zone,
                                           const RinTimeZoneRule* rule,
                                           int is_start, long* output) {
    if (rule->time_basis == RIN_TIME_ZONE_RULE_TIME_WALL) {
        *output = is_start ? zone->standard_offset : zone->daylight_offset;
        return 1;
    }
    if (rule->time_basis == RIN_TIME_ZONE_RULE_TIME_STANDARD) {
        *output = zone->standard_offset;
        return 1;
    }
    if (rule->time_basis == RIN_TIME_ZONE_RULE_TIME_UTC) {
        *output = 0;
        return 1;
    }
    errno = EINVAL;
    return 0;
}

static int rin_time_zone_is_daylight(const RinTimeZone* zone, time_t epoch,
                                     int* output) {
    struct tm standard;
    int64_t year;
    int64_t latest_transition = 0;
    int64_t value = (int64_t)epoch;
    int latest_is_start = 0;
    int have_transition = 0;
    int delta;
    if (!zone->has_daylight) {
        *output = 0;
        return 1;
    }
    if (!rin_time_split(epoch, zone->standard_offset, 0,
                        rin_time_zone_standard_name, &standard)) return 0;
    year = (int64_t)standard.tm_year + 1900;
    /* n365 can land in the next calendar year, so inspect adjacent seasons. */
    for (delta = -1; delta <= 1; ++delta) {
        int64_t candidate_year = year + (int64_t)delta;
        int64_t start;
        int64_t end;
        long start_offset;
        long end_offset;
        if (!rin_time_rule_local_seconds(candidate_year, &zone->start,
                                         &start) ||
            !rin_time_rule_local_seconds(candidate_year, &zone->end, &end))
            return 0;
        if (!rin_time_zone_transition_offset(zone, &zone->start, 1,
                                             &start_offset) ||
            !rin_time_zone_transition_offset(zone, &zone->end, 0,
                                             &end_offset))
            return 0;
        start -= start_offset;
        end -= end_offset;
        if (start <= value &&
            (!have_transition || start >= latest_transition)) {
            latest_transition = start;
            latest_is_start = 1;
            have_transition = 1;
        }
        if (end <= value &&
            (!have_transition || end > latest_transition)) {
            latest_transition = end;
            latest_is_start = 0;
            have_transition = 1;
        }
    }
    *output = have_transition && latest_is_start;
    return 1;
}

static int rin_time_local_with_zone(const RinTimeZone* zone, time_t epoch,
                                    struct tm* output) {
    int is_daylight = 0;
    long offset;
    const char* zone_name;
    if (zone && zone->remote) {
        int offset_minutes = 0;
        if (!rin_system_timezone_offset ||
            !rin_system_timezone_offset(zone->remote_id, (int64_t)epoch,
                                        &offset_minutes, &is_daylight))
            return 0;
        offset = (long)offset_minutes * 60L;
        zone_name = is_daylight ? zone->daylight_name : zone->standard_name;
        return rin_time_split(epoch, offset, is_daylight, zone_name, output);
    }
    if (!rin_time_zone_is_daylight(zone, epoch, &is_daylight)) return 0;
    return rin_time_split(epoch,
                          is_daylight ? zone->daylight_offset
                                      : zone->standard_offset,
                          is_daylight,
                          is_daylight ? rin_time_zone_daylight_name
                                      : rin_time_zone_standard_name,
                          output);
}

static int rin_time_zone_snapshot(RinTimeZone* output) {
    if (!rin_time_zone_refresh()) return 0;
    rin_time_zone_acquire();
    *output = rin_time_zone;
    rin_time_zone_release();
    return 1;
}

struct tm* gmtime_r(const time_t* timer, struct tm* result) {
    int saved_errno = errno;
    if (!timer || !result) {
        errno = EINVAL;
        return NULL;
    }
    if (!rin_time_split(*timer, 0, 0, "UTC", result)) return NULL;
    errno = saved_errno;
    return result;
}

struct tm* gmtime(const time_t* timer) {
    return gmtime_r(timer, &rin_time_zone_tm_buffer);
}

struct tm* localtime_r(const time_t* timer, struct tm* result) {
    RinTimeZone zone;
    int saved_errno = errno;
    if (!timer || !result) {
        errno = EINVAL;
        return NULL;
    }
    if (!rin_time_zone_snapshot(&zone) ||
        !rin_time_local_with_zone(&zone, *timer, result)) return NULL;
    errno = saved_errno;
    return result;
}

struct tm* localtime(const time_t* timer) {
    return localtime_r(timer, &rin_time_zone_tm_buffer);
}

static int rin_time_wall_seconds(const struct tm* input, int64_t* output,
                                 struct tm* normalized) {
    int64_t year = (int64_t)input->tm_year + 1900;
    int64_t month = input->tm_mon;
    int64_t month_year = rin_time_floor_divide(month, 12);
    int64_t days;
    int64_t seconds;
    time_t wall_epoch;
    year += month_year;
    month -= month_year * 12;
    days = rin_time_days_from_civil(year, (unsigned int)month + 1u, 1u) +
           (int64_t)input->tm_mday - 1;
    seconds = days * 86400 + (int64_t)input->tm_hour * 3600 +
              (int64_t)input->tm_min * 60 + input->tm_sec;
    if (!rin_time_epoch_value(seconds, &wall_epoch) ||
        !rin_time_split(wall_epoch, 0, 0, "UTC", normalized)) return 0;
    *output = seconds;
    return 1;
}

static int rin_time_same_wall(const struct tm* left, const struct tm* right) {
    return left->tm_sec == right->tm_sec && left->tm_min == right->tm_min &&
           left->tm_hour == right->tm_hour && left->tm_mday == right->tm_mday &&
           left->tm_mon == right->tm_mon && left->tm_year == right->tm_year;
}

static int rin_time_mktime_candidate(const RinTimeZone* zone, int64_t wall,
                                     const struct tm* normalized, long offset,
                                     int expected_daylight, time_t* epoch,
                                     struct tm* converted,
                                     int* conversion_succeeded) {
    int64_t value = wall - offset;
    *conversion_succeeded = 0;
    if (!rin_time_epoch_value(value, epoch) ||
        !rin_time_local_with_zone(zone, *epoch, converted)) return 0;
    *conversion_succeeded = 1;
    return converted->tm_isdst == expected_daylight &&
           rin_time_same_wall(converted, normalized);
}

static int rin_time_mktime_spring_gap(int standard_converted,
                                      const struct tm* standard_result,
                                      time_t standard_epoch,
                                      int daylight_converted,
                                      const struct tm* daylight_result,
                                      time_t daylight_epoch) {
    return standard_converted && daylight_converted &&
           standard_result->tm_isdst > 0 && daylight_result->tm_isdst == 0 &&
           standard_epoch > daylight_epoch;
}

typedef struct {
    time_t epoch;
    struct tm value;
    int valid;
} RinRemoteMktimeCandidate;

static int rin_time_mktime_remote(const RinTimeZone* zone, int64_t wall,
                                  const struct tm* normalized,
                                  const struct tm* original,
                                  time_t* output_epoch,
                                  struct tm* output_value) {
    static const int64_t sample_deltas[] = {
        -172800, -86400, -21600, -3600, 0, 3600, 21600, 86400, 172800
    };
    long offsets[16];
    size_t offset_count = 0u;
    RinRemoteMktimeCandidate standard = {0};
    RinRemoteMktimeCandidate daylight = {0};
    RinRemoteMktimeCandidate gap = {0};
    int64_t sample_epoch;
    size_t index;
    if (!zone || !zone->remote || !normalized || !original ||
        !output_epoch || !output_value || !rin_system_timezone_offset)
        return 0;
    for (index = 0u; index < sizeof(sample_deltas) / sizeof(sample_deltas[0]); ++index) {
        int offset_minutes = 0;
        int in_dst = 0;
        sample_epoch = wall + sample_deltas[index];
        if (!rin_system_timezone_offset(zone->remote_id, sample_epoch,
                                        &offset_minutes, &in_dst))
            continue;
        for (size_t offset_index = 0u; offset_index < offset_count; ++offset_index)
            if (offsets[offset_index] == (long)offset_minutes * 60L)
                goto next_sample;
        if (offset_count < sizeof(offsets) / sizeof(offsets[0]))
            offsets[offset_count++] = (long)offset_minutes * 60L;
next_sample:
        ;
    }
    for (index = 0u; index < offset_count; ++index) {
        int64_t candidate_seconds = wall - (int64_t)offsets[index];
        int actual_offset_minutes = 0;
        int in_dst = 0;
        time_t candidate_epoch;
        struct tm converted;
        struct tm converted_normalized;
        if (!rin_time_epoch_value(candidate_seconds, &candidate_epoch) ||
            !rin_system_timezone_offset(zone->remote_id, candidate_seconds,
                                        &actual_offset_minutes, &in_dst) ||
            actual_offset_minutes * 60L != offsets[index] ||
            !rin_time_split(candidate_epoch, (long)actual_offset_minutes * 60L,
                            in_dst, in_dst ? zone->daylight_name : zone->standard_name,
                            &converted))
            continue;
        if (rin_time_same_wall(&converted, normalized)) {
            if (in_dst) {
                if (!daylight.valid) {
                    daylight.epoch = candidate_epoch;
                    daylight.value = converted;
                    daylight.valid = 1;
                }
            } else if (!standard.valid) {
                standard.epoch = candidate_epoch;
                standard.value = converted;
                standard.valid = 1;
            }
        } else if (!gap.valid) {
            int64_t converted_wall;
            if (rin_time_wall_seconds(&converted, &converted_wall,
                                      &converted_normalized) &&
                converted_wall >= wall && converted_wall - wall <= 7200LL &&
                (original->tm_isdst < 0 || original->tm_isdst == in_dst)) {
                gap.epoch = candidate_epoch;
                gap.value = converted;
                gap.valid = 1;
            }
        }
    }
    if (original->tm_isdst > 0 && daylight.valid) {
        *output_epoch = daylight.epoch;
        *output_value = daylight.value;
        return 1;
    }
    if (original->tm_isdst == 0 && standard.valid) {
        *output_epoch = standard.epoch;
        *output_value = standard.value;
        return 1;
    }
    if (original->tm_isdst < 0 && standard.valid) {
        *output_epoch = standard.epoch;
        *output_value = standard.value;
        return 1;
    }
    if (original->tm_isdst < 0 && daylight.valid) {
        *output_epoch = daylight.epoch;
        *output_value = daylight.value;
        return 1;
    }
    if (gap.valid) {
        *output_epoch = gap.epoch;
        *output_value = gap.value;
        return 1;
    }
    return 0;
}

time_t timegm(struct tm* value) {
    struct tm normalized;
    int64_t seconds;
    time_t result;
    int saved_errno = errno;
    if (!value) {
        errno = EINVAL;
        return (time_t)-1;
    }
    if (!rin_time_wall_seconds(value, &seconds, &normalized) ||
        !rin_time_epoch_value(seconds, &result)) return (time_t)-1;
    *value = normalized;
    errno = saved_errno;
    return result;
}

time_t mktime(struct tm* value) {
    RinTimeZone zone;
    struct tm original;
    struct tm normalized;
    struct tm standard_result;
    struct tm daylight_result;
    int64_t wall;
    time_t standard_epoch = (time_t)-1;
    time_t daylight_epoch = (time_t)-1;
    int standard_converted = 0;
    int daylight_converted = 0;
    int spring_gap;
    int standard_valid;
    int daylight_valid = 0;
    int saved_errno = errno;
    if (!value) {
        errno = EINVAL;
        return (time_t)-1;
    }
    original = *value;
    if (!rin_time_zone_snapshot(&zone) ||
        !rin_time_wall_seconds(&original, &wall, &normalized)) return (time_t)-1;
    if (zone.remote) {
        if (!rin_time_mktime_remote(&zone, wall, &normalized, &original,
                                    &standard_epoch, &standard_result)) {
            errno = EINVAL;
            return (time_t)-1;
        }
        *value = standard_result;
        errno = saved_errno;
        return standard_epoch;
    }
    standard_valid = rin_time_mktime_candidate(
        &zone, wall, &normalized, zone.standard_offset, 0,
        &standard_epoch, &standard_result, &standard_converted);
    if (zone.has_daylight)
        daylight_valid = rin_time_mktime_candidate(
            &zone, wall, &normalized, zone.daylight_offset, 1,
            &daylight_epoch, &daylight_result, &daylight_converted);
    spring_gap = rin_time_mktime_spring_gap(
        standard_converted, &standard_result, standard_epoch,
        daylight_converted, &daylight_result, daylight_epoch);

    if (original.tm_isdst > 0) {
        if (daylight_valid) {
            *value = daylight_result;
            errno = saved_errno;
            return daylight_epoch;
        }
        if (spring_gap) {
            *value = standard_result;
            errno = saved_errno;
            return standard_epoch;
        }
        errno = EINVAL;
        return (time_t)-1;
    }
    if (original.tm_isdst == 0 || !zone.has_daylight) {
        if (standard_valid) {
            *value = standard_result;
            errno = saved_errno;
            return standard_epoch;
        }
        if (original.tm_isdst == 0 && spring_gap) {
            *value = daylight_result;
            errno = saved_errno;
            return daylight_epoch;
        }
        errno = EINVAL;
        return (time_t)-1;
    }
    if (standard_valid) {
        *value = standard_result;
        errno = saved_errno;
        return standard_epoch;
    }
    if (daylight_valid) {
        *value = daylight_result;
        errno = saved_errno;
        return daylight_epoch;
    }
    /* For automatic selection, prefer the forward daylight result in a gap. */
    if (spring_gap) {
        *value = standard_result;
        errno = saved_errno;
        return standard_epoch;
    }
    errno = EINVAL;
    return (time_t)-1;
}
