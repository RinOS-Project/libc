/* SPDX-License-Identifier: MIT */
/* C11 UTF-16 and UTF-32 multibyte conversion runtime. */

#include "uchar.h"

#ifndef _MSVCRT_COMPAT

#define RIN_UCHAR_STATE_PENDING_LOW UINT32_MAX
#define RIN_UCHAR_STATE_PENDING_HIGH (UINT32_MAX - UINT32_C(1))

static __thread mbstate_t g_mbrtoc16_private_state = {0u, 0u};
static __thread mbstate_t g_c16rtomb_private_state = {0u, 0u};
static __thread mbstate_t g_mbrtoc32_private_state = {0u, 0u};
static __thread mbstate_t g_c32rtomb_private_state = {0u, 0u};

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
    if (state->state == RIN_UCHAR_STATE_PENDING_HIGH)
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
        state->state == RIN_UCHAR_STATE_PENDING_HIGH)
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

#endif /* !_MSVCRT_COMPAT */
