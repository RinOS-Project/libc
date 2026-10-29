/* SPDX-License-Identifier: MIT */
/* RinOS libc - bounded POSIX regular-expression subset. */

#ifndef _REGEX_H
#define _REGEX_H

#include "stddef.h"
#include "stdint.h"
#include "stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long regoff_t;
typedef struct { size_t re_nsub; void* _internal; } regex_t;
typedef struct { regoff_t rm_so; regoff_t rm_eo; } regmatch_t;

#define REG_EXTENDED 0x01
#define REG_ICASE 0x02
#define REG_NOSUB 0x04
#define REG_NEWLINE 0x08
#define REG_NOTBOL 0x01
#define REG_NOTEOL 0x02
#define REG_STARTEND 0x04

#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ECOLLATE 3
#define REG_ECTYPE 4
#define REG_EESCAPE 5
#define REG_ESUBREG 6
#define REG_EBRACK 7
#define REG_EPAREN 8
#define REG_EBRACE 9
#define REG_BADBR 10
#define REG_ERANGE 11
#define REG_ESPACE 12
#define REG_BADRPT 13
#define REG_ENOSYS 14
#define REG_INVARG 15

#define _REGEX_MAGIC UINT32_C(0x31455852)
#define _REGEX_MAX_PATTERN 4096u
#define _REGEX_MAX_REPEAT 4096u
#define _REGEX_MAX_INSTRUCTIONS 16384u
#define _REGEX_MAX_INPUT (1024u * 1024u)
#define _REGEX_MAX_STEPS UINT64_C(67108864)
#define _REGEX_MAX_GROUP_DEPTH 64u
#define _REGEX_MAX_GROUPS 64u
#define _REGEX_MAX_BACKREF_DEPTH 256u
#define _REGEX_MAX_BACKREF_CAPTURE 4096u

#define _REGEX_OP_BYTE 1u
#define _REGEX_OP_ANY 2u
#define _REGEX_OP_CLASS 3u
#define _REGEX_OP_SPLIT 4u
#define _REGEX_OP_JUMP 5u
#define _REGEX_OP_BOL 6u
#define _REGEX_OP_EOL 7u
#define _REGEX_OP_MATCH 8u
#define _REGEX_OP_SAVE_START 9u
#define _REGEX_OP_SAVE_END 10u
#define _REGEX_OP_BACKREF 11u

/* Compile-only flag.  It is never stored in regex_t; it requests capture
 * markers even for REG_NOSUB programs so that a backreference can still be
 * evaluated while keeping the public match array suppressed. */
#define _REGEX_COMPILE_CAPTURE 0x80000000

typedef struct {
    uint8_t operation;
    uint8_t value;
    uint16_t reserved;
    uint32_t first;
    uint32_t second;
    uint8_t bitmap[32];
} _regex_instruction_t;

typedef struct {
    uint32_t magic;
    uint32_t instruction_count;
    uint32_t group_count;
    size_t allocation_size;
    int cflags;
    _regex_instruction_t* instructions;
} _regex_internal_t;

static inline void _regex_zero(void* pointer, size_t size) {
    unsigned char* output = (unsigned char*)pointer;
    while (size-- != 0u) *output++ = 0u;
}

static inline int _regex_all_zero(const void* pointer, size_t size) {
    const unsigned char* input = (const unsigned char*)pointer;
    unsigned char combined = 0u;
    while (size-- != 0u) combined |= *input++;
    return combined == 0u;
}

static inline char _regex_ascii_fold(char value) {
    if (value >= 'A' && value <= 'Z') return (char)(value + ('a' - 'A'));
    return value;
}

static inline void _regex_bitmap_add(uint8_t bitmap[32], uint8_t value) {
    bitmap[value >> 3u] |= (uint8_t)(1u << (value & 7u));
}

static inline int _regex_bitmap_has(const uint8_t bitmap[32], uint8_t value) {
    return (bitmap[value >> 3u] & (uint8_t)(1u << (value & 7u))) != 0u;
}

static inline int _regex_name_equal(const char* begin, size_t size,
                                    const char* expected) {
    size_t index = 0u;
    while (index < size && expected[index] != '\0') {
        if (begin[index] != expected[index]) return 0;
        ++index;
    }
    return index == size && expected[index] == '\0';
}

