/*
* RPN.c   RPN Hexadecimal Calculator.
*			  Loosely based upon Ray Duncan's HexCalc2 from "Advanced OS/2
*			  Programming"
*
* This calculator emulates a simple Hewlett-Packard-style 4-level stack of
* 32-bit integers.	There is no floating point (yet!).
*
* History:
*	 06/29/90	 BruceK   Created from OS/2 sample app by Ray Duncan.
*	 11/26/91	 BruceK   Rewrite for Windows NT
*	 04/16/92	 BruceK   Make it smart about number entry overflow.
*	 06/12/92	 BruceK   Make it handle clipboard copy/paste
*	 08/17/95	 BruceK   Make it work with current Win32 SDK
*	 08/25/97	 BruceK   Save/Restore state to registry
*
*	 11/17/99	 DanLo	  Implement variable wordsize, 64bit integer math,
*						  and many miscellaneous tweaks/cleanup.
*    03/21/00    JeffMill Added tooltips (search for NO_TOOLTIPS)
*
*/

#include <windows.h>
#ifndef NO_TOOLTIPS
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include "rpn.h"


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DisplayControlProc(HWND hwnd, UINT message, WPARAM wParam,
								 LPARAM lParam);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
static void CalcIt(DWORD dwOperation, HWND hwnd);
BOOL HelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
               UCHAR* szStringBuffer, DWORD cbStringBuffer );
static void PaintStackDisplay(HWND hwnd, HDC hdc, const CALCULATOR_STATE *state);
void DebugPrintf(const char * format, ...);
static BOOL NumTest(NUMBER X, USHORT Radix, USHORT Digit, USHORT usIntegerType, USHORT WordSize);
DWORD CreateCalc(CALC_MODE CalcMode);
BOOL ClipboardPaste(PNUMBER pdwNumber, USHORT Radix, USHORT usIntegerType);
BOOL ClipboardCopy(NUMBER dwNumber, USHORT Radix, USHORT usIntegerType);

extern LRESULT CALLBACK FWndProc(HWND hwnd, UINT message, WPARAM wParam,
							  LPARAM lParam);
extern LRESULT CALLBACK FDisplayControlProc(HWND hwnd, UINT message, WPARAM wParam,
										 LPARAM lParam);


CALCULATOR_STATE Calculator;
const CALCULATOR_STATE CalculatorDefaults = {
	// Start in 64-bit unsigned hex; Last Base initially selects decimal.
	{16, 64, INTEGER_TYPE_UNSIGNED, 10, {0, 0, 0, 0}},
	// Float starts in Auto with ten significant digits, degrees, and inverse off.
	{{0.0, 0.0, 0.0, 0.0}, "", FLOAT_SIGNIFICANT_DIGITS, FLOAT_DISPLAY_AUTO, FLOAT_ANGLE_DEG, FALSE},
	CALC_MODE_BINARY, FALSE, FALSE
};
BOOL fHelpKeysMode = FALSE; 					// Help keys mode
BOOL fKeypadHidden = FALSE;						// Keypad rolled up
int cyFullWindow = 0;							// Window height with keypad
int cyCollapsedWindow = 0;						// Window height without keypad
HWND hDisplay;									// handle to display window
HWND hwndDialog;
DWORD cyChar;
HANDLE hInst;
BOOL fFirstTime = TRUE;
BOOL bQuit = FALSE;

DWORD dwWindowX = 0;
DWORD dwWindowY = 0;

#ifndef NO_TOOLTIPS
HWND hwndTT = NULL;
#endif
   

int APIENTRY WinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR	   lpCmdLine,
					 int	   nCmdShow) {
	MSG msg;
	HACCEL accelerators;
	WNDCLASS wndclass;
	DWORD rc;
#ifndef NO_TOOLTIPS
    INITCOMMONCONTROLSEX iccex;
#endif
	
	DebugPrintf("Entering WinMain\n");
	if (! hPrevInstance)
    {

#ifndef NO_TOOLTIPS
        /* Initialize tooltip control */

        iccex.dwICC = ICC_BAR_CLASSES;
        iccex.dwSize = sizeof( iccex );
        InitCommonControlsEx( &iccex );
#endif

	/*  
	* Register window classes
		*/
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = RPN_CLASS;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = DLGWINDOWEXTRA;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(RPN_ICON));
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
		
		if (RegisterClass(&wndclass) == 0) {
			DebugPrintf("Couldn't register binary dialog class, error = %u\n",
				rc = GetLastError());
			return(rc);
		}
		
		wndclass.lpszClassName = RPNF_CLASS;
		
		if (RegisterClass(&wndclass) == 0) {
			DebugPrintf("Couldn't register float dialog class, error = %u\n",
				rc = GetLastError());
			return(rc);
		}
		
		wndclass.lpfnWndProc = DisplayControlProc;
		wndclass.cbWndExtra = 0;
		wndclass.hIcon = NULL;
		wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wndclass.lpszClassName = "Display";
		
		if (RegisterClass(&wndclass) == 0) {
			DebugPrintf("Couldn't register Display class, error = %u\n",
				rc = GetLastError());
			return(rc);
		}
		
		wndclass.lpszClassName = "FDisplay";
		
		if (RegisterClass(&wndclass) == 0) {
			DebugPrintf("Couldn't register FDisplay class, error = %u\n",
				rc = GetLastError());
			return(rc);
		}
	}
	
	hInst = hInstance;
	accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(RPN_ACCELERATORS));
	if (!accelerators) {
		rc = GetLastError();
		DebugPrintf("Could not load clipboard shortcuts: %u\n", rc);
		return rc ? (int)rc : ERROR_RESOURCE_NAME_NOT_FOUND;
	}
	
	RestoreRPNState();
	
	if (rc = CreateCalc(Calculator.Mode))
		return(rc);
	
	// Windows Message Loop
	// When we get a WM_QUIT message, GetMessage returns 0.
	
	while (GetMessage(&msg, NULL, 0, 0)) {
		if ((msg.hwnd == hwndDialog || IsChild(hwndDialog, msg.hwnd)) &&
			TranslateAccelerator(hwndDialog, accelerators, &msg))
			continue;
		TranslateMessage(&msg);
		if (msg.message == WM_COMMAND && msg.wParam == IDM_CHANGE_MODE)
			CreateCalc(Calculator.Mode);
		else
			DispatchMessage(&msg);
	}
	
	
	if (hDisplay) {
		DebugPrintf("Destroy display window handle %u\n", hDisplay);
		// must destroy the current DisplayWindow
		DestroyWindow(hDisplay);
	}
	
	if (hwndDialog) {
		DebugPrintf("Destroy the dialog window hwnd = %u\n", hwndDialog);
		DestroyWindow(hwndDialog);
	}
	
	DebugPrintf("main returned from message loop\n");
	
	rc = SaveRPNState();
	if (rc != ERROR_SUCCESS) {
		DebugPrintf("Could not save calculator state: %u\n", rc);
		return (int)rc;
	}
	
	return((int)msg.wParam);
}


void WmCommandProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    // Help mode intercepts the command before it can change calculator state.
    if ( fHelpKeysMode ) 
    {
        UCHAR szStringBuffer[ MAX_STRING_SIZE ];

        if ( HelpKeys( (DWORD)wParam, hInst, hwnd,
                       szStringBuffer, MAX_STRING_SIZE ) )
        {
            MessageBox(hwnd, szStringBuffer, "RPN Help Keys Mode",
	    		MB_OK | MB_ICONINFORMATION);
        }
    }
    else
        CalcIt((DWORD)wParam, hwnd);
}


void WmNotifyProc( HWND hwnd, int idCtrl, LPNMHDR pnmh )
{
#ifndef NO_TOOLTIPS

    static UCHAR szStringBuffer[ MAX_STRING_SIZE ];
    LPNMTTDISPINFO lpnmtdi;

    switch ( pnmh->code )
    {
    case TTN_GETDISPINFO:
        lpnmtdi = (LPNMTTDISPINFO)pnmh;
        
        SendMessage( pnmh->hwndFrom, TTM_SETMAXTIPWIDTH, 0, 300 );

        if ( HelpKeys( GetDlgCtrlID( (HWND)pnmh->idFrom ),
                       lpnmtdi->hinst, pnmh->hwndFrom, 
                       szStringBuffer, MAX_STRING_SIZE ) )
        {
            lpnmtdi->lpszText = szStringBuffer;
        }

        break;
        
    default:
        DebugPrintf( "Unknown WM_NOTIFY(%u)\n", pnmh->code );
        break;
    }

#endif
}

#ifndef NO_TOOLTIPS

struct ENUMCHILDSTRUCT
{
    HWND hwndParent;
    HWND hwndTT;
    BOOL fAdd;
};

BOOL CALLBACK EnumChildWindowsProc( HWND hwnd, LPARAM lParam )
{
    TOOLINFO ti;
    struct ENUMCHILDSTRUCT* p = (struct ENUMCHILDSTRUCT*)lParam;
 
    ZeroMemory( &ti, sizeof(ti) );
    ti.cbSize = sizeof( TOOLINFO );
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = p->hwndParent;
    ti.uId = (WPARAM)hwnd;
    ti.hinst = hInst;
    ti.lpszText = LPSTR_TEXTCALLBACK;

    SendMessage( p->hwndTT, 
                 p->fAdd ? TTM_ADDTOOL : TTM_DELTOOL,
                 0, (LPARAM)&ti);

    return TRUE;
}

#endif

static void TrimUnusedCheckGutter(HMENU popup)
{
    int count = GetMenuItemCount(popup);
    int index;
    MENUINFO menu = {sizeof(menu), MIM_STYLE};

    if (count <= 0)
        return;
    for (index = 0; index < count; ++index) {
        MENUITEMINFO item = {sizeof(item), MIIM_ID | MIIM_STATE | MIIM_FTYPE | MIIM_BITMAP | MIIM_CHECKMARKS};
        if (!GetMenuItemInfo(popup, index, TRUE, &item)) {
            DebugPrintf("Could not inspect popup item: %u\n", GetLastError());
            return;
        }
        // View and Help toggle checkmarks even if currently unchecked.
        if (item.wID == IDM_VIEW_KEYPAD || item.wID == IDM_HELP_HELP_KEYS ||
            (item.fState & MFS_CHECKED) || (item.fType & MFT_RADIOCHECK) ||
            item.hbmpItem || item.hbmpChecked || item.hbmpUnchecked)
            return;
    }

    if (!GetMenuInfo(popup, &menu)) {
        DebugPrintf("Could not inspect popup style: %u\n", GetLastError());
        return;
    }
    if (!(menu.dwStyle & MNS_NOCHECK)) {
        menu.dwStyle |= MNS_NOCHECK;
        if (!SetMenuInfo(popup, &menu))
            DebugPrintf("Could not trim the popup checkmark gutter: %u\n", GetLastError());
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    HWND hButton;
    
    switch(message) 
    {
    case WM_CREATE:
        DebugPrintf("WM_CREATE message. hwnd = %u\n", hwnd);
        break;
        
    case WM_SHOWWINDOW:
        DebugPrintf("WM_SHOWWINDOW message. hwnd = %u\n", hwnd);
        if (wParam) 
        {
            SetWindowPos(hwnd, HWND_TOP, (LONG)dwWindowX, (LONG)dwWindowY, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
            DebugPrintf("SetWindowPos(%u, %u)\n", dwWindowX, dwWindowY);
        }
        break;
        
    case WM_INITMENUPOPUP:
        if (!HIWORD(lParam))
            TrimUnusedCheckGutter((HMENU)wParam);
        break;

    case WM_KEYDOWN:
        // Non-character keys need to be trapped here so that WM_CHAR
        // can operate on them.
        if (wParam == VK_DELETE)
            wParam = DELETE_KEY;	// will fall through to WM_CHAR
        else if (wParam == VK_LEFT)
            wParam = VK_BACK;		// will fall through to WM_CHAR
        else if (wParam == VK_UP || wParam == VK_DOWN)
        {
            if (fHelpKeysMode)
                SendMessage(hwnd, WM_COMMAND, IDM_VIEW_KEYPAD, 0);
            else
                SetKeypadVisible(hwnd, wParam == VK_DOWN);
            return(0);
        }
        else
            break;					// doesn't need translation
        
    case WM_CHAR:
        //			  DebugPrintf("WM_CHAR message: %u\n", wParam);
        wParam = toupper((char)wParam);
        
        if (hButton = GetDlgItem(hwnd, (int)wParam)) {
            SendMessage(hButton, BM_SETSTATE, 1, 0L);
            SendMessage(hButton, BM_SETSTATE, 0, 0L);
        }
        
        //
        // Note that there are some keybindings that don't map onto
        // buttons (ex: unary shift operators). Just let the logic
        // of the command/calc procs handle the error conditions.
        //
        
        // Fall through
        
    case WM_COMMAND:
        // WM_CHAR reaches this branch with a character, not a notification.
        if (message == WM_COMMAND)
            wParam = LOWORD(wParam);
        DebugPrintf("WM_COMMAND message %u, hwnd = %u\n", wParam, hwnd);
        SetFocus(hwnd);

        // Keypad visibility is shared by both calculator modes.
        if (wParam == IDM_VIEW_KEYPAD && ! fHelpKeysMode)
        {
            SetKeypadVisible(hwnd, fKeypadHidden);
            return(0);
        }
        
        switch (Calculator.Mode)
        {
        case CALC_MODE_BINARY:
            WmCommandProc(hwnd, message, wParam, lParam);
            break;
        case CALC_MODE_FLOAT:
            FWmCommandProc(hwnd, message, wParam, lParam);
            break;
        }
        return(0);
        
        
    case WM_MOVE:
        // Ignore the provisional position of a new dialog and minimized bounds.
        if (hwnd == hwndDialog && IsWindowVisible(hwnd) && !IsIconic(hwnd))
        {
            RECT bounds;
            if (GetWindowRect(hwnd, &bounds)) {
                dwWindowX = (DWORD)bounds.left;
                dwWindowY = (DWORD)bounds.top;
            } else {
                DebugPrintf("GetWindowRect failed: %u\n", GetLastError());
            }
        }
        break;
        
    case WM_DESTROY:
        DebugPrintf("WM_DESTROY message, hwnd = %u\n", hwnd);

        if ( hwndTT )
        {
            DestroyWindow( hwndTT );
            hwndTT = NULL;
        }
        
        if (bQuit)
            PostQuitMessage(0);
        
        return(0);
        break;
        
    case WM_SYSCOMMAND:
        DebugPrintf("WM_SYSCOMMAND message %u, hwnd = %u\n", wParam, hwnd);
        
        switch (wParam) {
        case SC_CLOSE:
            bQuit = TRUE;
            break;
            
        case SC_MOVE:
            // the user is moving the window!
            
            break;
        }
        
        break;
        
        //		  default:
        //			  DebugPrintf("Unhandled message %u\n", message);

    case WM_NOTIFY:
        switch (Calculator.Mode)
        {
        case CALC_MODE_BINARY:
            WmNotifyProc(hwnd, (int)wParam, (LPNMHDR)lParam);
            break;
        case CALC_MODE_FLOAT:
            FWmNotifyProc(hwnd, (int)wParam, (LPNMHDR)lParam);
            break;
        }
        return(0);
    }
    
    return(DefWindowProc(hwnd, message, wParam, lParam));
}



LRESULT CALLBACK DisplayControlProc(HWND hwnd, UINT message, WPARAM wParam,
								 LPARAM lParam) {
	
	HDC hdc;
	TEXTMETRIC TextMetric;
	PAINTSTRUCT ps;
	
	switch(message) {
	case WM_CREATE:
		DebugPrintf("WM_CREATE Display hwnd:%u\n", hwnd);
		hdc = GetDC(hwnd);
		
		//			  SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
		GetTextMetrics(hdc, &TextMetric);
		cyChar = TextMetric.tmHeight;
		ReleaseDC(hwnd, hdc);
		return(0);
		break;
		
	case WM_PAINT:
        {
    		hdc = BeginPaint(hwnd, &ps);

		    //			  DebugPrintf("WM_PAINT message hwnd:%x\n", hwnd);

		    PaintStackDisplay(hwnd, hdc, &Calculator);

            EndPaint(hwnd, &ps);

		    return(0);
        }
		break;
		
	case WM_DESTROY:
		DebugPrintf("WM_DESTROY Display hwnd:%u\n", hwnd);
		return(0);
		break;
	}
	
	return(DefWindowProc(hwnd, message, wParam, lParam));
}


void RedrawDisplay(HWND hwnd) {
	
	//	  DebugPrintf("RedrawDisplay: hwnd = %u\n", hwnd);
	
	InvalidateRect(hwnd, NULL, TRUE);	// invalidate display control window
	UpdateWindow(hwnd);
}


void CALLBACK ClearErrorTimer(HWND hwnd, UINT message, UINT_PTR id, DWORD time) {

	Calculator.Error = FALSE;
	RedrawDisplay(hwnd);
}


static void PaintStackDisplay(HWND hwnd, HDC hdc, const CALCULATOR_STATE *state) {
	DISPLAY_LINES lines;
	RECT rect;
	int row;

	if (!ComposeDisplayLines(state, &lines))
		DebugPrintf("Could not format one or more stack display rows\n");

	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	GetClientRect(hwnd, &rect);
	Rectangle(hdc, 0, 0, rect.right, rect.bottom);
	
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));	// fixed font
	SetBkColor(hdc, RGB(0, 0, 0));					 // Black background color
	
	SetTextColor(hdc, RGB(0, 255, 0));				 // Green text color
	TextOut(hdc, 1, 1, lines.Text[0], (int)strlen(lines.Text[0]));

	// Start the error indicator's timer after drawing the status row.
	if (state->Error)
		SetTimer(hwnd, IDD_CLEAR_ERROR, 300, ClearErrorTimer);
	
	SetTextColor(hdc, RGB(255, 0, 0));				 // Red text color
	for (row = 1; row < ARRAYSIZE(lines.Text); ++row)
		TextOut(hdc, 1, 1 + row * (int)cyChar, lines.Text[row], (int)strlen(lines.Text[row]));
}



