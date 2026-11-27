#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

// The wire schema is unchanged, but current saves write Auto (0) rather
// than legacy Fix (1). Fixtures do not use the production writer.
static const struct {
    const char *name;
    DWORD value;
} savedSettings[] = {
    {"Radix", 8}, {"WordSize", 16}, {"IntegerType", 2}, {"LastRadix", 2},
    {"CalcMode", 2}, {"WindowX", 0xffffff88}, {"WindowY", 144}, {"KeypadHidden", 1},
    {"SignificantDigits", 10}, {"FloatDisplayMode", 0}, {"FloatAngleType", 3}
};

static const BYTE integerBytes[] = {
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // -2
    0x34, 0x12, 0, 0, 0, 0, 0, 0,                   // 0x1234
    0, 0, 0, 0, 0, 0, 0, 0,                        // 0
    0, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff     // -32768
};
static const BYTE floatBytes[] = {
    0, 0, 0, 0, 0, 0, 0xf8, 0x3f, // 1.5
    0, 0, 0, 0, 0, 0, 0x02, 0xc0, // -2.25
    0, 0, 0, 0, 0, 0, 0, 0x80,    // -0.0
    0, 0, 0, 0, 0, 0, 0x20, 0x40  // 8.0
};

static int Check(int condition, const char *detail)
{
    if (!condition)
        printf("FAIL: registry: %s\n", detail);
    return condition ? 0 : 1;
}

static int WriteValue(HKEY key, const char *name, DWORD type, const void *value, DWORD length)
{
    return Check(RegSetValueEx(key, name, 0, type, value, length) == ERROR_SUCCESS, name);
}

static int WriteDword(HKEY key, const char *name, DWORD value)
{
    return WriteValue(key, name, REG_DWORD, &value, sizeof(value));
}

static int CheckValue(HKEY key, const char *name, DWORD expectedType,
                      const void *expected, DWORD length)
{
    BYTE bytes[32];
    DWORD type, size = sizeof(bytes);
    return Check(RegQueryValueEx(key, name, NULL, &type, bytes, &size) == ERROR_SUCCESS &&
                 type == expectedType && size == length && memcmp(bytes, expected, length) == 0,
                 name);
}

static int CheckLegacyWrite(HKEY key)
{
    CALCULATOR_STATE state = {
        {8, 16, INTEGER_TYPE_2S_COMP, 2, {(NUMBER)-2, 0x1234, 0, (NUMBER)-32768}},
        {{1.5, -2.25, -0.0, 8.0}, "9.5", 4, FLOAT_DISPLAY_ENG, FLOAT_ANGLE_GRAD, TRUE},
        CALC_MODE_FLOAT, TRUE, TRUE
    };
    RPN_WINDOW_STATE window = {0xffffff88, 144, TRUE};
    DWORD count;
    size_t i;
    int failures = 0;

    if (Check(SaveRPNStateToKey(key, &state, &window) == ERROR_SUCCESS, "save into an owned key"))
        return 1;
    for (i = 0; i < ARRAYSIZE(savedSettings); ++i)
        failures += CheckValue(key, savedSettings[i].name, REG_DWORD,
                               &savedSettings[i].value, sizeof(savedSettings[i].value));
    failures += CheckValue(key, "Binary Stack", REG_BINARY, integerBytes, sizeof(integerBytes));
    failures += CheckValue(key, "Float Stack", REG_BINARY, floatBytes, sizeof(floatBytes));
    failures += Check(RegQueryInfoKey(key, NULL, NULL, NULL, NULL, NULL, NULL, &count,
                                      NULL, NULL, NULL, NULL) == ERROR_SUCCESS &&
                      count == ARRAYSIZE(savedSettings) + 2,
                      "only legacy settings and stacks are persisted, not transient state");
    return failures;
}

static int CheckLegacyRead(HKEY key)
{
    CALCULATOR_STATE state = {0};
    RPN_WINDOW_STATE window;
    size_t i;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(savedSettings); ++i)
        failures += WriteDword(key, savedSettings[i].name, savedSettings[i].value);
    failures += WriteValue(key, "Binary Stack", REG_BINARY, integerBytes, sizeof(integerBytes));
    failures += WriteValue(key, "Float Stack", REG_BINARY, floatBytes, sizeof(floatBytes));
    // The pinned legacy executable wrote Fix (1) and ten significant digits.
    failures += WriteDword(key, "FloatDisplayMode", 1);
    if (failures)
        return failures;

    RestoreRPNStateFromKey(key, &state, &window);
    failures += Check(state.Mode == CALC_MODE_FLOAT && !state.Error &&
                      state.Integer.Radix == 8 && state.Integer.WordSize == 16 &&
                      state.Integer.IntegerType == INTEGER_TYPE_2S_COMP &&
                      state.Integer.LastRadix == 2 && state.Float.AngleType == FLOAT_ANGLE_GRAD,
                      "restore valid mode and settings from an independently seeded key");
    failures += Check((LONG)window.X == -120 && window.Y == 144 && window.KeypadHidden,
                      "restore signed window coordinates and keypad state");
    failures += Check(memcmp(&state.Integer.Stack, integerBytes, sizeof(integerBytes)) == 0 &&
                      memcmp(&state.Float.Stack, floatBytes, sizeof(floatBytes)) == 0,
                      "restore both stacks byte for byte, including negative zero");
    failures += Check(!state.Entering && !state.Float.Inverse && state.Float.Entry[0] == '\0' &&
                      state.Float.SignificantDigits == 10 && state.Float.DisplayMode == FLOAT_DISPLAY_AUTO,
                      "restore legacy Fix as Auto; don't restore transient state");

    // Edited values must not select unsupported formatting or alter precision.
    failures += WriteDword(key, "SignificantDigits", 4);
    failures += WriteDword(key, "FloatDisplayMode", 2);
    if (failures)
        return failures;
    RestoreRPNStateFromKey(key, &state, &window);
    failures += Check(!state.Error && state.Float.SignificantDigits == 10 &&
                      state.Float.DisplayMode == FLOAT_DISPLAY_AUTO,
                      "ignore edited precision and Scientific without marking valid data corrupt");
    return failures;
}