static inline int _regex_ascii_class(uint8_t bitmap[32],
                                     const char* name, size_t size) {
    uint32_t value;
    int recognized = 1;
    for (value = 0u; value < 256u; ++value) {
        int selected = 0;
        unsigned char c = (unsigned char)value;
        if (_regex_name_equal(name, size, "alnum"))
            selected = (c >= '0' && c <= '9') ||
                       (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z');
        else if (_regex_name_equal(name, size, "alpha"))
            selected = (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z');
        else if (_regex_name_equal(name, size, "blank"))
            selected = c == ' ' || c == '\t';
        else if (_regex_name_equal(name, size, "cntrl"))
            selected = c < 0x20u || c == 0x7fu;
        else if (_regex_name_equal(name, size, "digit"))
            selected = c >= '0' && c <= '9';
        else if (_regex_name_equal(name, size, "graph"))
            selected = c >= 0x21u && c <= 0x7eu;
        else if (_regex_name_equal(name, size, "lower"))
            selected = c >= 'a' && c <= 'z';
        else if (_regex_name_equal(name, size, "print"))
            selected = c >= 0x20u && c <= 0x7eu;
        else if (_regex_name_equal(name, size, "punct"))
            selected = c >= 0x21u && c <= 0x7eu &&
                       !((c >= '0' && c <= '9') ||
                         (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z'));
        else if (_regex_name_equal(name, size, "space"))
            selected = c == ' ' || (c >= '\t' && c <= '\r');
        else if (_regex_name_equal(name, size, "upper"))
            selected = c >= 'A' && c <= 'Z';
        else if (_regex_name_equal(name, size, "xdigit"))
            selected = (c >= '0' && c <= '9') ||
                       (c >= 'A' && c <= 'F') ||
                       (c >= 'a' && c <= 'f');
        else { recognized = 0; break; }
        if (selected) _regex_bitmap_add(bitmap, c);
    }
    return recognized;
}

static inline int _regex_parse_class(const char* pattern,
                                     size_t pattern_size, size_t* offset,
                                     _regex_instruction_t* atom,
                                     int cflags);

static inline int _regex_pattern_has_backref(const char* pattern,
                                             size_t pattern_size,
                                             int cflags) {
    size_t offset = 0u;
    while (offset < pattern_size) {
        if (pattern[offset] == '[') {
            _regex_instruction_t ignored;
            size_t next = offset;
            if (_regex_parse_class(pattern, pattern_size, &next, &ignored,
                                   cflags) == 0) {
                offset = next;
                continue;
            }
        }
        if (pattern[offset] == '\\' && offset + 1u < pattern_size) {
            if (pattern[offset + 1u] >= '0' &&
                pattern[offset + 1u] <= '9') return 1;
            offset += 2u;
            continue;
        }
        ++offset;
    }
    return 0;
}

#define _REGEX_BRACKET_BYTE 0
#define _REGEX_BRACKET_COLLATING 1
#define _REGEX_BRACKET_EQUIVALENCE 2

static inline int _regex_parse_bracket_element(
        const char* pattern, size_t pattern_size, size_t* offset,
        uint8_t* value_out, int* kind_out) {
    size_t current = *offset;
    if (current < pattern_size && pattern[current] == '[' &&
        current + 1u < pattern_size &&
        (pattern[current + 1u] == '.' || pattern[current + 1u] == '=')) {
        char delimiter = pattern[current + 1u];
        size_t element_begin = current + 2u;
        size_t element_end = element_begin;
        while (element_end + 1u < pattern_size &&
               !(pattern[element_end] == delimiter &&
                 pattern[element_end + 1u] == ']'))
            ++element_end;
        if (element_end + 1u >= pattern_size ||
            element_end != element_begin + 1u)
            return REG_ECOLLATE;
        *value_out = (uint8_t)pattern[element_begin];
        *kind_out = delimiter == '.' ? _REGEX_BRACKET_COLLATING :
                                       _REGEX_BRACKET_EQUIVALENCE;
        *offset = element_end + 2u;
        return 0;
    }
    if (current >= pattern_size) return REG_EBRACK;
    *value_out = (uint8_t)pattern[current];
    *kind_out = _REGEX_BRACKET_BYTE;
    *offset = current + 1u;
    return 0;
}

static inline int _regex_parse_class(const char* pattern,
                                     size_t pattern_size, size_t* offset,
                                     _regex_instruction_t* atom,
                                     int cflags) {
    size_t current = *offset + 1u;
    int have_item = 0;
    int negated = 0;
    if (current >= pattern_size) return REG_EBRACK;
    _regex_zero(atom, sizeof(*atom));
    atom->operation = _REGEX_OP_CLASS;
    if (pattern[current] == '^') { negated = 1; ++current; }
    if (current < pattern_size && pattern[current] == ']') {
        _regex_bitmap_add(atom->bitmap, (uint8_t)']');
        have_item = 1;
        ++current;
    }
    while (current < pattern_size && pattern[current] != ']') {
        uint8_t first;
        int first_kind;
        int result;
        if (pattern[current] == '[' && current + 1u < pattern_size &&
            pattern[current + 1u] == ':') {
            size_t name_begin = current + 2u;
            size_t end = name_begin;
            while (end + 1u < pattern_size &&
                   !(pattern[end] == ':' && pattern[end + 1u] == ']')) ++end;
            if (end + 1u >= pattern_size) return REG_EBRACK;
            if (end == name_begin || !_regex_ascii_class(
                    atom->bitmap, pattern + name_begin, end - name_begin))
                return REG_ECTYPE;
            current = end + 2u;
            have_item = 1;
            continue;
        }
        result = _regex_parse_bracket_element(
            pattern, pattern_size, &current, &first, &first_kind);
        if (result != 0) return result;
        if (current < pattern_size && pattern[current] == '-' &&
            current + 1u < pattern_size && pattern[current + 1u] != ']') {
            uint8_t last;
            int last_kind;
            uint32_t value;
            ++current;
            result = _regex_parse_bracket_element(
                pattern, pattern_size, &current, &last, &last_kind);
            if (result != 0) return result;
            if (first_kind == _REGEX_BRACKET_EQUIVALENCE ||
                last_kind == _REGEX_BRACKET_EQUIVALENCE || last == '[' ||
                first > last)
                return REG_ERANGE;
            for (value = first; value <= last; ++value)
                _regex_bitmap_add(atom->bitmap, (uint8_t)value);
        } else {
            _regex_bitmap_add(atom->bitmap, first);
        }
        have_item = 1;
    }
    if (current >= pattern_size || pattern[current] != ']' || !have_item)
        return REG_EBRACK;
    ++current;
    if ((cflags & REG_ICASE) != 0) {
        uint32_t value;
        for (value = 'A'; value <= 'Z'; ++value) {
            uint8_t lower = (uint8_t)(value + ('a' - 'A'));
            if (_regex_bitmap_has(atom->bitmap, (uint8_t)value) ||
                _regex_bitmap_has(atom->bitmap, lower)) {
                _regex_bitmap_add(atom->bitmap, (uint8_t)value);
                _regex_bitmap_add(atom->bitmap, lower);
            }
        }
    }
    atom->value = (uint8_t)negated;
    *offset = current;
    return 0;
}

static inline int _regex_parse_decimal(const char* pattern,
                                       size_t pattern_size, size_t* offset,
                                       uint32_t* value_out) {
    size_t current = *offset;
    uint32_t value = 0u;
    int have_digit = 0;
    while (current < pattern_size && pattern[current] >= '0' &&
           pattern[current] <= '9') {
        uint32_t digit = (uint32_t)(pattern[current] - '0');
        have_digit = 1;
        if (value > (_REGEX_MAX_REPEAT - digit) / 10u) return REG_BADBR;
        value = value * 10u + digit;
        ++current;
    }
    if (!have_digit) return REG_BADBR;
    *offset = current;
    *value_out = value;
    return 0;
}

static inline int _regex_bre_interval_close(const char* pattern,
                                             size_t pattern_size,
                                             size_t offset) {
    return offset + 1u < pattern_size && pattern[offset] == '\\' &&
           pattern[offset + 1u] == '}';
}

static inline int _regex_emit(_regex_instruction_t* instructions,
                              uint32_t capacity, uint32_t* count,
                              const _regex_instruction_t* instruction) {
    if (*count >= _REGEX_MAX_INSTRUCTIONS ||
        (instructions && *count >= capacity)) return REG_ESPACE;
    if (instructions) instructions[*count] = *instruction;
    ++*count;
    return 0;
}

static inline int _regex_emit_repeat(_regex_instruction_t* instructions,
                                     uint32_t capacity, uint32_t* count,
                                     const _regex_instruction_t* atom,
                                     uint32_t minimum, uint32_t maximum) {
    uint32_t index;
    int result;
    for (index = 0u; index < minimum; ++index) {
        result = _regex_emit(instructions, capacity, count, atom);
        if (result != 0) return result;
    }
    if (maximum == UINT32_MAX) {
        _regex_instruction_t split;
        _regex_instruction_t jump;
        uint32_t split_index = *count;
        _regex_zero(&split, sizeof(split));
        split.operation = _REGEX_OP_SPLIT;
        split.first = split_index + 1u;
        split.second = split_index + 3u;
        result = _regex_emit(instructions, capacity, count, &split);
        if (result != 0) return result;
        result = _regex_emit(instructions, capacity, count, atom);
        if (result != 0) return result;
        _regex_zero(&jump, sizeof(jump));
        jump.operation = _REGEX_OP_JUMP;
        jump.first = split_index;
        return _regex_emit(instructions, capacity, count, &jump);
    }
    for (index = minimum; index < maximum; ++index) {
        _regex_instruction_t split;
        uint32_t split_index = *count;
        _regex_zero(&split, sizeof(split));
        split.operation = _REGEX_OP_SPLIT;
        split.first = split_index + 1u;
        split.second = split_index + 2u;
        result = _regex_emit(instructions, capacity, count, &split);
        if (result != 0) return result;
        result = _regex_emit(instructions, capacity, count, atom);
        if (result != 0) return result;
    }
    return 0;
}

static inline int _regex_compile_expression(
        const char* pattern, size_t pattern_size, int cflags,
        _regex_instruction_t* instructions, uint32_t capacity,
        uint32_t* count, uint32_t* group_cursor, uint32_t depth);

static inline int _regex_parse_postfix_repeat(
        const char* pattern, size_t pattern_size, int extended,
        size_t* current, uint32_t* minimum, uint32_t* maximum) {
    size_t repeat;
    int result;
    *minimum = 1u;
    *maximum = 1u;
    if (*current >= pattern_size) return 0;
    if (pattern[*current] == '*') {
        *minimum = 0u;
        *maximum = UINT32_MAX;
        ++*current;
        return 0;
    }
    if (extended && pattern[*current] == '+') {
        *maximum = UINT32_MAX;
        ++*current;
        return 0;
    }
    if (extended && pattern[*current] == '?') {
        *minimum = 0u;
        ++*current;
        return 0;
    }
    /* POSIX BRE spells the one-or-more and optional postfix operators as
     * escaped bytes. Consume them after the atom so a leading \+ / \? is
     * still rejected as a dangling repetition operator. */
    if (!extended && *current + 1u < pattern_size &&
        pattern[*current] == '\\' && pattern[*current + 1u] == '+') {
        *maximum = UINT32_MAX;
        *current += 2u;
        return 0;
    }
    if (!extended && *current + 1u < pattern_size &&
        pattern[*current] == '\\' && pattern[*current + 1u] == '?') {
        *minimum = 0u;
        *current += 2u;
        return 0;
    }
    if (extended && pattern[*current] == '{') {
        repeat = *current + 1u;
        result = _regex_parse_decimal(pattern, pattern_size, &repeat, minimum);
        if (result != 0) return result;
        if (repeat >= pattern_size) return REG_EBRACE;
        if (pattern[repeat] == '}') {
            *maximum = *minimum;
            *current = repeat + 1u;
            return 0;
        }
        if (pattern[repeat] != ',') return REG_BADBR;
        ++repeat;
        if (repeat < pattern_size && pattern[repeat] == '}') {
            *maximum = UINT32_MAX;
            *current = repeat + 1u;
            return 0;
        }
        result = _regex_parse_decimal(pattern, pattern_size, &repeat, maximum);
        if (result != 0) return result;
        if (repeat >= pattern_size || pattern[repeat] != '}')
            return repeat >= pattern_size ? REG_EBRACE : REG_BADBR;
        if (*maximum < *minimum) return REG_BADBR;
        *current = repeat + 1u;
        return 0;
    }
    if (!extended && *current + 1u < pattern_size &&
        pattern[*current] == '\\' && pattern[*current + 1u] == '{') {
        repeat = *current + 2u;
        result = _regex_parse_decimal(pattern, pattern_size, &repeat, minimum);
        if (result != 0) return result;
        if (repeat >= pattern_size) return REG_EBRACE;
        if (_regex_bre_interval_close(pattern, pattern_size, repeat)) {
            *maximum = *minimum;
            *current = repeat + 2u;
            return 0;
        }
        if (pattern[repeat] != ',') {
            if (pattern[repeat] == '\\' && repeat + 1u >= pattern_size)
                return REG_EBRACE;
            return REG_BADBR;
        }
        ++repeat;
        if (_regex_bre_interval_close(pattern, pattern_size, repeat)) {
            *maximum = UINT32_MAX;
            *current = repeat + 2u;
            return 0;
        }
        result = _regex_parse_decimal(pattern, pattern_size, &repeat, maximum);
        if (result != 0) return result;
        if (!_regex_bre_interval_close(pattern, pattern_size, repeat)) {
            if (repeat >= pattern_size ||
                (pattern[repeat] == '\\' && repeat + 1u >= pattern_size))
                return REG_EBRACE;
            return REG_BADBR;
        }
        if (*maximum < *minimum) return REG_BADBR;
        *current = repeat + 2u;
    }
    return 0;
}

static inline int _regex_find_group_end(
        const char* pattern, size_t pattern_size, size_t group_begin,
        int cflags, size_t* group_end) {
    size_t current = group_begin;
    uint32_t nesting = 0u;
    int extended = (cflags & REG_EXTENDED) != 0;
    if (current >= pattern_size ||
        (extended ? pattern[current] != '('
                  : current + 1u >= pattern_size || pattern[current] != '\\' ||
                    pattern[current + 1u] != '('))
        return REG_EPAREN;
    while (current < pattern_size) {
        if (pattern[current] == '\\') {
            if (current + 1u >= pattern_size) return REG_EESCAPE;
            if (!extended && pattern[current + 1u] == '(') {
                if (++nesting > _REGEX_MAX_GROUP_DEPTH) return REG_ESPACE;
            } else if (!extended && pattern[current + 1u] == ')') {
                if (nesting == 0u) return REG_EPAREN;
                --nesting;
                if (nesting == 0u) {
                    *group_end = current;
                    return 0;
                }
            }
            current += 2u;
            continue;
        }
        if (pattern[current] == '[') {
            _regex_instruction_t ignored;
            int result = _regex_parse_class(
                pattern, pattern_size, &current, &ignored, cflags);
            if (result != 0) return result;
            continue;
        }
        if (extended && pattern[current] == '(') {
            if (++nesting > _REGEX_MAX_GROUP_DEPTH) return REG_ESPACE;
        } else if (extended && pattern[current] == ')') {
            if (nesting == 0u) return REG_EPAREN;
            --nesting;
            if (nesting == 0u) {
                *group_end = current;
                return 0;
            }
        }
        ++current;
    }
    return REG_EPAREN;
}

static inline int _regex_emit_expression_repeat(
        const char* pattern, size_t pattern_size, int cflags,
        _regex_instruction_t* instructions, uint32_t capacity,
        uint32_t* count, uint32_t minimum, uint32_t maximum,
        uint32_t* group_cursor, uint32_t depth, int capture,
        uint32_t capture_group) {
    uint32_t index;
    uint32_t body_group_start = *group_cursor;
    uint32_t body_group_end = *group_cursor;
    int body_compiled = 0;
    int result;
    for (index = 0u; index < minimum; ++index) {
        uint32_t iteration_group_cursor = body_group_start;
        if (capture) {
            _regex_instruction_t marker;
            _regex_zero(&marker, sizeof(marker));
            marker.operation = _REGEX_OP_SAVE_START;
            marker.first = capture_group;
            result = _regex_emit(instructions, capacity, count, &marker);
            if (result != 0) return result;
        }
        result = _regex_compile_expression(
            pattern, pattern_size, cflags, instructions, capacity, count,
            &iteration_group_cursor, depth);
        if (result != 0) return result;
        if (!body_compiled) {
            body_group_end = iteration_group_cursor;
            body_compiled = 1;
        }
        *group_cursor = body_group_end;
        if (capture) {
            _regex_instruction_t marker;
            _regex_zero(&marker, sizeof(marker));
            marker.operation = _REGEX_OP_SAVE_END;
            marker.first = capture_group;
            result = _regex_emit(instructions, capacity, count, &marker);
            if (result != 0) return result;
        }
    }
    if (maximum == UINT32_MAX) {
        _regex_instruction_t split;
        _regex_instruction_t jump;
        uint32_t split_index = *count;
        _regex_zero(&split, sizeof(split));
        split.operation = _REGEX_OP_SPLIT;
        split.first = split_index + 1u;
        split.second = split_index + 3u;
        result = _regex_emit(instructions, capacity, count, &split);
        if (result != 0) return result;
        {
            uint32_t iteration_group_cursor = body_group_start;
            if (capture) {
                _regex_instruction_t marker;
                _regex_zero(&marker, sizeof(marker));
                marker.operation = _REGEX_OP_SAVE_START;
                marker.first = capture_group;
                result = _regex_emit(instructions, capacity, count, &marker);
                if (result != 0) return result;
            }
            result = _regex_compile_expression(
                pattern, pattern_size, cflags, instructions, capacity, count,
                &iteration_group_cursor, depth);
            if (!body_compiled) {
                body_group_end = iteration_group_cursor;
                body_compiled = 1;
            }
            *group_cursor = body_group_end;
            if (capture) {
                _regex_instruction_t marker;
                _regex_zero(&marker, sizeof(marker));
                marker.operation = _REGEX_OP_SAVE_END;
                marker.first = capture_group;
                result = _regex_emit(instructions, capacity, count, &marker);
                if (result != 0) return result;
            }
        }
        if (result != 0) return result;
        _regex_zero(&jump, sizeof(jump));
        jump.operation = _REGEX_OP_JUMP;
        jump.first = split_index;
        result = _regex_emit(instructions, capacity, count, &jump);
        if (result != 0) return result;
        if (instructions) instructions[split_index].second = *count;
        return 0;
    }
    for (index = minimum; index < maximum; ++index) {
        _regex_instruction_t split;
        uint32_t split_index = *count;
        uint32_t iteration_group_cursor = body_group_start;
        _regex_zero(&split, sizeof(split));
        split.operation = _REGEX_OP_SPLIT;
        split.first = split_index + 1u;
        split.second = split_index + 2u;
        result = _regex_emit(instructions, capacity, count, &split);
        if (result != 0) return result;
        if (capture) {
            _regex_instruction_t marker;
            _regex_zero(&marker, sizeof(marker));
            marker.operation = _REGEX_OP_SAVE_START;
            marker.first = capture_group;
            result = _regex_emit(instructions, capacity, count, &marker);
            if (result != 0) return result;
        }
        result = _regex_compile_expression(
            pattern, pattern_size, cflags, instructions, capacity, count,
            &iteration_group_cursor, depth);
        if (result != 0) return result;
        if (!body_compiled) {
            body_group_end = iteration_group_cursor;
            body_compiled = 1;
        }
        *group_cursor = body_group_end;
        if (capture) {
            _regex_instruction_t marker;
            _regex_zero(&marker, sizeof(marker));
            marker.operation = _REGEX_OP_SAVE_END;
            marker.first = capture_group;
            result = _regex_emit(instructions, capacity, count, &marker);
            if (result != 0) return result;
        }
        if (instructions) instructions[split_index].second = *count;
    }
    return 0;
}

static inline int _regex_compile_sequence(
        const char* pattern, size_t pattern_size, int cflags,
        _regex_instruction_t* instructions, uint32_t capacity,
        uint32_t* count, uint32_t* group_cursor, uint32_t depth) {
    size_t current = 0u;
    int extended = (cflags & REG_EXTENDED) != 0;
    while (current < pattern_size) {
        _regex_instruction_t atom;
        uint32_t minimum;
        uint32_t maximum;
        int assertion = 0;
        int result;
        char value = pattern[current];
        _regex_zero(&atom, sizeof(atom));
        if (value == '*' || (extended &&
            (value == '+' || value == '?' || value == '{')) ||
            (!extended && value == '\\' && current + 1u < pattern_size &&
             (pattern[current + 1u] == '{' ||
              pattern[current + 1u] == '+' ||
              pattern[current + 1u] == '?')))
            return REG_BADRPT;
        if (extended && value == '}') return REG_EBRACE;
        if (!extended && value == '\\' && current + 1u < pattern_size &&
            pattern[current + 1u] == '}')
            return REG_EBRACE;
        if ((extended && value == ')') ||
            (!extended && value == '\\' && current + 1u < pattern_size &&
             pattern[current + 1u] == ')')) return REG_EPAREN;
        if ((extended && value == '|') ||
            (!extended && value == '\\' && current + 1u < pattern_size &&
             pattern[current + 1u] == '|')) return REG_BADPAT;
        if ((extended && value == '(') ||
            (!extended && value == '\\' && current + 1u < pattern_size &&
             pattern[current + 1u] == '(')) {
            size_t group_end;
            size_t group_start = current + (extended ? 1u : 2u);
            uint32_t group_index;
            int capture = (cflags & REG_NOSUB) == 0 ||
                          (cflags & _REGEX_COMPILE_CAPTURE) != 0;
            result = _regex_find_group_end(
                pattern, pattern_size, current, cflags, &group_end);
            if (result != 0) return result;
            if (group_end == group_start) return REG_BADPAT;
            if (capture) {
                if (*group_cursor >= _REGEX_MAX_GROUPS) return REG_ESPACE;
                group_index = *group_cursor;
                ++*group_cursor;
            } else {
                group_index = 0u;
            }
            current = group_end + (extended ? 1u : 2u);
            result = _regex_parse_postfix_repeat(
                pattern, pattern_size, extended, &current, &minimum, &maximum);
            if (result != 0) return result;
            result = _regex_emit_expression_repeat(
                pattern + group_start, group_end - group_start, cflags,
                instructions, capacity, count, minimum, maximum, group_cursor,
                depth + 1u, capture, group_index);
            if (result != 0) return result;
            continue;
        }
        if (value == '^' && current == 0u) {
            atom.operation = _REGEX_OP_BOL;
            assertion = 1;
            ++current;
        } else if (value == '$' && current + 1u == pattern_size) {
            atom.operation = _REGEX_OP_EOL;
            assertion = 1;
            ++current;
        } else if (value == '[') {
            result = _regex_parse_class(pattern, pattern_size, &current,
                                        &atom, cflags);
            if (result != 0) return result;
        } else if (value == '.') {
            atom.operation = _REGEX_OP_ANY;
            ++current;
        } else if (value == '\\') {
            size_t reference_begin;
            uint32_t reference = 0u;
            ++current;
            if (current == pattern_size) return REG_EESCAPE;
            value = pattern[current];
            if (value >= '0' && value <= '9') {
                reference_begin = current;
                while (current < pattern_size &&
                       pattern[current] >= '0' && pattern[current] <= '9') {
                    uint32_t digit = (uint32_t)(pattern[current] - '0');
                    if (reference > (_REGEX_MAX_GROUPS - digit) / 10u)
                        return REG_ESUBREG;
                    reference = reference * 10u + digit;
                    ++current;
                }
                if (reference == 0u || reference > *group_cursor ||
                    current == reference_begin)
                    return REG_ESUBREG;
                atom.operation = _REGEX_OP_BACKREF;
                atom.first = reference - 1u;
            } else {
                ++current;
            }
            if (atom.operation == 0u) {
                atom.operation = _REGEX_OP_BYTE;
                atom.value = (uint8_t)value;
            }
        } else {
            atom.operation = _REGEX_OP_BYTE;
            atom.value = (uint8_t)value;
            ++current;
        }
        if (!assertion) {
            result = _regex_parse_postfix_repeat(
                pattern, pattern_size, extended, &current, &minimum, &maximum);
            if (result != 0) return result;
        }
        result = assertion
            ? _regex_emit(instructions, capacity, count, &atom)
            : _regex_emit_repeat(instructions, capacity, count, &atom,
                                 minimum, maximum);
        if (result != 0) return result;
    }
    return 0;
}

static inline int _regex_find_branch_end(
        const char* pattern, size_t pattern_size, size_t branch_begin,
        int cflags, size_t* branch_end, int* has_separator,
        size_t* separator_size) {
    size_t current = branch_begin;
    uint32_t nesting = 0u;
    int extended = (cflags & REG_EXTENDED) != 0;
    while (current < pattern_size) {
        if (pattern[current] == '\\') {
            if (current + 1u >= pattern_size) return REG_EESCAPE;
            if (!extended && pattern[current + 1u] == '(') {
                if (++nesting > _REGEX_MAX_GROUP_DEPTH) return REG_ESPACE;
            } else if (!extended && pattern[current + 1u] == ')') {
                if (nesting == 0u) return REG_EPAREN;
                --nesting;
            } else if (!extended && pattern[current + 1u] == '|' &&
                       nesting == 0u) {
                *branch_end = current;
                *has_separator = 1;
                *separator_size = 2u;
                return 0;
            }
            current += 2u;
            continue;
        }
        if (pattern[current] == '[') {
            _regex_instruction_t ignored;
            int result = _regex_parse_class(
                pattern, pattern_size, &current, &ignored, cflags);
            if (result != 0) return result;
            continue;
        }
        if (extended && pattern[current] == '(') {
            if (++nesting > _REGEX_MAX_GROUP_DEPTH) return REG_ESPACE;
        } else if (extended && pattern[current] == ')') {
            if (nesting == 0u) return REG_EPAREN;
            --nesting;
        } else if (extended && pattern[current] == '|' && nesting == 0u) {
            *branch_end = current;
            *has_separator = 1;
            *separator_size = 1u;
            return 0;
        }
        ++current;
    }
    if (nesting != 0u) return REG_EPAREN;
    *branch_end = pattern_size;
    *has_separator = 0;
    *separator_size = 0u;
    return 0;
}

static inline int _regex_compile_expression(
        const char* pattern, size_t pattern_size, int cflags,
        _regex_instruction_t* instructions, uint32_t capacity,
        uint32_t* count, uint32_t* group_cursor, uint32_t depth) {
    size_t branch_begin = 0u;
    uint32_t expression_begin = *count;
    int have_alternation = 0;
    int result;
    if (depth > _REGEX_MAX_GROUP_DEPTH) return REG_ESPACE;
    for (;;) {
        size_t branch_end;
        size_t separator_size;
        int has_separator;
        result = _regex_find_branch_end(
            pattern, pattern_size, branch_begin, cflags,
            &branch_end, &has_separator, &separator_size);
        if (result != 0) return result;
        if ((has_separator || have_alternation) && branch_end == branch_begin)
            return REG_BADPAT;
        if (has_separator) {
            _regex_instruction_t split;
            _regex_instruction_t jump;
            uint32_t split_index = *count;
            have_alternation = 1;
            _regex_zero(&split, sizeof(split));
            split.operation = _REGEX_OP_SPLIT;
            split.first = split_index + 1u;
            result = _regex_emit(instructions, capacity, count, &split);
            if (result != 0) return result;
            result = _regex_compile_sequence(
                pattern + branch_begin, branch_end - branch_begin,
                cflags, instructions, capacity, count, group_cursor, depth);
            if (result != 0) return result;
            _regex_zero(&jump, sizeof(jump));
            jump.operation = _REGEX_OP_JUMP;
            jump.first = UINT32_MAX;
            result = _regex_emit(instructions, capacity, count, &jump);
            if (result != 0) return result;
            if (instructions) instructions[split_index].second = *count;
            branch_begin = branch_end + separator_size;
            continue;
        }
        result = _regex_compile_sequence(
            pattern + branch_begin, branch_end - branch_begin,
            cflags, instructions, capacity, count, group_cursor, depth);
        if (result != 0) return result;
        break;
    }
    if (instructions && have_alternation) {
        uint32_t index;
        for (index = expression_begin; index < *count; ++index) {
            if (instructions[index].operation == _REGEX_OP_JUMP &&
                instructions[index].first == UINT32_MAX)
                instructions[index].first = *count;
        }
    }
    return 0;
}

static inline int _regex_compile_pass(const char* pattern,
                                      size_t pattern_size, int cflags,
                                      _regex_instruction_t* instructions,
                                      uint32_t capacity,
                                      uint32_t* instruction_count,
                                      uint32_t* group_count_out) {
    uint32_t count = 0u;
    uint32_t group_cursor = 0u;
    _regex_instruction_t match;
    int result = _regex_compile_expression(
        pattern, pattern_size, cflags, instructions, capacity, &count,
        &group_cursor, 0u);
    if (result != 0) return result;
    _regex_zero(&match, sizeof(match));
    match.operation = _REGEX_OP_MATCH;
    result = _regex_emit(instructions, capacity, &count, &match);
    if (result != 0) return result;
    *instruction_count = count;
    if (group_count_out) *group_count_out = group_cursor;
    return 0;
}

static inline int _regex_program_valid(const _regex_internal_t* internal) {
    const int known_flags = REG_EXTENDED | REG_ICASE | REG_NOSUB | REG_NEWLINE;
    uint32_t index;
    size_t expected_size;
    uint64_t size_max = (uint64_t)(size_t)-1;
    if (!internal || internal->magic != _REGEX_MAGIC ||
        internal->instruction_count == 0u ||
        internal->instruction_count > _REGEX_MAX_INSTRUCTIONS ||
        internal->group_count > _REGEX_MAX_GROUPS ||
        (internal->cflags & ~known_flags) != 0 || !internal->instructions)
        return 0;
    if ((uint64_t)internal->instruction_count >
        (size_max - sizeof(*internal)) / sizeof(_regex_instruction_t))
        return 0;
    expected_size = sizeof(*internal) +
                    (size_t)internal->instruction_count *
                        sizeof(_regex_instruction_t);
    if (internal->allocation_size != expected_size ||
        internal->instructions[internal->instruction_count - 1u].operation !=
            _REGEX_OP_MATCH) return 0;
    for (index = 0u; index < internal->instruction_count; ++index) {
        const _regex_instruction_t* instruction = &internal->instructions[index];
        if (instruction->reserved != 0u) return 0;
        switch (instruction->operation) {
            case _REGEX_OP_BYTE:
            case _REGEX_OP_ANY:
            case _REGEX_OP_CLASS:
                if (index + 1u >= internal->instruction_count ||
                    instruction->first != 0u || instruction->second != 0u)
                    return 0;
                if (instruction->operation != _REGEX_OP_CLASS &&
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap))) return 0;
                if (instruction->operation == _REGEX_OP_CLASS &&
                    instruction->value > 1u) return 0;
                if (instruction->operation == _REGEX_OP_ANY &&
                    instruction->value != 0u) return 0;
                break;
            case _REGEX_OP_SPLIT:
                if (instruction->value != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)) ||
                    instruction->first >= internal->instruction_count ||
                    instruction->second >= internal->instruction_count)
                    return 0;
                break;
            case _REGEX_OP_JUMP:
                if (instruction->value != 0u || instruction->second != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)) ||
                    instruction->first >= internal->instruction_count)
                    return 0;
                break;
            case _REGEX_OP_BOL:
            case _REGEX_OP_EOL:
                if (index + 1u >= internal->instruction_count ||
                    instruction->value != 0u || instruction->first != 0u ||
                    instruction->second != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap))) return 0;
                break;
            case _REGEX_OP_SAVE_START:
            case _REGEX_OP_SAVE_END:
                if (instruction->value != 0u || instruction->second != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)) ||
                    instruction->first >= internal->group_count ||
                    index + 1u >= internal->instruction_count)
                    return 0;
                break;
            case _REGEX_OP_BACKREF:
                if (instruction->value != 0u || instruction->second != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)) ||
                    instruction->first >= internal->group_count ||
                    index + 1u >= internal->instruction_count)
                    return 0;
                break;
            case _REGEX_OP_MATCH:
                if (index + 1u != internal->instruction_count ||
                    instruction->value != 0u || instruction->first != 0u ||
                    instruction->second != 0u ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap))) return 0;
                break;
            default: return 0;
        }
    }
    return 1;
}

