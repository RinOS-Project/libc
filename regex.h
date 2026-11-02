/* SPDX-License-Identifier: MIT */
/* RinOS libc - bounded POSIX regular-expression subset. */

#ifndef _REGEX_H
#define _REGEX_H

#include "stddef.h"
#include "stdint.h"
#include "locale.h"
#include "stdlib.h"
#include "wctype.h"

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
#define _REGEX_OP_CHAR 12u

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
    uint32_t kind;
    uint32_t first_offset;
    uint32_t first_count;
    uint32_t second_offset;
    uint32_t second_count;
    wctype_t descriptor;
} _regex_class_element_t;

typedef struct {
    uint32_t source_offset;
    uint32_t source_size;
    uint32_t element_begin;
    uint32_t element_count;
    uint8_t negated;
    uint8_t reserved[3];
} _regex_class_t;

#define _REGEX_CLASS_CHARACTER 1u
#define _REGEX_CLASS_RANGE 2u
#define _REGEX_CLASS_PROPERTY 3u
#define _REGEX_CLASS_COLLATING 4u
#define _REGEX_CLASS_EQUIVALENCE 5u
#define _REGEX_MAX_COLLATION_CHARS 32u

typedef struct {
    uint32_t magic;
    uint32_t instruction_count;
    uint32_t group_count;
    size_t allocation_size;
    int cflags;
    _regex_instruction_t* instructions;
    char* pattern;
    size_t pattern_size;
    locale_t locale;
    _regex_class_t* classes;
    uint32_t class_count;
    uint32_t class_capacity;
    _regex_class_element_t* class_elements;
    uint32_t class_element_count;
    uint32_t class_element_capacity;
    uint32_t* class_characters;
    uint32_t class_character_count;
    uint32_t class_character_capacity;
    uint8_t has_variable_class;
    uint8_t reserved[7];
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

static inline int _regex_name_equal(const char* begin, size_t size,
                                    const char* expected);

/* Invalid multibyte input is retained as a private one-byte character so the
 * bounded engine can still match binary data without splitting valid UTF-8. */
static inline int _regex_decode_one(const char* input, size_t available,
                                    uint32_t* character, size_t* width) {
    rin_unicode_mbstate_t state = {0};
    uint32_t decoded = 0u;
    size_t consumed;
    if (!input || available == 0u || !character || !width) return 0;
#if defined(RIN_USERSPACE)
    consumed = rin_locale_current_mbrtowc32(
        &decoded, input, available,
        (struct rin_unicode_mbstate*)(void*)&state);
#else
    consumed = rin_unicode_mbrtowc32(&decoded, input, available, &state);
#endif
    if (consumed == (size_t)-1 || consumed == (size_t)-2 ||
        consumed > available) {
        *character = UINT32_C(0x110000) + (uint8_t)input[0];
        *width = 1u;
        return 1;
    }
    if (consumed == 0u) consumed = 1u;
    *character = (uint32_t)decoded;
    *width = consumed;
    return 1;
}

static inline uint32_t _regex_class_property(const char* name,
                                             size_t length) {
    if (_regex_name_equal(name, length, "alnum"))
        return RIN_LOCALE_CTYPE_ALNUM;
    if (_regex_name_equal(name, length, "alpha"))
        return RIN_LOCALE_CTYPE_ALPHA;
    if (_regex_name_equal(name, length, "blank"))
        return RIN_LOCALE_CTYPE_BLANK;
    if (_regex_name_equal(name, length, "cntrl"))
        return RIN_LOCALE_CTYPE_CNTRL;
    if (_regex_name_equal(name, length, "digit"))
        return RIN_LOCALE_CTYPE_DIGIT;
    if (_regex_name_equal(name, length, "graph"))
        return RIN_LOCALE_CTYPE_GRAPH;
    if (_regex_name_equal(name, length, "lower"))
        return RIN_LOCALE_CTYPE_LOWER;
    if (_regex_name_equal(name, length, "print"))
        return RIN_LOCALE_CTYPE_PRINT;
    if (_regex_name_equal(name, length, "punct"))
        return RIN_LOCALE_CTYPE_PUNCT;
    if (_regex_name_equal(name, length, "space"))
        return RIN_LOCALE_CTYPE_SPACE;
    if (_regex_name_equal(name, length, "upper"))
        return RIN_LOCALE_CTYPE_UPPER;
    if (_regex_name_equal(name, length, "xdigit"))
        return RIN_LOCALE_CTYPE_XDIGIT;
    return 0u;
}

static inline uint32_t _regex_fold(uint32_t character) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_wide_ctype_map(
        character, RIN_LOCALE_CTYPE_TOLOWER);
#else
    return rin_unicode_tolower(character);
#endif
}

static inline int _regex_iswctype(uint32_t character, wctype_t descriptor) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_wide_ctype_iswctype(
        character, (unsigned long)descriptor);
#else
    return rin_unicode_iswctype(character, (unsigned long)descriptor);
#endif
}

