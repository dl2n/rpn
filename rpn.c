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
void CalcIt(PSTACK pStack, DWORD dwOperation, HWND hwnd);
BOOL HelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
               UCHAR* szStringBuffer, DWORD cbStringBuffer );
void PaintStackDisplay(HWND hwnd, HDC hdc, PSTACK pStack, USHORT Radix);
void DebugPrintf(const char * format, ...);
char * IntToA(NUMBER Value, char * psString, ULONG Length, USHORT Radix, USHORT IntegerType, BOOLEAN Adorn);
BOOL IsBaseDigit(UCHAR ch, USHORT Radix);
void EnterOp(PSTACK pStack);
BOOL NumTest(NUMBER X, USHORT Radix, USHORT Digit, USHORT usIntegerType);
DWORD CreateCalc(USHORT CalcMode);
BOOL StrNumTest(PUCHAR lpNumStr, USHORT Radix, USHORT usIntegerType,
				PNUMBER pNumber);
BOOL ClipboardPaste(PNUMBER pdwNumber, USHORT Radix, USHORT usIntegerType);
BOOL ClipboardCopy(NUMBER dwNumber, USHORT Radix, USHORT usIntegerType);

extern LRESULT CALLBACK FWndProc(HWND hwnd, UINT message, WPARAM wParam,
							  LPARAM lParam);
extern LRESULT CALLBACK FDisplayControlProc(HWND hwnd, UINT message, WPARAM wParam,
										 LPARAM lParam);


USHORT Radix = 16;								// hexadecimal
USHORT WordSize = 64;							// Default word size: 64.
USHORT usIntegerType = INTEGER_TYPE_UNSIGNED;	// Default: 2's complement.
USHORT LastRadix = 10;							// decimal
STACK Stack = {0, 0, 0, 0}; 					// global binary stack
BOOL fEnter = FALSE;
BOOL fHelpKeysMode = FALSE; 					// Help keys mode
HWND hDisplay;									// handle to display window
HWND hwndDialog;
DWORD cyChar;
HANDLE hInst;
USHORT usCalcMode = CALC_MODE_BINARY;
BOOL fFirstTime = TRUE;
BOOL bFirstMove = TRUE;
BOOL bQuit = FALSE;

BOOL ErrorFlag = FALSE;

DWORD dwWindowX = 0;
DWORD dwWindowY = 0;
DWORD MenuY;
DWORD CaptionY;
DWORD BorderX;
DWORD BorderY;

#ifndef NO_TOOLTIPS
HWND hwndTT = NULL;
#endif
   

int APIENTRY WinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR	   lpCmdLine,
					 int	   nCmdShow) {
	MSG msg;
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
	
	MenuY = GetSystemMetrics(SM_CYMENU);
	CaptionY = GetSystemMetrics(SM_CYCAPTION);
	BorderX = GetSystemMetrics(SM_CXDLGFRAME);
	BorderY = GetSystemMetrics(SM_CYDLGFRAME);
	DebugPrintf("MenuY = %u, CaptionY = %u\n", MenuY, CaptionY);
	
	hInst = hInstance;
	
	RestoreRPNState();
	
	if (rc = CreateCalc(usCalcMode))
		return(rc);
	
	// Windows Message Loop
	// When we get a WM_QUIT message, GetMessage returns 0.
	
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		if (msg.message == WM_COMMAND && msg.wParam == IDM_CHANGE_MODE)
			CreateCalc(usCalcMode);
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
	
	SaveRPNState();
	
	return((int)msg.wParam);
}


void WmCommandProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    
	USHORT Digit;

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
    {

		//
		// Check for digit keys:
        //
        
        // digit in current base
        if (IsBaseDigit((UCHAR)wParam, Radix)) {
            
            if (! fEnter) {
                EnterOp(&Stack);
                Stack.X = 0;
                fEnter = TRUE;
            }
            
            Digit = (USHORT)((UCHAR)wParam - (isdigit((UCHAR)wParam)
                ? '0' : 'A' - 10));
            //                    DebugPrintf("Digit received: %d\n", Digit);
            
            if (NumTest(Stack.X, Radix, Digit, usIntegerType)) {
            /*
            * Signed decimal numbers need special treatment if
            * they are negative.
                */
                if ((Radix == 10) && (usIntegerType !=
                    INTEGER_TYPE_UNSIGNED) &&
                    ((SIGNED_NUMBER)Stack.X < 0))
                    Stack.X = Radix * Stack.X - Digit;
                else
                    Stack.X = Radix * Stack.X + Digit;
                RedrawDisplay(hDisplay);
            }
            else {
                DebugPrintf("Overflow, don't add digit\n");
            }
        }
        
        else {  // operation pressed.
            CalcIt(&Stack, (DWORD)wParam, hwnd);
        }
    }
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    HWND hButton;
    
    switch(message) 
    {
    case WM_CREATE:
        DebugPrintf("WM_CREATE message. hwnd = %u\n", hwnd);
        bFirstMove = TRUE;
        break;
        
    case WM_SHOWWINDOW:
        DebugPrintf("WM_SHOWWINDOW message. hwnd = %u\n", hwnd);
        if (wParam) 
        {
            SetWindowPos(hwnd, HWND_TOP, dwWindowX, dwWindowY, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
            DebugPrintf("SetWindowPos(%u, %u)\n", dwWindowX, dwWindowY);
        }
        break;
        
    case WM_KEYDOWN:
        // Non-character keys need to be trapped here so that WM_CHAR
        // can operate on them.
        if (wParam == VK_DELETE)
            wParam = DELETE_KEY;	// will fall through to WM_CHAR
        else if (wParam == VK_LEFT)
            wParam = VK_BACK;		// will fall through to WM_CHAR
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
        DebugPrintf("WM_COMMAND message %u, hwnd = %u\n", wParam, hwnd);
        SetFocus(hwnd);
        
        switch (usCalcMode) 
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
        DebugPrintf("WM_MOVE: (%u, %u), [0x%x, 0x%x]\n",
            (DWORD)((MAKEPOINTS(lParam)).x),
            (DWORD)((MAKEPOINTS(lParam)).y),
            (DWORD)((MAKEPOINTS(lParam)).x),
            (DWORD)((MAKEPOINTS(lParam)).y));
        
        // if this is the automatic move instruction and we have
        // displayed a window already (ie, we are changing modes)
        // reset the move coordinates before they are processed.

      
        if (! bFirstMove && (MAKEPOINTS(lParam).x > 0) && (MAKEPOINTS(lParam).y > 0)) 
        {    
            dwWindowX = (DWORD)((MAKEPOINTS(lParam)).x) - BorderX;
            dwWindowY = (DWORD)((MAKEPOINTS(lParam)).y) -
                (MenuY + CaptionY + BorderY);
        }
        bFirstMove = FALSE;
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
        switch (usCalcMode) 
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

		    switch (usCalcMode) 
            {
		    case CALC_MODE_BINARY:
			    PaintStackDisplay(hwnd, hdc, &Stack, Radix);
			    break;
		
            case CALC_MODE_FLOAT:
			    FPaintStackDisplay(hwnd, hdc, &FStack, pEntryString);
			    break;
		    }

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

	ErrorFlag = FALSE;
	RedrawDisplay(hwnd);
}


void PaintStackDisplay(HWND hwnd, HDC hdc, PSTACK pStack, USHORT Radix) {
	char psString[DISPLAY_WIDTH + 1];  // 64 bits + 1 for null
	RECT rect;
	char *psIntegerType;
	char *psBase;

	//	  DebugPrintf("PaintStackDisplay(hwnd: %u)\n", hwnd);
	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	GetClientRect(hwnd, &rect);
	Rectangle(hdc, 0, 0, rect.right, rect.bottom);
	
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));	// fixed font
	SetBkColor(hdc, RGB(0, 0, 0));					 // Black background color
	
	SetTextColor(hdc, RGB(0, 255, 0));				 // Green text color

	switch(usIntegerType) {
	case INTEGER_TYPE_1S_COMP:
		psIntegerType = "~1s";
		break;
	case INTEGER_TYPE_2S_COMP:
		psIntegerType = "~2s";
		break;
	default:
		psIntegerType = "Unsigned";
		break;
	}

	switch(Radix) {
	case 16:
		psBase = "Hex";
		break;
	case 10:
		psBase = "Dec";
		break;
	case 8:
		psBase = "Oct";
		break;
	default:
		psBase = "Bin";
		break;
	}

	sprintf_s(psString, sizeof(psString), "%c   %s  %-2u     %s",
			(ErrorFlag? '!':' '),
			psBase,
			WordSize,
			psIntegerType);
	TextOut(hdc, 1, 1, psString, (int)strlen(psString));

	//
	//  Queue timer to clear the error flag.
	//

    if (ErrorFlag) {
		SetTimer(hwnd, IDD_CLEAR_ERROR, 300, ClearErrorTimer);
	}
	
	SetTextColor(hdc, RGB(255, 0, 0));				 // Red text color
	
	IntToA(pStack->T, psString, sizeof(psString), Radix, usIntegerType, TRUE);
	TextOut(hdc, 1, 1 + (int)cyChar, psString, (int)strlen(psString));
	
	IntToA(pStack->Z, psString, sizeof(psString), Radix, usIntegerType, TRUE);
	TextOut(hdc, 1, 1 + 2 * (int)cyChar, psString, (int)strlen(psString));
	
	IntToA(pStack->Y, psString, sizeof(psString), Radix, usIntegerType, TRUE);
	TextOut(hdc, 1, 1 + 3 * (int)cyChar, psString, (int)strlen(psString));
	
	IntToA(pStack->X, psString, sizeof(psString), Radix, usIntegerType, TRUE);
	TextOut(hdc, 1, 1 + 4 * (int)cyChar, psString, (int)strlen(psString));
}