static inline int _regex_count_groups(const char* pattern,
                                      size_t pattern_size, int cflags,
                                      size_t* count_out) {
    size_t current = 0u;
    size_t count = 0u;
    uint32_t nesting = 0u;
    int extended = (cflags & REG_EXTENDED) != 0;
    while (current < pattern_size) {
        if (pattern[current] == '\\') {
            if (current + 1u >= pattern_size) return REG_EESCAPE;
            if (!extended && pattern[current + 1u] == '(') {
                if (++count == (size_t)-1 ||
                    ++nesting > _REGEX_MAX_GROUP_DEPTH)
                    return REG_ESPACE;
            } else if (!extended && pattern[current + 1u] == ')') {
                if (nesting == 0u) return REG_EPAREN;
                --nesting;
            }
            current += 2u;
            continue;
        }
        if (pattern[current] == '[') {
            _regex_instruction_t ignored;
            int result = _regex_parse_class(
                pattern, pattern_size, &current, &ignored, cflags);
            if (result != 0) return result;
            continue;
        }
        if (extended && pattern[current] == '(') {
            if (++count == (size_t)-1 ||
                ++nesting > _REGEX_MAX_GROUP_DEPTH)
                return REG_ESPACE;
        } else if (extended && pattern[current] == ')') {
            if (nesting == 0u) return REG_EPAREN;
            --nesting;
        }
        ++current;
    }
    if (nesting != 0u) return REG_EPAREN;
    *count_out = count;
    return 0;
}