static inline int _regex_collate(const uint32_t* left,
                                 const uint32_t* right) {
#if defined(RIN_USERSPACE)
    return rin_locale_current_wcscoll32(left, right);
#else
    return rin_unicode_wcscoll32(left, right);
#endif
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

static inline int _regex_parse_class(const char* pattern,
                                     size_t pattern_size, size_t* offset,
                                     _regex_instruction_t* atom,
                                     int cflags,
                                     const char* root_pattern);

static inline int _regex_pattern_has_backref(const char* pattern,
                                             size_t pattern_size,
                                             int cflags) {
    size_t offset = 0u;
    while (offset < pattern_size) {
        if (pattern[offset] == '[') {
            _regex_instruction_t ignored;
            size_t next = offset;
            if (_regex_parse_class(pattern, pattern_size, &next, &ignored,
                                   cflags, pattern) == 0) {
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

static inline int _regex_parse_class(const char* pattern,
                                     size_t pattern_size, size_t* offset,
                                     _regex_instruction_t* atom,
                                     int cflags,
                                     const char* root_pattern) {
    size_t class_begin = *offset;
    size_t current = class_begin + 1u;
    int have_item = 0;
    int negated = 0;
    if (current >= pattern_size) return REG_EBRACK;
    _regex_zero(atom, sizeof(*atom));
    atom->operation = _REGEX_OP_CLASS;
    if (pattern[current] == '^') { negated = 1; ++current; }
    if (current < pattern_size && pattern[current] == ']') {
        have_item = 1;
        ++current;
    }
    while (current < pattern_size && pattern[current] != ']') {
        if (pattern[current] == '[' && current + 1u < pattern_size &&
            (pattern[current + 1u] == ':' ||
             pattern[current + 1u] == '.' ||
             pattern[current + 1u] == '=')) {
            char delimiter = pattern[current + 1u];
            size_t content_begin = current + 2u;
            size_t end = content_begin;
            while (end + 1u < pattern_size &&
                   !(pattern[end] == delimiter && pattern[end + 1u] == ']'))
                ++end;
            if (end + 1u >= pattern_size) return REG_EBRACK;
            if (end == content_begin) return REG_ECOLLATE;
            if (delimiter == ':' &&
                _regex_class_property(pattern + content_begin,
                                      end - content_begin) == 0u)
                return REG_ECTYPE;
            current = end + 2u;
        } else {
            /* Delimiters are ASCII and cannot occur in a UTF-8 continuation
             * byte, so advancing by bytes remains safe while finding ']'. */
            ++current;
        }
        have_item = 1;
    }
    if (current >= pattern_size || pattern[current] != ']' || !have_item)
        return REG_EBRACK;
    ++current;
    if (!root_pattern || pattern + class_begin < root_pattern ||
        (size_t)(pattern + class_begin - root_pattern) > UINT32_MAX ||
        current - class_begin > UINT32_MAX)
        return REG_ESPACE;
    atom->value = (uint8_t)negated;
    atom->first = (uint32_t)(pattern + class_begin - root_pattern);
    atom->second = (uint32_t)(current - class_begin);
    (void)cflags;
    *offset = current;
    return 0;
}

static inline int _regex_class_add_element(
        _regex_internal_t* internal, _regex_class_t* expression,
        uint32_t kind, const uint32_t* first, size_t first_count,
        const uint32_t* second, size_t second_count, wctype_t descriptor) {
    _regex_class_element_t* element;
    if (!internal || !expression ||
        internal->class_element_count >= internal->class_element_capacity)
        return REG_ESPACE;
    if ((first_count != 0u &&
         (!first || first_count > _REGEX_MAX_COLLATION_CHARS ||
          first_count > internal->class_character_capacity -
                            internal->class_character_count)) ||
        (second_count != 0u &&
         (!second || second_count > _REGEX_MAX_COLLATION_CHARS ||
          second_count > internal->class_character_capacity -
                             internal->class_character_count - first_count)))
        return REG_ESPACE;
    element = &internal->class_elements[internal->class_element_count++];
    element->first_offset = internal->class_character_count;
    element->first_count = (uint32_t)first_count;
    for (size_t index = 0u; index < first_count; ++index)
        internal->class_characters[internal->class_character_count++] =
            first[index];
    element->second_offset = internal->class_character_count;
    element->second_count = (uint32_t)second_count;
    for (size_t index = 0u; index < second_count; ++index)
        internal->class_characters[internal->class_character_count++] =
            second[index];
    element->kind = kind;
    element->descriptor = descriptor;
    if ((kind == _REGEX_CLASS_RANGE &&
         (first_count > 1u || second_count > 1u)) ||
        kind == _REGEX_CLASS_COLLATING || kind == _REGEX_CLASS_EQUIVALENCE)
        internal->has_variable_class = 1u;
    ++expression->element_count;
    return 0;
}

/* Parse one literal or collating/equivalence element from a bracket class.
 * Collating and equivalence elements may contain multiple locale characters. */
static inline int _regex_class_parse_element(
        const char* pattern, size_t end, size_t* offset,
        uint32_t* kind, uint32_t characters[_REGEX_MAX_COLLATION_CHARS],
        size_t* character_count) {
    size_t current = *offset;
    if (current >= end || !kind || !characters || !character_count)
        return REG_EBRACK;
    *kind = _REGEX_CLASS_CHARACTER;
    if (pattern[current] == '[' && current + 1u < end &&
        (pattern[current + 1u] == '.' || pattern[current + 1u] == '=')) {
        char delimiter = pattern[current + 1u];
        size_t content_begin = current + 2u;
        size_t content_end = content_begin;
        while (content_end + 1u < end &&
               !(pattern[content_end] == delimiter &&
                 pattern[content_end + 1u] == ']'))
            ++content_end;
        if (content_end + 1u >= end || content_end == content_begin)
            return REG_ECOLLATE;
        current = content_begin;
        *character_count = 0u;
        while (current < content_end) {
            size_t width;
            uint32_t character;
            if (*character_count >= _REGEX_MAX_COLLATION_CHARS)
                return REG_ESPACE;
            if (!_regex_decode_one(pattern + current, content_end - current,
                                   &character, &width) ||
                width == 0u || character > UINT32_C(0x10ffff))
                return REG_ECOLLATE;
            characters[(*character_count)++] = character;
            current += width;
        }
        if (*character_count > 1u && rin_locale_current_mb_cur_max() <= 1)
            return REG_ECOLLATE;
        *kind = delimiter == '.' ? _REGEX_CLASS_COLLATING
                                 : _REGEX_CLASS_EQUIVALENCE;
        *offset = content_end + 2u;
        return 0;
    }
    {
        size_t width;
        if (!_regex_decode_one(pattern + current, end - current,
                               &characters[0], &width))
            return REG_ECTYPE;
        *character_count = 1u;
        *offset = current + width;
    }
    if (*character_count == 0u)
        return REG_ECTYPE;
    return 0;
}

static inline int _regex_compile_class_record(
        _regex_internal_t* internal, uint32_t source_offset,
        uint32_t source_size, _regex_class_t* expression) {
    size_t current = (size_t)source_offset + 1u;
    size_t end = (size_t)source_offset + source_size - 1u;
    int have_item = 0;
    int result;
    if (!internal || !expression || source_size < 2u ||
        end > internal->pattern_size ||
        internal->pattern[source_offset] != '[' ||
        internal->pattern[end] != ']')
        return REG_EBRACK;
    _regex_zero(expression, sizeof(*expression));
    expression->source_offset = source_offset;
    expression->source_size = source_size;
    expression->element_begin = internal->class_element_count;
    if (current < end && internal->pattern[current] == '^') {
        expression->negated = 1u;
        ++current;
    }
    if (current < end && internal->pattern[current] == ']') {
        const uint32_t close_bracket[] = {']'};
        result = _regex_class_add_element(
            internal, expression, _REGEX_CLASS_CHARACTER,
            close_bracket, 1u, NULL, 0u, 0ul);
        if (result != 0) return result;
        have_item = 1;
        ++current;
    }
    while (current < end) {
        uint32_t first_kind;
        uint32_t first[_REGEX_MAX_COLLATION_CHARS];
        size_t first_count;
        uint32_t second_kind;
        uint32_t second[_REGEX_MAX_COLLATION_CHARS];
        size_t second_count;
        if (internal->pattern[current] == '[' && current + 1u < end &&
            internal->pattern[current + 1u] == ':') {
            size_t name_begin = current + 2u;
            size_t name_end = name_begin;
            uint32_t property;
            wctype_t descriptor;
            char name[32];
            size_t length;
            while (name_end + 1u < end &&
                   !(internal->pattern[name_end] == ':' &&
                     internal->pattern[name_end + 1u] == ']'))
                ++name_end;
            if (name_end + 1u >= end) return REG_EBRACK;
            length = name_end - name_begin;
            property = _regex_class_property(
                internal->pattern + name_begin, length);
            if (property == 0u || length == 0u || length >= sizeof(name))
                return REG_ECTYPE;
            for (size_t index = 0u; index < length; ++index)
                name[index] = internal->pattern[name_begin + index];
            name[length] = '\0';
            descriptor = wctype(name);
            if (descriptor == (wctype_t)0u) return REG_ECTYPE;
            result = _regex_class_add_element(
                internal, expression, _REGEX_CLASS_PROPERTY,
                NULL, 0u, NULL, 0u, descriptor);
            if (result != 0) return result;
            current = name_end + 2u;
            have_item = 1;
            continue;
        }
        result = _regex_class_parse_element(
            internal->pattern, end, &current, &first_kind, first,
            &first_count);
        if (result != 0) return result;
        if (current < end && internal->pattern[current] == '-' &&
            current + 1u < end && internal->pattern[current + 1u] != ']') {
            uint32_t first_text[_REGEX_MAX_COLLATION_CHARS + 1u];
            uint32_t second_text[_REGEX_MAX_COLLATION_CHARS + 1u];
            ++current;
            result = _regex_class_parse_element(
                internal->pattern, end, &current, &second_kind, second,
                &second_count);
            if (result != 0) return result;
            if (first_kind == _REGEX_CLASS_EQUIVALENCE ||
                second_kind == _REGEX_CLASS_EQUIVALENCE ||
                first_kind == _REGEX_CLASS_PROPERTY ||
                second_kind == _REGEX_CLASS_PROPERTY)
                return REG_ERANGE;
            for (size_t index = 0u; index < first_count; ++index)
                first_text[index] = first[index];
            first_text[first_count] = 0u;
            for (size_t index = 0u; index < second_count; ++index)
                second_text[index] = second[index];
            second_text[second_count] = 0u;
            if (_regex_collate(first_text, second_text) > 0)
                return REG_ERANGE;
            result = _regex_class_add_element(
                internal, expression, _REGEX_CLASS_RANGE,
                first, first_count, second, second_count, 0ul);
        } else {
            result = _regex_class_add_element(
                internal, expression, first_kind, first, first_count,
                NULL, 0u, 0ul);
        }
        if (result != 0) return result;
        have_item = 1;
    }
    if (!have_item) return REG_EBRACK;
    return 0;
}

static inline int _regex_compile_class_table(_regex_internal_t* internal) {
    uint32_t instruction_index;
    for (instruction_index = 0u;
         instruction_index < internal->instruction_count;
         ++instruction_index) {
        _regex_instruction_t* instruction =
            &internal->instructions[instruction_index];
        uint32_t class_index;
        uint32_t source_offset;
        uint32_t source_size;
        if (instruction->operation != _REGEX_OP_CLASS) continue;
        source_offset = instruction->first;
        source_size = instruction->second;
        for (class_index = 0u; class_index < internal->class_count;
             ++class_index) {
            if (internal->classes[class_index].source_offset == source_offset &&
                internal->classes[class_index].source_size == source_size)
                break;
        }
        if (class_index == internal->class_count) {
            int result;
            if (internal->class_count >= internal->class_capacity)
                return REG_ESPACE;
            result = _regex_compile_class_record(
                internal, source_offset, source_size,
                &internal->classes[class_index]);
            if (result != 0) return result;
            ++internal->class_count;
        }
        instruction->first = class_index;
        instruction->second = 0u;
    }
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
        const char* root_pattern,
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
        int cflags, const char* root_pattern, size_t* group_end) {
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
                pattern, pattern_size, &current, &ignored, cflags,
                root_pattern);
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
        const char* root_pattern,
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
            pattern, pattern_size, cflags, root_pattern,
            instructions, capacity, count,
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
                pattern, pattern_size, cflags, root_pattern,
                instructions, capacity, count,
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
            pattern, pattern_size, cflags, root_pattern,
            instructions, capacity, count,
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
        const char* root_pattern,
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
                pattern, pattern_size, current, cflags, root_pattern,
                &group_end);
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
                root_pattern, instructions, capacity, count,
                minimum, maximum, group_cursor,
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
                                        &atom, cflags, root_pattern);
            if (result != 0) return result;
        } else if (value == '.') {
            atom.operation = _REGEX_OP_ANY;
            ++current;
        } else if (value == '\\') {
            size_t reference_begin;
            uint32_t reference = 0u;
            uint32_t literal;
            size_t literal_width;
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
                if (!_regex_decode_one(pattern + current,
                        pattern_size - current, &literal, &literal_width))
                    return REG_ECTYPE;
                current += literal_width;
            }
            if (atom.operation == 0u) {
                if (literal <= UINT8_MAX) {
                    atom.operation = _REGEX_OP_BYTE;
                    atom.value = (uint8_t)literal;
                } else {
                    atom.operation = _REGEX_OP_CHAR;
                    atom.first = literal;
                }
            }
        } else {
            uint32_t literal;
            size_t literal_width;
            if (!_regex_decode_one(pattern + current,
                    pattern_size - current, &literal, &literal_width))
                return REG_ECTYPE;
            if (literal <= UINT8_MAX) {
                atom.operation = _REGEX_OP_BYTE;
                atom.value = (uint8_t)literal;
            } else {
                atom.operation = _REGEX_OP_CHAR;
                atom.first = literal;
            }
            current += literal_width;
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
        int cflags, const char* root_pattern,
        size_t* branch_end, int* has_separator,
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
                pattern, pattern_size, &current, &ignored, cflags,
                root_pattern);
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
        const char* root_pattern,
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
            pattern, pattern_size, branch_begin, cflags, root_pattern,
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
                cflags, root_pattern, instructions, capacity, count,
                group_cursor, depth);
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
            cflags, root_pattern, instructions, capacity, count,
            group_cursor, depth);
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
        pattern, pattern_size, cflags, pattern,
        instructions, capacity, &count,
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
    uint32_t has_variable_class = 0u;
    size_t expected_size;
    uint64_t size_max = (uint64_t)(size_t)-1;
    if (!internal || internal->magic != _REGEX_MAGIC ||
        internal->instruction_count == 0u ||
        internal->instruction_count > _REGEX_MAX_INSTRUCTIONS ||
        internal->group_count > _REGEX_MAX_GROUPS ||
        (internal->cflags & ~known_flags) != 0 || !internal->instructions ||
        internal->pattern_size > _REGEX_MAX_PATTERN || !internal->pattern ||
        !internal->locale ||
        internal->class_count > internal->class_capacity ||
        internal->class_capacity > _REGEX_MAX_PATTERN + 1u ||
        internal->class_element_count > internal->class_element_capacity ||
        internal->class_element_capacity > _REGEX_MAX_PATTERN + 1u ||
        internal->class_character_count >
            internal->class_character_capacity ||
        internal->class_character_capacity > _REGEX_MAX_PATTERN + 1u ||
        internal->has_variable_class > 1u ||
        (internal->class_count != 0u && !internal->classes) ||
        (internal->class_element_count != 0u && !internal->class_elements) ||
        (internal->class_character_count != 0u &&
         !internal->class_characters) ||
        !_regex_all_zero(internal->reserved, sizeof(internal->reserved)) ||
        internal->pattern[internal->pattern_size] != '\0')
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
    for (index = 0u; index < internal->class_count; ++index) {
        const _regex_class_t* expression = &internal->classes[index];
        if (expression->source_size < 2u ||
            expression->source_offset > internal->pattern_size ||
            expression->source_size > internal->pattern_size -
                                      expression->source_offset ||
            expression->element_begin > internal->class_element_count ||
            expression->element_count > internal->class_element_count -
                                       expression->element_begin ||
            expression->negated > 1u ||
            expression->reserved[0] != 0u ||
            expression->reserved[1] != 0u ||
            expression->reserved[2] != 0u)
            return 0;
    }
    for (index = 0u; index < internal->class_element_count; ++index) {
        const _regex_class_element_t* element =
            &internal->class_elements[index];
        if ((element->kind == _REGEX_CLASS_RANGE &&
             (element->first_count > 1u || element->second_count > 1u)) ||
            element->kind == _REGEX_CLASS_COLLATING ||
            element->kind == _REGEX_CLASS_EQUIVALENCE)
            has_variable_class = 1u;
        if (element->kind < _REGEX_CLASS_CHARACTER ||
            element->kind > _REGEX_CLASS_EQUIVALENCE ||
            element->first_offset > internal->class_character_count ||
            element->second_offset > internal->class_character_count ||
            (element->kind == _REGEX_CLASS_PROPERTY &&
             element->descriptor == (wctype_t)0u) ||
            (element->kind != _REGEX_CLASS_PROPERTY &&
             element->descriptor != (wctype_t)0u) ||
            (element->kind == _REGEX_CLASS_PROPERTY &&
             (element->first_count != 0u || element->second_count != 0u)) ||
            (element->kind == _REGEX_CLASS_CHARACTER &&
             element->first_count != 1u) ||
            (element->kind != _REGEX_CLASS_PROPERTY &&
             (element->first_count == 0u ||
              element->first_count > _REGEX_MAX_COLLATION_CHARS ||
              element->first_offset > internal->class_character_count ||
              element->first_count > internal->class_character_count -
                                     element->first_offset)) ||
            (element->kind == _REGEX_CLASS_RANGE
                 ? (element->second_count == 0u ||
                    element->second_count > _REGEX_MAX_COLLATION_CHARS ||
                    element->second_offset > internal->class_character_count ||
                    element->second_count > internal->class_character_count -
                                            element->second_offset)
                 : element->second_count != 0u))
            return 0;
    }
    if (has_variable_class != internal->has_variable_class) return 0;
    for (index = 0u; index < internal->class_character_count; ++index)
        if (internal->class_characters[index] > UINT32_C(0x1100ff))
            return 0;
    for (index = 0u; index < internal->instruction_count; ++index) {
        const _regex_instruction_t* instruction = &internal->instructions[index];
        if (instruction->reserved != 0u) return 0;
        switch (instruction->operation) {
            case _REGEX_OP_BYTE:
            case _REGEX_OP_ANY:
                if (index + 1u >= internal->instruction_count ||
                    instruction->first != 0u || instruction->second != 0u)
                    return 0;
                if (!_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap))) return 0;
                if (instruction->operation == _REGEX_OP_ANY &&
                    instruction->value != 0u) return 0;
                break;
            case _REGEX_OP_CHAR:
                if (index + 1u >= internal->instruction_count ||
                    instruction->value != 0u || instruction->second != 0u ||
                    instruction->first > UINT32_C(0x1100FF) ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)))
                    return 0;
                break;
            case _REGEX_OP_CLASS:
                if (index + 1u >= internal->instruction_count ||
                    instruction->first >= internal->class_count ||
                    instruction->second != 0u || instruction->value > 1u ||
                    internal->classes[instruction->first].negated !=
                        instruction->value ||
                    !_regex_all_zero(instruction->bitmap,
                                     sizeof(instruction->bitmap)))
                    return 0;
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
                                      const char* root_pattern,
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
                pattern, pattern_size, &current, &ignored, cflags,
                root_pattern);
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