char * IntToA(NUMBER Value, char * psString, ULONG Length, USHORT Radix, USHORT IntegerType, USHORT WordSize, BOOLEAN Adorn) {
	char digits[sizeof(NUMBER) * CHAR_BIT + 1];
	char *number = Adorn ? digits : psString;
	ULONG capacity = Adorn ? sizeof(digits) : Length;
	
	switch (IntegerType) {
	case INTEGER_TYPE_UNSIGNED:
		if (_ui64toa_s(Value, number, capacity, Radix) != 0)
			return(NULL);
		break;
		
	case INTEGER_TYPE_1S_COMP:
		//BUGBUG: Not yet implemented...
		return(0);
		break;
		
		//
		//	Smash the value according to the current wordsize restrictions.
		//	Signed numbers are fully sign extended on the stack, but we only
		//	permit ourselves to dump the wordsize bits on non-decimal radices.
		//
		
	case INTEGER_TYPE_2S_COMP:
		
		if (Radix != 10) {
			switch (WordSize) {
			case 8:
				Value = (UCHAR)Value;
				break;
			case 16:
				Value = (USHORT)Value;
				break;
			case 32:
				Value = (ULONG)Value;
				break;
			}
		}
		if (_i64toa_s((SIGNED_NUMBER)Value, number, capacity, Radix) != 0)
			return(NULL);
		break;
		
	default:
		return(0);
		break;
	}
	
	if (Adorn) {

		ULONG SplitUnit;
		size_t Remain, Slop;
		PUCHAR SplitPoint = psString;
		UCHAR SplitChar = ' ';

		switch (Radix) {
		case 2:
			//  byte split for binary
			SplitUnit = 8;
			break;
		case 8:
			//  12bit split for octal
			SplitUnit = 4;
			break;
		case 10:
			//  thousands split for decimal
			SplitUnit = 3;
			SplitChar = ',';
			break;
		case 16:
			//  32bit split for hexidecimal
			SplitUnit = 8;
			break;
		}

		// Check the grouped length before copying to the fixed-width row.
		Remain = strlen(digits + (digits[0] == '-'));
		if (strlen(digits) + (Remain - 1) / SplitUnit >= Length)
			return NULL;
		strcpy_s(psString, Length, digits);

		//
		//  Shove past the sign if existant.
		//
		
		if (*SplitPoint == '-') {
			SplitPoint++;
		}

		//
		// Advance beyond the head MSD's of the number.
		//

		Remain = strlen(SplitPoint);
		Slop = Remain % SplitUnit;

		if (!Slop) {
			Slop = SplitUnit;
		}

		//
		//  Now we chunk forward in the unit we are splitting,
		//  shoving the remaining string up by one byte, dropping
		//  the seperator in the hole.
		//
		
		for (Remain -= Slop, SplitPoint += Slop;
			 Remain;
			 Remain -= SplitUnit, SplitPoint += SplitUnit) {

			memmove(SplitPoint + 1,
					SplitPoint,
					Remain + 1);
			*SplitPoint++ = SplitChar;
		}

		//
		//  And right justfiy, space pad.
		//

	    Remain = strlen(psString);
	    memmove(psString + (Length - 1 - Remain),
			    psString,
				Remain + 1);
		memset(psString, ' ', Length - 1 - Remain);
	}
	
	return psString;
}