static inline int regcomp(regex_t* preg, const char* pattern, int cflags) {
    const int known_flags = REG_EXTENDED | REG_ICASE | REG_NOSUB | REG_NEWLINE;
    _regex_internal_t* internal;
    uint32_t instruction_count = 0u;
    size_t pattern_size = 0u;
    size_t group_count = 0u;
    uint32_t compiled_group_count = 0u;
    int compile_flags;
    size_t allocation_size;
    uint64_t size_max = (uint64_t)(size_t)-1;
    int result;
    if (!preg || !pattern) return REG_INVARG;
    preg->_internal = NULL;
    preg->re_nsub = 0u;
    if ((cflags & ~known_flags) != 0) return REG_INVARG;
    while (pattern_size <= _REGEX_MAX_PATTERN && pattern[pattern_size] != '\0')
        ++pattern_size;
    if (pattern_size > _REGEX_MAX_PATTERN) return REG_ESPACE;
    compile_flags = cflags;
    if (_regex_pattern_has_backref(pattern, pattern_size, cflags))
        compile_flags |= _REGEX_COMPILE_CAPTURE;
    result = _regex_count_groups(pattern, pattern_size, cflags, &group_count);
    if (result != 0) return result;
    if (group_count > _REGEX_MAX_GROUPS) return REG_ESPACE;
    result = _regex_compile_pass(pattern, pattern_size, compile_flags, NULL, 0u,
                                 &instruction_count, &compiled_group_count);
    if (result != 0) return result;
    if ((cflags & REG_NOSUB) == 0 && compiled_group_count != group_count)
        return REG_BADPAT;
    if ((uint64_t)instruction_count >
        (size_max - sizeof(*internal)) / sizeof(_regex_instruction_t))
        return REG_ESPACE;
    allocation_size = sizeof(*internal) +
                      (size_t)instruction_count * sizeof(_regex_instruction_t);
    internal = (_regex_internal_t*)malloc(sizeof(*internal));
    if (!internal) return REG_ESPACE;
    _regex_zero(internal, sizeof(*internal));
    internal->instructions = (_regex_instruction_t*)malloc(
        (size_t)instruction_count * sizeof(_regex_instruction_t));
    if (!internal->instructions) { free(internal); return REG_ESPACE; }
    _regex_zero(internal->instructions,
                (size_t)instruction_count * sizeof(_regex_instruction_t));
    result = _regex_compile_pass(pattern, pattern_size, compile_flags,
                                 internal->instructions, instruction_count,
                                 &internal->instruction_count,
                                 &compiled_group_count);
    if (result != 0 || internal->instruction_count != instruction_count ||
        ((cflags & REG_NOSUB) == 0 && compiled_group_count != group_count)) {
        free(internal->instructions);
        free(internal);
        return result != 0 ? result : REG_BADPAT;
    }
    internal->allocation_size = allocation_size;
    internal->group_count = (uint32_t)group_count;
    internal->cflags = cflags;
    internal->magic = _REGEX_MAGIC;
    if (!_regex_program_valid(internal)) {
        free(internal->instructions);
        free(internal);
        return REG_BADPAT;
    }
    preg->_internal = internal;
    preg->re_nsub = group_count;
    return 0;
}

