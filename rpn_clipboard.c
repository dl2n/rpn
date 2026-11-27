#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

char *ExtractPasteToken(const char *text, size_t capacity)
{
    const char *end = memchr(text, '\0', capacity);
    const char *start = text;
    const char *stop;
    size_t length;
    char *token;

    if (!end)
        return NULL;
    // Preserve the legacy paste rule: skip leading whitespace and read
    // one token, without modifying the source clipboard allocation.
    while (start < end && (unsigned char)*start <= MAX_CONTROL_CHAR)
        ++start;
    stop = start;
    while (stop < end && (unsigned char)*stop > MAX_CONTROL_CHAR)
        ++stop;
    if (stop == start)
        return NULL;

    length = stop - start;
    token = malloc(length + 1);
    if (token) {
        memcpy(token, start, length);
        token[length] = '\0';
    }
    return token;
}
