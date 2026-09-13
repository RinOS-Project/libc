/* SPDX-License-Identifier: MIT */

#include "time.h"
#include "../libunicode/rin_unicode.h"

#define RIN_STRFTIME_FORMAT_MAX (1024u * 1024u)
#define RIN_STRFTIME_LOCALE_TEXT_MAX 1024u
#define RIN_STRFTIME_ZONE_MAX 63u
#define RIN_STRFTIME_RECURSION_MAX 4u

typedef struct {
    char* output;
    size_t capacity;
    size_t length;
    int error;
#if defined(RIN_USERSPACE)
    locale_t locale;
#endif
} RinStrftimeWriter;

static const char* rin_strftime_weekday(const RinStrftimeWriter* writer,
                                        int weekday, int abbreviated) {
#if defined(RIN_USERSPACE)
    if (!writer) return NULL;
    return writer->locale == LC_GLOBAL_LOCALE
               ? rin_locale_weekday(weekday, abbreviated)
               : rin_locale_l_weekday(writer->locale, weekday, abbreviated);
#else
    (void)writer;
    return _rin_time_weekday(weekday, abbreviated);
#endif
}

static const char* rin_strftime_month(const RinStrftimeWriter* writer,
                                      int month, int abbreviated) {
#if defined(RIN_USERSPACE)
    if (!writer) return NULL;
    return writer->locale == LC_GLOBAL_LOCALE
               ? rin_locale_month(month, abbreviated)
               : rin_locale_l_month(writer->locale, month, abbreviated);
#else
    (void)writer;
    return _rin_time_month(month, abbreviated);
#endif
}

static const char* rin_strftime_am_pm(const RinStrftimeWriter* writer,
                                      int hour) {
#if defined(RIN_USERSPACE)
    if (!writer) return NULL;
    return writer->locale == LC_GLOBAL_LOCALE
               ? rin_locale_am_pm(hour)
               : rin_locale_l_am_pm(writer->locale, hour);
#else
    (void)writer;
    return hour < 12 ? "AM" : "PM";
#endif
}

static const char* rin_strftime_time_format(const RinStrftimeWriter* writer,
                                            char conversion) {
#if defined(RIN_USERSPACE)
    if (!writer) return NULL;
    return writer->locale == LC_GLOBAL_LOCALE
               ? rin_locale_time_format(conversion)
               : rin_locale_l_time_format(writer->locale, conversion);
#else
    (void)writer;
    switch (conversion) {
        case 'c': return "%a %b %e %H:%M:%S %Y";
        case 'x': return "%m/%d/%y";
        case 'X': return "%H:%M:%S";
        default: return NULL;
    }
#endif
}

/* Hosted CRT headers annotate the format parameter as nonnull even though
 * Rin's bounded formatter deliberately reports EINVAL for a NULL format.
 * Keep that contract while hiding the comparison from -Wnonnull-compare. */
__attribute__((noinline)) static int rin_strftime_pointer_is_null(
    const void* pointer) {
    return pointer == NULL;
}

static int rin_strftime_bounded_length(const char* text, size_t maximum,
                                       size_t* length) {
    size_t index = 0u;
    if (!text || !length) return 0;
    while (index < maximum && text[index] != '\0') ++index;
    if (index == maximum) return 0;
    *length = index;
    return 1;
}

static void rin_strftime_character(RinStrftimeWriter* writer, char value) {
    if (writer->error != 0) return;
    if (writer->length == SIZE_MAX) {
        writer->error = EOVERFLOW;
        return;
    }
    if (writer->output) {
        if (writer->length >= writer->capacity) {
            writer->error = EOVERFLOW;
            return;
        }
        writer->output[writer->length] = value;
    }
    ++writer->length;
}

static void rin_strftime_text(RinStrftimeWriter* writer, const char* text,
                              size_t maximum) {
    size_t length = 0u;
    size_t valid_prefix = 0u;
    if (writer->error != 0) return;
    if (!rin_strftime_bounded_length(text, maximum, &length)) {
        writer->error = EOVERFLOW;
        return;
    }
    if (!rin_unicode_validate_utf8(text, length, &valid_prefix) ||
        valid_prefix != length) {
        writer->error = EILSEQ;
        return;
    }
    for (size_t index = 0u; index < length; ++index)
        rin_strftime_character(writer, text[index]);
}

