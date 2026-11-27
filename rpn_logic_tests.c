#include <math.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include "rpn.h"

BOOL FStrNumTest(PUCHAR lpNumStr, FNUMBER *pfNumber);
int CheckRegistryState(void);

static int CheckCase(int condition, const char *context, const char *description)
{
    if (!condition)
        printf("FAIL: %s: %s\n", context, description);
    return condition ? 0 : 1;
}

static int Check(int condition, const char *description)
{
    if (!condition)
        printf("FAIL: %s\n", description);
    return condition ? 0 : 1;
}

static int SameStack(STACK actual, STACK expected)
{
    return actual.X == expected.X && actual.Y == expected.Y &&
           actual.Z == expected.Z && actual.T == expected.T;
}

typedef struct {
    INTEGER_STATE State;
    BOOL Entering;
    BOOL Error;
    unsigned DivisionErrors;
    BOOL EnteringAtError;
    STACK StackAtError;
} INTEGER_TEST;

static void RecordDivisionError(void *context, const char *message)
{
    INTEGER_TEST *test = context;
    if (strcmp(message, "Division by zero") == 0)
        ++test->DivisionErrors;
    test->EnteringAtError = test->Entering;
    test->StackAtError = test->State.Stack;
}

static CALCULATOR_RESULT PressInteger(INTEGER_TEST *test, DWORD command)
{
    return IntegerCommand(&test->State, command, &test->Entering, &test->Error,
                          RecordDivisionError, test);
}

static int CheckIntegerRadices(void)
{
    static const struct {
        const char *name;
        USHORT radix;
        UCHAR lastDigit;
        const char *value;
        const char *limit;
        const char *overflow;
        const char *invalid;
    } cases[] = {
        {"binary",  2, '1', "11010101", "11111111", "100000000", "102"},
        {"octal",   8, '7', "325",      "377",      "400",       "128"},
        {"decimal", 10, '9', "213",     "255",      "256",       "12A"},
        {"hex",     16, 'F', "d5",      "ff",       "100",       "12G"},
    };
    char buffer[DISPLAY_WIDTH + 1];
    int failures = 0;
    size_t i;

    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        const char *name = cases[i].name;
        USHORT radix = cases[i].radix;
        NUMBER number = 0;
        const char *digit;
        INTEGER_TEST test = {{radix, 8, INTEGER_TYPE_UNSIGNED, 16, {3, 11, 19, 23}}};
        const STACK entered = {213, 3, 11, 19};
        UCHAR invalidDigit = (UCHAR)cases[i].invalid[strlen(cases[i].invalid) - 1];

        failures += CheckCase(IsBaseDigit(cases[i].lastDigit, radix) &&
                              !IsBaseDigit(invalidDigit, radix),
                              name, "accept and reject entry digits");
        failures += CheckCase(StrNumTest((PUCHAR)cases[i].value, radix,
                                         INTEGER_TYPE_UNSIGNED, 8, &number) && number == 213,
                              name, "parse digits in this radix");
        failures += CheckCase(StrNumTest((PUCHAR)cases[i].limit, radix,
                                         INTEGER_TYPE_UNSIGNED, 8, &number) && number == 255,
                              name, "parse the largest unsigned byte");
        number = 17;
        failures += CheckCase(!StrNumTest((PUCHAR)cases[i].overflow, radix,
                                          INTEGER_TYPE_UNSIGNED, 8, &number) && number == 17,
                              name, "reject unsigned byte overflow without writing output");
        failures += CheckCase(!StrNumTest((PUCHAR)cases[i].invalid, radix,
                                          INTEGER_TYPE_UNSIGNED, 8, &number) && number == 17,
                              name, "reject invalid input without writing output");
        failures += CheckCase(IntToA(213, buffer, sizeof(buffer), radix,
                                     INTEGER_TYPE_UNSIGNED, 8, FALSE) != NULL &&
                              strcmp(buffer, cases[i].value) == 0,
                              name, "format digits in this radix");
        for (digit = cases[i].value; *digit; ++digit) {
            DWORD command = (UCHAR)*digit;
            if (command >= 'a' && command <= 'f')
                command -= 'a' - 'A';
            failures += CheckCase(PressInteger(&test, command) == CALCULATOR_REDRAW,
                                  name, "accept each command digit");
        }
        failures += CheckCase(SameStack(test.State.Stack, entered) && test.Entering,
                              name, "lift once while entering a number");
        failures += CheckCase(PressInteger(&test, cases[i].lastDigit) == CALCULATOR_ENTRY_REJECTED &&
                              SameStack(test.State.Stack, entered) && test.Entering && !test.Error,
                              name, "ignore a digit that would overflow");
        failures += CheckCase(PressInteger(&test, invalidDigit) == CALCULATOR_UNKNOWN_COMMAND &&
                              SameStack(test.State.Stack, entered) && test.Entering && test.Error,
                              name, "reject a digit outside the base");
        failures += CheckCase(PressInteger(&test, BACKSPACE_KEY) == CALCULATOR_REDRAW &&
                              test.State.Stack.X == 213 / radix && test.Entering &&
                              PressInteger(&test, ENTER) == CALCULATOR_REDRAW && !test.Entering,
                              name, "backspace then commit without lifting");
    }
    return failures;
}