static inline void _regex_internal_destroy(_regex_internal_t* internal) {
    if (!internal) return;
    if (internal->locale) freelocale(internal->locale);
    free(internal->class_elements);
    free(internal->class_characters);
    free(internal->classes);
    free(internal->pattern);
    free(internal->instructions);
    _regex_zero(internal, sizeof(*internal));
    free(internal);
}

static inline int regcomp(regex_t* preg, const char* pattern, int cflags) {
    const int known_flags = REG_EXTENDED | REG_ICASE | REG_NOSUB | REG_NEWLINE;
    _regex_internal_t* internal = NULL;
    uint32_t instruction_count = 0u;
    size_t pattern_size = 0u;
    size_t group_count = 0u;
    uint32_t compiled_group_count = 0u;
    int compile_flags;
    size_t allocation_size;
    uint32_t class_capacity;
    uint64_t size_max = (uint64_t)(size_t)-1;
    locale_t compile_locale = (locale_t)0;
    locale_t previous_locale = (locale_t)0;
    int locale_switched = 0;
    int final_result = REG_ESPACE;
    int result;
    if (!preg || !pattern) return REG_INVARG;
    preg->_internal = NULL;
    preg->re_nsub = 0u;
    if ((cflags & ~known_flags) != 0) return REG_INVARG;
    compile_locale = duplocale(uselocale((locale_t)0));
    if (!compile_locale) return REG_ESPACE;
    previous_locale = uselocale(compile_locale);
    if (!previous_locale) {
        freelocale(compile_locale);
        return REG_ESPACE;
    }
    locale_switched = 1;
    while (pattern_size <= _REGEX_MAX_PATTERN && pattern[pattern_size] != '\0')
        ++pattern_size;
    if (pattern_size > _REGEX_MAX_PATTERN) goto regcomp_done;
    compile_flags = cflags;
    if (_regex_pattern_has_backref(pattern, pattern_size, cflags))
        compile_flags |= _REGEX_COMPILE_CAPTURE;
    result = _regex_count_groups(
        pattern, pattern_size, cflags, pattern, &group_count);
    if (result != 0) {
        final_result = result;
        goto regcomp_done;
    }
    if (group_count > _REGEX_MAX_GROUPS) goto regcomp_done;
    result = _regex_compile_pass(pattern, pattern_size, compile_flags, NULL, 0u,
                                 &instruction_count, &compiled_group_count);
    if (result != 0) {
        final_result = result;
        goto regcomp_done;
    }
    if ((cflags & REG_NOSUB) == 0 && compiled_group_count != group_count)
        { final_result = REG_BADPAT; goto regcomp_done; }
    if ((uint64_t)instruction_count >
        (size_max - sizeof(*internal)) / sizeof(_regex_instruction_t))
        goto regcomp_done;
    allocation_size = sizeof(*internal) +
                      (size_t)instruction_count * sizeof(_regex_instruction_t);
    internal = (_regex_internal_t*)malloc(sizeof(*internal));
    if (!internal) goto regcomp_done;
    _regex_zero(internal, sizeof(*internal));
    internal->locale = compile_locale;
    compile_locale = (locale_t)0;
    class_capacity = (uint32_t)pattern_size + 1u;
    internal->class_capacity = class_capacity;
    internal->class_element_capacity = class_capacity;
    internal->class_character_capacity = class_capacity;
    internal->pattern_size = pattern_size;
    internal->pattern = (char*)malloc(pattern_size + 1u);
    internal->classes = (_regex_class_t*)malloc(
        (size_t)class_capacity * sizeof(*internal->classes));
    internal->class_elements = (_regex_class_element_t*)malloc(
        (size_t)class_capacity * sizeof(*internal->class_elements));
    internal->class_characters = (uint32_t*)malloc(
        (size_t)class_capacity * sizeof(*internal->class_characters));
    internal->instructions = (_regex_instruction_t*)malloc(
        (size_t)instruction_count * sizeof(_regex_instruction_t));
    if (!internal->pattern || !internal->classes ||
        !internal->class_elements || !internal->class_characters ||
        !internal->instructions) {
        goto regcomp_done;
    }
    for (size_t pattern_index = 0u; pattern_index < pattern_size;
         ++pattern_index)
        internal->pattern[pattern_index] = pattern[pattern_index];
    internal->pattern[pattern_size] = '\0';
    _regex_zero(internal->instructions,
                (size_t)instruction_count * sizeof(_regex_instruction_t));
    result = _regex_compile_pass(pattern, pattern_size, compile_flags,
                                 internal->instructions, instruction_count,
                                 &internal->instruction_count,
                                 &compiled_group_count);
    if (result != 0 || internal->instruction_count != instruction_count ||
        ((cflags & REG_NOSUB) == 0 && compiled_group_count != group_count)) {
        final_result = result != 0 ? result : REG_BADPAT;
        goto regcomp_done;
    }
    result = _regex_compile_class_table(internal);
    if (result != 0) {
        final_result = result;
        goto regcomp_done;
    }
    internal->allocation_size = allocation_size;
    internal->group_count = (uint32_t)group_count;
    internal->cflags = cflags;
    internal->magic = _REGEX_MAGIC;
    if (!_regex_program_valid(internal)) {
        final_result = REG_BADPAT;
        goto regcomp_done;
    }
    preg->_internal = internal;
    preg->re_nsub = group_count;
    internal = NULL;
    final_result = 0;
regcomp_done:
    if (locale_switched) (void)uselocale(previous_locale);
    if (internal) _regex_internal_destroy(internal);
    if (compile_locale) freelocale(compile_locale);
    return final_result;
}

