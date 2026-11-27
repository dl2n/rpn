#include <io.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <commctrl.h>
#include "rpn.h"

// Only the desktop test copy links this transport. It never uses the
// system clipboard and refuses to create a missing fixture.
static FILE *OpenTestClipboard(const wchar_t *mode)
{
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    wchar_t *name;
    FILE *file;

    if (!length || length >= ARRAYSIZE(path))
        return NULL;
    name = wcsrchr(path, L'\\');
    if (!name || wcscpy_s(name + 1, ARRAYSIZE(path) - (name + 1 - path),
                          L"rpn-test-clipboard.txt") != 0 ||
        _wfopen_s(&file, path, mode) != 0)
        return NULL;
    return file;
}

char *ReadClipboardToken(HWND owner)
{
    char text[4096];
    size_t length;
    BOOL valid;
    FILE *file = OpenTestClipboard(L"rb");
    (void)owner;

    if (!file)
        return NULL;
    length = fread(text, 1, sizeof(text) - 1, file);
    valid = !ferror(file) && fgetc(file) == EOF && feof(file);
    fclose(file);
    text[length] = '\0';
    return valid ? ExtractPasteToken(text, length + 1) : NULL;
}

BOOL WriteClipboardText(HWND owner, const char *text)
{
    size_t length = strlen(text);
    FILE *file = OpenTestClipboard(L"rb+");
    BOOL written;
    (void)owner;

    if (!file)
        return FALSE;
    written = fwrite(text, 1, length, file) == length && fflush(file) == 0 &&
              _chsize_s(_fileno(file), length) == 0;
    return fclose(file) == 0 && written;
}