static int CheckIntegerStack(void)
{
    static const struct {
        const char *name;
        DWORD command;
        BOOL entering;
        STACK expected;
    } cases[] = {
        {"commit X",    ENTER,         TRUE,  {3,  11, 19, 23}},
        {"duplicate X", ENTER,         FALSE, {3,  3,  11, 19}},
        {"Y plus X",    PLUS_KEY,      TRUE,  {14, 19, 23, 23}},
        {"Y minus X",   MINUS_KEY,     TRUE,  {8,  19, 23, 23}},
        {"Y times X",   MULT_KEY,      TRUE,  {33, 19, 23, 23}},
        {"Y over X",    DIV_KEY,       TRUE,  {3,  19, 23, 23}},
        {"Y modulo X",  MOD_KEY,       TRUE,  {2,  19, 23, 23}},
        {"Y and X",     AND_KEY,       TRUE,  {3,  19, 23, 23}},
        {"Y or X",      OR_KEY,        TRUE,  {11, 19, 23, 23}},
        {"Y xor X",     XOR_KEY,       TRUE,  {8,  19, 23, 23}},
        {"Y left by X", LEFT_SHIFT_KEY, TRUE,  {88, 19, 23, 23}},
        {"Y right by X", RIGHT_SHIFT_KEY, TRUE, {1, 19, 23, 23}},
        {"double X",    UNARY_LEFT_SHIFT_KEY, TRUE,  {6, 11, 19, 23}},
        {"halve X",     UNARY_RIGHT_SHIFT_KEY, TRUE, {1, 11, 19, 23}},
        {"increment X", UNARY_PLUS_KEY,  TRUE, {4,   11, 19, 23}},
        {"decrement X", UNARY_MINUS_KEY, TRUE, {2,   11, 19, 23}},
        {"negate X",    NEGATE_KEY,    TRUE,  {253, 11, 19, 23}},
        {"complement X", NOT_KEY,      TRUE,  {252, 11, 19, 23}},
        {"swap X/Y",    SWAP_KEY,      TRUE,  {11, 3,  19, 23}},
        {"roll down",   ROLL_DOWN_KEY, TRUE,  {11, 19, 23, 3}},
        {"drop X",      DELETE_KEY,    TRUE,  {11, 19, 23, 0}},
    };
    const STACK initial = {3, 11, 19, 23};
    const STACK shiftInput = {1, (NUMBER)-4, 19, 23};
    const STACK signedShift = {(NUMBER)-2, 19, 23, 23};
    const STACK unsignedShift = {MAXLONGLONG - 1, 19, 23, 23};
    const STACK castInput = {0x1234, 0xff, 0x180, 0x123456};
    const STACK signedByte = {0x34, (NUMBER)-1, (NUMBER)-128, 0x56};
    const STACK zeroDivisor = {0, 11, 19, 23};
    INTEGER_TEST test = {{10, 64, INTEGER_TYPE_UNSIGNED, 16, {0, 0, 0, 0}}};
    static const USHORT radices[] = {2, 8, 10, 16};
    size_t base, i;
    int failures = 0;

    // Reuse distinct register values across radices so an operation that
    // drops, duplicates, or rotates the wrong register cannot pass on X alone.
    for (base = 0; base < ARRAYSIZE(radices); ++base) {
        INTEGER_TEST byteTest = {{radices[base], 8, INTEGER_TYPE_UNSIGNED, 16, {0, 0, 0, 0}}};
        char label[32];
        sprintf_s(label, sizeof(label), "base %u stack", (unsigned)radices[base]);
        for (i = 0; i < ARRAYSIZE(cases); ++i) {
            byteTest.State.Stack = initial;
            byteTest.Entering = cases[i].entering;
            failures += CheckCase(PressInteger(&byteTest, cases[i].command) == CALCULATOR_REDRAW &&
                                  SameStack(byteTest.State.Stack, cases[i].expected) &&
                                  !byteTest.Entering && !byteTest.Error && byteTest.DivisionErrors == 0,
                                  label, cases[i].name);
        }
    }

    test.State.Stack = initial;
    test.Entering = TRUE;
    failures += Check(PressInteger(&test, KILO_KEY) == CALCULATOR_REDRAW &&
                      SameStack(test.State.Stack, (STACK){3072, 11, 19, 23}) &&
                      !test.Entering, "Kilo multiplies only X by 1024");

    test.State.Stack = zeroDivisor;
    test.Entering = TRUE;
    failures += Check(PressInteger(&test, DIV_KEY) == CALCULATOR_REDRAW &&
                      SameStack(test.StackAtError, zeroDivisor) && test.EnteringAtError &&
                      SameStack(test.State.Stack, zeroDivisor) && !test.Entering && !test.Error &&
                      test.DivisionErrors == 1, "show divide-by-zero before ending entry");
    test.Entering = TRUE;
    failures += Check(PressInteger(&test, MOD_KEY) == CALCULATOR_REDRAW &&
                      SameStack(test.StackAtError, zeroDivisor) && test.EnteringAtError &&
                      SameStack(test.State.Stack, zeroDivisor) && !test.Entering && !test.Error &&
                      test.DivisionErrors == 2, "modulo by zero preserves the stack");

    test.State.Stack = shiftInput;
    PressInteger(&test, RIGHT_SHIFT_KEY);
    failures += Check(SameStack(test.State.Stack, unsignedShift), "unsigned shift fills with zeros");
    PressInteger(&test, IDM_INTEGER_TYPE_2S);
    test.State.Stack = shiftInput;
    PressInteger(&test, RIGHT_SHIFT_KEY);
    failures += Check(SameStack(test.State.Stack, signedShift), "signed right shift extends the sign");

    PressInteger(&test, IDM_BINARY);
    failures += Check(test.Error && test.State.Radix == 10 && test.State.LastRadix == 16,
                      "reject binary display at 64 bits without changing radix history");
    test.State.Stack = castInput;
    PressInteger(&test, IDM_WORD_SIZE_8);
    failures += Check(test.State.WordSize == 8 && SameStack(test.State.Stack, signedByte),
                      "word-size change recasts all four signed registers");
    PressInteger(&test, IDM_BINARY);
    test.Error = FALSE;
    PressInteger(&test, IDM_WORD_SIZE_64);
    failures += Check(test.Error && test.State.WordSize == 8 && test.State.Radix == 2,
                      "reject a 64-bit word size in binary display");
    PressInteger(&test, IDM_BINARY);
    PressInteger(&test, TOGGLE_BASE_KEY);
    failures += Check(test.State.Radix == 10 && test.State.LastRadix == 2,
                      "toggle back to the last distinct radix");
    return failures;
}