static inline int _regex_bol(const _regex_internal_t* internal,
                             const unsigned char* input, size_t position,
                             int eflags) {
    if (position == 0u && (eflags & REG_NOTBOL) == 0) return 1;
    return (internal->cflags & REG_NEWLINE) != 0 && position != 0u &&
           input[position - 1u] == (unsigned char)'\n';
}

static inline int _regex_eol(const _regex_internal_t* internal,
                             const unsigned char* input, size_t input_size,
                             size_t position, int eflags) {
    if (position == input_size && (eflags & REG_NOTEOL) == 0) return 1;
    return (internal->cflags & REG_NEWLINE) != 0 && position < input_size &&
           input[position] == (unsigned char)'\n';
}

static inline void _regex_copy_captures(size_t* destination,
                                        const size_t* source,
                                        size_t capture_slots) {
    size_t index;
    for (index = 0u; index < capture_slots; ++index)
        destination[index] = source[index];
}

static inline int _regex_closure(const _regex_internal_t* internal,
                                 const unsigned char* input,
                                 size_t input_size, size_t position,
                                 int eflags, size_t* states, size_t* queue,
                                 uint8_t* queued, size_t* capture_state,
                                 size_t capture_slots, uint64_t* steps) {
    const size_t none = (size_t)-1;
    uint32_t count = internal->instruction_count;
    uint32_t head = 0u, tail = 0u, queued_count = 0u, index;
    for (index = 0u; index < count; ++index) queued[index] = 0u;
    for (index = 0u; index < count; ++index) {
        if (states[index] != none) {
            queue[tail] = index;
            tail = (tail + 1u) % count;
            queued[index] = 1u;
            ++queued_count;
        }
    }
    while (queued_count != 0u) {
        const _regex_instruction_t* instruction;
        size_t origin;
        uint32_t pc = (uint32_t)queue[head];
        head = (head + 1u) % count;
        --queued_count;
        queued[pc] = 0u;
        if (++*steps > _REGEX_MAX_STEPS) return 0;
        origin = states[pc];
        instruction = &internal->instructions[pc];
#define _REGEX_OFFER(target_) do {                                         \
    uint32_t _target = (uint32_t)(target_);                                \
    int _replace = origin < states[_target];                               \
    if (!_replace && origin == states[_target] && capture_slots != 0u &&   \
        (instruction->operation == _REGEX_OP_SAVE_START ||                 \
         instruction->operation == _REGEX_OP_SAVE_END)) {                  \
        size_t _slot = (size_t)instruction->first * 2u +                   \
                       (instruction->operation == _REGEX_OP_SAVE_END);     \
        if (capture_state[(size_t)_target * capture_slots + _slot] !=       \
                (size_t)-1 &&                                               \
            position > capture_state[(size_t)_target * capture_slots +      \
                                     _slot])                                 \
            _replace = 1;                                                   \
    }                                                                        \
    if (_replace) {                                                          \
        states[_target] = origin;                                          \
        if (capture_slots != 0u) {                                         \
            _regex_copy_captures(                                          \
                capture_state + (size_t)_target * capture_slots,          \
                capture_state + (size_t)pc * capture_slots, capture_slots);\
            if (instruction->operation == _REGEX_OP_SAVE_START)            \
                capture_state[(size_t)_target * capture_slots +            \
                              (size_t)instruction->first * 2u] = position;\
            else if (instruction->operation == _REGEX_OP_SAVE_END)         \
                capture_state[(size_t)_target * capture_slots +            \
                              (size_t)instruction->first * 2u + 1u] =     \
                    position;                                               \
        }                                                                    \
        if (!queued[_target]) {                                            \
            if (queued_count >= count) return 0;                           \
            queue[tail] = _target;                                         \
            tail = (tail + 1u) % count;                                    \
            queued[_target] = 1u;                                          \
            ++queued_count;                                                \
        }                                                                  \
    }                                                                      \
} while (0)
        if (instruction->operation == _REGEX_OP_SPLIT) {
            _REGEX_OFFER(instruction->first);
            _REGEX_OFFER(instruction->second);
        } else if (instruction->operation == _REGEX_OP_JUMP) {
            _REGEX_OFFER(instruction->first);
        } else if (instruction->operation == _REGEX_OP_BOL &&
                   _regex_bol(internal, input, position, eflags)) {
            _REGEX_OFFER(pc + 1u);
        } else if (instruction->operation == _REGEX_OP_EOL &&
                   _regex_eol(internal, input, input_size, position, eflags)) {
            _REGEX_OFFER(pc + 1u);
        } else if (instruction->operation == _REGEX_OP_SAVE_START ||
                   instruction->operation == _REGEX_OP_SAVE_END) {
            _REGEX_OFFER(pc + 1u);
        }
#undef _REGEX_OFFER
    }
    return 1;
}