static void rin_strftime_number(RinStrftimeWriter* writer,
                                unsigned int value, unsigned int width,
                                char padding) {
    char digits[10];
    unsigned int count = 0u;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count < width) {
        rin_strftime_character(writer, padding);
        --width;
    }
    while (count > 0u) rin_strftime_character(writer, digits[--count]);
}

static int rin_strftime_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int rin_strftime_iso_weeks(int year, int january_first_weekday) {
    return january_first_weekday == 4 ||
           (january_first_weekday == 3 && rin_strftime_leap_year(year))
               ? 53
               : 52;
}

static void rin_strftime_iso_fields(const struct tm* value, int* iso_year,
                                    int* iso_week) {
    int year = value->tm_year + 1900;
    int iso_weekday = value->tm_wday == 0 ? 7 : value->tm_wday;
    int week = (value->tm_yday + 1 - iso_weekday + 10) / 7;
    int january_first = value->tm_wday - value->tm_yday % 7;
    if (january_first < 0) january_first += 7;
    if (week < 1) {
        int previous_year = year - 1;
        int previous_january_first =
            january_first - (rin_strftime_leap_year(previous_year) ? 2 : 1);
        if (previous_january_first < 0) previous_january_first += 7;
        *iso_year = previous_year;
        *iso_week =
            rin_strftime_iso_weeks(previous_year, previous_january_first);
    } else if (week > rin_strftime_iso_weeks(year, january_first)) {
        *iso_year = year + 1;
        *iso_week = 1;
    } else {
        *iso_year = year;
        *iso_week = week;
    }
}

static int rin_strftime_tm_valid(const struct tm* value) {
    if (!value) return 0;
    return value->tm_sec >= 0 && value->tm_sec <= 60 &&
           value->tm_min >= 0 && value->tm_min <= 59 &&
           value->tm_hour >= 0 && value->tm_hour <= 23 &&
           value->tm_mday >= 1 && value->tm_mday <= 31 &&
           value->tm_mon >= 0 && value->tm_mon <= 11 &&
           value->tm_year >= -1900 && value->tm_year <= 8099 &&
           value->tm_wday >= 0 && value->tm_wday <= 6 &&
           value->tm_yday >= 0 && value->tm_yday <= 365 &&
           value->tm_isdst >= -1 && value->tm_isdst <= 1;
}

static int rin_strftime_modifier_valid(char modifier, char conversion) {
    if (modifier == '\0') return 1;
    if (modifier == 'E') {
        return conversion == 'c' || conversion == 'C' || conversion == 'x' ||
               conversion == 'X' || conversion == 'y' || conversion == 'Y';
    }
    if (modifier == 'O') {
        return conversion == 'd' || conversion == 'e' || conversion == 'H' ||
               conversion == 'I' || conversion == 'm' || conversion == 'M' ||
               conversion == 'S' || conversion == 'u' || conversion == 'U' ||
               conversion == 'V' || conversion == 'w' || conversion == 'W' ||
               conversion == 'y';
    }
    return 0;
}

static int rin_strftime_format(RinStrftimeWriter* writer, const char* format,
                               const struct tm* value, unsigned int depth);

static void rin_strftime_composite(RinStrftimeWriter* writer,
                                   const char* format,
                                   const struct tm* value,
                                   unsigned int depth) {
    if (depth >= RIN_STRFTIME_RECURSION_MAX ||
        !rin_strftime_format(writer, format, value, depth + 1u)) {
        if (writer->error == 0) writer->error = EINVAL;
    }
}

static void rin_strftime_offset(RinStrftimeWriter* writer,
                                const struct tm* value) {
    long offset = value->tm_gmtoff;
    unsigned long absolute;
    unsigned int minutes;
    if (offset <= -86400L || offset >= 86400L || offset % 60L != 0L) {
        writer->error = EINVAL;
        return;
    }
    rin_strftime_character(writer, offset < 0L ? '-' : '+');
    absolute = (unsigned long)(offset < 0L ? -offset : offset);
    minutes = (unsigned int)(absolute / 60u);
    rin_strftime_number(writer, minutes / 60u, 2u, '0');
    rin_strftime_number(writer, minutes % 60u, 2u, '0');
}

