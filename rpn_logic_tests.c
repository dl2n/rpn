#include <math.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include "rpn.h"

char *IntToA(NUMBER Value, char *psString, ULONG Length, USHORT Radix,
             USHORT IntegerType, BOOLEAN Adorn);
BOOL StrNumTest(PUCHAR lpNumStr, USHORT Radix, USHORT usIntegerType,
                PNUMBER pNumber);
char *FToA(FNUMBER FX, char *psString, ULONG Length, USHORT SignificantDigits,
           USHORT usFloatDisplayMode, BOOLEAN Adorn);
BOOL FStrNumTest(PUCHAR lpNumStr, FNUMBER *pfNumber);

static int Check(int condition, const char *description)
{
    if (!condition)
        printf("FAIL: %s\n", description);
    return condition ? 0 : 1;
}

int main(void)
{
    char buffer[DISPLAY_WIDTH + 1];
    NUMBER number;
    FNUMBER fNumber;
    int failures = 0;

    failures += Check(IntToA(0xfeed, buffer, sizeof(buffer), 16,
                             INTEGER_TYPE_UNSIGNED, FALSE) != NULL &&
                      strcmp(buffer, "feed") == 0,
                      "format unsigned hexadecimal");
    failures += Check(IntToA((NUMBER)-1, buffer, sizeof(buffer), 10,
                             INTEGER_TYPE_2S_COMP, FALSE) != NULL &&
                      strcmp(buffer, "-1") == 0,
                      "format signed decimal");
    failures += Check(StrNumTest((PUCHAR)"FF", 16, INTEGER_TYPE_UNSIGNED,
                                 &number) && number == 0xff,
                      "parse valid hexadecimal");
    failures += Check(!StrNumTest((PUCHAR)"FG", 16, INTEGER_TYPE_UNSIGNED,
                                  &number),
                      "reject invalid hexadecimal");
    failures += Check(FToA(12345.0, buffer, sizeof(buffer), 10,
                           FLOAT_DISPLAY_FIX, FALSE) != NULL &&
                      strcmp(buffer, "12345.") == 0,
                      "format floating point");
    failures += Check(FStrNumTest((PUCHAR)"-12.5", &fNumber) &&
                      fabs(fNumber + 12.5) < 0.000001,
                      "parse floating point");

    if (failures != 0)
        return 1;

    puts("RPN logic tests passed.");
    return 0;
}