static inline int _regex_bol(const _regex_internal_t* internal,
                             const uint32_t* input, size_t position,
                             int eflags) {
    if (position == 0u && (eflags & REG_NOTBOL) == 0) return 1;
    return (internal->cflags & REG_NEWLINE) != 0 && position != 0u &&
           input[position - 1u] == (uint32_t)'\n';
}

static inline int _regex_eol(const _regex_internal_t* internal,
                             const uint32_t* input, size_t input_size,
                             size_t position, int eflags) {
    if (position == input_size && (eflags & REG_NOTEOL) == 0) return 1;
    return (internal->cflags & REG_NEWLINE) != 0 && position < input_size &&
           input[position] == (uint32_t)'\n';
}

static inline void _regex_copy_captures(size_t* destination,
                                        const size_t* source,
                                        size_t capture_slots) {
    size_t index;
    for (index = 0u; index < capture_slots; ++index)
        destination[index] = source[index];
}

/* Capture vectors exist only for states present in the active frontier. */
static inline int _regex_capture_state_reserve(
        size_t** storage_io, size_t capture_slots, size_t used_states,
        size_t required_states, size_t state_limit, size_t* capacity_io) {
    size_t capacity;
    size_t elements;
    size_t bytes;
    size_t* replacement;
    if (!storage_io || !capacity_io || capture_slots == 0u ||
        required_states > state_limit || used_states > *capacity_io)
        return 0;
    if (required_states <= *capacity_io) return 1;
    if (required_states > (size_t)-1 / capture_slots) return 0;
    capacity = *capacity_io != 0u ? *capacity_io : 1u;
    while (capacity < required_states) {
        if (capacity > state_limit / 2u) capacity = state_limit;
        else capacity *= 2u;
    }
    if (capacity > (size_t)-1 / capture_slots) return 0;
    elements = capacity * capture_slots;
    if (elements > (size_t)-1 / sizeof(*replacement)) return 0;
    bytes = elements * sizeof(*replacement);
    replacement = (size_t*)malloc(bytes);
    if (!replacement) return 0;
    if (*storage_io && used_states != 0u)
        _regex_copy_captures(replacement, *storage_io,
                             used_states * capture_slots);
    free(*storage_io);
    *storage_io = replacement;
    *capacity_io = capacity;
    return 1;
}