static int rin_strftime_format(RinStrftimeWriter* writer, const char* format,
                               const struct tm* value, unsigned int depth) {
    size_t format_length = 0u;
    size_t valid_prefix = 0u;
    size_t maximum = depth == 0u ? RIN_STRFTIME_FORMAT_MAX
                                 : RIN_STRFTIME_LOCALE_TEXT_MAX;
    int year = value->tm_year + 1900;
    if (!rin_strftime_bounded_length(format, maximum, &format_length)) {
        writer->error = EOVERFLOW;
        return 0;
    }
    if (!rin_unicode_validate_utf8(format, format_length, &valid_prefix) ||
        valid_prefix != format_length) {
        writer->error = EILSEQ;
        return 0;
    }

    while (writer->error == 0 && *format != '\0') {
        char modifier = '\0';
        char conversion;
        int iso_year;
        int iso_week;
        if (*format != '%') {
            rin_strftime_character(writer, *format++);
            continue;
        }
        ++format;
        if (*format == 'E' || *format == 'O') modifier = *format++;
        conversion = *format++;
        if (conversion == '\0' ||
            !rin_strftime_modifier_valid(modifier, conversion)) {
            writer->error = EINVAL;
            break;
        }

        switch (conversion) {
            case 'a':
                rin_strftime_text(writer,
                                  rin_strftime_weekday(writer, value->tm_wday, 1),
                                  RIN_STRFTIME_LOCALE_TEXT_MAX);
                break;
            case 'A':
                rin_strftime_text(writer,
                                  rin_strftime_weekday(writer, value->tm_wday, 0),
                                  RIN_STRFTIME_LOCALE_TEXT_MAX);
                break;
            case 'b':
            case 'h':
                rin_strftime_text(writer,
                                  rin_strftime_month(writer, value->tm_mon, 1),
                                  RIN_STRFTIME_LOCALE_TEXT_MAX);
                break;
            case 'B':
                rin_strftime_text(writer,
                                  rin_strftime_month(writer, value->tm_mon, 0),
                                  RIN_STRFTIME_LOCALE_TEXT_MAX);
                break;
            case 'c':
                rin_strftime_composite(writer,
                                       rin_strftime_time_format(writer, 'c'),
                                       value, depth);
                break;
            case 'C':
                rin_strftime_number(writer, (unsigned int)(year / 100), 2u,
                                    '0');
                break;
            case 'd':
                rin_strftime_number(writer, (unsigned int)value->tm_mday, 2u,
                                    '0');
                break;
            case 'D':
                rin_strftime_composite(writer, "%m/%d/%y", value, depth);
                break;
            case 'e':
                rin_strftime_number(writer, (unsigned int)value->tm_mday, 2u,
                                    ' ');
                break;
            case 'F':
                rin_strftime_composite(writer, "%Y-%m-%d", value, depth);
                break;
            case 'g':
                rin_strftime_iso_fields(value, &iso_year, &iso_week);
                rin_strftime_number(writer, (unsigned int)(iso_year % 100),
                                    2u, '0');
                break;
            case 'G':
                rin_strftime_iso_fields(value, &iso_year, &iso_week);
                rin_strftime_number(writer, (unsigned int)iso_year, 4u, '0');
                break;
            case 'H':
                rin_strftime_number(writer, (unsigned int)value->tm_hour, 2u,
                                    '0');
                break;
            case 'I': {
                unsigned int hour = (unsigned int)(value->tm_hour % 12);
                rin_strftime_number(writer, hour == 0u ? 12u : hour, 2u, '0');
                break;
            }
            case 'j':
                rin_strftime_number(writer, (unsigned int)value->tm_yday + 1u,
                                    3u, '0');
                break;
            case 'm':
                rin_strftime_number(writer, (unsigned int)value->tm_mon + 1u,
                                    2u, '0');
                break;
            case 'M':
                rin_strftime_number(writer, (unsigned int)value->tm_min, 2u,
                                    '0');
                break;
            case 'n': rin_strftime_character(writer, '\n'); break;
            case 'p':
                rin_strftime_text(writer,
                                  rin_strftime_am_pm(writer, value->tm_hour),
                                  RIN_STRFTIME_LOCALE_TEXT_MAX);
                break;
            case 'r':
                rin_strftime_composite(writer, "%I:%M:%S %p", value, depth);
                break;
            case 'R':
                rin_strftime_composite(writer, "%H:%M", value, depth);
                break;
            case 'S':
                rin_strftime_number(writer, (unsigned int)value->tm_sec, 2u,
                                    '0');
                break;
            case 't': rin_strftime_character(writer, '\t'); break;
            case 'T':
                rin_strftime_composite(writer, "%H:%M:%S", value, depth);
                break;
            case 'u':
                rin_strftime_number(
                    writer,
                    (unsigned int)(value->tm_wday == 0 ? 7 : value->tm_wday),
                    1u, '0');
                break;
            case 'U':
                rin_strftime_number(
                    writer,
                    (unsigned int)((value->tm_yday + 7 - value->tm_wday) / 7),
                    2u, '0');
                break;
            case 'V':
                rin_strftime_iso_fields(value, &iso_year, &iso_week);
                rin_strftime_number(writer, (unsigned int)iso_week, 2u, '0');
                break;
            case 'w':
                rin_strftime_number(writer, (unsigned int)value->tm_wday, 1u,
                                    '0');
                break;
            case 'W':
                rin_strftime_number(
                    writer,
                    (unsigned int)((value->tm_yday + 7 -
                                    ((value->tm_wday + 6) % 7)) /
                                   7),
                    2u, '0');
                break;
            case 'x':
                rin_strftime_composite(writer,
                                       rin_strftime_time_format(writer, 'x'),
                                       value, depth);
                break;
            case 'X':
                rin_strftime_composite(writer,
                                       rin_strftime_time_format(writer, 'X'),
                                       value, depth);
                break;
            case 'y':
                rin_strftime_number(writer, (unsigned int)(year % 100), 2u,
                                    '0');
                break;
            case 'Y':
                rin_strftime_number(writer, (unsigned int)year, 4u, '0');
                break;
            case 'z': rin_strftime_offset(writer, value); break;
            case 'Z':
                rin_strftime_text(
                    writer,
                    value->tm_zone && value->tm_zone[0] != '\0'
                        ? value->tm_zone
                        : "UTC",
                    RIN_STRFTIME_ZONE_MAX + 1u);
                break;
            case '%': rin_strftime_character(writer, '%'); break;
            default: writer->error = EINVAL; break;
        }
    }
    return writer->error == 0;
}