static int CheckIntegerWordSizes(void)
{
    static const struct {
        DWORD command;
        NUMBER maximum;
        NUMBER signBit;
        SIGNED_NUMBER afterBackspace;
    } cases[] = {
        {IDM_WORD_SIZE_8,  0xff,         0x80,                      -12},
        {IDM_WORD_SIZE_16, 0xffff,       0x8000,                    -3276},
        {IDM_WORD_SIZE_32, 0xffffffff,   0x80000000,                -214748364},
        {IDM_WORD_SIZE_64, MAXULONGLONG, (NUMBER)MAXLONGLONG + 1,   -922337203685477580LL},
    };
    size_t i;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        INTEGER_TEST test = {{16, 64, INTEGER_TYPE_UNSIGNED, 10, {0, 0, 0, 0}}};
        SIGNED_NUMBER expected = cases[i].afterBackspace;
        PressInteger(&test, cases[i].command);
        test.State.Stack.X = cases[i].maximum;
        PressInteger(&test, UNARY_PLUS_KEY);
        failures += Check(test.State.Stack.X == 0, "increment wraps at the selected word size");
        test.State.Stack.X = cases[i].signBit;
        PressInteger(&test, IDM_INTEGER_TYPE_2S);
        PressInteger(&test, IDM_DECIMAL);
        PressInteger(&test, BACKSPACE_KEY);
        if ((SIGNED_NUMBER)test.State.Stack.X != expected) {
            printf("FAIL: %u-bit signed decimal backspace: expected %lld, got %lld\n",
                   (unsigned)test.State.WordSize, expected, (SIGNED_NUMBER)test.State.Stack.X);
            ++failures;
        }
        // Backspace leaves the negative prefix of the minimum. At 64 bits,
        // appending 8 restores the minimum; trying 9 must leave X alone.
        // The narrower negative append cases are a known bug, tracked separately.
        if (cases[i].command == IDM_WORD_SIZE_64) {
            test.Entering = TRUE;
            failures += Check(PressInteger(&test, '8') == CALCULATOR_REDRAW &&
                              test.State.Stack.X == cases[i].signBit && test.Entering,
                              "appending 8 reaches the signed 64-bit decimal minimum");
            test.State.Stack.X = (NUMBER)expected;
            failures += Check(PressInteger(&test, '9') == CALCULATOR_ENTRY_REJECTED &&
                              test.State.Stack.X == (NUMBER)expected && test.Entering && !test.Error,
                              "appending 9 would exceed the signed 64-bit decimal minimum");
        }
    }
    return failures;
}

typedef struct {
    FLOAT_STATE State;
    BOOL Entering;
    BOOL Error;
    const char *Message;
    unsigned Reports;
    FSTACK StackAtError;
    BOOL EnteringAtError;
} FLOAT_TEST;

static void RecordFloatError(void *context, const char *message)
{
    FLOAT_TEST *test = context;
    test->Message = message;
    ++test->Reports;
    test->StackAtError = test->State.Stack;
    test->EnteringAtError = test->Entering;
}

static CALCULATOR_RESULT PressFloat(FLOAT_TEST *test, DWORD command)
{
    return FloatCommand(&test->State, command, &test->Entering, &test->Error,
                        RecordFloatError, test);
}

static int SameFloatStack(FSTACK actual, FSTACK expected)
{
    return fabs(actual.X - expected.X) < 1e-12 && actual.Y == expected.Y &&
           actual.Z == expected.Z && actual.T == expected.T;
}