static int CheckPartialRead(HKEY key)
{
    CALCULATOR_STATE state = {0};
    RPN_WINDOW_STATE window = {7, 11, TRUE};
    int failures = 0;

    failures += WriteDword(key, "Radix", 10);
    failures += WriteValue(key, "Float Stack", REG_BINARY, floatBytes, sizeof(floatBytes));
    if (failures)
        return failures;

    RestoreRPNStateFromKey(key, &state, &window);
    failures += Check(state.Integer.Radix == 10 && state.Integer.LastRadix == 10 &&
                      state.Integer.WordSize == 64 && state.Mode == CALC_MODE_BINARY &&
                      !state.Error && window.X == 0 && window.Y == 0 && !window.KeypadHidden,
                      "missing fields keep startup defaults without flagging an error");
    failures += Check(state.Integer.Stack.X == 0 &&
                      memcmp(&state.Float.Stack, floatBytes, sizeof(floatBytes)) == 0,
                      "partial saves restore the stack present and default the missing one");
    return failures;
}

static int CheckMalformedRead(HKEY key)
{
    CALCULATOR_STATE state = {0};
    RPN_WINDOW_STATE window;
    const DWORD wrongType = 2;
    const BYTE shortValue[] = {2, 0};
    const BYTE oversizedStack[33] = {0};
    const STACK emptyInteger = {0};
    const FSTACK emptyFloat = {0};
    int failures = 0;

    failures += WriteDword(key, "Radix", 2);
    failures += WriteDword(key, "WordSize", 64);
    failures += WriteDword(key, "LastRadix", 2);
    failures += WriteDword(key, "IntegerType", 0x10002);
    failures += WriteDword(key, "KeypadHidden", 7);
    failures += WriteValue(key, "CalcMode", REG_BINARY, &wrongType, sizeof(wrongType));
    failures += WriteValue(key, "FloatAngleType", REG_DWORD, shortValue, sizeof(shortValue));
    failures += WriteValue(key, "WindowX", REG_BINARY, &wrongType, sizeof(wrongType));
    failures += WriteDword(key, "WindowY", 144);
    failures += WriteValue(key, "Binary Stack", REG_BINARY, integerBytes, sizeof(integerBytes) - 1);
    failures += WriteValue(key, "Float Stack", REG_BINARY, floatBytes, sizeof(floatBytes));
    if (failures)
        return failures;

    RestoreRPNStateFromKey(key, &state, &window);
    failures += Check(state.Error && state.Integer.Radix == 16 && state.Integer.WordSize == 64 &&
                      state.Integer.LastRadix == 2,
                      "recover 64-bit binary display without discarding width or radix history");
    failures += Check(state.Integer.IntegerType == INTEGER_TYPE_UNSIGNED &&
                      state.Mode == CALC_MODE_BINARY && state.Float.AngleType == FLOAT_ANGLE_DEG &&
                      window.X == 0 && window.Y == 144 && !window.KeypadHidden,
                      "reject malformed values before narrowing and keep unrelated valid settings");
    failures += Check(memcmp(&state.Integer.Stack, &emptyInteger, sizeof(emptyInteger)) == 0 &&
                      memcmp(&state.Float.Stack, floatBytes, sizeof(floatBytes)) == 0,
                      "reject a short integer stack without affecting a valid float stack");

    failures += WriteValue(key, "Binary Stack", REG_BINARY, integerBytes, sizeof(integerBytes));
    failures += WriteValue(key, "Float Stack", REG_BINARY, oversizedStack, sizeof(oversizedStack));
    if (failures)
        return failures;
    RestoreRPNStateFromKey(key, &state, &window);
    failures += Check(memcmp(&state.Integer.Stack, integerBytes, sizeof(integerBytes)) == 0 &&
                      memcmp(&state.Float.Stack, &emptyFloat, sizeof(emptyFloat)) == 0,
                      "reject an oversized float stack without affecting a valid integer stack");
    return failures;
}

int CheckRegistryState(void)
{
    static int (*const cases[])(HKEY) = {
        CheckLegacyWrite, CheckLegacyRead, CheckPartialRead, CheckMalformedRead
    };
    const ULONGLONG stamp = GetTickCount64();
    size_t i;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        char path[128];
        DWORD disposition;
        HKEY key;
        sprintf_s(path, sizeof(path), "Software\\RPNRegistryTests-%lu-%llu-%zu",
                  GetCurrentProcessId(), stamp, i);
        if (Check(RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_READ | KEY_WRITE,
                                 NULL, &key, &disposition) == ERROR_SUCCESS,
                  "create a disposable test key"))
            return failures + 1;
        // Never run tests or clean up unless this call created the key.
        if (disposition != REG_CREATED_NEW_KEY) {
            RegCloseKey(key);
            return failures + Check(FALSE, "the test key already existed");
        }
        failures += cases[i](key);
        failures += Check(RegCloseKey(key) == ERROR_SUCCESS, "close the test key");
        failures += Check(RegDeleteKey(HKEY_CURRENT_USER, path) == ERROR_SUCCESS, "remove the owned test key");
    }
    return failures;
}