static inline int _regex_closure(const _regex_internal_t* internal,
                                 const uint32_t* input,
                                 size_t input_size, size_t position,
                                 int eflags, size_t* states,
                                 uint32_t* state_generation,
                                 uint32_t state_epoch,
                                 uint32_t* active_indices,
                                 uint32_t* active_count, size_t* queue,
                                 uint32_t* queued_generation,
                                 uint32_t queue_epoch,
                                 uint32_t* capture_index_by_pc,
                                 size_t** capture_state_io,
                                 size_t* capture_capacity_io,
                                 size_t capture_slots, uint64_t* steps) {
    const size_t none = (size_t)-1;
    uint32_t count = internal->instruction_count;
    uint32_t head = 0u, tail = 0u, queued_count = 0u, seed;
    size_t* capture_state = capture_state_io ? *capture_state_io : NULL;
    for (seed = 0u; seed < *active_count; ++seed) {
        uint32_t pc = active_indices[seed];
        if (pc >= count || state_generation[pc] != state_epoch) return 0;
        if (queued_generation[pc] == queue_epoch) continue;
        if (queued_count >= count) return 0;
        queue[tail] = pc;
        tail = (tail + 1u) % count;
        queued_generation[pc] = queue_epoch;
        ++queued_count;
    }
    while (queued_count != 0u) {
        const _regex_instruction_t* instruction;
        size_t origin;
        uint32_t pc = (uint32_t)queue[head];
        head = (head + 1u) % count;
        --queued_count;
        queued_generation[pc] = 0u;
        if (++*steps > _REGEX_MAX_STEPS) return 0;
        origin = states[pc];
        instruction = &internal->instructions[pc];
#define _REGEX_OFFER(target_) do {                                         \
    uint32_t _target = (uint32_t)(target_);                                \
    size_t _target_origin = state_generation[_target] == state_epoch       \
        ? states[_target] : none;                                           \
    int _replace = origin < _target_origin;                                 \
    if (!_replace && origin == _target_origin && capture_slots != 0u &&    \
        (instruction->operation == _REGEX_OP_SAVE_START ||                 \
         instruction->operation == _REGEX_OP_SAVE_END)) {                  \
        size_t _slot = (size_t)instruction->first * 2u +                   \
                       (instruction->operation == _REGEX_OP_SAVE_END);     \
        size_t _target_state =                                               \
            (size_t)capture_index_by_pc[_target] * capture_slots;            \
        if (capture_state[_target_state + _slot] !=                          \
                (size_t)-1 &&                                               \
            position > capture_state[_target_state + _slot])                \
            _replace = 1;                                                   \
    }                                                                        \
    if (_replace) {                                                          \
        if (state_generation[_target] != state_epoch) {                     \
            if (*active_count >= count) return 0;                           \
            if (capture_slots != 0u &&                                     \
                !_regex_capture_state_reserve(                              \
                    capture_state_io, capture_slots, *active_count,          \
                    (size_t)*active_count + 1u, count,                      \
                    capture_capacity_io)) return 0;                         \
            capture_state = capture_state_io                                \
                ? *capture_state_io : NULL;                                  \
            state_generation[_target] = state_epoch;                       \
            active_indices[(*active_count)++] = _target;                    \
            if (capture_slots != 0u)                                        \
                capture_index_by_pc[_target] = *active_count - 1u;          \
        }                                                                    \
        states[_target] = origin;                                          \
        if (capture_slots != 0u) {                                         \
            size_t _source_state =                                          \
                (size_t)capture_index_by_pc[pc] * capture_slots;            \
            size_t _target_state =                                          \
                (size_t)capture_index_by_pc[_target] * capture_slots;      \
            _regex_copy_captures(                                          \
                capture_state + _target_state,                              \
                capture_state + _source_state, capture_slots);              \
            if (instruction->operation == _REGEX_OP_SAVE_START)            \
                capture_state[_target_state +                               \
                              (size_t)instruction->first * 2u] = position; \
            else if (instruction->operation == _REGEX_OP_SAVE_END)         \
                capture_state[_target_state +                               \
                              (size_t)instruction->first * 2u + 1u] =      \
                    position;                                              \
        }                                                                    \
        if (queued_generation[_target] != queue_epoch) {                   \
            if (queued_count >= count) return 0;                           \
            queue[tail] = _target;                                         \
            tail = (tail + 1u) % count;                                    \
            queued_generation[_target] = queue_epoch;                      \
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

static inline int _regex_collate_sequences(
        const _regex_internal_t* internal, const uint32_t* left,
        size_t left_count, const uint32_t* right, size_t right_count);

static inline int _regex_instruction_matches(
        const _regex_internal_t* internal,
        const _regex_instruction_t* instruction, uint32_t value) {
    if (instruction->operation == _REGEX_OP_BYTE) {
        if ((internal->cflags & REG_ICASE) != 0) {
            return _regex_fold(instruction->value) == _regex_fold(value);
        }
        return (uint32_t)instruction->value == value;
    }
    if (instruction->operation == _REGEX_OP_CHAR) {
        uint32_t expected = instruction->first;
        if ((internal->cflags & REG_ICASE) != 0) {
            expected = _regex_fold(expected);
            value = _regex_fold(value);
        }
        return expected == value;
    }
    if (instruction->operation == _REGEX_OP_ANY)
        return (internal->cflags & REG_NEWLINE) == 0 || value != '\n';
    if (instruction->operation == _REGEX_OP_CLASS) {
        const _regex_class_t* expression =
            &internal->classes[instruction->first];
        uint32_t index;
        int member = 0;
        if (expression->negated &&
            (internal->cflags & REG_NEWLINE) != 0 && value == '\n')
            return 0;
        for (index = expression->element_begin;
             index < expression->element_begin + expression->element_count;
             ++index) {
            const _regex_class_element_t* element =
                &internal->class_elements[index];
            if (element->kind == _REGEX_CLASS_PROPERTY) {
                member = _regex_iswctype(value, element->descriptor) != 0;
            } else if (element->kind == _REGEX_CLASS_RANGE) {
                const uint32_t* first = internal->class_characters +
                                        element->first_offset;
                const uint32_t* second = internal->class_characters +
                                         element->second_offset;
                uint32_t candidate_text[2] = {value, 0u};
                uint32_t first_text[2] = {first[0], 0u};
                uint32_t second_text[2] = {second[0], 0u};
                if (element->first_count != 1u ||
                    element->second_count != 1u)
                    return 0;
                if ((internal->cflags & REG_ICASE) != 0) {
                    candidate_text[0] = _regex_fold(candidate_text[0]);
                    first_text[0] = _regex_fold(first_text[0]);
                    second_text[0] = _regex_fold(second_text[0]);
                }
                member = _regex_collate(first_text, candidate_text) <= 0 &&
                         _regex_collate(candidate_text, second_text) <= 0;
            } else {
                uint32_t candidate = value;
                uint32_t expected = internal->class_characters[
                    element->first_offset];
                if (element->kind != _REGEX_CLASS_CHARACTER ||
                    element->first_count != 1u)
                    return 0;
                if ((internal->cflags & REG_ICASE) != 0) {
                    candidate = _regex_fold(candidate);
                    expected = _regex_fold(expected);
                }
                member = candidate == expected;
            }
            if (member) break;
        }
        return expression->negated ? !member : member;
    }
    return 0;
}

static inline int _regex_collate_sequences(
        const _regex_internal_t* internal, const uint32_t* left,
        size_t left_count, const uint32_t* right, size_t right_count) {
    uint32_t folded_left[_REGEX_MAX_COLLATION_CHARS + 1u];
    uint32_t folded_right[_REGEX_MAX_COLLATION_CHARS + 1u];
    size_t index;
    if (!left || !right || left_count == 0u || right_count == 0u ||
        left_count > _REGEX_MAX_COLLATION_CHARS ||
        right_count > _REGEX_MAX_COLLATION_CHARS)
        return 0;
    for (index = 0u; index < left_count; ++index) {
        if (left[index] > UINT32_C(0x10ffff)) break;
        folded_left[index] = (internal->cflags & REG_ICASE) != 0
            ? _regex_fold(left[index]) : left[index];
    }
    if (index != left_count) {
        for (index = 0u; index < left_count && index < right_count; ++index) {
            uint32_t left_value = (internal->cflags & REG_ICASE) != 0
                ? _regex_fold(left[index]) : left[index];
            uint32_t right_value = (internal->cflags & REG_ICASE) != 0
                ? _regex_fold(right[index]) : right[index];
            if (left_value != right_value)
                return left_value < right_value ? -1 : 1;
        }
        return left_count < right_count ? -1 :
               (left_count > right_count ? 1 : 0);
    }
    for (index = 0u; index < right_count; ++index) {
        if (right[index] > UINT32_C(0x10ffff)) break;
        folded_right[index] = (internal->cflags & REG_ICASE) != 0
            ? _regex_fold(right[index]) : right[index];
    }
    if (index != right_count) {
        for (index = 0u; index < left_count && index < right_count; ++index) {
            uint32_t left_value = (internal->cflags & REG_ICASE) != 0
                ? _regex_fold(left[index]) : left[index];
            uint32_t right_value = (internal->cflags & REG_ICASE) != 0
                ? _regex_fold(right[index]) : right[index];
            if (left_value != right_value)
                return left_value < right_value ? -1 : 1;
        }
        return left_count < right_count ? -1 :
               (left_count > right_count ? 1 : 0);
    }
    folded_left[left_count] = 0u;
    folded_right[right_count] = 0u;
    return _regex_collate(folded_left, folded_right);
}

static inline void _regex_class_add_end(size_t ends[_REGEX_MAX_COLLATION_CHARS],
                                        size_t* end_count, size_t end) {
    size_t index;
    for (index = 0u; index < *end_count; ++index)
        if (ends[index] == end) return;
    if (*end_count < _REGEX_MAX_COLLATION_CHARS)
        ends[(*end_count)++] = end;
}

/* Return all input boundaries a bracket expression can consume. POSIX
 * collating/equivalence members can span several decoded characters, so the
 * backtracking engine explores each bounded candidate length. */
static inline int _regex_class_match_ends(
        const _regex_internal_t* internal,
        const _regex_instruction_t* instruction, const uint32_t* input,
        size_t input_size, size_t position, size_t ends[_REGEX_MAX_COLLATION_CHARS],
        size_t* end_count, uint64_t* steps) {
    const _regex_class_t* expression;
    uint32_t index;
    size_t available;
    if (!internal || !instruction || !input || !ends || !end_count ||
        instruction->first >= internal->class_count ||
        position >= input_size)
        return 0;
    *end_count = 0u;
    expression = &internal->classes[instruction->first];
    available = input_size - position;
    if (available > _REGEX_MAX_COLLATION_CHARS)
        available = _REGEX_MAX_COLLATION_CHARS;
    for (index = expression->element_begin;
         index < expression->element_begin + expression->element_count;
         ++index) {
        const _regex_class_element_t* element =
            &internal->class_elements[index];
        size_t length;
        size_t candidate_limit = available;
        if (steps && ++*steps > _REGEX_MAX_STEPS) return -1;
        if (element->kind == _REGEX_CLASS_PROPERTY) {
            if (_regex_iswctype(input[position], element->descriptor))
                _regex_class_add_end(ends, end_count, position + 1u);
            continue;
        }
        if (element->kind == _REGEX_CLASS_CHARACTER) {
            if (element->first_count == 1u &&
                ((internal->cflags & REG_ICASE) != 0
                    ? _regex_fold(input[position]) == _regex_fold(
                          internal->class_characters[element->first_offset])
                    : input[position] ==
                          internal->class_characters[element->first_offset]))
                _regex_class_add_end(ends, end_count, position + 1u);
            continue;
        }
        if (element->kind == _REGEX_CLASS_RANGE) {
            candidate_limit = element->first_count > element->second_count
                ? element->first_count : element->second_count;
            if (candidate_limit > available) candidate_limit = available;
        }
        for (length = 1u; length <= candidate_limit; ++length) {
            int matches = 0;
            if (steps && ++*steps > _REGEX_MAX_STEPS) return -1;
            if (element->kind == _REGEX_CLASS_RANGE) {
                const uint32_t* first = internal->class_characters +
                                        element->first_offset;
                const uint32_t* second = internal->class_characters +
                                         element->second_offset;
                matches = _regex_collate_sequences(
                              internal, first, element->first_count,
                              input + position, length) <= 0 &&
                          _regex_collate_sequences(
                              internal, input + position, length,
                              second, element->second_count) <= 0;
            } else if (element->kind == _REGEX_CLASS_COLLATING ||
                       element->kind == _REGEX_CLASS_EQUIVALENCE) {
                matches = _regex_collate_sequences(
                    internal, input + position, length,
                    internal->class_characters + element->first_offset,
                    element->first_count) == 0;
            }
            if (matches) _regex_class_add_end(ends, end_count,
                                               position + length);
        }
    }
    if (expression->negated) {
        int newline_excluded = (internal->cflags & REG_NEWLINE) != 0 &&
                               input[position] == (uint32_t)'\n';
        if (*end_count != 0u || newline_excluded) {
            *end_count = 0u;
            return 0;
        }
        ends[0] = position + 1u;
        *end_count = 1u;
    }
    return (int)*end_count;
}

typedef struct {
    uint32_t pc;
    size_t position;
    size_t first_end;
    size_t saved_slot;
    size_t saved_value;
    size_t alternative_count;
    size_t alternative_index;
    size_t alternatives[_REGEX_MAX_COLLATION_CHARS];
    int first_result;
    uint8_t stage;
    size_t* original;
    size_t* first_captures;
} _regex_backref_frame_t;

typedef struct {
    const _regex_internal_t* internal;
    const uint32_t* input;
    const size_t* byte_offsets;
    size_t input_size;
    int eflags;
    size_t capture_slots;
    uint64_t steps;
    _regex_backref_frame_t* frames;
    size_t* snapshots;
} _regex_backref_context_t;

static inline void _regex_backref_frame_init(
        _regex_backref_context_t* context, size_t index,
        uint32_t pc, size_t position) {
    _regex_backref_frame_t* frame = &context->frames[index];
    frame->pc = pc;
    frame->position = position;
    frame->stage = 0u;
    if (context->capture_slots != 0u) {
        size_t slots_per_frame = context->capture_slots * 2u;
        frame->original = context->snapshots + index * slots_per_frame;
        frame->first_captures = frame->original + context->capture_slots;
    } else {
        frame->original = NULL;
        frame->first_captures = NULL;
    }
}

/* Backreferences need DFS because they cannot be represented by the
 * one-byte-at-a-time Thompson state set above.  Keep continuation and capture
 * snapshots in a bounded heap workspace so a small caller stack cannot be
 * exhausted by a deep path. */
static inline int _regex_backref_match(
        _regex_backref_context_t* context, uint32_t pc, size_t position,
        size_t* captures, size_t* end_out) {
    const size_t none = (size_t)-1;
    const size_t slots = context->capture_slots;
    size_t frame_count = 1u;
    size_t returned_end = none;
    int returned_result = 0;
    int returning = 0;

    _regex_backref_frame_init(context, 0u, pc, position);
    for (;;) {
        _regex_backref_frame_t* frame =
            &context->frames[frame_count - 1u];
        if (returning) {
            if (frame->stage == 0u) {
                --frame_count;
                if (frame_count == 0u) {
                    if (returned_result > 0) *end_out = returned_end;
                    return returned_result;
                }
                continue;
            }
            if (frame->stage == 1u) {
                --frame_count;
                if (frame_count == 0u) {
                    if (returned_result > 0) *end_out = returned_end;
                    return returned_result;
                }
                continue;
            }
            if (frame->stage == 2u) {
                if (returned_result <= 0)
                    captures[frame->saved_slot] = frame->saved_value;
                --frame_count;
                if (frame_count == 0u) {
                    if (returned_result > 0) *end_out = returned_end;
                    return returned_result;
                }
                continue;
            }
            if (frame->stage == 3u) {
                const _regex_instruction_t* split =
                    &context->internal->instructions[frame->pc];
                size_t index;
                frame->first_result = returned_result;
                frame->first_end = returned_end;
                for (index = 0u; index < slots; ++index)
                    frame->first_captures[index] = captures[index];
                for (index = 0u; index < slots; ++index)
                    captures[index] = frame->original[index];
                frame->stage = 4u;
                if (frame_count >= _REGEX_MAX_BACKREF_DEPTH) {
                    returned_result = -1;
                    returned_end = none;
                    continue;
                }
                _regex_backref_frame_init(context, frame_count,
                                          split->second, frame->position);
                ++frame_count;
                returning = 0;
                continue;
            }
            if (frame->stage == 4u) {
                size_t second_end = returned_end;
                int second_result = returned_result;
                size_t index;
                if (frame->first_result < 0 || second_result < 0) {
                    returned_result = -1;
                    returned_end = none;
                } else if (frame->first_result == 0 && second_result == 0) {
                    returned_result = 0;
                    returned_end = none;
                } else if (second_result != 0 &&
                           (frame->first_result == 0 ||
                            second_end > frame->first_end)) {
                    returned_result = 1;
                    returned_end = second_end;
                } else {
                    for (index = 0u; index < slots; ++index)
                        captures[index] = frame->first_captures[index];
                    returned_result = 1;
                    returned_end = frame->first_end;
                }
                --frame_count;
                if (frame_count == 0u) {
                    if (returned_result > 0) *end_out = returned_end;
                    return returned_result;
                }
                continue;
            }
            if (frame->stage == 5u) {
                size_t index;
                if (returned_result < 0) {
                    returned_result = -1;
                    returned_end = none;
                    --frame_count;
                } else {
                    if (returned_result > 0 &&
                        (frame->first_result == 0 ||
                         returned_end > frame->first_end)) {
                        frame->first_result = 1;
                        frame->first_end = returned_end;
                        for (index = 0u; index < slots; ++index)
                            frame->first_captures[index] = captures[index];
                    }
                    if (++frame->alternative_index <
                        frame->alternative_count) {
                        for (index = 0u; index < slots; ++index)
                            captures[index] = frame->original[index];
                        if (frame_count >= _REGEX_MAX_BACKREF_DEPTH) {
                            returned_result = -1;
                            returned_end = none;
                            --frame_count;
                        } else {
                            _regex_backref_frame_init(
                                context, frame_count, frame->pc + 1u,
                                frame->alternatives[
                                    frame->alternative_index]);
                            ++frame_count;
                            returning = 0;
                            continue;
                        }
                    } else {
                        returned_result = frame->first_result;
                        returned_end = frame->first_end;
                        if (returned_result > 0)
                            for (index = 0u; index < slots; ++index)
                                captures[index] = frame->first_captures[index];
                        --frame_count;
                    }
                }
                if (frame_count == 0u) {
                    if (returned_result > 0) *end_out = returned_end;
                    return returned_result;
                }
                continue;
            }
            returned_result = -1;
            returned_end = none;
            --frame_count;
            if (frame_count == 0u) return -1;
            continue;
        }

        if (++context->steps > _REGEX_MAX_STEPS ||
            frame->pc >= context->internal->instruction_count) {
            returned_result = -1;
            returned_end = none;
            returning = 1;
            continue;
        }
        {
            const _regex_instruction_t* instruction =
                &context->internal->instructions[frame->pc];
            uint32_t child_pc = 0u;
            size_t child_position = frame->position;
            uint8_t wait_stage = 1u;
            int push_child = 1;
            switch (instruction->operation) {
                case _REGEX_OP_MATCH:
                    returned_result = 1;
                    returned_end = frame->position;
                    returning = 1;
                    continue;
                case _REGEX_OP_BYTE:
                case _REGEX_OP_CHAR:
                case _REGEX_OP_ANY:
                    if (frame->position >= context->input_size ||
                        !_regex_instruction_matches(
                            context->internal, instruction,
                            context->input[frame->position])) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    child_pc = frame->pc + 1u;
                    child_position = frame->position + 1u;
                    break;
                case _REGEX_OP_CLASS: {
                    size_t ends[_REGEX_MAX_COLLATION_CHARS];
                    size_t end_count = 0u;
                    int matched = _regex_class_match_ends(
                        context->internal, instruction, context->input,
                        context->input_size, frame->position, ends,
                        &end_count, &context->steps);
                    size_t index;
                    if (matched < 0) {
                        returned_result = -1;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    if (end_count == 0u) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    if (end_count == 1u) {
                        child_pc = frame->pc + 1u;
                        child_position = ends[0];
                        break;
                    }
                    frame->stage = 5u;
                    frame->alternative_count = end_count;
                    frame->alternative_index = 0u;
                    frame->first_result = 0;
                    frame->first_end = none;
                    for (index = 0u; index < end_count; ++index)
                        frame->alternatives[index] = ends[index];
                    for (index = 0u; index < slots; ++index)
                        frame->original[index] = captures[index];
                    child_pc = frame->pc + 1u;
                    child_position = ends[0];
                    wait_stage = 5u;
                    break;
                }
                case _REGEX_OP_BACKREF: {
                    size_t slot = (size_t)instruction->first * 2u;
                    size_t begin;
                    size_t end;
                    size_t length;
                    size_t index;
                    if (slot + 1u >= slots) {
                        returned_result = -1;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    begin = captures[slot];
                    end = captures[slot + 1u];
                    if (begin == none || end == none || end < begin ||
                        end > context->input_size) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    length = end - begin;
                    if (frame->position > context->input_size ||
                        context->byte_offsets[end] -
                            context->byte_offsets[begin] >
                                _REGEX_MAX_BACKREF_CAPTURE ||
                        length > context->input_size - frame->position) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    for (index = 0u; index < length; ++index) {
                        uint32_t expected = context->input[begin + index];
                        uint32_t actual =
                            context->input[frame->position + index];
                        if ((context->internal->cflags & REG_ICASE) != 0) {
                            expected = _regex_fold(expected);
                            actual = _regex_fold(actual);
                        }
                        if (expected != actual) break;
                    }
                    if (index != length) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    child_pc = frame->pc + 1u;
                    child_position = frame->position + length;
                    break;
                }
                case _REGEX_OP_BOL:
                    if (!_regex_bol(context->internal, context->input,
                                    frame->position, context->eflags)) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    child_pc = frame->pc + 1u;
                    break;
                case _REGEX_OP_EOL:
                    if (!_regex_eol(context->internal, context->input,
                                    context->input_size, frame->position,
                                    context->eflags)) {
                        returned_result = 0;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    child_pc = frame->pc + 1u;
                    break;
                case _REGEX_OP_JUMP:
                    child_pc = instruction->first;
                    break;
                case _REGEX_OP_SAVE_START:
                case _REGEX_OP_SAVE_END: {
                    size_t slot = (size_t)instruction->first * 2u +
                        (instruction->operation == _REGEX_OP_SAVE_END);
                    if (slot >= slots) {
                        returned_result = -1;
                        returned_end = none;
                        returning = 1;
                        continue;
                    }
                    frame->saved_slot = slot;
                    frame->saved_value = captures[slot];
                    captures[slot] = frame->position;
                    frame->stage = 2u;
                    child_pc = frame->pc + 1u;
                    wait_stage = 2u;
                    break;
                }
                case _REGEX_OP_SPLIT: {
                    size_t index;
                    for (index = 0u; index < slots; ++index)
                        frame->original[index] = captures[index];
                    frame->stage = 3u;
                    child_pc = instruction->first;
                    wait_stage = 3u;
                    break;
                }
                default:
                    push_child = 0;
                    returned_result = -1;
                    returned_end = none;
                    returning = 1;
                    continue;
            }
            if (push_child) {
                if (frame_count >= _REGEX_MAX_BACKREF_DEPTH) {
                    returned_result = -1;
                    returned_end = none;
                    returning = 1;
                    continue;
                }
                frame->stage = wait_stage;
                _regex_backref_frame_init(context, frame_count,
                                          child_pc, child_position);
                ++frame_count;
            }
        }
    }
}

static inline int regexec(const regex_t* preg, const char* string,
                          size_t nmatch, regmatch_t pmatch[], int eflags) {
    const int known_flags = REG_NOTBOL | REG_NOTEOL | REG_STARTEND;
    const size_t none = (size_t)-1;
    const _regex_internal_t* internal;
    const unsigned char* input_bytes = NULL;
    uint32_t* input = NULL;
    size_t* byte_offsets = NULL;
    locale_t previous_locale = (locale_t)0;
    size_t region_begin = 0u, region_end = 0u;
    size_t input_byte_size = 0u, input_size = 0u;
    size_t *active = NULL, *next = NULL, *queue = NULL;
    uint32_t *active_generation = NULL, *next_generation = NULL;
    uint32_t *active_indices = NULL, *next_indices = NULL;
    size_t *active_captures = NULL, *next_captures = NULL;
    uint32_t *active_capture_index_by_pc = NULL;
    uint32_t *next_capture_index_by_pc = NULL;
    size_t *best_captures = NULL;
    size_t active_capture_capacity = 0u, next_capture_capacity = 0u;
    _regex_backref_frame_t* backref_frames = NULL;
    size_t* backref_snapshots = NULL;
    uint32_t* queued_generation = NULL;
    size_t best_begin = none, best_end = 0u, position;
    uint64_t steps = 0u;
    uint32_t count;
    uint32_t active_count = 0u, next_count = 0u;
    uint32_t active_epoch = 1u, next_epoch = 2u;
    uint32_t state_epoch = 2u, queue_epoch = 0u;
    size_t capture_slots;
    int has_backref = 0;
    int needs_captures = 0;
    int locale_switched = 0;
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
        input_bytes = (const unsigned char*)(const void*)begin_address;
        input_byte_size = region_end - region_begin;
    } else {
        input_bytes = (const unsigned char*)(const void*)string;
        while (region_end < _REGEX_MAX_INPUT &&
               input_bytes[region_end] != 0u)
            ++region_end;
        if (region_end == _REGEX_MAX_INPUT &&
            input_bytes[region_end] != 0u)
            return REG_ESPACE;
        input_byte_size = region_end;
    }
    previous_locale = uselocale(internal->locale);
    if (!previous_locale) return REG_ESPACE;
    locale_switched = 1;
    input = (uint32_t*)malloc((input_byte_size + 1u) * sizeof(*input));
    byte_offsets = (size_t*)malloc(
        (input_byte_size + 1u) * sizeof(*byte_offsets));
    if (!input || !byte_offsets) {
        result = REG_ESPACE;
        goto regex_exec_done;
    }
    {
        size_t byte_position = 0u;
        while (byte_position < input_byte_size) {
            size_t character_width;
            if (!_regex_decode_one((const char*)input_bytes + byte_position,
                    input_byte_size - byte_position, &input[input_size],
                    &character_width) || character_width == 0u) {
                result = REG_ESPACE;
                goto regex_exec_done;
            }
            byte_offsets[input_size++] = byte_position;
            byte_position += character_width;
        }
        byte_offsets[input_size] = input_byte_size;
    }
    count = internal->instruction_count;
    for (position = 0u; position < (size_t)count; ++position) {
        if (internal->instructions[position].operation == _REGEX_OP_BACKREF) {
            has_backref = 1;
            needs_captures = 1;
            break;
        }
    }
    if (internal->has_variable_class) has_backref = 1;
    capture_slots = ((internal->cflags & REG_NOSUB) == 0 || needs_captures)
        ? (size_t)internal->group_count * 2u : 0u;
    active = (size_t*)malloc((size_t)count * sizeof(*active));
    next = (size_t*)malloc((size_t)count * sizeof(*next));
    queue = (size_t*)malloc((size_t)count * sizeof(*queue));
    active_generation = (uint32_t*)malloc(
        (size_t)count * sizeof(*active_generation));
    next_generation = (uint32_t*)malloc(
        (size_t)count * sizeof(*next_generation));
    active_indices = (uint32_t*)malloc(
        (size_t)count * sizeof(*active_indices));
    next_indices = (uint32_t*)malloc(
        (size_t)count * sizeof(*next_indices));
    queued_generation = (uint32_t*)malloc(
        (size_t)count * sizeof(*queued_generation));
    if (capture_slots != 0u) {
        if ((size_t)count > (size_t)-1 /
                sizeof(*active_capture_index_by_pc)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        active_capture_index_by_pc = (uint32_t*)malloc(
            (size_t)count * sizeof(*active_capture_index_by_pc));
        next_capture_index_by_pc = (uint32_t*)malloc(
            (size_t)count * sizeof(*next_capture_index_by_pc));
        best_captures = (size_t*)malloc(capture_slots * sizeof(*best_captures));
    }
    if (has_backref) {
        size_t slots_per_frame;
        size_t snapshot_slots;
        if ((size_t)_REGEX_MAX_BACKREF_DEPTH >
            (size_t)-1 / sizeof(*backref_frames)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        if (capture_slots > (size_t)-1 / 2u) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        slots_per_frame = capture_slots * 2u;
        if (slots_per_frame != 0u &&
            (size_t)_REGEX_MAX_BACKREF_DEPTH >
                (size_t)-1 / slots_per_frame) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        snapshot_slots = (size_t)_REGEX_MAX_BACKREF_DEPTH * slots_per_frame;
        if (snapshot_slots > (size_t)-1 / sizeof(*backref_snapshots)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        backref_frames = (_regex_backref_frame_t*)malloc(
            (size_t)_REGEX_MAX_BACKREF_DEPTH * sizeof(*backref_frames));
        if (snapshot_slots != 0u)
            backref_snapshots = (size_t*)malloc(
                snapshot_slots * sizeof(*backref_snapshots));
    }
    if (!active || !next || !queue || !active_generation ||
        !next_generation || !active_indices || !next_indices ||
        !queued_generation ||
        (capture_slots != 0u &&
         (!active_capture_index_by_pc || !next_capture_index_by_pc ||
          !best_captures)) ||
        (has_backref &&
         (!backref_frames ||
          (capture_slots != 0u && !backref_snapshots)))) {
        result = REG_ESPACE;
        goto regex_exec_done;
    }
    _regex_zero(active_generation,
                (size_t)count * sizeof(*active_generation));
    _regex_zero(next_generation,
                (size_t)count * sizeof(*next_generation));
    _regex_zero(queued_generation,
                (size_t)count * sizeof(*queued_generation));
    if (capture_slots != 0u) {
        size_t capture_index;
        for (capture_index = 0u; capture_index < capture_slots; ++capture_index)
            best_captures[capture_index] = none;
    }
    if (has_backref) {
        _regex_backref_context_t context;
        size_t captures[_REGEX_MAX_GROUPS * 2u];
        size_t candidate_end = none;
        context.internal = internal;
        context.input = input;
        context.byte_offsets = byte_offsets;
        context.input_size = input_size;
        context.eflags = eflags;
        context.capture_slots = capture_slots;
        context.steps = 0u;
        context.frames = backref_frames;
        context.snapshots = backref_snapshots;
        for (position = 0u; position <= input_size; ++position) {
            size_t capture_index;
            int backref_result;
            for (capture_index = 0u; capture_index < capture_slots;
                 ++capture_index)
                captures[capture_index] = none;
            candidate_end = none;
            backref_result = _regex_backref_match(
                &context, 0u, position, captures, &candidate_end);
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
                pmatch[0].rm_so = (regoff_t)(
                    region_begin + byte_offsets[best_begin]);
                pmatch[0].rm_eo = (regoff_t)(
                    region_begin + byte_offsets[best_end]);
                for (index = 1u; index < nmatch; ++index) {
                    size_t slot = (index - 1u) * 2u;
                    if (index <= internal->group_count &&
                        best_captures[slot] != none &&
                        best_captures[slot + 1u] != none) {
                        pmatch[index].rm_so =
                            (regoff_t)(region_begin +
                                byte_offsets[best_captures[slot]]);
                        pmatch[index].rm_eo =
                            (regoff_t)(region_begin +
                                byte_offsets[best_captures[slot + 1u]]);
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
    for (position = 0u; position <= input_size; ++position) {
        uint32_t active_index;
        int live = 0;
        if (best_begin == none &&
            (active_generation[0] != active_epoch ||
             position < active[0])) {
            if (active_generation[0] != active_epoch) {
                if (active_count >= count) {
                    result = REG_ESPACE;
                    goto regex_exec_done;
                }
                if (capture_slots != 0u &&
                    !_regex_capture_state_reserve(
                        &active_captures, capture_slots, active_count,
                        (size_t)active_count + 1u, count,
                        &active_capture_capacity)) {
                    result = REG_ESPACE;
                    goto regex_exec_done;
                }
                active_generation[0] = active_epoch;
                active_indices[active_count++] = 0u;
                if (capture_slots != 0u)
                    active_capture_index_by_pc[0] = active_count - 1u;
            }
            active[0] = position;
            if (capture_slots != 0u) {
                size_t state_index =
                    (size_t)active_capture_index_by_pc[0] * capture_slots;
                size_t capture_index;
                for (capture_index = 0u; capture_index < capture_slots;
                     ++capture_index)
                    active_captures[state_index + capture_index] = none;
            }
        }
        if (queue_epoch == UINT32_MAX) {
            _regex_zero(queued_generation,
                        (size_t)count * sizeof(*queued_generation));
            queue_epoch = 1u;
        } else {
            ++queue_epoch;
        }
        if (!_regex_closure(internal, input, input_size, position, eflags,
                            active, active_generation, active_epoch,
                            active_indices, &active_count, queue,
                            queued_generation, queue_epoch,
                            active_capture_index_by_pc, &active_captures,
                            &active_capture_capacity, capture_slots, &steps)) {
            result = REG_ESPACE;
            goto regex_exec_done;
        }
        if (active_generation[count - 1u] == active_epoch) {
            size_t candidate = active[count - 1u];
            if (best_begin == none || candidate < best_begin) {
                best_begin = candidate;
                best_end = position;
                if (capture_slots != 0u)
                    _regex_copy_captures(best_captures,
                                         active_captures +
                                             (size_t)active_capture_index_by_pc[
                                                 count - 1u] * capture_slots,
                                         capture_slots);
            } else if (candidate == best_begin && position > best_end) {
                best_end = position;
                if (capture_slots != 0u)
                    _regex_copy_captures(best_captures,
                                         active_captures +
                                             (size_t)active_capture_index_by_pc[
                                                 count - 1u] * capture_slots,
                                         capture_slots);
            }
        }
        if (position == input_size) break;
        if (state_epoch == UINT32_MAX) {
            _regex_zero(active_generation,
                        (size_t)count * sizeof(*active_generation));
            _regex_zero(next_generation,
                        (size_t)count * sizeof(*next_generation));
            active_epoch = 1u;
            for (active_index = 0u; active_index < active_count;
                 ++active_index)
                active_generation[active_indices[active_index]] = active_epoch;
            state_epoch = active_epoch;
        }
        next_epoch = ++state_epoch;
        next_count = 0u;
        for (active_index = 0u; active_index < active_count; ++active_index) {
            const uint32_t pc = active_indices[active_index];
            const uint32_t target = pc + 1u;
            size_t origin;
            if (active_generation[pc] != active_epoch) {
                result = REG_ESPACE;
                goto regex_exec_done;
            }
            if (++steps > _REGEX_MAX_STEPS) {
                result = REG_ESPACE;
                goto regex_exec_done;
            }
            if (target >= count ||
                !_regex_instruction_matches(
                    internal, &internal->instructions[pc], input[position]))
                continue;
            origin = active[pc];
            if (next_generation[target] != next_epoch) {
                if (next_count >= count) {
                    result = REG_ESPACE;
                    goto regex_exec_done;
                }
                if (capture_slots != 0u &&
                    !_regex_capture_state_reserve(
                        &next_captures, capture_slots, next_count,
                        (size_t)next_count + 1u, count,
                        &next_capture_capacity)) {
                    result = REG_ESPACE;
                    goto regex_exec_done;
                }
                if (capture_slots != 0u) {
                    next_capture_index_by_pc[target] = next_count;
                    _regex_copy_captures(
                        next_captures + (size_t)next_count * capture_slots,
                        active_captures + (size_t)
                            active_capture_index_by_pc[pc] * capture_slots,
                        capture_slots);
                }
                next_generation[target] = next_epoch;
                next_indices[next_count++] = target;
                next[target] = origin;
            } else if (origin < next[target]) {
                next[target] = origin;
                if (capture_slots != 0u)
                    _regex_copy_captures(
                        next_captures + (size_t)
                            next_capture_index_by_pc[target] * capture_slots,
                        active_captures + (size_t)
                            active_capture_index_by_pc[pc] * capture_slots,
                        capture_slots);
            }
        }
        { size_t* swap = active; active = next; next = swap; }
        { uint32_t* swap = active_generation;
          active_generation = next_generation; next_generation = swap; }
        { uint32_t* swap = active_indices;
          active_indices = next_indices; next_indices = swap; }
        { size_t* swap = active_captures;
          active_captures = next_captures; next_captures = swap; }
        { uint32_t* swap = active_capture_index_by_pc;
          active_capture_index_by_pc = next_capture_index_by_pc;
          next_capture_index_by_pc = swap; }
        { size_t swap = active_capture_capacity;
          active_capture_capacity = next_capture_capacity;
          next_capture_capacity = swap; }
        active_count = next_count;
        next_count = 0u;
        active_epoch = next_epoch;
        if (best_begin != none) {
            for (active_index = 0u; active_index < active_count;
                 ++active_index) {
                const uint32_t pc = active_indices[active_index];
                if (active_generation[pc] == active_epoch &&
                    active[pc] <= best_begin) {
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
            pmatch[0].rm_so = (regoff_t)(
                region_begin + byte_offsets[best_begin]);
            pmatch[0].rm_eo = (regoff_t)(
                region_begin + byte_offsets[best_end]);
            for (index = 1u; index < nmatch; ++index) {
                size_t slot = (index - 1u) * 2u;
                if (index <= internal->group_count &&
                    best_captures[slot] != none &&
                    best_captures[slot + 1u] != none) {
                    pmatch[index].rm_so =
                        (regoff_t)(region_begin +
                            byte_offsets[best_captures[slot]]);
                    pmatch[index].rm_eo =
                        (regoff_t)(region_begin +
                            byte_offsets[best_captures[slot + 1u]]);
                } else {
                    pmatch[index].rm_so = (regoff_t)-1;
                    pmatch[index].rm_eo = (regoff_t)-1;
                }
            }
        }
        result = 0;
    }
regex_exec_done:
    free(byte_offsets);
    free(input);
    free(backref_frames);
    free(backref_snapshots);
    free(queued_generation);
    free(next_indices);
    free(active_indices);
    free(next_generation);
    free(active_generation);
    free(queue);
    free(best_captures);
    free(next_captures);
    free(active_captures);
    free(next_capture_index_by_pc);
    free(active_capture_index_by_pc);
    free(next);
    free(active);
    if (locale_switched) (void)uselocale(previous_locale);
    return result;
}

static inline void regfree(regex_t* preg) {
    if (preg) {
        _regex_internal_t* internal = (_regex_internal_t*)preg->_internal;
        if (internal && internal->magic == _REGEX_MAGIC) {
            internal->magic = 0u;
            _regex_internal_destroy(internal);
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
