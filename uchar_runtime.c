/* SPDX-License-Identifier: MIT */
/* C11 UTF-16 and UTF-32 multibyte conversion runtime. */

#include "uchar.h"

#ifndef _MSVCRT_COMPAT

#include "locale.h"

#define RIN_UCHAR_STATE_PENDING_LOW UINT32_MAX
#define RIN_UCHAR_STATE_PENDING_HIGH (UINT32_MAX - UINT32_C(1))
/* C23 mbrtoc8 stores remaining UTF-8 units (1-3) in state->codepoint with the
 * pending count in state->state; c8rtomb packs (total << 24) | partial bits
 * in state->codepoint with the missing-unit count in state->state. */
#define RIN_UCHAR_STATE_PENDING_C8_BASE (UINT32_MAX - UINT32_C(8))
#define RIN_UCHAR_STATE_C8RTOMB_BASE (UINT32_MAX - UINT32_C(16))

static __thread mbstate_t g_mbrtoc16_private_state = {0u, 0u};
static __thread mbstate_t g_c16rtomb_private_state = {0u, 0u};
static __thread mbstate_t g_mbrtoc32_private_state = {0u, 0u};
static __thread mbstate_t g_c32rtomb_private_state = {0u, 0u};
static __thread mbstate_t g_mbrtoc8_private_state = {0u, 0u};
static __thread mbstate_t g_c8rtomb_private_state = {0u, 0u};

static void rin_uchar_state_reset(mbstate_t* state)
{
    if (state) {
        state->state = 0u;
        state->codepoint = 0u;
    }
}

static size_t rin_uchar_invalid_state(mbstate_t* state)
{
    rin_uchar_state_reset(state);
    errno = EILSEQ;
    return (size_t)-1;
}

size_t mbrtoc16(char16_t* output, const char* input, size_t length,
                mbstate_t* state)
{
    uint32_t codepoint = 0u;
    size_t consumed;

    if (!state) state = &g_mbrtoc16_private_state;
    if (!input) {
        rin_uchar_state_reset(state);
        return 0u;
    }
    if (state->state == RIN_UCHAR_STATE_PENDING_LOW) {
        if (state->codepoint < UINT32_C(0xdc00) ||
            state->codepoint > UINT32_C(0xdfff))
            return rin_uchar_invalid_state(state);
        if (output) *output = (char16_t)state->codepoint;
        rin_uchar_state_reset(state);
        return (size_t)-3;
    }
    if (state->state == RIN_UCHAR_STATE_PENDING_HIGH ||
        (state->state >= RIN_UCHAR_STATE_PENDING_C8_BASE + 1u &&
         state->state <= RIN_UCHAR_STATE_PENDING_C8_BASE + 3u) ||
        (state->state >= RIN_UCHAR_STATE_C8RTOMB_BASE + 1u &&
         state->state <= RIN_UCHAR_STATE_C8RTOMB_BASE + 3u))
        return rin_uchar_invalid_state(state);
    if (length == 0u) return (size_t)-2;

    consumed = rin_locale_current_mbrtowc32(
        &codepoint, input, length, (struct rin_unicode_mbstate*)state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2) return consumed;
    if (codepoint > UINT32_C(0x10ffff) ||
        (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff)))
        return rin_uchar_invalid_state(state);

    if (codepoint <= UINT32_C(0xffff)) {
        if (output) *output = (char16_t)codepoint;
        return consumed;
    }

    codepoint -= UINT32_C(0x10000);
    if (output)
        *output = (char16_t)(UINT32_C(0xd800) + (codepoint >> 10u));
    state->codepoint = UINT32_C(0xdc00) + (codepoint & UINT32_C(0x3ff));
    state->state = RIN_UCHAR_STATE_PENDING_LOW;
    return consumed;
}

size_t c16rtomb(char* output, char16_t input, mbstate_t* state)
{
    uint32_t codepoint = (uint32_t)input;
    char reset_bytes[4];

    if (!state) state = &g_c16rtomb_private_state;
    if (!output) {
        rin_uchar_state_reset(state);
        return rin_locale_current_wcrtomb32(
            reset_bytes, 0u, (struct rin_unicode_mbstate*)state);
    }

    if (state->state == RIN_UCHAR_STATE_PENDING_HIGH) {
        const uint32_t high = state->codepoint;
        if (high < UINT32_C(0xd800) || high > UINT32_C(0xdbff) ||
            codepoint < UINT32_C(0xdc00) || codepoint > UINT32_C(0xdfff))
            return rin_uchar_invalid_state(state);
        rin_uchar_state_reset(state);
        codepoint = UINT32_C(0x10000) +
                    ((high - UINT32_C(0xd800)) << 10u) +
                    (codepoint - UINT32_C(0xdc00));
    } else if (state->state == RIN_UCHAR_STATE_PENDING_LOW) {
        return rin_uchar_invalid_state(state);
    } else if (state->state != 0u || state->codepoint != 0u) {
        return rin_uchar_invalid_state(state);
    } else if (codepoint >= UINT32_C(0xd800) &&
               codepoint <= UINT32_C(0xdbff)) {
        state->codepoint = codepoint;
        state->state = RIN_UCHAR_STATE_PENDING_HIGH;
        return 0u;
    } else if (codepoint >= UINT32_C(0xdc00) &&
               codepoint <= UINT32_C(0xdfff)) {
        return rin_uchar_invalid_state(state);
    }

    return rin_locale_current_wcrtomb32(
        output, codepoint, (struct rin_unicode_mbstate*)state);
}

