/*
* RPN_REG.c  RPN Registry functions
*
* History:
*        08/25/97     BruceK   Registry storage of calculator state
*/

#include <stddef.h>
#include <windows.h>
#include "rpn.h"

// Older versions read these records as four consecutive eight-byte registers.
C_ASSERT(sizeof(STACK) == 32 && offsetof(STACK, X) == 0 &&
         offsetof(STACK, Y) == 8 && offsetof(STACK, Z) == 16 && offsetof(STACK, T) == 24 &&
         sizeof(FSTACK) == 32 && offsetof(FSTACK, X) == 0 &&
         offsetof(FSTACK, Y) == 8 && offsetof(FSTACK, Z) == 16 && offsetof(FSTACK, T) == 24);

const TCHAR szRegKey[] = "Software\\BruceK\\RPN";

static BOOL ReadExactValue(HKEY key, LPCTSTR name, DWORD expectedType,
                          void *buffer, DWORD length, BOOL *recovered)
{
    DWORD type;
    DWORD size = length;
    DWORD status = RegQueryValueEx(key, name, NULL, &type, buffer, &size);

    // Older versions may not have written every setting. Absence isn't corruption.
    if (status == ERROR_FILE_NOT_FOUND)
        return FALSE;
    if (status != ERROR_SUCCESS || type != expectedType || size != length) {
        *recovered = TRUE;
        return FALSE;
    }
    return TRUE;
}

static DWORD ReadDword(HKEY key, LPCTSTR name, DWORD fallback, BOOL *recovered)
{
    DWORD value;
    return ReadExactValue(key, name, REG_DWORD, &value, sizeof(value), recovered) ?
           value : fallback;
}

static DWORD ReadSetting(HKEY key, LPCTSTR name, DWORD fallback,
                         const DWORD *allowed, size_t count, BOOL *recovered)
{
    DWORD value = ReadDword(key, name, fallback, recovered);
    size_t i;
    for (i = 0; i < count; ++i) {
        if (value == allowed[i])
            return value;
    }
    *recovered = TRUE;
    return fallback;
}

void RestoreRPNStateFromKey(HKEY key, CALCULATOR_STATE *state, RPN_WINDOW_STATE *window)
{
    static const DWORD radices[] = {2, 8, 10, 16};
    static const DWORD wordSizes[] = {8, 16, 32, 64};
    static const DWORD integerTypes[] = {INTEGER_TYPE_UNSIGNED, INTEGER_TYPE_2S_COMP};
    static const DWORD modes[] = {CALC_MODE_BINARY, CALC_MODE_FLOAT};
    static const DWORD angleTypes[] = {FLOAT_ANGLE_DEG, FLOAT_ANGLE_RAD, FLOAT_ANGLE_GRAD};
    static const DWORD booleanValues[] = {0, 1};
    BOOL recovered = FALSE;
    STACK integerStack;
    FSTACK floatStack;

    *state = CalculatorDefaults;
    state->Integer.Radix = (USHORT)ReadSetting(key, "Radix", state->Integer.Radix,
                                             radices, ARRAYSIZE(radices), &recovered);
    state->Integer.WordSize = (USHORT)ReadSetting(key, "WordSize", state->Integer.WordSize,
                                                 wordSizes, ARRAYSIZE(wordSizes), &recovered);
    state->Integer.IntegerType = (USHORT)ReadSetting(key, "IntegerType", state->Integer.IntegerType,
                                                     integerTypes, ARRAYSIZE(integerTypes), &recovered);
    state->Integer.LastRadix = (USHORT)ReadSetting(key, "LastRadix", state->Integer.LastRadix,
                                                   radices, ARRAYSIZE(radices), &recovered);
    if (state->Integer.Radix == 2 && state->Integer.WordSize == 64) {
        // Keep the user's word size. The active radix, not Last Base history,
        // determines whether a 64-bit register can fit on the display.
        state->Integer.Radix = CalculatorDefaults.Integer.Radix;
        recovered = TRUE;
    }
    state->Mode = (CALC_MODE)ReadSetting(key, "CalcMode", state->Mode,
                                        modes, ARRAYSIZE(modes), &recovered);
    state->Float.AngleType = (USHORT)ReadSetting(key, "FloatAngleType", state->Float.AngleType,
                                                 angleTypes, ARRAYSIZE(angleTypes), &recovered);
    // Keep signed window-coordinate bits intact. Precision and float display
    // mode stay at their startup defaults until the app has controls for them.
    window->X = ReadDword(key, "WindowX", 0, &recovered);
    window->Y = ReadDword(key, "WindowY", 0, &recovered);
    window->KeypadHidden = ReadSetting(key, "KeypadHidden", FALSE,
                                      booleanValues, ARRAYSIZE(booleanValues), &recovered) != 0;
    // A bad binary value may have filled part of the buffer. Publish only a
    // complete record; don't narrow or reinterpret saved register values.
    if (ReadExactValue(key, "Binary Stack", REG_BINARY, &integerStack, sizeof(integerStack), &recovered))
        state->Integer.Stack = integerStack;
    if (ReadExactValue(key, "Float Stack", REG_BINARY, &floatStack, sizeof(floatStack), &recovered))
        state->Float.Stack = floatStack;
    state->Error = recovered;
}