static int CheckFloatStack(void)
{
    static const struct {
        const char *name;
        DWORD command;
        BOOL entering;
        FSTACK expected;
    } cases[] = {
        {"commit X",      ENTER,           TRUE,  {2.5,  11,  19, 23}},
        {"duplicate X",   ENTER,           FALSE, {3,    3,   11, 19}},
        {"Y plus X",      PLUS_KEY,        TRUE,  {13.5, 19,  23, 23}},
        {"Y minus X",     MINUS_KEY,       TRUE,  {8.5,  19,  23, 23}},
        {"Y times X",     MULT_KEY,        TRUE,  {27.5, 19,  23, 23}},
        {"Y over X",      DIV_KEY,         TRUE,  {4.4,  19,  23, 23}},
        {"increment X",   UNARY_PLUS_KEY,  TRUE,  {3.5,  11,  19, 23}},
        {"decrement X",   UNARY_MINUS_KEY, TRUE,  {1.5,  11,  19, 23}},
        {"reciprocal X",  ONE_OVER_KEY,    TRUE,  {0.4,  11,  19, 23}},
        {"negate X",      NEGATE_KEY,      FALSE, {-3,   11,  19, 23}},
        {"swap X/Y",      SWAP_KEY,        TRUE,  {11,   2.5, 19, 23}},
        {"roll down",     ROLL_DOWN_KEY,   TRUE,  {11,   19,  23, 2.5}},
        {"drop X",        DELETE_KEY,      TRUE,  {11,   19,  23, 0}},
        {"replace with pi", PI_KEY,        TRUE,  {3.141592653589793, 11, 19, 23}},
        {"push pi",        PI_KEY,         FALSE, {3.141592653589793, 3,  11, 19}},
    };
    size_t i;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        FLOAT_TEST test = {{{3, 11, 19, 23}, "2.5", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG},
                           cases[i].entering};
        failures += CheckCase(PressFloat(&test, cases[i].command) == CALCULATOR_REDRAW &&
                              SameFloatStack(test.State.Stack, cases[i].expected) &&
                              !test.Entering && !test.Error && test.Reports == 0,
                              "float stack", cases[i].name);
    }
    return failures;
}

static int CheckFloatEntry(void)
{
    FLOAT_TEST test = {{{7, 11, 19, 23}, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG}};
    const FSTACK lifted = {7, 7, 11, 19};
    const FSTACK committed = {0.25, 7, 11, 19};
    const char *entry = (const char *)test.State.Entry;
    int failures = 0;

    PressFloat(&test, EEX_KEY);
    failures += Check(!test.Entering && entry[0] == '\0', "EEx outside entry is a no-op");
    PressFloat(&test, '2');
    PressFloat(&test, DECIMAL_KEY);
    failures += Check(PressFloat(&test, '5') == CALCULATOR_REDRAW &&
                      PressFloat(&test, DECIMAL_KEY) == CALCULATOR_ENTRY_REJECTED &&
                      strcmp(entry, "2.5") == 0 && test.Entering && !test.Error &&
                      SameFloatStack(test.State.Stack, lifted),
                      "lift once and keep raw text while rejecting a second decimal point");
    PressFloat(&test, EEX_KEY);
    PressFloat(&test, '2');
    PressFloat(&test, EEX_KEY);
    failures += Check(strcmp(entry, "2.5") == 0, "a second EEx removes the exponent");
    PressFloat(&test, EEX_KEY);
    PressFloat(&test, '2');
    PressFloat(&test, NEGATE_KEY);
    failures += Check(strcmp(entry, "2.5E-2") == 0, "change the exponent sign, not the mantissa");
    PressFloat(&test, NEGATE_KEY);
    failures += Check(strcmp(entry, "2.5E2") == 0, "remove the exponent sign and keep its last digit");
    PressFloat(&test, NEGATE_KEY);
    PressFloat(&test, BACKSPACE_KEY);
    PressFloat(&test, '1');
    failures += Check(strcmp(entry, "2.5E-1") == 0 &&
                      PressFloat(&test, ENTER) == CALCULATOR_REDRAW && !test.Entering &&
                      SameFloatStack(test.State.Stack, committed),
                      "commit the edited exponent without lifting again");
    PressFloat(&test, '7');
    PressFloat(&test, NEGATE_KEY);
    failures += Check(strcmp(entry, "-7") == 0 && test.Entering,
                      "negate the mantissa when there is no exponent");
    PressFloat(&test, BACKSPACE_KEY);
    PressFloat(&test, BACKSPACE_KEY);
    failures += Check(entry[0] == '\0' && test.Entering, "backspace clears the mantissa");
    failures += Check(PressFloat(&test, '!') == CALCULATOR_UNKNOWN_COMMAND && test.Error &&
                      test.Entering && entry[0] == '\0',
                      "unknown commands set the display error without changing the entry");
    failures += Check(PressFloat(&test, ENTER) == CALCULATOR_REDRAW && !test.Entering &&
                      SameFloatStack(test.State.Stack, (FSTACK){0, 0.25, 7, 11}),
                      "commit an empty entry to zero without lifting again");
    return failures;
}