/*** IsBaseDigit -- Is the specified character a digit in the specified base?
*
* ENTRY    - ch = character to test (id button)
*			 Radix = number base
* EXIT	   - returns TRUE if the character is a valid digit in the base.
* SYNOPSIS - Checks if the character is a member of the valid digit set for
*			 the specified base.
*/

BOOL IsBaseDigit(UCHAR ch, USHORT Radix)
{
	if (Radix <= 10)
		return (ch >= '0' && ((USHORT)(ch - '0') < Radix));
	else
		return ((ch >= '0' && ch <= '9') ||
		(ch >= 'A' && ((USHORT)(ch - ('A' - 10)) < Radix)));
}


/*
* DebugPrintf -- printf to the debugger console
* Takes printf style arguments.
* Expects newline characters at the end of the string.
*/
void DebugPrintf(const char * format, ...) {
	
#if DEBUG_MODE
	va_list marker;
	char String[512];
	
	va_start(marker, format);
	vsprintf_s(String, sizeof(String), format, marker);
	OutputDebugString(String);
	va_end(marker);
#else
	UNREFERENCED_PARAMETER(format);
#endif
}


/*
* ErrorBox -- printf to a pop-up box.
* Takes printf style arguments.
* Also prints to the debugger so that there is a record of the sequence of
* error boxes.
* Expects NO newline characters at the end of the string.  They would display
* as blocks in the popup.
*/
void ErrorBox(const char * format, ...) {
	va_list marker;
	char String[512];
	
	va_start(marker, format);
	vsprintf_s(String, sizeof(String), format, marker);
	OutputDebugString(String);
	OutputDebugString("\n");					// ErrorBox strings don't have
	// newline.
	//	  MessageBox(GetFocus(), String, "", MB_OK);
	
	MessageBox(hwndDialog, String, "Error!", MB_OK);
	
}


// Last Base tracks the previous distinct radix. The display is too narrow
// for 64-bit binary, so reject that combination without changing history.
static void SetIntegerBase(INTEGER_STATE *state, USHORT Base, BOOL *error)
{
	if (state->WordSize == 64 && Base == 2) {
		*error = TRUE;
		return;
	}
	if (state->Radix != Base) {
		state->LastRadix = state->Radix;
		state->Radix = Base;
	}
}

// Narrow to the selected word size; signed values stay sign-extended in
// the 64-bit registers, including when a narrower setting is selected.
static void CastNumber(const INTEGER_STATE *state, PNUMBER Castee)
{
	if (state->IntegerType != INTEGER_TYPE_UNSIGNED) {
		switch (state->WordSize) {
		case 32:
			*Castee = (LONG)((SIGNED_NUMBER)*Castee);
			break;
		case 16:
			*Castee = (SHORT)((SIGNED_NUMBER)*Castee);
			break;
		case 8:
			*Castee = (CHAR)((SIGNED_NUMBER)*Castee);
			break;
		}
	} else {
		switch (state->WordSize) {
		case 32:
			*Castee = (ULONG)*Castee;
			break;
		case 16:
			*Castee = (USHORT)*Castee;
			break;
		case 8:
			*Castee = (UCHAR)*Castee;
			break;
		}
	}
}


/*** CastStack -- Casts all numbers on the stack.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Recasts the entire stack.
*
*/

static void CastStack(INTEGER_STATE *state)
{
	CastNumber(state, &state->Stack.X);
	CastNumber(state, &state->Stack.Y);
	CastNumber(state, &state->Stack.Z);
	CastNumber(state, &state->Stack.T);
}


/*** ClearStack -- Clears the stack to all Zero's.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Puts a 0 in the entire stack.
*
*/

void ClearStack(PSTACK pStack)
{
	pStack->X = 0;
	pStack->Y = 0;
	pStack->Z = 0;
	pStack->T = 0;
}


/*** ChangeWordSIze -- Changes the input/display word size
*
* ENTRY    - Word size = new word size
* EXIT	   - none
* SYNOPSIS - Changes the current word size.
* EFFECTS  - The new size applies to all four registers immediately.
*
*/

static void ChangeWordSize(INTEGER_STATE *state, USHORT Size, BOOL *error)
{
	//
	//  Until the display can take 64bit binary, disable that mode.
	//

	if (Size == 64 && state->Radix == 2) {
		*error = TRUE;
		return;
	}

	state->WordSize = Size;
	
	//
	//	Recast the stack to match the new wordsize.
	//
	
	CastStack(state);
}

/*** ChangeIntegerType -- Changes the input/display integer representation
*
* ENTRY    - dwOperation = Identifier of menu item
* EXIT	   - none
* SYNOPSIS - Changes the current integer representation.
* EFFECTS  - The new representation applies to all four registers immediately.
*
*/

static void ChangeIntegerType(INTEGER_STATE *state, DWORD dwOperation)
{
	switch(dwOperation) {
	case IDM_INTEGER_TYPE_UNSIGNED:
		state->IntegerType = INTEGER_TYPE_UNSIGNED;
		break;
		
	case IDM_INTEGER_TYPE_1S:
		state->IntegerType = INTEGER_TYPE_1S_COMP;
		break;
		
	case IDM_INTEGER_TYPE_2S:
		state->IntegerType = INTEGER_TYPE_2S_COMP;
		break;
	default:
		break;
	}
	
	//
	//	Recast the stack to match the new integer type.
	//
	
	CastStack(state);
}


static void RollDownOp(PSTACK pStack, BOOL *entering)
{
	RollIntegerRegisters(pStack);
	*entering = FALSE;
}



/*** SwapOp -- Swap X and Y
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Swaps the contents of X and Y.
*
*/

static void SwapOp(PSTACK pStack, BOOL *entering)
{
	SwapIntegerRegisters(pStack);
	*entering = FALSE;
}


/*** KiloOp -- Multiplies current X entry by 1K
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Multiplies content of X by 1K.
*
*/

static void KiloOp(PSTACK pStack, BOOL *entering)
{
	pStack->X *= KILO;
	*entering = FALSE;
}


/*** NegOp -- X --> -X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract X from 0.  Doesn't drop the stack.
*
*/

static void NegOp(PSTACK pStack, BOOL *entering)
{
	(SIGNED_NUMBER)pStack->X = -(SIGNED_NUMBER)(pStack->X);
	*entering = FALSE;
}


/*** NotOp -- X --> ~(X)
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - X = ~X.
*
*/

static void NotOp(PSTACK pStack, BOOL *entering)
{
	pStack->X = ~(pStack->X);
	*entering = FALSE;
}