static size_t rin_strftime_run(char* destination, size_t maximum,
                               const char* format, const struct tm* value
#if defined(RIN_USERSPACE)
                               , locale_t locale
#endif
                               ) {
    RinStrftimeWriter sizing = {0};
    RinStrftimeWriter output;
    int saved_errno = errno;
#if defined(RIN_USERSPACE)
    sizing.locale = locale;
#endif
    if (!destination || rin_strftime_pointer_is_null(format) ||
        !rin_strftime_tm_valid(value)) {
        errno = EINVAL;
        return 0u;
    }
    if (!rin_strftime_format(&sizing, format, value, 0u)) {
        errno = sizing.error;
        return 0u;
    }
    if (maximum == 0u || sizing.length >= maximum) {
        errno = ERANGE;
        return 0u;
    }

    output = sizing;
    output.output = destination;
    output.capacity = sizing.length;
    output.length = 0u;
    output.error = 0;
    if (!rin_strftime_format(&output, format, value, 0u) ||
        output.length != sizing.length) {
        destination[0] = '\0';
        errno = output.error != 0 ? output.error : EIO;
        return 0u;
    }
    destination[output.length] = '\0';
    errno = saved_errno;
    return output.length;
}

size_t strftime(char* destination, size_t maximum, const char* format,
                const struct tm* value) {
#if defined(RIN_USERSPACE)
    return rin_strftime_run(destination, maximum, format, value,
                            LC_GLOBAL_LOCALE);
#else
    return rin_strftime_run(destination, maximum, format, value);
#endif
}

#if defined(RIN_USERSPACE)
size_t strftime_l(char* destination, size_t maximum, const char* format,
                  const struct tm* value, locale_t locale) {
    if (!locale) {
        errno = EINVAL;
        return 0u;
    }
    return rin_strftime_run(destination, maximum, format, value, locale);
}
#endif