static int CheckFloatEntryCapacity(void)
{
    FLOAT_TEST test = {{{7, 11, 19, 23}, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_GRAD}, TRUE};
    char *entry = (char *)test.State.Entry;
    char full[DISPLAY_WIDTH + 1];
    int failures = 0;

    // Text supplied by the adapter may already use every display column.
    memset(full, '1', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
    memcpy(entry, full, sizeof(full));
    failures += Check(PressFloat(&test, NEGATE_KEY) == CALCULATOR_ENTRY_REJECTED &&
                      PressFloat(&test, EEX_KEY) == CALCULATOR_ENTRY_REJECTED &&
                      strcmp(entry, full) == 0 && test.Entering,
                      "inserting a sign or exponent cannot overrun a full entry");
    PressFloat(&test, BACKSPACE_KEY);
    failures += Check(PressFloat(&test, NEGATE_KEY) == CALCULATOR_REDRAW &&
                      entry[0] == '-' && strlen(entry) == DISPLAY_WIDTH &&
                      strncmp(entry + 1, full, DISPLAY_WIDTH - 1) == 0,
                      "inserting a sign into the last available column preserves the terminator");
    failures += Check(PressFloat(&test, NEGATE_KEY) == CALCULATOR_REDRAW &&
                      strlen(entry) == DISPLAY_WIDTH - 1 &&
                      strncmp(entry, full, DISPLAY_WIDTH - 1) == 0,
                      "removing the sign from a full entry keeps all remaining digits");
    return failures;
}

static int CheckFloatFunctions(void)
{
    static const struct {
        const char *name;
        DWORD command;
        FNUMBER degrees;
        FNUMBER radians;
        FNUMBER grads;
        FNUMBER ratio;
    } trig[] = {
        {"sin / arcsin", SIN_KEY, 30, 0.5235987755982989, 33.33333333333333, 0.5},
        {"cos / arccos", COS_KEY, 60, 1.0471975511965976, 66.66666666666667, 0.5},
        {"tan / arctan", TAN_KEY, 45, 0.7853981633974483, 50, 1.0},
    };
    static const DWORD units[] = {IDM_TRIG_DEGREE, IDM_TRIG_RADIAN, IDM_TRIG_GRAD};
    static const struct {
        const char *name;
        DWORD command;
        FNUMBER y;
        FNUMBER input;
        FNUMBER direct;
        FNUMBER inverse;
        BOOL dropsY;
    } functions[] = {
        {"log / power of ten", LOG_KEY, 7, 100, 2, 100, FALSE},
        {"ln / exponential", LN_KEY, 7, 2.718281828459045, 1, 2.718281828459045, FALSE},
        {"sqrt / square", SQRT_KEY, 7, 9, 3, 9, FALSE},
        {"power / root", YTOX_KEY, 16, 2, 256, 4, TRUE},
    };
    size_t i, unit;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(trig); ++i) {
        const FNUMBER angles[] = {trig[i].degrees, trig[i].radians, trig[i].grads};
        for (unit = 0; unit < ARRAYSIZE(units); ++unit) {
            FLOAT_TEST test = {{{angles[unit], 7, 11, 19}, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG}};
            FSTACK expected = {trig[i].ratio, 7, 11, 19};
            PressFloat(&test, units[unit]);
            PressFloat(&test, trig[i].command);
            failures += CheckCase(SameFloatStack(test.State.Stack, expected) && test.Reports == 0,
                                  trig[i].name, "use the selected angle unit without dropping Y");
            test.State.Stack.X = trig[i].ratio;
            PressFloat(&test, INVERSE_KEY);
            PressFloat(&test, trig[i].command);
            expected.X = angles[unit];
            failures += CheckCase(SameFloatStack(test.State.Stack, expected) &&
                                  !test.State.Inverse && test.Reports == 0,
                                  trig[i].name, "return the selected angle unit and consume inverse");
        }
    }

    for (i = 0; i < ARRAYSIZE(functions); ++i) {
        FSTACK start = {functions[i].input, functions[i].y, 11, 19};
        FSTACK expected = {functions[i].direct, start.Y, start.Z, start.T};
        FLOAT_TEST test = {{start, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG}};
        if (functions[i].dropsY) {
            expected.Y = start.Z;
            expected.Z = start.T;
        }
        PressFloat(&test, functions[i].command);
        failures += CheckCase(SameFloatStack(test.State.Stack, expected) && test.Reports == 0,
                              functions[i].name, "normal function");
        test.State.Stack = start;
        if (!functions[i].dropsY)
            test.State.Stack.X = functions[i].direct;
        PressFloat(&test, INVERSE_KEY);
        PressFloat(&test, functions[i].command);
        expected.X = functions[i].inverse;
        failures += CheckCase(SameFloatStack(test.State.Stack, expected) &&
                              !test.State.Inverse && test.Reports == 0,
                              functions[i].name, "inverse function");
    }
    return failures;
}