size_t mbrtoc32(char32_t* output, const char* input, size_t length,
                mbstate_t* state)
{
    uint32_t codepoint = 0u;
    size_t consumed;

    if (!state) state = &g_mbrtoc32_private_state;
    if (!input) {
        rin_uchar_state_reset(state);
        return 0u;
    }
    if (state->state == RIN_UCHAR_STATE_PENDING_LOW ||
        state->state == RIN_UCHAR_STATE_PENDING_HIGH ||
        (state->state >= RIN_UCHAR_STATE_PENDING_C8_BASE + 1u &&
         state->state <= RIN_UCHAR_STATE_PENDING_C8_BASE + 3u) ||
        (state->state >= RIN_UCHAR_STATE_C8RTOMB_BASE + 1u &&
         state->state <= RIN_UCHAR_STATE_C8RTOMB_BASE + 3u))
        return rin_uchar_invalid_state(state);
    if (length == 0u) return (size_t)-2;

    consumed = rin_locale_current_mbrtowc32(
        &codepoint, input, length, (struct rin_unicode_mbstate*)state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2) return consumed;
    if (codepoint > UINT32_C(0x10ffff) ||
        (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff)))
        return rin_uchar_invalid_state(state);
    if (output) *output = (char32_t)codepoint;
    return consumed;
}

size_t c32rtomb(char* output, char32_t input, mbstate_t* state)
{
    char reset_bytes[4];

    if (!state) state = &g_c32rtomb_private_state;
    if (!output) {
        rin_uchar_state_reset(state);
        return rin_locale_current_wcrtomb32(
            reset_bytes, 0u, (struct rin_unicode_mbstate*)state);
    }
    if (state->state == RIN_UCHAR_STATE_PENDING_LOW ||
        state->state == RIN_UCHAR_STATE_PENDING_HIGH ||
        state->state != 0u || state->codepoint != 0u)
        return rin_uchar_invalid_state(state);
    return rin_locale_current_wcrtomb32(
        output, (uint32_t)input, (struct rin_unicode_mbstate*)state);
}