DWORD SaveRPNStateToKey(HKEY key, const CALCULATOR_STATE *state, const RPN_WINDOW_STATE *window)
{
    // Preserve the original value names, types, and stack byte layout.
    // Never persist entry text, transient flags, or the enclosing structs.
    const struct {
        LPCTSTR name;
        DWORD value;
    } settings[] = {
        {"Radix", state->Integer.Radix},
        {"WordSize", state->Integer.WordSize},
        {"IntegerType", state->Integer.IntegerType},
        {"LastRadix", state->Integer.LastRadix},
        {"CalcMode", state->Mode},
        {"WindowX", window->X},
        {"WindowY", window->Y},
        {"KeypadHidden", window->KeypadHidden ? 1 : 0},
        {"SignificantDigits", FLOAT_SIGNIFICANT_DIGITS},
        {"FloatDisplayMode", FLOAT_DISPLAY_AUTO},
        {"FloatAngleType", state->Float.AngleType},
    };
    size_t i;
    DWORD status;

    for (i = 0; i < ARRAYSIZE(settings); ++i) {
        status = RegSetValueEx(key, settings[i].name, 0, REG_DWORD,
                               (const BYTE *)&settings[i].value, sizeof(settings[i].value));
        if (status != ERROR_SUCCESS)
            return status;
    }
    status = RegSetValueEx(key, "Binary Stack", 0, REG_BINARY,
                           (const BYTE *)&state->Integer.Stack, sizeof(state->Integer.Stack));
    if (status != ERROR_SUCCESS)
        return status;
    return RegSetValueEx(key, "Float Stack", 0, REG_BINARY,
                         (const BYTE *)&state->Float.Stack, sizeof(state->Float.Stack));
}

DWORD SaveRPNState(void)
{
    const RPN_WINDOW_STATE window = {dwWindowX, dwWindowY, fKeypadHidden};
    HKEY key;
    DWORD status = RegCreateKeyEx(HKEY_CURRENT_USER, szRegKey, 0, NULL, 0, KEY_SET_VALUE,
                                 NULL, &key, NULL);
    if (status == ERROR_SUCCESS) {
        status = SaveRPNStateToKey(key, &Calculator, &window);
        RegCloseKey(key);
    }
    return status;
}

void RestoreRPNState(void)
{
    RPN_WINDOW_STATE window = {0};
    HKEY key;
    DWORD status = RegOpenKeyEx(HKEY_CURRENT_USER, szRegKey, 0, KEY_QUERY_VALUE, &key);

    if (status == ERROR_SUCCESS) {
        RestoreRPNStateFromKey(key, &Calculator, &window);
        RegCloseKey(key);
    } else {
        Calculator = CalculatorDefaults;
        Calculator.Error = status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND;
    }
    dwWindowX = window.X;
    dwWindowY = window.Y;
    fKeypadHidden = window.KeypadHidden;
}
