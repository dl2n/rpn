#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

// Generate typed moves for both stack records from the same register order.
// The command handlers still own entry, coercion, and the shared flags.
#define DEFINE_STACK_MOVES(kind, stackType, numberType)                   \
    void Swap##kind##Registers(stackType *stack) {                        \
        numberType x = stack->X;                                         \
        stack->X = stack->Y;                                             \
        stack->Y = x;                                                    \
    }                                                                    \
    void Drop##kind##Y(stackType *stack) {                                \
        stack->Y = stack->Z;                                             \
        stack->Z = stack->T;                                             \
    }                                                                    \
    void Roll##kind##Registers(stackType *stack) {                        \
        numberType x = stack->X;                                         \
        stack->X = stack->Y;                                             \
        Drop##kind##Y(stack);                                             \
        stack->T = x;                                                    \
    }                                                                    \
    void Duplicate##kind##X(stackType *stack) {                           \
        stack->T = stack->Z;                                             \
        stack->Z = stack->Y;                                             \
        stack->Y = stack->X;                                             \
    }                                                                    \
    void Drop##kind##X(stackType *stack) {                                \
        stack->X = stack->Y;                                             \
        Drop##kind##Y(stack);                                             \
        stack->T = 0;                                                    \
    }

DEFINE_STACK_MOVES(Integer, STACK, NUMBER)
DEFINE_STACK_MOVES(Float, FSTACK, FNUMBER)