size_t mbrtoc8(char8_t* output, const char* input, size_t length,
               mbstate_t* state)
{
    uint32_t codepoint = 0u;
    size_t consumed;
    unsigned char encoded[4];
    unsigned int count;
    unsigned int i;

    if (!state) state = &g_mbrtoc8_private_state;
    if (!input) {
        rin_uchar_state_reset(state);
        return 0u;
    }
    if (state->state >= RIN_UCHAR_STATE_PENDING_C8_BASE + 1u &&
        state->state <= RIN_UCHAR_STATE_PENDING_C8_BASE + 3u) {
        unsigned int remaining =
            (unsigned int)(state->state - RIN_UCHAR_STATE_PENDING_C8_BASE);
        uint32_t pending = state->codepoint;
        if (output) *output = (char8_t)(pending & 0xffu);
        pending >>= 8u;
        if (remaining == 1u) {
            rin_uchar_state_reset(state);
        } else {
            state->codepoint = pending;
            state->state = RIN_UCHAR_STATE_PENDING_C8_BASE + (remaining - 1u);
        }
        return (size_t)-3;
    }
    if (state->state == RIN_UCHAR_STATE_PENDING_LOW ||
        state->state == RIN_UCHAR_STATE_PENDING_HIGH ||
        (state->state >= RIN_UCHAR_STATE_C8RTOMB_BASE + 1u &&
         state->state <= RIN_UCHAR_STATE_C8RTOMB_BASE + 3u))
        return rin_uchar_invalid_state(state);
    if (length == 0u) return (size_t)-2;

    consumed = rin_locale_current_mbrtowc32(
        &codepoint, input, length, (struct rin_unicode_mbstate*)state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2) return consumed;
    if (codepoint > UINT32_C(0x10ffff) ||
        (codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff)))
        return rin_uchar_invalid_state(state);

    /* UTF-8 encode the scalar; NUL encodes to a single zero unit. */
    if (codepoint < UINT32_C(0x80)) {
        count = 1u;
        encoded[0] = (unsigned char)codepoint;
    } else if (codepoint < UINT32_C(0x800)) {
        count = 2u;
        encoded[0] = (unsigned char)(0xc0u | (codepoint >> 6u));
        encoded[1] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else if (codepoint < UINT32_C(0x10000)) {
        count = 3u;
        encoded[0] = (unsigned char)(0xe0u | (codepoint >> 12u));
        encoded[1] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        encoded[2] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else {
        count = 4u;
        encoded[0] = (unsigned char)(0xf0u | (codepoint >> 18u));
        encoded[1] = (unsigned char)(0x80u | ((codepoint >> 12u) & 0x3fu));
        encoded[2] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        encoded[3] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    }
    if (output) *output = (char8_t)encoded[0];
    if (count > 1u) {
        uint32_t pending = 0u;
        for (i = count - 1u; i >= 1u; --i) {
            pending = (pending << 8u) | encoded[i];
        }
        state->codepoint = pending;
        state->state = RIN_UCHAR_STATE_PENDING_C8_BASE + (count - 1u);
    }
    return consumed;
}

size_t c8rtomb(char* output, char8_t input, mbstate_t* state)
{
    uint32_t unit = (uint32_t)input;
    char reset_bytes[4];

    if (!state) state = &g_c8rtomb_private_state;
    if (!output) {
        rin_uchar_state_reset(state);
        return rin_locale_current_wcrtomb32(
            reset_bytes, 0u, (struct rin_unicode_mbstate*)state);
    }

    if (state->state >= RIN_UCHAR_STATE_C8RTOMB_BASE + 1u &&
        state->state <= RIN_UCHAR_STATE_C8RTOMB_BASE + 3u) {
        /* A pending sequence continues only with a continuation unit. */
        unsigned int remaining =
            (unsigned int)(state->state - RIN_UCHAR_STATE_C8RTOMB_BASE);
        unsigned int total =
            (unsigned int)((state->codepoint >> 24u) & 0xffu);
        uint32_t partial = state->codepoint & UINT32_C(0xffffff);
        if (unit < UINT32_C(0x80) || unit > UINT32_C(0xbf))
            return rin_uchar_invalid_state(state);
        partial = (partial << 6u) | (unit & UINT32_C(0x3f));
        if (remaining > 1u) {
            state->codepoint = ((uint32_t)total << 24u) | partial;
            state->state = RIN_UCHAR_STATE_C8RTOMB_BASE + (remaining - 1u);
            return 0u;
        }
        /* Final unit: reject overlong, surrogate, and out-of-range forms. */
        if ((total == 2u && partial < UINT32_C(0x80)) ||
            (total == 3u && partial < UINT32_C(0x800)) ||
            (total == 4u && partial < UINT32_C(0x10000)) ||
            partial > UINT32_C(0x10ffff) ||
            (partial >= UINT32_C(0xd800) && partial <= UINT32_C(0xdfff)))
            return rin_uchar_invalid_state(state);
        rin_uchar_state_reset(state);
        return rin_locale_current_wcrtomb32(
            output, partial, (struct rin_unicode_mbstate*)state);
    }
    if (state->state != 0u || state->codepoint != 0u)
        return rin_uchar_invalid_state(state);

    if (unit < UINT32_C(0x80)) {
        return rin_locale_current_wcrtomb32(
            output, unit, (struct rin_unicode_mbstate*)state);
    }
    if (unit >= UINT32_C(0xc2) && unit <= UINT32_C(0xdf)) {
        state->codepoint = (UINT32_C(2) << 24u) | (unit & UINT32_C(0x1f));
        state->state = RIN_UCHAR_STATE_C8RTOMB_BASE + 1u;
        return 0u;
    }
    if (unit >= UINT32_C(0xe0) && unit <= UINT32_C(0xef)) {
        state->codepoint = (UINT32_C(3) << 24u) | (unit & UINT32_C(0x0f));
        state->state = RIN_UCHAR_STATE_C8RTOMB_BASE + 2u;
        return 0u;
    }
    if (unit >= UINT32_C(0xf0) && unit <= UINT32_C(0xf4)) {
        state->codepoint = (UINT32_C(4) << 24u) | (unit & UINT32_C(0x07));
        state->state = RIN_UCHAR_STATE_C8RTOMB_BASE + 3u;
        return 0u;
    }
    return rin_uchar_invalid_state(state);
}

#endif /* !_MSVCRT_COMPAT */
