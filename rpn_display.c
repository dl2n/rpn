#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

static BOOL ComposeIntegerLines(const INTEGER_STATE *state, BOOL error, DISPLAY_LINES *lines)
{
    const STACK *stack = &state->Stack;
    const NUMBER values[] = {stack->T, stack->Z, stack->Y, stack->X};
    const char *integerType;
    const char *base;
    BOOL valid = TRUE;
    size_t i;

    switch (state->IntegerType) {
    case INTEGER_TYPE_1S_COMP: integerType = "~1s"; break;
    case INTEGER_TYPE_2S_COMP: integerType = "~2s"; break;
    default: integerType = "Unsigned"; break;
    }

    switch (state->Radix) {
    case 16: base = "Hex"; break;
    case 10: base = "Dec"; break;
    case 8:  base = "Oct"; break;
    default: base = "Bin"; break;
    }

    sprintf_s(lines->Text[0], sizeof(lines->Text[0]), "%c   %s  %-2u     %s",
              error ? '!' : ' ', base, (unsigned)state->WordSize, integerType);
    for (i = 0; i < ARRAYSIZE(values); ++i) {
        char *row = lines->Text[i + 1];
        if (!IntToA(values[i], row, sizeof(lines->Text[0]), state->Radix,
                    state->IntegerType, state->WordSize, TRUE)) {
            row[0] = '\0';
            valid = FALSE;
        }
    }
    return valid;
}

static BOOL ComposeFloatLines(const FLOAT_STATE *state, BOOL entering, BOOL error,
                              DISPLAY_LINES *lines)
{
    const FSTACK *stack = &state->Stack;
    const FNUMBER values[] = {stack->T, stack->Z, stack->Y, stack->X};
    const char *angle;
    BOOL valid = TRUE;
    size_t i;

    switch (state->AngleType) {
    case FLOAT_ANGLE_RAD:  angle = "Rad"; break;
    case FLOAT_ANGLE_GRAD: angle = "Grad"; break;
    default: angle = "Degree"; break;
    }

    sprintf_s(lines->Text[0], sizeof(lines->Text[0]), "%c         %6s      %s",
              error ? '!' : ' ', angle, state->Inverse ? "Inverse" : "");
    for (i = 0; i < ARRAYSIZE(values); ++i) {
        char *row = lines->Text[i + 1];
        if (entering && i == ARRAYSIZE(values) - 1) {
            memcpy(row, state->Entry, DISPLAY_WIDTH);
            row[DISPLAY_WIDTH] = '\0';
        } else if (!FToA(values[i], row, sizeof(lines->Text[0]), state->SignificantDigits,
                         state->DisplayMode, TRUE)) {
            row[0] = '\0';
            valid = FALSE;
        }
    }
    return valid;
}

BOOL ComposeDisplayLines(const CALCULATOR_STATE *state, DISPLAY_LINES *lines)
{
    switch (state->Mode) {
    case CALC_MODE_BINARY:
        return ComposeIntegerLines(&state->Integer, state->Error, lines);
    case CALC_MODE_FLOAT:
        return ComposeFloatLines(&state->Float, state->Entering, state->Error, lines);
    default:
        memset(lines, 0, sizeof(*lines));
        return FALSE;
    }
}