char * IntToA(NUMBER Value, char * psString, ULONG Length, USHORT Radix, USHORT IntegerType, BOOLEAN Adorn) {
	
	switch (IntegerType) {
	case INTEGER_TYPE_UNSIGNED:
		if (_ui64toa_s(Value, psString, Length, Radix) != 0)
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
		if (_i64toa_s((SIGNED_NUMBER)Value, psString, Length, Radix) != 0)
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


/*** ChangeBase -- Changes the input/display base
*
* ENTRY    - Base = new base
* EXIT	   - none
* SYNOPSIS - Changes the current number base.  Also updates the last used
*			 base for use by the LastBase command.	Only changes the base
*			 if the new base is different from the current one, so that the
*			 Last Base is always different than the current base.
* EFFECTS  - Effects globals: Radix and LastRadix.
*
*/

void ChangeBase(USHORT Base)
{
	
	//
	//  Until the display can take 64bit binary, disable that mode.
	//

	if (WordSize == 64 && Base == 2) {
		ErrorFlag = TRUE;
		return;
	}
	if (Radix != Base) {
		LastRadix = Radix;
		Radix = Base;
	}
}


/*** CastNumber -- Smashes the input according to the current wordsize.
*
* ENTRY    - Number - pointer to number to cast
* EXIT	   - none
* SYNOPSIS - Strips or promotes the number according to the current
*			 wordsize, sign extending as appropriate to the current
*			 integer type.
* EFFECTS  - Uses global: WordSize
*
*/

void CastNumber(PNUMBER Castee)
{
	if (usIntegerType != INTEGER_TYPE_UNSIGNED) {
		switch (WordSize) {
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
		switch (WordSize) {
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

void CastStack(PSTACK pStack)
{
	CastNumber(&pStack->X);
	CastNumber(&pStack->Y);
	CastNumber(&pStack->Z);
	CastNumber(&pStack->T);
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
* EFFECTS  - Effects globals: WordSize, recasts the stack
*
*/

void ChangeWordSize(PSTACK pStack, USHORT Size)
{
	//
	//  Until the display can take 64bit binary, disable that mode.
	//

	if (Size == 64 && Radix == 2) {
		ErrorFlag = TRUE;
		return;
	}

	WordSize = Size;
	
	//
	//	Recast the stack to match the new wordsize.
	//
	
	CastStack(pStack);
}

/*** ChangeIntegerType -- Changes the input/display integer representation
*
* ENTRY    - dwOperation = Identifier of menu item
* EXIT	   - none
* SYNOPSIS - Changes the current integer representation.
* EFFECTS  - Effects globals: usIntegerType, recasts the stack
*
*/

void ChangeIntegerType(PSTACK pStack, DWORD dwOperation, HWND hwnd)
{
	switch(dwOperation) {
	case IDM_INTEGER_TYPE_UNSIGNED:
		usIntegerType = INTEGER_TYPE_UNSIGNED;
		break;
		
	case IDM_INTEGER_TYPE_1S:
		usIntegerType = INTEGER_TYPE_1S_COMP;
		break;
		
	case IDM_INTEGER_TYPE_2S:
		usIntegerType = INTEGER_TYPE_2S_COMP;
		break;
	default:
		break;
	}
	
	//
	//	Recast the stack to match the new integer type.
	//
	
	CastStack(pStack);
}


/*** DropY -- Drops Y from the stack, copying Z and T down.
*
* ENTRY    - pStack
* EXIT	   - none.
* SYNOPSIS - Moves Z over Y and T over Z.  Keeps the contents of T.
* WARNINGS -
* EFFECTS  -
*
*/

void DropY(PSTACK pStack)
{
	pStack->Y = pStack->Z;
	pStack->Z = pStack->T;
}


/*** RollDownOp -- Roll the stack down one
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Rolls the contents of the stack down one level; ie, X -> T,
*			 Y -> X, etc.
*
*/

void RollDownOp(PSTACK pStack)
{
	register NUMBER Temp;
	
	
	Temp = pStack->X;
	pStack->X = pStack->Y;
	pStack->Y = pStack->Z;
	pStack->Z = pStack->T;
	pStack->T = Temp;
	fEnter = FALSE;
}



/*** SwapOp -- Swap X and Y
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Swaps the contents of X and Y.
*
*/

void SwapOp(PSTACK pStack)
{
	register NUMBER Temp;
	
	
	Temp = pStack->X;
	pStack->X = pStack->Y;
	pStack->Y = Temp;
	fEnter = FALSE;
}


/*** KiloOp -- Multiplies current X entry by 1K
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Multiplies content of X by 1K.
*
*/

void KiloOp(PSTACK pStack)
{
	pStack->X *= KILO;
	fEnter = FALSE;
}


/*** NegOp -- X --> -X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract X from 0.  Doesn't drop the stack.
*
*/

void NegOp(PSTACK pStack)
{
	(SIGNED_NUMBER)pStack->X = -(SIGNED_NUMBER)(pStack->X);
	fEnter = FALSE;
}


/*** NotOp -- X --> ~(X)
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - X = ~X.
*
*/

void NotOp(PSTACK pStack)
{
	pStack->X = ~(pStack->X);
	fEnter = FALSE;
}


/*** DivideOp -- (Y / X) --> X
*
* ENTRY    - hwnd = handle to the current window
*			 pStack
* EXIT	   - none
* SYNOPSIS - Divide Y by X into X.	 Move stack down, clear fEnter.
*
*/

void DivideOp(PSTACK pStack)
{
	if (pStack->X != 0) {
		pStack->X = pStack->Y / pStack->X;
		DropY(pStack);
	}
	else
		ErrorBox("Division by zero");
	
	fEnter = FALSE;
}


/*** ModOp -- (Y % X) --> X
*
* ENTRY    - hwnd = handle to the current window
*			 pStack
* EXIT	   - none
* SYNOPSIS - Mod Y by X into X.   Move stack down, clear fEnter.
*
*/

void ModOp(PSTACK pStack)
{
	if (pStack->X != 0) {
		pStack->X = pStack->Y % pStack->X;
		DropY(pStack);
	}
	else
		ErrorBox("Division by zero");
	fEnter = FALSE;
}


/*** AddOp -- (X + Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Add Y to X, move stack down, set fEnter to FALSE
*
*/

void AddOp(PSTACK pStack)
{
	pStack->X += pStack->Y;
	DropY(pStack);
	fEnter = FALSE;
}


/*** SubtractOp -- (Y - X) --> X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract X from Y, move stack down, set fEnter to FALSE
*
*/

void SubtractOp(PSTACK pStack)
{
	pStack->X = pStack->Y - pStack->X;
	DropY(pStack);
	fEnter = FALSE;
}


/*** UnaryAddOp -- (X + 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Add 1 to X, set fEnter to FALSE
*
*/

void UnaryAddOp(PSTACK pStack)
{
	pStack->X += 1;
	fEnter = FALSE;
}


/*** UnarySubtractOp -- (X - 1) --> X
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Subtract 1 from X, set fEnter to FALSE
*
*/

void UnarySubtractOp(PSTACK pStack)
{
	pStack->X -= 1;
	fEnter = FALSE;
}


/*** MultiplyOp -- (X * Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X * Y to X, move stack down, set fEnter to FALSE
*
*/

void MultiplyOp(PSTACK pStack)
{
	pStack->X *= pStack->Y;
	DropY(pStack);
	fEnter = FALSE;
}


/*** AndOp -- (X AND Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X AND Y to X, move stack down, set fEnter to FALSE
*
*/

void AndOp(PSTACK pStack)
{
	pStack->X &= pStack->Y;
	DropY(pStack);
	fEnter = FALSE;
}


/*** OrOp -- (X OR Y) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X OR Y to X, move stack down, set fEnter to FALSE
*
*/

void OrOp(PSTACK pStack)
{
	pStack->X |= pStack->Y;
	DropY(pStack);
	fEnter = FALSE;
}


/*** XorOp -- (Y ^ X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y XOR X --> X, move stack down, set fEnter to FALSE
*
*/

void XorOp(PSTACK pStack)
{
	pStack->X ^= pStack->Y;
	DropY(pStack);
	fEnter = FALSE;
}


/*** LeftShiftOp -- (Y << X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y shifted left by X bits to X, move stack down, set fEnter
*			 to FALSE
*
*/

void LeftShiftOp(PSTACK pStack)
{

	pStack->X = pStack->Y << pStack->X;
	DropY(pStack);
	fEnter = FALSE;
}


/*** RightShiftOp -- (Y >> X) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - Y Shifted right by X bits to X, move stack down, set fEnter
*			 to FALSE
*
*/

void RightShiftOp(PSTACK pStack)
{

	//
	//  Right shift is arithmetic if signed.
	//
	
	if (usIntegerType == INTEGER_TYPE_UNSIGNED) {
		pStack->X = pStack->Y >> pStack->X;
	} else {
		(SIGNED_NUMBER)pStack->X = (SIGNED_NUMBER)pStack->Y >> pStack->X;
	}
	DropY(pStack);
	fEnter = FALSE;
}


/*** UnaryLeftShiftOp -- (X << 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X shifted left by 1 bit to X, set fEnter to FALSE
*
*/

void UnaryLeftShiftOp(PSTACK pStack)
{
	pStack->X = pStack->X << 1;
	fEnter = FALSE;
}


/*** UnaryRightShiftOp -- (X >> 1) --> X
*
* ENTRY    - pStack
* EXIT	   -
* SYNOPSIS - X Shifted right by 1 bits to X, set fEnter to FALSE
*
*/

void UnaryRightShiftOp(PSTACK pStack)
{
	//
	//  Right shift is arithmetic if signed.
	//
	
	if (usIntegerType == INTEGER_TYPE_UNSIGNED) {
		pStack->X = pStack->X >> 1;
	} else {
		(SIGNED_NUMBER)pStack->X = (SIGNED_NUMBER)pStack->X >> 1;
	}
	fEnter = FALSE;
}


/*** EnterOp -- Enter the number or Dup.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - If fEnter, we should turn off fEnter else we should move the
*			 stack up.
*
*/

void EnterOp(PSTACK pStack)
{
	if (fEnter) {
		fEnter = FALSE;
		
		//
		//	Cast the number: for signed operations, this
		//	will sign extend the full number.
		//
		
		CastNumber(&pStack->X);
		
	} else {
		pStack->T = pStack->Z;
		pStack->Z = pStack->Y;
		pStack->Y = pStack->X;
	}
}


/*** DropOp -- Drop the value in X.
*
* ENTRY    - pStack
*
*/

void DropOp(PSTACK pStack)
{
	pStack->X = pStack->Y;
	pStack->Y = pStack->Z;
	pStack->Z = pStack->T;
	pStack->T = 0;
	fEnter = FALSE;
}


/*** BackSpaceOp -- removes the right-most digit from X.
*
* ENTRY    - pStack
* EXIT	   - none
* SYNOPSIS - Removes the right-most digit from X, depending on the current
*			 base and integer type.
*
*/

void BackSpaceOp(PSTACK pStack)
{
	if ((usIntegerType != INTEGER_TYPE_UNSIGNED) && (Radix == 10))
		(SIGNED_NUMBER)(pStack->X) =
		(SIGNED_NUMBER)(pStack->X) / (signed short)Radix;
	else	// unsigned digit entry mode
		pStack->X = pStack->X / Radix;
}


/*** CalcIt -- Decide which operation to perform
*
* ENTRY    - pStack
*			 dwOperation = the operation to perform
*			 hwnd = handle to calculator window
* EXIT	   - none
* SYNOPSIS - Delegates authority to one of the Op routines.
*
*/

void CalcIt(PSTACK pStack, DWORD dwOperation, HWND hwnd)
{
	//	  DebugPrintf("CalcIt(%u)\n", dwOperation);
	
	switch(dwOperation) {
	case ENTER: 					EnterOp(pStack);
		break;
	case SWAP_KEY:					SwapOp(pStack);
		break;
	case PLUS_KEY:					AddOp(pStack);
		break;
	case MINUS_KEY: 				SubtractOp(pStack);
		break;
	case UNARY_PLUS_KEY:			UnaryAddOp(pStack);
		break;
	case UNARY_MINUS_KEY:			UnarySubtractOp(pStack);
		break;
	case MULT_KEY:					MultiplyOp(pStack);
		break;
	case DIV_KEY:					DivideOp(pStack);
		break;
	case MOD_KEY:					ModOp(pStack);
		break;
	case AND_KEY:					AndOp(pStack);
		break;
	case OR_KEY:					OrOp(pStack);
		break;
	case XOR_KEY:					XorOp(pStack);
		break;
	case LEFT_SHIFT_KEY:			LeftShiftOp(pStack);
		break;
	case RIGHT_SHIFT_KEY:			RightShiftOp(pStack);
		break;
	case UNARY_LEFT_SHIFT_KEY:		UnaryLeftShiftOp(pStack);
		break;
	case UNARY_RIGHT_SHIFT_KEY:		UnaryRightShiftOp(pStack);
		break;
	case NEGATE_KEY:				NegOp(pStack);
		break;
	case NOT_KEY:					NotOp(pStack);
		break;
	case BACKSPACE_KEY: 			BackSpaceOp(pStack);
		break;
	case DELETE_KEY:				DropOp(pStack);
		break;
	case ROLL_DOWN_KEY: 			RollDownOp(pStack);
		break;
	case KILO_KEY:					KiloOp(pStack);
		break;
	case TOGGLE_BASE_KEY:			ChangeBase(LastRadix);
		break;
		
	case IDM_BINARY:		ChangeBase(2);
		break;
	case IDM_OCTAL: 		ChangeBase(8);
		break;
	case IDM_DECIMAL:		ChangeBase(10);
		break;
	case IDM_HEXADECIMAL:	ChangeBase(16);
		break;
	case IDM_WORD_SIZE_8:	ChangeWordSize(pStack, 8);
		break;
	case IDM_WORD_SIZE_16:	ChangeWordSize(pStack, 16);
		break;
	case IDM_WORD_SIZE_32:	ChangeWordSize(pStack, 32);
		break;
	case IDM_WORD_SIZE_64:	ChangeWordSize(pStack, 64);
		break;
		
	case IDM_INTEGER_TYPE_UNSIGNED:
	case IDM_INTEGER_TYPE_1S:
	case IDM_INTEGER_TYPE_2S:
		ChangeIntegerType(pStack, dwOperation, hwnd);
		break;
		
	case IDM_FLOAT:
		usCalcMode = CALC_MODE_FLOAT;
		PostMessage(hwnd, WM_COMMAND, IDM_CHANGE_MODE, 0);
		return; 	// don't redraw the stack display
		
		break;
		
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
		ClipboardPaste(&pStack->X, Radix, usIntegerType);
		break;
		
	case IDM_PASTE_Y:
		ClipboardPaste(&pStack->Y, Radix, usIntegerType);
		break;
		
	case IDM_PASTE_Z:
		ClipboardPaste(&pStack->Z, Radix, usIntegerType);
		break;
		
	case IDM_PASTE_T:
		ClipboardPaste(&pStack->T, Radix, usIntegerType);
		break;
		
	default:
		DebugPrintf("Don't recognize %u operation\n", dwOperation);
		ErrorFlag = TRUE;
		break;
	}
		
	//
	//	Recast the number in X according to the current wordsize.
	//	This will remove extra precision from the 64bit base number.
	//
		
	CastNumber(&pStack->X);
		
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
        switch ( Radix )
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
*
* OUTPUTS: Returns TRUE if we have room to put the digit in the number
*		   without overflow.
*
* Uses variations on this algorithm:
*	   if ((MAXULONG - Digit) / Radix) >= X: return TRUE
*/

BOOL NumTest(NUMBER X, USHORT Radix, USHORT Digit, USHORT usIntegerType) {
	if ((usIntegerType != INTEGER_TYPE_UNSIGNED) && (Radix == 10)) {
		// signed decimal entry is broken into positive/negative cases
		if (((SIGNED_NUMBER)X) < 0) { // signed number is negative
			return (((SIGNED_NUMBER)(MinSignedWordSize() + Digit) / Radix) <=
				((SIGNED_NUMBER)X));
		}
		else {	// signed number is positive.
			return (((MaxSignedWordSize() - Digit) / Radix) >= ((SIGNED_NUMBER)X));
		}
	}
	else {		// unsigned digit entry, don't worry about +/-
		return (((MaxWordSize() - Digit) / Radix) >= X);
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
BOOL StrNumTest(PUCHAR lpNumStr, USHORT Radix, USHORT usIntegerType,
				PNUMBER pNumber) {
	NUMBER Number = 0;
	size_t dwStrLength = strlen(lpNumStr);
	USHORT Digit;
	BOOL fReturn = TRUE;
	DWORD i;
	
	
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
			NumTest(Number, Radix, Digit, usIntegerType)) {
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
DWORD CreateCalc(USHORT CalcMode) {
	HWND hwnd;
	DWORD rc;
	LPTSTR lpszDlgTemp;
	DWORD DisplayItem;
#ifndef NO_TOOLTIPS
    struct ENUMCHILDSTRUCT ecs;
#endif
	
	usCalcMode = CalcMode;	// update global CalcMode status
	
	switch (usCalcMode) {
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
	
	DebugPrintf("ShowWindow, hwnd = %u\n", hwnd);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	
	switch (usCalcMode) {
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
	HGLOBAL hClipboardMem;
	PUCHAR	pchClipboard;
	UCHAR	StrNum[DISPLAY_WIDTH + 1];
	size_t	dwLength;
	
	
	IntToA(Number, StrNum, sizeof(StrNum), Radix, usIntegerType, FALSE);
	
	hClipboardMem = GlobalAlloc(GHND | GMEM_DDESHARE,
		(dwLength = strlen(StrNum)) + 1);
	
	DebugPrintf("Copy String: [%s], Length: %u\n", StrNum, dwLength);
	
	if (! hClipboardMem) {
		DebugPrintf("Clipboard Copy couldn't allocate %u bytes of memory\n",
			dwLength);
		return(FALSE);
	}
	
	if (!(pchClipboard = GlobalLock(hClipboardMem))) {
		DebugPrintf("Clipboard Copy GlobalLock couldn't lock memory\n");
		GlobalFree(hClipboardMem);
		return(FALSE);
	}
	
	memcpy(pchClipboard, StrNum, dwLength + 1);
	
	GlobalUnlock(hClipboardMem);
	OpenClipboard(hwndDialog);		  // HWND?
	
	EmptyClipboard();
	
	// Note: SetClipboardData implicitly frees hClipboardMem.
	SetClipboardData(CF_TEXT, hClipboardMem);
	
	CloseClipboard();
	return(TRUE);
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
	HGLOBAL hClipboardMem;
	PUCHAR	pClipboardMem;
	NUMBER	Number = 0;
	BOOL	fReturn = TRUE;
	PUCHAR	pTemp;
	
	
	OpenClipboard(hwndDialog);
	
	if (hClipboardMem = GetClipboardData(CF_TEXT)) {
		if (! (pClipboardMem = GlobalLock(hClipboardMem))) {
			DebugPrintf("Clipboard Paste: Couldn't lock clipboard memory\n");
			return(FALSE);
		}
		
		// Get past any whitespace
		while(*pClipboardMem <= MAX_CONTROL_CHAR)
			pClipboardMem++;
		
		
		// Strip off any control characters (like the CR/LF which the console
		// window copy function puts there and any trailing spaces.)
		pTemp = pClipboardMem;
		while (*pTemp > MAX_CONTROL_CHAR)
			pTemp++;
		*pTemp = '\0';
		
		if (StrNumTest(pClipboardMem, Radix, usIntegerType, &Number)) {
			// valid number in Number, store and cast
			*pNumber = Number;
			CastNumber(pNumber);
		}
		else {	// warn user that the clipboard doesn't contain a valid number
				ErrorBox("The clipboard does not contain a\n\
					valid number in the current base.");
				fReturn = FALSE;
		}
		GlobalUnlock(hClipboardMem);
	}
	else {
	ErrorBox("The clipboard does not contain a number\n\
		in the correct format.");
	fReturn = FALSE;
	}
	
	CloseClipboard();
	return(fReturn);
}