// Present divide-by-zero before ending entry or recasting X. A modal dialog
// can repaint the calculator while these values still describe the input.
static void DivideOp(PSTACK pStack, BOOL *entering,
                     CALCULATOR_REPORT_ERROR reportError, void *context)
{
	if (pStack->X != 0) {
		pStack->X = pStack->Y / pStack->X;
		DropIntegerY(pStack);
	}
	else
		reportError(context, "Division by zero");
	
	*entering = FALSE;
}


/*** ModOp -- (Y % X) --> X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Mod Y by X into X.   Move stack down, clear fEnter.
*
*/

static void ModOp(PSTACK pStack, BOOL *entering,
                  CALCULATOR_REPORT_ERROR reportError, void *context)
{
	if (pStack->X != 0) {
		pStack->X = pStack->Y % pStack->X;
		DropIntegerY(pStack);
	}
	else
		reportError(context, "Division by zero");
	*entering = FALSE;
}


/*** AddOp -- (X + Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Add Y to X, move stack down, set fEnter to FALSE
*
*/

static void AddOp(PSTACK pStack, BOOL *entering)
{
	pStack->X += pStack->Y;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** SubtractOp -- (Y - X) --> X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract X from Y, move stack down, set fEnter to FALSE
*
*/

static void SubtractOp(PSTACK pStack, BOOL *entering)
{
	pStack->X = pStack->Y - pStack->X;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** UnaryAddOp -- (X + 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Add 1 to X, set fEnter to FALSE
*
*/

static void UnaryAddOp(PSTACK pStack, BOOL *entering)
{
	pStack->X += 1;
	*entering = FALSE;
}


/*** UnarySubtractOp -- (X - 1) --> X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract 1 from X, set fEnter to FALSE
*
*/

static void UnarySubtractOp(PSTACK pStack, BOOL *entering)
{
	pStack->X -= 1;
	*entering = FALSE;
}


/*** MultiplyOp -- (X * Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X * Y to X, move stack down, set fEnter to FALSE
*
*/

static void MultiplyOp(PSTACK pStack, BOOL *entering)
{
	pStack->X *= pStack->Y;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** AndOp -- (X AND Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X AND Y to X, move stack down, set fEnter to FALSE
*
*/

static void AndOp(PSTACK pStack, BOOL *entering)
{
	pStack->X &= pStack->Y;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** OrOp -- (X OR Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X OR Y to X, move stack down, set fEnter to FALSE
*
*/

static void OrOp(PSTACK pStack, BOOL *entering)
{
	pStack->X |= pStack->Y;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** XorOp -- (Y ^ X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y XOR X --> X, move stack down, set fEnter to FALSE
*
*/

static void XorOp(PSTACK pStack, BOOL *entering)
{
	pStack->X ^= pStack->Y;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** LeftShiftOp -- (Y << X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y shifted left by X bits to X, move stack down, set fEnter
*			 to FALSE
*
*/

static void LeftShiftOp(PSTACK pStack, BOOL *entering)
{

	pStack->X = pStack->Y << pStack->X;
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** RightShiftOp -- (Y >> X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y Shifted right by X bits to X, move stack down, set fEnter
*			 to FALSE
*
*/

static void RightShiftOp(PSTACK pStack, BOOL *entering, USHORT integerType)
{

	//
	//  Right shift is arithmetic if signed.
	//
	
	if (integerType == INTEGER_TYPE_UNSIGNED) {
		pStack->X = pStack->Y >> pStack->X;
	} else {
		(SIGNED_NUMBER)pStack->X = (SIGNED_NUMBER)pStack->Y >> pStack->X;
	}
	DropIntegerY(pStack);
	*entering = FALSE;
}


/*** UnaryLeftShiftOp -- (X << 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X shifted left by 1 bit to X, set fEnter to FALSE
*
*/

static void UnaryLeftShiftOp(PSTACK pStack, BOOL *entering)
{
	pStack->X = pStack->X << 1;
	*entering = FALSE;
}


/*** UnaryRightShiftOp -- (X >> 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X Shifted right by 1 bits to X, set fEnter to FALSE
*
*/

static void UnaryRightShiftOp(PSTACK pStack, BOOL *entering, USHORT integerType)
{
	//
	//  Right shift is arithmetic if signed.
	//
	
	if (integerType == INTEGER_TYPE_UNSIGNED) {
		pStack->X = pStack->X >> 1;
	} else {
		(SIGNED_NUMBER)pStack->X = (SIGNED_NUMBER)pStack->X >> 1;
	}
	*entering = FALSE;
}


/*** EnterOp -- Enter the number or Dup.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - If fEnter, we should turn off fEnter else we should move the
*			 stack up.
*
*/

static void EnterOp(INTEGER_STATE *state, BOOL *entering)
{
	PSTACK pStack = &state->Stack;
	if (*entering) {
		*entering = FALSE;
		
		//
		//	Cast the number: for signed operations, this
		//	will sign extend the full number.
		//
		
		CastNumber(state, &pStack->X);
		
	} else {
		DuplicateIntegerX(pStack);
	}
}


/*** DropOp -- Drop the value in X.
*
* ENTRY    - pStack
*
*/

static void DropOp(PSTACK pStack, BOOL *entering)
{
	DropIntegerX(pStack);
	*entering = FALSE;
}


/*** BackSpaceOp -- removes the right-most digit from X.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Removes the right-most digit from X, depending on the current
*			 base and integer type.
*
*/

static void BackSpaceOp(INTEGER_STATE *state)
{
	PSTACK pStack = &state->Stack;
	if ((state->IntegerType != INTEGER_TYPE_UNSIGNED) && (state->Radix == 10))
		(SIGNED_NUMBER)(pStack->X) =
		(SIGNED_NUMBER)(pStack->X) / (signed short)state->Radix;
	else	// unsigned digit entry mode
		pStack->X = pStack->X / state->Radix;
}


// Digit entry keeps the raw value in X until commit. Operations instead
// recast X before repainting, even when they leave the stack unchanged.
CALCULATOR_RESULT IntegerCommand(INTEGER_STATE *state, DWORD dwOperation, BOOL *entering,
                                BOOL *error, CALCULATOR_REPORT_ERROR reportError, void *context)
{
	PSTACK pStack = &state->Stack;
	UCHAR ch = (UCHAR)dwOperation;
	USHORT radix = state->Radix;
	CALCULATOR_RESULT result = CALCULATOR_REDRAW;

	if (IsBaseDigit(ch, radix)) {
		USHORT digit = (USHORT)(ch - (isdigit(ch) ? '0' : 'A' - 10));
		if (!*entering) {
			EnterOp(state, entering);
			pStack->X = 0;
			*entering = TRUE;
		}
		if (!NumTest(pStack->X, radix, digit, state->IntegerType, state->WordSize))
			return CALCULATOR_ENTRY_REJECTED;
		// In signed decimal, appending to a negative X extends its magnitude.
		if (radix == 10 && state->IntegerType != INTEGER_TYPE_UNSIGNED &&
			(SIGNED_NUMBER)pStack->X < 0)
			pStack->X = radix * pStack->X - digit;
		else
			pStack->X = radix * pStack->X + digit;
		return CALCULATOR_REDRAW;
	}
	
	switch(dwOperation) {
	case ENTER: 					EnterOp(state, entering);
		break;
	case SWAP_KEY:					SwapOp(pStack, entering);
		break;
	case PLUS_KEY:					AddOp(pStack, entering);
		break;
	case MINUS_KEY: 				SubtractOp(pStack, entering);
		break;
	case UNARY_PLUS_KEY:			UnaryAddOp(pStack, entering);
		break;
	case UNARY_MINUS_KEY:			UnarySubtractOp(pStack, entering);
		break;
	case MULT_KEY:					MultiplyOp(pStack, entering);
		break;
	case DIV_KEY:					DivideOp(pStack, entering, reportError, context);
		break;
	case MOD_KEY:					ModOp(pStack, entering, reportError, context);
		break;
	case AND_KEY:					AndOp(pStack, entering);
		break;
	case OR_KEY:					OrOp(pStack, entering);
		break;
	case XOR_KEY:					XorOp(pStack, entering);
		break;
	case LEFT_SHIFT_KEY:			LeftShiftOp(pStack, entering);
		break;
	case RIGHT_SHIFT_KEY:			RightShiftOp(pStack, entering, state->IntegerType);
		break;
	case UNARY_LEFT_SHIFT_KEY:		UnaryLeftShiftOp(pStack, entering);
		break;
	case UNARY_RIGHT_SHIFT_KEY:		UnaryRightShiftOp(pStack, entering, state->IntegerType);
		break;
	case NEGATE_KEY:				NegOp(pStack, entering);
		break;
	case NOT_KEY:					NotOp(pStack, entering);
		break;
	case BACKSPACE_KEY: 			BackSpaceOp(state);
		break;
	case DELETE_KEY:				DropOp(pStack, entering);
		break;
	case ROLL_DOWN_KEY: 			RollDownOp(pStack, entering);
		break;
	case KILO_KEY:					KiloOp(pStack, entering);
		break;
	case TOGGLE_BASE_KEY:			SetIntegerBase(state, state->LastRadix, error);
		break;
		
	case IDM_BINARY:		SetIntegerBase(state, 2, error);
		break;
	case IDM_OCTAL: 		SetIntegerBase(state, 8, error);
		break;
	case IDM_DECIMAL:		SetIntegerBase(state, 10, error);
		break;
	case IDM_HEXADECIMAL:	SetIntegerBase(state, 16, error);
		break;
	case IDM_WORD_SIZE_8:	ChangeWordSize(state, 8, error);
		break;
	case IDM_WORD_SIZE_16:	ChangeWordSize(state, 16, error);
		break;
	case IDM_WORD_SIZE_32:	ChangeWordSize(state, 32, error);
		break;
	case IDM_WORD_SIZE_64:	ChangeWordSize(state, 64, error);
		break;
		
	case IDM_INTEGER_TYPE_UNSIGNED:
	case IDM_INTEGER_TYPE_1S:
	case IDM_INTEGER_TYPE_2S:
		ChangeIntegerType(state, dwOperation);
		break;

	default:
		*error = TRUE;
		result = CALCULATOR_UNKNOWN_COMMAND;
		break;
	}

	CastNumber(state, &pStack->X);
	return result;
}

CALCULATOR_RESULT CalculatorCommand(CALCULATOR_STATE *state, DWORD command,
                                   CALCULATOR_REPORT_ERROR reportError, void *context)
{
	switch (state->Mode) {
	case CALC_MODE_BINARY:
		if (command == IDM_FLOAT) {
			state->Mode = CALC_MODE_FLOAT;
			return CALCULATOR_MODE_CHANGED;
		}
		return IntegerCommand(&state->Integer, command, &state->Entering, &state->Error,
		                      reportError, context);
	case CALC_MODE_FLOAT: {
		USHORT radix;
		switch (command) {
		case IDM_BINARY:      radix = 2;  break;
		case IDM_OCTAL:       radix = 8;  break;
		case IDM_DECIMAL:     radix = 10; break;
		case IDM_HEXADECIMAL: radix = 16; break;
		default:
			return FloatCommand(&state->Float, command, &state->Entering, &state->Error,
			                    reportError, context);
		}
		// Returning to integer mode does not commit entry, recast X, or
		// depend on whether the requested radix is allowed at this width.
		SetIntegerBase(&state->Integer, radix, &state->Error);
		state->Mode = CALC_MODE_BINARY;
		return CALCULATOR_MODE_CHANGED;
	}
	default:
		state->Error = TRUE;
		return CALCULATOR_UNKNOWN_COMMAND;
	}
}

static void ShowCalculatorError(void *context, const char *message)
{
	(void)context;
	ErrorBox("%s", message);
}

void RunCalculatorCommand(HWND hwnd, DWORD command)
{
	CALCULATOR_RESULT result = CalculatorCommand(&Calculator, command, ShowCalculatorError, NULL);
	if (result == CALCULATOR_MODE_CHANGED) {
		if (Calculator.Mode == CALC_MODE_BINARY)
			bQuit = FALSE;
		PostMessage(hwnd, WM_COMMAND, IDM_CHANGE_MODE, 0);
		return;
	}
	if (result == CALCULATOR_ENTRY_REJECTED) {
		DebugPrintf("%s\n", Calculator.Mode == CALC_MODE_BINARY
		            ? "Overflow, don't add digit"
		            : "Couldn't add character to entry string.");
		return;
	}
	if (result == CALCULATOR_UNKNOWN_COMMAND)
		DebugPrintf("Don't recognize %u operation\n", command);
	RedrawDisplay(hDisplay);
}

// Keep commands that affect the window or clipboard here. Dispatch numeric
// commands using the same state and flags that painting and persistence see.
static void CalcIt(DWORD dwOperation, HWND hwnd)
{
	PSTACK pStack = &Calculator.Integer.Stack;
	USHORT Radix = Calculator.Integer.Radix;
	USHORT usIntegerType = Calculator.Integer.IntegerType;

	switch(dwOperation) {
	case IDM_HELP_ABOUT:
		DialogBox(GetModuleHandle(NULL), "AboutBox", hwnd,
			About);
		break;
		
	case IDM_HELP_HELP_KEYS:
		fHelpKeysMode = TRUE;	// Get us into HelpKeysMode
		CheckMenuItem(GetMenu(hwnd), IDM_HELP_HELP_KEYS, MF_CHECKED);
		break;
		
	case IDM_COPY_X:
		ClipboardCopy(pStack->X, Radix, usIntegerType);
		break;
		
	case IDM_COPY_Y:
		ClipboardCopy(pStack->Y, Radix, usIntegerType);
		break;
		
	case IDM_COPY_Z:
		ClipboardCopy(pStack->Z, Radix, usIntegerType);
		break;
		
	case IDM_COPY_T:
		ClipboardCopy(pStack->T, Radix, usIntegerType);
		break;
		
	case IDM_PASTE_X:
		if (!ClipboardPaste(&pStack->X, Radix, usIntegerType))
			return;
		break;
		
	case IDM_PASTE_Y:
		if (!ClipboardPaste(&pStack->Y, Radix, usIntegerType))
			return;
		break;
		
	case IDM_PASTE_Z:
		if (!ClipboardPaste(&pStack->Z, Radix, usIntegerType))
			return;
		break;
		
	case IDM_PASTE_T:
		if (!ClipboardPaste(&pStack->T, Radix, usIntegerType))
			return;
		break;
		
	default:
		RunCalculatorCommand(hwnd, dwOperation);
		return;
	}
		
	//
	// Clipboard and help actions also recast X on return. A mode switch
	// instead leaves the stack alone and redraws only the recreated window.
	//
		
	CastNumber(&Calculator.Integer, &pStack->X);
		
	RedrawDisplay(hDisplay);
}


/*** HelpKeys -- Give a help popup on whatever key was pressed
*
* ENTRY    - DWORD dwOperation = key pressed code
*			 HANDLE hInst = instance handle
*			 HWND hwnd = Window handle
* EXIT	   - none
* SYNOPSIS - Display a message box with the string that corresponds to
*			 the dwOperation.  The String Table is set up with a
*			 correspondence between the key codes and the string
*			 identifiers: StringID = KeyCode | 0x200.
*/

BOOL HelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
               UCHAR* szStringBuffer,
               DWORD cbStringBuffer )
{
    BOOL fRet = FALSE;

	DWORD StringSize;
	
	DebugPrintf("HelpKeys(%u)\n", dwOperation);
	
	if (IsBaseDigit((UCHAR)dwOperation, 16)) {
		dwOperation = ID_DIGIT;
	}

    if ( dwOperation == IDD_DISPLAY )
    {
        switch ( Calculator.Integer.Radix )
        {
        case 2:
            dwOperation = IDS_BINARY;
            break;

        case 8:
            dwOperation = IDS_OCTAL;
            break;

        case 10:
            dwOperation = IDS_DECIMAL;
            break;

        case 16:
            dwOperation = IDS_HEXADECIMAL;
        }
    }

	switch(dwOperation) {
		
	case IDM_HELP_HELP_KEYS:
		fHelpKeysMode = FALSE;	// Get us out of HelpKeysMode
		CheckMenuItem(GetMenu(hwnd), IDM_HELP_HELP_KEYS, MF_UNCHECKED);
		break;
		
	case IDM_HELP_ABOUT:
		DialogBox(GetModuleHandle(NULL), "AboutBox", hwnd,
			About);
		break;

	default:
		if ((StringSize = LoadString(hInst, dwOperation | IDS_HELP_PREFIX,
			szStringBuffer, MAX_STRING_SIZE)) == 0) 
        {
			strcpy_s(szStringBuffer, cbStringBuffer,
				"No help available for this key");
			StringSize = (DWORD)strlen(szStringBuffer);
		}
        else
        {
            fRet = TRUE;
        }
		
//		MessageBox(hwnd, szStringBuffer, "RPN Help Keys Mode",
//			MB_OK | MB_ICONINFORMATION);
		break;
		
	}

    return fRet;
}


/*
* About(HWND, unsigned, WORD, LONG)
*
* PURPOSE:	Processes messages for "About" dialog box
*
* MESSAGES:
*
*	  WM_INITDIALOG - initialize dialog box
*	  WM_COMMAND	- Input received
*/
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG:
		return (TRUE);
		break;
		
	case WM_COMMAND:
		switch (wParam) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, TRUE);
			return(TRUE);
		}
	}
	return (FALSE);
	UNREFERENCED_PARAMETER(lParam);
}


/*
* NumTest
*
* INPUTS:  X = current number value
*		   Radix = current entry radix
*		   Digit = digit to add
*		   usIntegerType = flag specifying type of integer we are entering
*		   WordSize = maximum width of the value in bits
*
* OUTPUTS: Returns TRUE if we have room to put the digit in the number
*		   without overflow.
*
* Uses variations on this algorithm:
*	   if ((MAXULONG - Digit) / Radix) >= X: return TRUE
*/

static BOOL NumTest(NUMBER X, USHORT Radix, USHORT Digit, USHORT usIntegerType, USHORT WordSize) {
	if ((usIntegerType != INTEGER_TYPE_UNSIGNED) && (Radix == 10)) {
		// signed decimal entry is broken into positive/negative cases
		if (((SIGNED_NUMBER)X) < 0) { // signed number is negative
			return (((SIGNED_NUMBER)(MinSignedWordSize(WordSize) + Digit) / Radix) <=
				((SIGNED_NUMBER)X));
		}
		else {	// signed number is positive.
			return (((MaxSignedWordSize(WordSize) - Digit) / Radix) >= ((SIGNED_NUMBER)X));
		}
	}
	else {		// unsigned digit entry, don't worry about +/-
		return (((MaxWordSize(WordSize) - Digit) / Radix) >= X);
	}
}


/*
* StrNumTest
*
* INPUTS:  lpNumStr -> string containing potential number
*		   Radix = current radix
*		   usIntegerType = current integer type
*		   pNumber -> number returned
*
* OUTPUTS: reutrns TRUE if the string represents a valid number in the current
*		   radix and usIntegerType.
*
* SUMMARY: Builds the number, a character at a time, repeatedly calling
*		   NumTest to validate each character.
*/
BOOL StrNumTest(PUCHAR lpNumStr, USHORT Radix, USHORT usIntegerType, USHORT WordSize,
				PNUMBER pNumber) {
	NUMBER Number = 0;
	size_t dwStrLength = strlen(lpNumStr);
	USHORT Digit;
	BOOL fReturn = TRUE;
	size_t i;
	
	
	DebugPrintf("String: [%s]\n", lpNumStr);
	DebugPrintf("String length = %u\n", dwStrLength);
	for (i = 0; i < dwStrLength; i++) {
		
		//
		//	Skip backticks, they seperate chunky printouts (64bit numbers)
		//
		
		if (lpNumStr[i] == '`') {
			continue;
		}
		
		Digit = (USHORT)(toupper((UCHAR)lpNumStr[i]) -
			(isdigit(lpNumStr[i]) ? '0' : 'A' - 10));

		DebugPrintf("Char: %c, Digit: 0x%x\n", lpNumStr[i], Digit);
		if ((Digit < Radix) &&
			NumTest(Number, Radix, Digit, usIntegerType, WordSize)) {
		/*
		* Signed decimal numbers need special treatment if
		* they are negative.
			*/
			if ((Radix == 10) && (usIntegerType !=
				INTEGER_TYPE_UNSIGNED) &&
				((SIGNED_NUMBER)Number < 0))
				Number = Radix * Number - Digit;
			else
				Number = Radix * Number + Digit;
		}
		else {
			fReturn = FALSE;
			break;
		}
	}
	
	if (fReturn)
		*pNumber = Number;
	
	return(fReturn);
}


/*
* SetKeypadVisible
*
* INPUTS:  hwnd = calculator dialog window
*		   fShow = TRUE to show (unroll) the keypad, FALSE to hide (roll up)
* SUMMARY: Resize the dialog so that only the stack/status display is visible
*		   when the keypad is hidden.  The top-left corner stays in place.
*/
void SetKeypadVisible(HWND hwnd, BOOL fShow)
{
	RECT rcWindow;
	HMENU hMenu = GetMenu(hwnd);

	fKeypadHidden = ! fShow;

	if (hMenu)
		CheckMenuItem(hMenu, IDM_VIEW_KEYPAD, fShow ? MF_CHECKED : MF_UNCHECKED);

	if (cyFullWindow == 0 || ! GetWindowRect(hwnd, &rcWindow))
		return;

	SetWindowPos(hwnd, NULL, 0, 0, rcWindow.right - rcWindow.left,
		fShow ? cyFullWindow : cyCollapsedWindow,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}


/*
* CreateCalc
*
* INPUTS:  CalcMode = CALC_MODE_BINARY || CALC_MODE_FLOAT
* OUTPUTS: Returns an error code
* SUMMARY: Destroy the current calculator dialog and create a new dialog
*		   calculator in the CALC_MODE specified.
* WARNING: This function should only be called from the MAIN function.	It
*		   shouldn't be called within the context of a window message to
*		   the dialog.
*/
DWORD CreateCalc(CALC_MODE CalcMode) {
	HWND hwnd;
	DWORD rc;
	LPTSTR lpszDlgTemp;
	DWORD DisplayItem;
#ifndef NO_TOOLTIPS
    struct ENUMCHILDSTRUCT ecs;
#endif
	
	Calculator.Mode = CalcMode;
	
	switch (Calculator.Mode) {
	case CALC_MODE_BINARY:
		lpszDlgTemp = MAKEINTRESOURCE(RPN_DIALOG);
		break;
		
	case CALC_MODE_FLOAT:
		lpszDlgTemp = MAKEINTRESOURCE(RPNF_DIALOG);
		break;
		
	default:
		lpszDlgTemp = NULL;
		break;
	}
	
	if ((hwnd = CreateDialog(hInst, lpszDlgTemp, 0, NULL)) == NULL) {
		DebugPrintf("ERROR: CreateDialog --> %u\n", rc = GetLastError());
		return(rc);
	}
	DebugPrintf("CreateDialog --> hwnd: %u\n", hwnd);
	
	
	if (! fFirstTime) {
		DebugPrintf("Destroy old display window handle %u\n", hDisplay);
		// must destroy the current DisplayWindow
		DestroyWindow(hDisplay);
		//		  CloseHandle(hDisplay);				  // is this sufficient?
		
		DebugPrintf("Destroy the dialog window hwnd = %u\n", hwndDialog);
		// must destroy the current dialog window
		//

#ifndef NO_TOOLTIPS
        if ( hwndTT )
        {
            DebugPrintf( "Destroy the tooltip window hwnd = %u\n", hwndTT );

            ecs.hwndParent = hwnd;
            ecs.hwndTT = hwndTT;
            ecs.fAdd = FALSE;

            EnumChildWindows( hwnd, EnumChildWindowsProc, (LPARAM)&ecs );

            DestroyWindow( hwndTT );
            hwndTT = NULL;
        }
#endif
		
		DestroyWindow(hwndDialog);

	}
	
	hwndDialog = hwnd;	  // set global dialog handle for ErrorBox use
	
	switch (Calculator.Mode) {
	case CALC_MODE_BINARY:
		DisplayItem = IDD_DISPLAY;
		break;
	case CALC_MODE_FLOAT:
		DisplayItem =  IDD_DISPLAY_FLOAT;
		break;
	}
	
	if ((hDisplay = GetDlgItem(hwnd, DisplayItem)) == 0) {
		DebugPrintf("Bad hDisplay [%u], error = %u\n",
			hDisplay, rc = GetLastError());
		return(rc);
	}
	else
		DebugPrintf("New Display handle: hDisplay = %u\n", hDisplay);

	//
	// Record the expanded and collapsed window heights, then apply the
	// keypad state before the window is shown.  The collapsed height
	// drops everything in the client area below the display control.
	//
	{
		RECT rcWindow, rcClient, rcDisplay;

		GetWindowRect(hwnd, &rcWindow);
		GetClientRect(hwnd, &rcClient);
		GetWindowRect(hDisplay, &rcDisplay);
		MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rcDisplay, 2);

		cyFullWindow = rcWindow.bottom - rcWindow.top;
		cyCollapsedWindow = cyFullWindow - (rcClient.bottom - rcDisplay.bottom);
		SetKeypadVisible(hwnd, ! fKeypadHidden);
	}

	DebugPrintf("ShowWindow, hwnd = %u\n", hwnd);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	
	InvalidateRect(hDisplay, NULL, TRUE);	  // Paint the display now

#ifndef NO_TOOLTIPS

    hwndTT = CreateWindowEx( WS_EX_TOPMOST, TOOLTIPS_CLASS,
        NULL,
        WS_POPUP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd,
        NULL,
        hInst,
        NULL );
    
    SetWindowPos( hwndTT, HWND_TOPMOST, 
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    
    ecs.hwndParent = hwnd;
    ecs.hwndTT = hwndTT;
    ecs.fAdd = TRUE;

    EnumChildWindows( hwnd, EnumChildWindowsProc, (LPARAM)&ecs );

#endif

	fFirstTime = FALSE;
	
	return(NO_ERROR);
}


/*
* ClipboardCopy
*
* INPUTS:  Number = number to copy
*		   Radix = current number base
*		   usIntegerType = integer representation
* OUTPUTS: TRUE on success
*
* SUMMARY:
*	   Copy the number to the clipboard in the current radix and number type.
*	   Copies as CF_TEXT format.
*
*/
BOOL ClipboardCopy(NUMBER Number, USHORT Radix, USHORT usIntegerType) {
	char text[sizeof(NUMBER) * CHAR_BIT + 1];
	if (!IntToA(Number, text, sizeof(text), Radix, usIntegerType, Calculator.Integer.WordSize, FALSE) ||
		!WriteClipboardText(hwndDialog, text)) {
		ErrorBox("Could not copy the number to the clipboard.");
		return FALSE;
	}
	return TRUE;
}


/*
* ClipboardPaste
*
* INPUTS:  pNumber -> number to paste to
*		   Radix = current number base
*		   usIntegerType = integer representation
* OUTPUTS: TRUE if the paste was successful
*
* SUMMARY:
*	   Copy the number from the clipboard to the number pointer.  Use the
*	   current radix and number type.  Only pastes the CF_TEXT clipboard
*	   format.
*
*/
BOOL ClipboardPaste(PNUMBER pNumber, USHORT Radix, USHORT usIntegerType) {
	char *text = ReadClipboardToken(hwndDialog);
	NUMBER number;
	BOOL valid = text && StrNumTest((PUCHAR)text, Radix, usIntegerType, Calculator.Integer.WordSize, &number);
	free(text);
	if (!valid) {
		ErrorBox("The clipboard does not contain a valid number in the current base.");
		return FALSE;
	}
	CastNumber(&Calculator.Integer, &number);
	*pNumber = number;
	return TRUE;
}