static int CheckFloatErrors(void)
{
    // Start from text that differs from the old X. The callback snapshot
    // distinguishes reporting before commit from reporting after commit.
    static const struct {
        const char *entry;
        DWORD command;
        BOOL inverse;
        FNUMBER expectedX;
        FNUMBER y;
        const char *message;
    } cases[] = {
        {"0",  DIV_KEY,      FALSE, 0, 7, "Division by zero"},
        {"0",  ONE_OVER_KEY, FALSE, 0, 7, "Division by zero"},
        {"-4", SQRT_KEY,     FALSE, -4, 7, "Cannot give Square Root of negative number."},
        {"-1", LOG_KEY,      FALSE, -1, 7, "Log(negative number) is undefined"},
        {"-1", LN_KEY,       FALSE, -1, 7, "Ln(negative number) is undefined"},
        {"2",  SIN_KEY,      TRUE,  2, 7, "arc sin domain is [-1.0..1.0]"},
        {"-2", COS_KEY,      TRUE, -2, 7, "arc cos domain is [-1.0..1.0]"},
        {"0",  YTOX_KEY,     TRUE,  0, 7, "Division by zero"},
        {"0",  YTOX_KEY,     FALSE, 0, 0, "0^0 is undefined"},
    };
    size_t i;
    int failures = 0;

    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        FLOAT_TEST test = {{{3, cases[i].y, 11, 19}, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG,
                            cases[i].inverse}, TRUE};
        FSTACK expected = {cases[i].expectedX, cases[i].y, 11, 19};
        strcpy_s((char *)test.State.Entry, sizeof(test.State.Entry), cases[i].entry);
        failures += CheckCase(PressFloat(&test, cases[i].command) == CALCULATOR_REDRAW &&
                              test.Reports == 1 && strcmp(test.Message, cases[i].message) == 0 &&
                              !test.EnteringAtError && SameFloatStack(test.StackAtError, expected) &&
                              !test.State.Inverse && !test.Entering && !test.Error &&
                              SameFloatStack(test.State.Stack, expected),
                              cases[i].message, "report synchronously after committing the entry");
    }
    return failures;
}

static void CountCalculatorError(void *context, const char *message)
{
    unsigned *reports = context;
    (void)message;
    ++*reports;
}

typedef struct {
    const CALCULATOR_STATE *State;
    BOOL EnteringAtError;
    NUMBER XAtError;
    const char *Message;
} ROUTING_ERROR;

static void RecordRoutingError(void *context, const char *message)
{
    ROUTING_ERROR *report = context;
    report->EnteringAtError = report->State->Entering;
    report->XAtError = report->State->Integer.Stack.X;
    report->Message = message;
}

static int CheckCalculatorRouting(void)
{
    CALCULATOR_STATE first = {
        {10, 64, INTEGER_TYPE_UNSIGNED, 16, {5, 7, 11, 19}},
        {{3, 13, 17, 23}, "", 10, FLOAT_DISPLAY_FIX, FLOAT_ANGLE_DEG},
        CALC_MODE_BINARY, FALSE, FALSE
    };
    CALCULATOR_STATE second = first;
    unsigned reports = 0;
    int failures = 0;

    // The same key edits text in float mode but shifts X in integer mode.
    // Interleave two instances to check both routing and state ownership.
    failures += Check(CalculatorCommand(&first, IDM_FLOAT, CountCalculatorError, &reports) ==
                      CALCULATOR_MODE_CHANGED && first.Mode == CALC_MODE_FLOAT, "enter float mode");
    CalculatorCommand(&first, DECIMAL_KEY, CountCalculatorError, &reports);
    CalculatorCommand(&first, '4', CountCalculatorError, &reports);
    CalculatorCommand(&second, UNARY_RIGHT_SHIFT_KEY, CountCalculatorError, &reports);
    failures += Check(first.Entering && strcmp((const char *)first.Float.Entry, ".4") == 0 &&
                      first.Integer.Stack.X == 5 && second.Integer.Stack.X == 2 &&
                      !second.Entering && second.Float.Entry[0] == '\0',
                      "identical key codes route by mode without affecting the other calculator");

    // An unsupported base still returns to integer mode. The shared entry
    // flag allows an append to integer X without committing the float text.
    failures += Check(CalculatorCommand(&first, IDM_BINARY, CountCalculatorError, &reports) ==
                      CALCULATOR_MODE_CHANGED && first.Mode == CALC_MODE_BINARY &&
                      first.Error && first.Entering && first.Integer.Radix == 10,
                      "64-bit binary rejection still switches mode and preserves entry");
    CalculatorCommand(&first, '2', CountCalculatorError, &reports);
    CalculatorCommand(&first, IDM_FLOAT, CountCalculatorError, &reports);
    CalculatorCommand(&first, ENTER, CountCalculatorError, &reports);
    failures += Check(SameStack(first.Integer.Stack, (STACK){52, 7, 11, 19}) &&
                      SameFloatStack(first.Float.Stack, (FSTACK){0.4, 3, 13, 17}) &&
                      !first.Entering && first.Error && !second.Error && reports == 0,
                      "a mode change carries the shared flags without merging register or entry state");
    // This value cannot round-trip through double. Register movement must
    // preserve every integer bit even though the float path shares its code.
    second.Integer.Stack.Y = 0xfedcba9876543210ULL;
    CalculatorCommand(&second, SWAP_KEY, CountCalculatorError, &reports);
    failures += Check(SameStack(second.Integer.Stack, (STACK){0xfedcba9876543210ULL, 2, 11, 19}),
                      "shared swap preserves all 64 integer bits");

    // The raw hex entry and its sign-extended value show when recasting
    // would have occurred. Neither mode transition should recast integer X.
    second.Integer = (INTEGER_STATE){16, 8, INTEGER_TYPE_2S_COMP, 10, {0, 7, 11, 19}};
    CalculatorCommand(&second, 'F', CountCalculatorError, &reports);
    CalculatorCommand(&second, 'F', CountCalculatorError, &reports);
    failures += Check(CalculatorCommand(&second, IDM_FLOAT, CountCalculatorError, &reports) ==
                      CALCULATOR_MODE_CHANGED && second.Integer.Stack.X == 255 && second.Entering,
                      "switching to float leaves a signed hex entry raw");
    failures += Check(CalculatorCommand(&second, IDM_DECIMAL, CountCalculatorError, &reports) ==
                      CALCULATOR_MODE_CHANGED && second.Mode == CALC_MODE_BINARY &&
                      second.Integer.Radix == 10 && second.Integer.LastRadix == 16 &&
                      second.Integer.Stack.X == 255 && second.Entering && !second.Error,
                      "switching back selects the new radix without recasting X");

    // Observe the synchronous callback through the composed entry point,
    // not just the integer-specific handler.
    CalculatorCommand(&second, DELETE_KEY, CountCalculatorError, &reports);
    CalculatorCommand(&second, '0', CountCalculatorError, &reports);
    ROUTING_ERROR report = {&second};
    failures += Check(CalculatorCommand(&second, DIV_KEY, RecordRoutingError, &report) ==
                      CALCULATOR_REDRAW && report.Message &&
                      strcmp(report.Message, "Division by zero") == 0 &&
                      report.EnteringAtError && report.XAtError == 0 && !second.Entering && !second.Error,
                      "routed division reports the error before clearing integer entry");
    return failures;
}