static inline int _regex_instruction_matches(
        const _regex_internal_t* internal,
        const _regex_instruction_t* instruction, unsigned char value) {
    if (instruction->operation == _REGEX_OP_BYTE) {
        unsigned char expected = instruction->value;
        if ((internal->cflags & REG_ICASE) != 0) {
            expected = (unsigned char)_regex_ascii_fold((char)expected);
            value = (unsigned char)_regex_ascii_fold((char)value);
        }
        return expected == value;
    }
    if (instruction->operation == _REGEX_OP_ANY)
        return (internal->cflags & REG_NEWLINE) == 0 || value != '\n';
    if (instruction->operation == _REGEX_OP_CLASS) {
        int member = _regex_bitmap_has(instruction->bitmap, value);
        if (instruction->value != 0u &&
            (internal->cflags & REG_NEWLINE) != 0 && value == '\n') return 0;
        return instruction->value != 0u ? !member : member;
    }
    return 0;
}

typedef struct {
    const _regex_internal_t* internal;
    const unsigned char* input;
    size_t input_size;
    int eflags;
    size_t capture_slots;
    uint64_t steps;
} _regex_backref_context_t;

/* Backreferences are not regular and therefore cannot be represented by the
 * one-byte-at-a-time Thompson state set above.  Keep them on a separate,
 * bounded DFS path.  The caller supplies a capture array that is copied at
 * alternation points, so failed branches never leak partial captures. */
