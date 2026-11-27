#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

char *ReadClipboardToken(HWND owner)
{
    char *token = NULL;
    if (OpenClipboard(owner)) {
        HGLOBAL data = GetClipboardData(CF_TEXT);
        SIZE_T size = data ? GlobalSize(data) : 0;
        const char *text = size ? GlobalLock(data) : NULL;
        if (text) {
            token = ExtractPasteToken(text, size);
            GlobalUnlock(data);
        }
        CloseClipboard();
    }
    return token;
}

BOOL WriteClipboardText(HWND owner, const char *text)
{
    size_t size = strlen(text) + 1;
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
    char *buffer;
    BOOL written = FALSE;

    if (!data)
        return FALSE;
    buffer = GlobalLock(data);
    if (buffer) {
        memcpy(buffer, text, size);
        GlobalUnlock(data);
        if (OpenClipboard(owner)) {
            written = EmptyClipboard() && SetClipboardData(CF_TEXT, data) != NULL;
            CloseClipboard();
        }
    }
    // On success the system owns the handle; otherwise we still own it.
    if (!written)
        GlobalFree(data);
    return written;
}