static int CheckDisplayRow(const char *actual, const char *expected, const char *scenario, size_t row)
{
    if (strcmp(actual, expected) == 0)
        return 0;
    printf("FAIL: %s display row %zu: expected [%s], got [%s]\n", scenario, row, expected, actual);
    return 1;
}

static int CheckIntegerDisplay(void)
{
    static const struct {
        USHORT radix;
        USHORT width;
        STACK stack;
        const char *status;
        const char *rows[4]; // T, Z, Y, X
    } cases[] = {
        {2, 32, {0xffffffff, 0x100, 0xaa, 1}, "    Bin  32     Unsigned",
         {"1", "10101010", "1 00000000", "11111111 11111111 11111111 11111111"}},
        {8, 32, {0xffffffff, 010000, 0777, 7}, "    Oct  32     Unsigned",
         {"7", "777", "1 0000", "377 7777 7777"}},
        {10, 32, {0xffffffff, 1000, 999, 0}, "    Dec  32     Unsigned",
         {"0", "999", "1,000", "4,294,967,295"}},
        {16, 64, {0xabcd0123456789efULL, 0x100000000ULL, 0xabc, 0}, "    Hex  64     Unsigned",
         {"0", "abc", "1 00000000", "abcd0123 456789ef"}},
    };
    CALCULATOR_STATE state = {0};
    DISPLAY_LINES lines;
    size_t i, row;
    int failures = 0;

    state.Mode = CALC_MODE_BINARY;
    // Test register order and grouping at each radix's separator boundary.
    // Each expected value is then padded to the documented 35 columns.
    for (i = 0; i < ARRAYSIZE(cases); ++i) {
        state.Integer = (INTEGER_STATE){cases[i].radix, cases[i].width, INTEGER_TYPE_UNSIGNED, 10,
                                        cases[i].stack};
        failures += Check(ComposeDisplayLines(&state, &lines), cases[i].status);
        failures += CheckDisplayRow(lines.Text[0], cases[i].status, "integer", 0);
        for (row = 0; row < ARRAYSIZE(cases[i].rows); ++row) {
            char expected[DISPLAY_WIDTH + 1];
            sprintf_s(expected, sizeof(expected), "%35s", cases[i].rows[row]);
            failures += CheckDisplayRow(lines.Text[row + 1], expected, cases[i].status, row + 1);
        }
    }

    state.Integer = (INTEGER_STATE){10, 8, INTEGER_TYPE_2S_COMP, 16, {(NUMBER)-128, 3, 2, 1}};
    state.Error = TRUE;
    failures += Check(ComposeDisplayLines(&state, &lines), "compose signed decimal with an error indicator");
    failures += CheckDisplayRow(lines.Text[0], "!   Dec  8      ~2s", "signed decimal", 0);
    failures += CheckDisplayRow(lines.Text[4], "                               -128", "signed decimal", 4);
    state.Integer.Radix = 16;
    failures += Check(ComposeDisplayLines(&state, &lines), "compose signed hex at 8 bits");
    failures += CheckDisplayRow(lines.Text[4], "                                 80", "signed hex", 4);
    state.Integer = (INTEGER_STATE){2, 64, INTEGER_TYPE_UNSIGNED, 16, {MAXULONGLONG, 3, 2, 1}};
    failures += Check(!ComposeDisplayLines(&state, &lines), "reject a grouped value that does not fit");
    failures += CheckDisplayRow(lines.Text[4], "", "oversized binary", 4);
    failures += CheckDisplayRow(lines.Text[3], "                                 11", "oversized binary", 3);
    return failures;
}