static inline int _regex_backref_match(
        _regex_backref_context_t* context, uint32_t pc, size_t position,
        size_t* captures, size_t* end_out, uint32_t depth) {
    const size_t none = (size_t)-1;
    const _regex_instruction_t* instruction;
    size_t slots = context->capture_slots;
    size_t original[_REGEX_MAX_GROUPS * 2u];
    size_t candidate[_REGEX_MAX_GROUPS * 2u];
    size_t end_first = none;
    size_t end_second = none;
    int first_result;
    int second_result;
    if (depth >= _REGEX_MAX_BACKREF_DEPTH ||
        ++context->steps > _REGEX_MAX_STEPS ||
        pc >= context->internal->instruction_count) return -1;
    instruction = &context->internal->instructions[pc];
    switch (instruction->operation) {
        case _REGEX_OP_MATCH:
            *end_out = position;
            return 1;
        case _REGEX_OP_BYTE:
        case _REGEX_OP_ANY:
        case _REGEX_OP_CLASS:
            if (position >= context->input_size ||
                !_regex_instruction_matches(context->internal, instruction,
                                             context->input[position]))
                return 0;
            return _regex_backref_match(context, pc + 1u, position + 1u,
                                        captures, end_out, depth + 1u);
        case _REGEX_OP_BACKREF: {
            size_t slot = (size_t)instruction->first * 2u;
            size_t begin;
            size_t end;
            size_t length;
            size_t index;
            if (slot + 1u >= slots) return -1;
            begin = captures[slot];
            end = captures[slot + 1u];
            if (begin == none || end == none || end < begin) return 0;
            length = end - begin;
            if (position > context->input_size ||
                length > _REGEX_MAX_BACKREF_CAPTURE ||
                length > context->input_size - position) return 0;
            for (index = 0u; index < length; ++index) {
                unsigned char expected = context->input[begin + index];
                unsigned char actual = context->input[position + index];
                if ((context->internal->cflags & REG_ICASE) != 0) {
                    expected = (unsigned char)_regex_ascii_fold((char)expected);
                    actual = (unsigned char)_regex_ascii_fold((char)actual);
                }
                if (expected != actual) return 0;
            }
            return _regex_backref_match(context, pc + 1u,
                                        position + length, captures, end_out,
                                        depth + 1u);
        }
        case _REGEX_OP_BOL:
            if (!_regex_bol(context->internal, context->input, position,
                            context->eflags)) return 0;
            return _regex_backref_match(context, pc + 1u, position, captures,
                                        end_out, depth + 1u);
        case _REGEX_OP_EOL:
            if (!_regex_eol(context->internal, context->input,
                            context->input_size, position, context->eflags))
                return 0;
            return _regex_backref_match(context, pc + 1u, position, captures,
                                        end_out, depth + 1u);
        case _REGEX_OP_JUMP:
            return _regex_backref_match(context, instruction->first, position,
                                        captures, end_out, depth + 1u);
        case _REGEX_OP_SAVE_START:
        case _REGEX_OP_SAVE_END: {
            size_t slot = (size_t)instruction->first * 2u +
                          (instruction->operation == _REGEX_OP_SAVE_END);
            size_t previous;
            int result;
            if (slot >= slots) return -1;
            previous = captures[slot];
            captures[slot] = position;
            result = _regex_backref_match(context, pc + 1u, position,
                                          captures, end_out, depth + 1u);
            if (result <= 0) captures[slot] = previous;
            return result;
        }
        case _REGEX_OP_SPLIT:
            for (size_t index = 0u; index < slots; ++index)
                original[index] = captures[index];
            first_result = _regex_backref_match(
                context, instruction->first, position, captures, &end_first,
                depth + 1u);
            for (size_t index = 0u; index < slots; ++index)
                candidate[index] = captures[index];
            for (size_t index = 0u; index < slots; ++index)
                captures[index] = original[index];
            second_result = _regex_backref_match(
                context, instruction->second, position, captures, &end_second,
                depth + 1u);
            if (first_result < 0 || second_result < 0) return -1;
            if (first_result == 0 && second_result == 0) return 0;
            if (second_result != 0 &&
                (first_result == 0 || end_second > end_first)) {
                *end_out = end_second;
                return 1;
            }
            for (size_t index = 0u; index < slots; ++index)
                captures[index] = candidate[index];
            *end_out = end_first;
            return 1;
        default:
            return -1;
    }
}

static inline int regexec(const regex_t* preg, const char* string,
                          size_t nmatch, regmatch_t pmatch[], int eflags) {
    const int known_flags = REG_NOTBOL | REG_NOTEOL | REG_STARTEND;
    const size_t none = (size_t)-1;
    const _regex_internal_t* internal;
    const unsigned char* input;
    size_t region_begin = 0u, region_end = 0u, input_size;
    size_t *active = NULL, *next = NULL, *queue = NULL;
    size_t *active_captures = NULL, *next_captures = NULL;
    size_t *best_captures = NULL;
    uint8_t* queued = NULL;
    size_t best_begin = none, best_end = 0u, position;
    uint64_t steps = 0u;
    uint32_t count;
    size_t capture_slots;
    int has_backref = 0;
    int result = REG_NOMATCH;
    if (!preg || !preg->_internal || !string) return REG_INVARG;
    if ((eflags & ~known_flags) != 0) return REG_INVARG;
    internal = (const _regex_internal_t*)preg->_internal;
    if (!_regex_program_valid(internal)) return REG_BADPAT;
    if ((eflags & REG_STARTEND) != 0) {
        uintptr_t base = (uintptr_t)(const void*)string;
        uintptr_t begin_address, end_address;
        if (!pmatch || nmatch == 0u || pmatch[0].rm_so < 0 ||
            pmatch[0].rm_eo < pmatch[0].rm_so) return REG_INVARG;
        region_begin = (size_t)pmatch[0].rm_so;
        region_end = (size_t)pmatch[0].rm_eo;
        if (region_end - region_begin > _REGEX_MAX_INPUT ||
            region_begin > (size_t)(UINTPTR_MAX - base) ||
            region_end > (size_t)(UINTPTR_MAX - base)) return REG_INVARG;
        begin_address = base + (uintptr_t)region_begin;
        end_address = base + (uintptr_t)region_end;
        if (begin_address > end_address) return REG_INVARG;
        input = (const unsigned char*)(const void*)begin_address;
        input_size = region_end - region_begin;
    } else {
        input = (const unsigned char*)(const void*)string;
        while (region_end < _REGEX_MAX_INPUT && input[region_end] != 0u)
            ++region_end;
        if (region_end == _REGEX_MAX_INPUT && input[region_end] != 0u)
            return REG_ESPACE;
        input_size = region_end;
    }
    count = internal->instruction_count;
    for (position = 0u; position < (size_t)count; ++position) {
        if (internal->instructions[position].operation == _REGEX_OP_BACKREF) {
            has_backref = 1;
            break;
        }
    }
    capture_slots = (size_t)internal->group_count * 2u;
    active = (size_t*)malloc((size_t)count * sizeof(*active));
    next = (size_t*)malloc((size_t)count * sizeof(*next));
    queue = (size_t*)malloc((size_t)count * sizeof(*queue));
    queued = (uint8_t*)malloc((size_t)count * sizeof(*queued));
    if (capture_slots != 0u) {
        if ((size_t)count > (size_t)-1 / capture_slots ||
            (size_t)count * capture_slots > (size_t)-1 / sizeof(*active_captures)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        active_captures = (size_t*)malloc(
            (size_t)count * capture_slots * sizeof(*active_captures));
        next_captures = (size_t*)malloc(
            (size_t)count * capture_slots * sizeof(*next_captures));
        best_captures = (size_t*)malloc(capture_slots * sizeof(*best_captures));
    }
    if (!active || !next || !queue || !queued ||
        (capture_slots != 0u &&
         (!active_captures || !next_captures || !best_captures))) {
        result = REG_ESPACE;
        goto regex_exec_done;
    }
    if (capture_slots != 0u) {
        size_t capture_index;
        for (capture_index = 0u;
             capture_index < (size_t)count * capture_slots; ++capture_index)
            active_captures[capture_index] = none;
        for (capture_index = 0u; capture_index < capture_slots; ++capture_index)
            best_captures[capture_index] = none;
    }
    if (has_backref) {
        _regex_backref_context_t context;
        size_t captures[_REGEX_MAX_GROUPS * 2u];
        size_t candidate_end = none;
        context.internal = internal;
        context.input = input;
        context.input_size = input_size;
        context.eflags = eflags;
        context.capture_slots = capture_slots;
        context.steps = 0u;
        for (position = 0u; position <= input_size; ++position) {
            size_t capture_index;
            int backref_result;
            for (capture_index = 0u; capture_index < capture_slots;
                 ++capture_index)
                captures[capture_index] = none;
            candidate_end = none;
            backref_result = _regex_backref_match(
                &context, 0u, position, captures, &candidate_end, 0u);
            if (backref_result < 0) {
                result = REG_ESPACE;
                goto regex_exec_done;
            }
            if (backref_result != 0) {
                best_begin = position;
                best_end = candidate_end;
                _regex_copy_captures(best_captures, captures, capture_slots);
                break;
            }
        }
        if (best_begin != none) {
            if ((internal->cflags & REG_NOSUB) == 0 && pmatch && nmatch != 0u) {
                size_t index;
                pmatch[0].rm_so = (regoff_t)(region_begin + best_begin);
                pmatch[0].rm_eo = (regoff_t)(region_begin + best_end);
                for (index = 1u; index < nmatch; ++index) {
                    size_t slot = (index - 1u) * 2u;
                    if (index <= internal->group_count &&
                        best_captures[slot] != none &&
                        best_captures[slot + 1u] != none) {
                        pmatch[index].rm_so =
                            (regoff_t)(region_begin + best_captures[slot]);
                        pmatch[index].rm_eo =
                            (regoff_t)(region_begin + best_captures[slot + 1u]);
                    } else {
                        pmatch[index].rm_so = (regoff_t)-1;
                        pmatch[index].rm_eo = (regoff_t)-1;
                    }
                }
            }
            result = 0;
        }
        goto regex_exec_done;
    }
    for (position = 0u; position < count; ++position) active[position] = none;
    for (position = 0u; position <= input_size; ++position) {
        uint32_t index;
        int live = 0;
        if (best_begin == none && position < active[0]) {
            active[0] = position;
            if (capture_slots != 0u)
                _regex_zero(active_captures,
                            capture_slots * sizeof(*active_captures));
            if (capture_slots != 0u) {
                size_t capture_index;
                for (capture_index = 0u; capture_index < capture_slots;
                     ++capture_index)
                    active_captures[capture_index] = none;
            }
        }
        if (!_regex_closure(internal, input, input_size, position, eflags,
                            active, queue, queued, active_captures,
                            capture_slots, &steps)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        if (active[count - 1u] != none) {
            size_t candidate = active[count - 1u];
            if (best_begin == none || candidate < best_begin) {
                best_begin = candidate;
                best_end = position;
                if (capture_slots != 0u)
                    _regex_copy_captures(best_captures,
                                         active_captures +
                                             (size_t)(count - 1u) * capture_slots,
                                         capture_slots);
            } else if (candidate == best_begin && position > best_end) {
                best_end = position;
                if (capture_slots != 0u)
                    _regex_copy_captures(best_captures,
                                         active_captures +
                                             (size_t)(count - 1u) * capture_slots,
                                         capture_slots);
            }
        }
        if (position == input_size) break;
        for (index = 0u; index < count; ++index) next[index] = none;
        if (steps > _REGEX_MAX_STEPS - count) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        steps += count;
        for (index = 0u; index + 1u < count; ++index) {
            if (active[index] != none &&
                _regex_instruction_matches(internal,
                    &internal->instructions[index], input[position])) {
                size_t origin = active[index];
                if (origin < next[index + 1u]) {
                    next[index + 1u] = origin;
                    if (capture_slots != 0u)
                        _regex_copy_captures(
                            next_captures + (size_t)(index + 1u) * capture_slots,
                            active_captures + (size_t)index * capture_slots,
                            capture_slots);
                }
            }
        }
        { size_t* swap = active; active = next; next = swap; }
        { size_t* swap = active_captures;
          active_captures = next_captures; next_captures = swap; }
        if (best_begin != none) {
            for (index = 0u; index < count; ++index) {
                if (active[index] != none &&
                    active[index] <= best_begin) {
                    live = 1;
                    break;
                }
            }
            if (!live) break;
        }
    }
    if (best_begin != none) {
        if ((internal->cflags & REG_NOSUB) == 0 && pmatch && nmatch != 0u) {
            size_t index;
            pmatch[0].rm_so = (regoff_t)(region_begin + best_begin);
            pmatch[0].rm_eo = (regoff_t)(region_begin + best_end);
            for (index = 1u; index < nmatch; ++index) {
                size_t slot = (index - 1u) * 2u;
                if (index <= internal->group_count &&
                    best_captures[slot] != none &&
                    best_captures[slot + 1u] != none) {
                    pmatch[index].rm_so =
                        (regoff_t)(region_begin + best_captures[slot]);
                    pmatch[index].rm_eo =
                        (regoff_t)(region_begin + best_captures[slot + 1u]);
                } else {
                    pmatch[index].rm_so = (regoff_t)-1;
                    pmatch[index].rm_eo = (regoff_t)-1;
                }
            }
        }
        result = 0;
    }
regex_exec_done:
    free(queued);
    free(queue);
    free(best_captures);
    free(next_captures);
    free(active_captures);
    free(next);
    free(active);
    return result;
}

static inline void regfree(regex_t* preg) {
    if (preg) {
        _regex_internal_t* internal = (_regex_internal_t*)preg->_internal;
        if (internal && internal->magic == _REGEX_MAGIC) {
            internal->magic = 0u;
            free(internal->instructions);
            internal->instructions = NULL;
            free(internal);
        }
        preg->_internal = NULL;
        preg->re_nsub = 0u;
    }
}

static inline size_t regerror(int errcode, const regex_t* preg,
                              char* errbuf, size_t errbuf_size) {
    const char* msg;
    size_t length = 0u;
    (void)preg;
    switch (errcode) {
        case 0: msg = "Success"; break;
        case REG_NOMATCH: msg = "No match"; break;
        case REG_BADPAT: msg = "Invalid pattern"; break;
        case REG_ECOLLATE: msg = "Invalid collating element"; break;
        case REG_ECTYPE: msg = "Invalid character class"; break;
        case REG_EESCAPE: msg = "Trailing backslash"; break;
        case REG_ESUBREG: msg = "Invalid back reference"; break;
        case REG_EBRACK: msg = "Unmatched bracket"; break;
        case REG_EPAREN: msg = "Unmatched parenthesis"; break;
        case REG_EBRACE: msg = "Unmatched brace"; break;
        case REG_BADBR: msg = "Invalid brace content"; break;
        case REG_ERANGE: msg = "Invalid range"; break;
        case REG_ESPACE: msg = "Out of memory or resource limit"; break;
        case REG_BADRPT: msg = "Invalid repetition"; break;
        case REG_ENOSYS: msg = "Regular expression feature not implemented"; break;
        case REG_INVARG: msg = "Invalid argument or flag"; break;
        default: msg = "Unknown error"; break;
    }
    while (msg[length] != '\0') ++length;
    ++length;
    if (errbuf && errbuf_size != 0u) {
        size_t index = 0u;
        while (index + 1u < errbuf_size && index + 1u < length) {
            errbuf[index] = msg[index];
            ++index;
        }
        errbuf[index] = '\0';
    }
    return length;
}

#ifdef __cplusplus
}
#endif
#endif /* _REGEX_H */