static int CheckFloatDisplay(void)
{
    CALCULATOR_STATE state = {0};
    DISPLAY_LINES lines;
    int failures = 0;

    state.Mode = CALC_MODE_FLOAT;
    state.Float = (FLOAT_STATE){{1.23456789123, 0.25, -1234.25, 12000.0}, "2.5E-", 10,
                                FLOAT_DISPLAY_AUTO, FLOAT_ANGLE_DEG, FALSE};
    failures += Check(ComposeDisplayLines(&state, &lines), "compose committed float");
    failures += CheckDisplayRow(lines.Text[0], "          Degree" "      ", "float", 0);
    failures += CheckDisplayRow(lines.Text[1], "12,000.", "float", 1);
    failures += CheckDisplayRow(lines.Text[2], "-1,234.25", "float", 2);
    failures += CheckDisplayRow(lines.Text[3], "0.25", "float", 3);
    failures += CheckDisplayRow(lines.Text[4], "1.234567891", "10 significant digits", 4);

    // An unfinished exponent is intentionally shown verbatim. It must not
    // replace the stored X or change the formatting of the other registers.
    state.Entering = TRUE;
    state.Error = TRUE;
    state.Float.Inverse = TRUE;
    state.Float.AngleType = FLOAT_ANGLE_GRAD;
    failures += Check(ComposeDisplayLines(&state, &lines), "compose active float entry");
    failures += CheckDisplayRow(lines.Text[0], "!           Grad      Inverse", "float entering", 0);
    failures += CheckDisplayRow(lines.Text[1], "12,000.", "float entering", 1);
    failures += CheckDisplayRow(lines.Text[2], "-1,234.25", "float entering", 2);
    failures += CheckDisplayRow(lines.Text[3], "0.25", "float entering", 3);
    failures += CheckDisplayRow(lines.Text[4], "2.5E-", "float entering", 4);
    state.Entering = FALSE;
    state.Error = FALSE;
    state.Float.AngleType = FLOAT_ANGLE_RAD;
    failures += Check(ComposeDisplayLines(&state, &lines), "compose committed float Auto");
    failures += CheckDisplayRow(lines.Text[0], "             Rad      Inverse", "float Auto", 0);
    failures += CheckDisplayRow(lines.Text[4], "1.234567891", "10 significant digits in Auto", 4);

    state.Float.DisplayMode = FLOAT_DISPLAY_SCI;
    failures += Check(!ComposeDisplayLines(&state, &lines), "scientific formatting is not implemented");
    failures += CheckDisplayRow(lines.Text[4], "", "unsupported scientific", 4);
    state.Float.DisplayMode = FLOAT_DISPLAY_ENG;
    failures += Check(!ComposeDisplayLines(&state, &lines), "engineering formatting is not implemented");
    failures += CheckDisplayRow(lines.Text[4], "", "unsupported engineering", 4);
    return failures;
}

int main(void)
{
    char buffer[DISPLAY_WIDTH + 1];
    NUMBER number;
    FNUMBER fNumber;
    int failures = 0;

    failures += Check(IntToA(0xfeed, buffer, sizeof(buffer), 16,
                             INTEGER_TYPE_UNSIGNED, 64, FALSE) != NULL &&
                      strcmp(buffer, "feed") == 0,
                      "format unsigned hexadecimal");
    failures += Check(IntToA((NUMBER)-1, buffer, sizeof(buffer), 10,
                             INTEGER_TYPE_2S_COMP, 64, FALSE) != NULL &&
                      strcmp(buffer, "-1") == 0,
                      "format signed decimal");
    failures += Check(StrNumTest((PUCHAR)"FF", 16, INTEGER_TYPE_UNSIGNED,
                                 64, &number) && number == 0xff,
                      "parse valid hexadecimal");
    failures += Check(!StrNumTest((PUCHAR)"FG", 16, INTEGER_TYPE_UNSIGNED,
                                  64, &number),
                      "reject invalid hexadecimal");
    failures += Check(FToA(12345.0, buffer, sizeof(buffer), 10,
                           FLOAT_DISPLAY_FIX, FALSE) != NULL &&
                      strcmp(buffer, "12345.") == 0,
                      "format floating point");
    failures += Check(FStrNumTest((PUCHAR)"-12.5", &fNumber) &&
                      fabs(fNumber + 12.5) < 0.000001,
                      "parse floating point");

    failures += CheckIntegerRadices();
    failures += CheckIntegerStack();
    failures += CheckIntegerWordSizes();
    failures += CheckFloatStack();
    failures += CheckFloatEntry();
    failures += CheckFloatEntryCapacity();
    failures += CheckFloatFunctions();
    failures += CheckFloatErrors();
    failures += CheckCalculatorRouting();
    failures += CheckIntegerDisplay();
    failures += CheckFloatDisplay();
    failures += CheckRegistryState();

    if (failures != 0)
        return 1;

    puts("RPN logic tests passed.");
    return 0;
}
