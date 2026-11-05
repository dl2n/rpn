/*
* RPNF.c  RPN Floating Point Calculator.
*			  Loosely based upon Ray Duncan's HexCalc2 from "Advanced OS/2
*			  Programming"
*
* This calculator emulates a simple Hewlett-Packard-style 4-level stack of
* double floats.
*
* History:
*	 04/18/92	 BruceK   Copy from rpn.c and make it "float"
*	 08/17/95	 BruceK   Make it work with current Win32 SDK
*    03/21/00    JeffMill Added tooltips (search for NO_TOOLTIPS)
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
#include <math.h>
#include <float.h>
#include "rpn.h"

// no _fpclass function in the libraries
#define FLOAT_BUG	TRUE

extern INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void FCalcIt(PFSTACK pStack, DWORD dwOperation, HWND hwnd);
BOOL FHelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
                UCHAR* szStringBuffer, DWORD cbStringBuffer );
extern void DebugPrintf(const char * format, ...);
int IsFloatChar(UCHAR ch);
void FEnterOp(PFSTACK pFStack, PUCHAR pEntryString);
BOOL FNumTest(PUCHAR pEntryString, UCHAR Char);
char * FToA(FNUMBER FX, char * psString, ULONG Length, USHORT SignificantDigits,
			USHORT usFloatDisplayMode, BOOLEAN Adorn);
extern void ErrorBox(const char * format, ...);
extern DWORD CreateCalc(USHORT CalcMode);
extern BOOL IsBaseDigit(UCHAR ch, USHORT Radix);
BOOL FClipboardCopy(FNUMBER fNumber, USHORT usSignificantDigits,
					USHORT usFloatDisplayMode);
BOOL FClipboardPaste(FNUMBER * pfNumber);
BOOL FStrNumTest(PUCHAR lpNumStr, FNUMBER * pfNumber);
BOOL StrClipboardCopy(PUCHAR pString);
BOOL StrClipboardPaste(PUCHAR pString);

FSTACK FStack = {0.0, 0.0, 0.0, 0.0};	// global floating point stack
UCHAR pEntryString[DISPLAY_WIDTH + 1] = "";
USHORT usSignificantDigits = 10;
USHORT usFloatDisplayMode = FLOAT_DISPLAY_FIX;
USHORT usFloatAngleType = FLOAT_ANGLE_DEG;
extern BOOL fEnter;
extern BOOL fHelpKeysMode;	   // Help keys mode
extern HWND hDisplay;				   // handle to display window
extern HWND hwndDialog;
extern DWORD cyChar;
extern HANDLE hInst;
BOOL fInverse = FALSE;
// extern BOOL fDontQuit;
extern BOOL bQuit;
extern USHORT usCalcMode;
extern DWORD dwWindowX;
extern DWORD dwWindowY;
extern BOOL fFirstTime;
extern BOOL bFirstMove;
extern DWORD MenuY;
extern DWORD CaptionY;


void FWmCommandProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( fHelpKeysMode ) 
    {
        UCHAR szStringBuffer[ MAX_STRING_SIZE ];

		if ( 		FHelpKeys( (DWORD)wParam, hInst, hwnd, 
                        szStringBuffer, MAX_STRING_SIZE ) )
        {
    		MessageBox(hwnd, szStringBuffer, "RPN Help Keys Mode",
	    		MB_OK | MB_ICONINFORMATION);
        }

    }
	else 
    {		// not in HelpKeysMode
				/*
				* Check for digit keys:
		*/
		
		// valid float digit (or .) ?
		if (IsFloatChar((UCHAR)wParam)) {
			
			if (! fEnter) { // fEnter is entry mode flag
				// If we aren't in entry mode, get us into entry mode.
				// Move the contents of the stack up one level so that
				// there is room in X for a new number.
				FEnterOp(&FStack, pEntryString);
				pEntryString[0] = '\0';
				fEnter = TRUE;
			}
			
			DebugPrintf("char: %c\n", (UCHAR)wParam);
			
			if (FNumTest(pEntryString, (UCHAR)wParam)) {
				RedrawDisplay(hDisplay);
			}
			else {
				DebugPrintf("Couldn't add character to entry string.\n");
			}
		}
		
		else {	// operation pressed.
			FCalcIt(&FStack, (DWORD)wParam, hwnd);
		}
	}
}

void FWmNotifyProc( HWND hwnd, int idCtrl, LPNMHDR pnmh )
{
#ifndef NO_TOOLTIPS

    static UCHAR szStringBuffer[ MAX_STRING_SIZE ];
    LPNMTTDISPINFO lpnmtdi;

    switch ( pnmh->code )
    {
    case TTN_GETDISPINFO:
        lpnmtdi = (LPNMTTDISPINFO)pnmh;

        SendMessage( pnmh->hwndFrom, TTM_SETMAXTIPWIDTH, 0, 300 );

        if ( FHelpKeys( GetDlgCtrlID( (HWND)pnmh->idFrom ),
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

void FPaintStackDisplay(HWND hwnd, HDC hdc, PFSTACK pFStack,
						UCHAR * pEntryString) {
	
	char psString[DISPLAY_WIDTH + 1];  // 64 bits + 1 for null
	char *psFloatAngleType;
	RECT rect;
	
	
	SelectObject(hdc, GetStockObject(BLACK_PEN));
	SelectObject(hdc, GetStockObject(BLACK_BRUSH));
	GetClientRect(hwnd, &rect);
	Rectangle(hdc, 0, 0, rect.right, rect.bottom);
	
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));	// fixed font
	SetBkColor(hdc, RGB(0, 0, 0));					 // Black background color
	
	SetTextColor(hdc, RGB(0, 255, 0));				 // Green text color

	switch(usFloatAngleType) {
	case FLOAT_ANGLE_RAD:
		psFloatAngleType = "Rad";
		break;
	case FLOAT_ANGLE_GRAD:
		psFloatAngleType = "Grad";
		break;
	default:
		psFloatAngleType = "Degree";
		break;
	}

	sprintf_s(psString, sizeof(psString), "%c         %6s      %s",
			(ErrorFlag? '!':' '),
		    psFloatAngleType,
			fInverse ? "Inverse" : "");
	TextOut(hdc, 1, 1, psString, (int)strlen(psString));

    //
	//  Queue timer to clear the error flag.
	//

    if (ErrorFlag) {
		SetTimer(hwnd, IDD_CLEAR_ERROR, 300, ClearErrorTimer);
	}

	
	SetTextColor(hdc, RGB(255, 0, 0));				 // Red text color
	
	FToA(pFStack->T, psString, sizeof(psString), usSignificantDigits, usFloatDisplayMode, TRUE);
	TextOut(hdc, 1, 1 + (int)cyChar, psString, (int)strlen(psString));
	
	FToA(pFStack->Z, psString, sizeof(psString), usSignificantDigits, usFloatDisplayMode, TRUE);
	TextOut(hdc, 1, 1 + 2 * (int)cyChar, psString, (int)strlen(psString));
	
	FToA(pFStack->Y, psString, sizeof(psString), usSignificantDigits, usFloatDisplayMode, TRUE);
	TextOut(hdc, 1, 1 + 3 * (int)cyChar, psString, (int)strlen(psString));
	
	if (fEnter)
		strcpy_s(psString, sizeof(psString), pEntryString);
	else
		FToA(pFStack->X, psString, sizeof(psString), usSignificantDigits, usFloatDisplayMode, TRUE);
	TextOut(hdc, 1, 1 + 4 * (int)cyChar, psString, (int)strlen(psString));
}



char * FToA(FNUMBER FX, char * psString, ULONG Length, USHORT SignificantDigits, USHORT usFloatDisplayMode, BOOLEAN Adorn) {
	
	PCHAR PeriodIndex;
	size_t CurLen;

	switch(usFloatDisplayMode) {
	case FLOAT_DISPLAY_AUTO:
		if (_gcvt_s(psString, Length, FX, SignificantDigits) != 0)
			return(NULL);
		break;
		
	case FLOAT_DISPLAY_FIX:
		if (_gcvt_s(psString, Length, FX, SignificantDigits) != 0)
			return(NULL);
		break;
		
	case FLOAT_DISPLAY_SCI:
		// BUGBUG: Not yet implemented, fall through.
		
	case FLOAT_DISPLAY_ENG:
		// BUGBUG: Not yet implemented, fall through.
		
	default:
		return(0);
		break;
	}

	//
	//  Tack the period on if not already present.
	//
	
	if ((PeriodIndex = strchr(psString, '.')) == NULL) {
		CurLen = strlen(psString);
		*(psString + CurLen) = '.';
		*(psString + CurLen + 1) = '\0';
	}


	if (Adorn) {

		size_t SplitUnit = 3;
		size_t Remain, Slop;
		PUCHAR SplitPoint = psString;
		UCHAR SplitChar = ',';

		//
		//  Shove past the sign if existant.
		//
		
		if (*SplitPoint == '-') {
			SplitPoint++;
		}

		//
		//  Advance beyond the head MSD's of the number.
		//

		Remain = strchr(SplitPoint, '.') - SplitPoint;
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
					strlen(SplitPoint) + 1);
			*SplitPoint++ = SplitChar;
		}

		//
		//  If/when fix mode is implemented, right justification would go here.
		//
	}

	return psString;
}



/*** IsFloatChar -- Is the specified character valid character for an FNUMBER?
*
* ENTRY    - ch = character to test (id button)
* EXIT	   - returns TRUE if the character is a valid FNUMBER character.
* SUMMARY  - doesn't worry about exponents or signs, those are handled
*			 as operations.  Check for digits or '.'.
*/

int IsFloatChar(UCHAR ch)
{
	if (isdigit(ch))
		return(TRUE);
	if (ch == '.')
		return(TRUE);
	
	return(FALSE);
}


/*** FDropY -- Drops Y from the FStack, copying Z and T down.
*
* ENTRY    - pFStack
* EXIT	   - none.
* SYNOPSIS - Moves Z over Y and T over Z.  Keeps the contents of T.
* WARNINGS -
* EFFECTS  -
*
*/

void FDropY(PFSTACK pFStack)
{
	pFStack->Y = pFStack->Z;
	pFStack->Z = pFStack->T;
}


/*** FRollDownOp -- Roll the FStack down one
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Rolls the contents of the FStack down one level; ie, X -> T,
*			 Y -> X, etc.
*
*/

void FRollDownOp(PFSTACK pFStack)
{
	FNUMBER Temp;
	
	
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	Temp = pFStack->X;
	pFStack->X = pFStack->Y;
	pFStack->Y = pFStack->Z;
	pFStack->Z = pFStack->T;
	pFStack->T = Temp;
}


/*** FSwapOp -- Swap X and Y
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Swaps the contents of X and Y.
*
*/

void FSwapOp(PFSTACK pFStack)
{
	FNUMBER Temp;
	
	
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	Temp = pFStack->X;
	pFStack->X = pFStack->Y;
	pFStack->Y = Temp;
}


/*** FNegOp -- X --> -X
*
* ENTRY    - pFStack
*			 pEntryString
*			 fEnter
* EXIT	   - none
* SYNOPSIS - Subtract X from 0.  Doesn't drop the stack and doesn't affect
*			 the entry flag.
*
*/

void FNegOp(PFSTACK pFStack, PUCHAR pEntryString, BOOL fEnter)
{
	PUCHAR pTemp;
	size_t Length;
	
	
	if (fEnter) {
		// If we have an exponent, we should change the sign of that
		if (pTemp = strchr(pEntryString, 'E')) {
			pEntryString = pTemp + 1;
		}
		
		// change the sign
		if (Length = strlen(pEntryString)) {
			if (*pEntryString == '-') { // sign already negative, remove it
				while (*pEntryString != '\0') {
					*pEntryString = *(pEntryString + 1);
					pEntryString++;
				}
			}
			else {						// sign positive, add a '-' char
				pTemp = pEntryString + Length;
				while (pTemp >= pEntryString) {
					*(pTemp + 1) = *pTemp;
					pTemp--;
				}
				*pEntryString = '-';
			}
		}
		else {	// empty string, give us a negative anyway
			*pEntryString = '-';
			*(pEntryString+1) = '\0';
		}
	}
	
	else
		pFStack->X = -(pFStack->X);
}


/*
* FEExOp
*
* INPUTS:  pEntryString
* OUTPUTS:
* SUMARY:
*
*/
void FEExOp(PUCHAR pEntryString, BOOL fEnter) {
	PUCHAR pTemp;
	size_t Length;
	
	if (fEnter) {  // nop if not in entry mode
		if (pTemp = strchr(pEntryString, 'E')) {
			// already has an exponent, truncate it.
			*pTemp = '\0';
		}
		else { // Add an 'E' to the end of the string.
			if (! (Length = strlen(pEntryString))) {
				// if there is no mantissa, provide a 1
				pEntryString[Length++] = '1';
			}
			
			pEntryString[Length++] = 'E';
			pEntryString[Length] = '\0';
		}
	}
}


/*** FDivideOp -- (Y / X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Divide Y by X into X.	 Move stack down, clear fEnter.
*
*/

void FDivideOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	if (pFStack->X != 0.0) {
		pFStack->X = pFStack->Y / pFStack->X;
		FDropY(pFStack);
	}
	else
		ErrorBox("Division by zero");
}


/*** FOneOverOp -- (1 / X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Divide 1 by X into X.	 Move stack down, clear fEnter.
*
*/

void FOneOverOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	if (pFStack->X != 0.0) {
		pFStack->X = 1.0 / pFStack->X;
	}
	else
		ErrorBox("Division by zero");
}


/*** FAddOp -- (X + Y) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - Add Y to X, move stack down, set fEnter to FALSE
*
*/

void FAddOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X += pFStack->Y;
	FDropY(pFStack);
}


/*** FSubtractOp -- (Y - X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Subtract X from Y, move stack down, set fEnter to FALSE.
* WARNINGS -
* EFFECTS  -
*
*/

void FSubtractOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X = pFStack->Y - pFStack->X;
	FDropY(pFStack);
}


/*** FUnaryAddOp -- (X + 1) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - Add 1 to X, set fEnter to FALSE
*
*/

void FUnaryAddOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X += 1.0;
}


/*** FUnarySubtractOp -- (X - 1) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Subtract 1 from X, set fEnter to FALSE
* WARNINGS -
* EFFECTS  -
*
*/

void FUnarySubtractOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X -= 1.0;
}


/*** FMultiplyOp -- (X * Y) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - X * Y to X, move stack down, set fEnter to FALSE
*
*/

void FMultiplyOp(PFSTACK pFStack)
{
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X *= pFStack->Y;
	FDropY(pFStack);
}


/*** ConvertXToRadians
* INPUTS	pFStack
*			usFloatAngleType
*/
void ConvertXToRadians(PFSTACK pFStack, USHORT usFloatAngleType) {
	switch (usFloatAngleType) {
	case FLOAT_ANGLE_DEG:
		pFStack->X = pFStack->X * RAD_PER_DEG;
		break;
		
	case FLOAT_ANGLE_RAD:
		// No conversion necesary.
		break;
		
	case FLOAT_ANGLE_GRAD:
		pFStack->X = pFStack->X * RAD_PER_GRAD;
		break;
		
	default:
		DebugPrintf("ConvertXToRadians: Bogus usFloatAngleType: %u\n",
			usFloatAngleType);
		break;
	}
}


/*** ConvertXFromRadians
* INPUTS	pFStack
*			usFloatAngleType
*/
void ConvertXFromRadians(PFSTACK pFStack, USHORT usFloatAngleType) {
	switch (usFloatAngleType) {
	case FLOAT_ANGLE_DEG:
		pFStack->X = pFStack->X * DEG_PER_RAD;
		break;
		
	case FLOAT_ANGLE_RAD:
		// No conversion necesary.
		break;
		
	case FLOAT_ANGLE_GRAD:
		pFStack->X = pFStack->X * GRAD_PER_RAD;
		break;
		
	default:
		DebugPrintf("ConvertXToRadians: Bogus usFloatAngleType: %u\n",
			usFloatAngleType);
		break;
	}
	
}


/*** FArcSinOp
*
* INPUTS   pFStack
*
*/
void FArcSinOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	if (pFStack->X < -1.0 || pFStack->X > 1.0) {
		ErrorBox("arc sin domain is [-1.0..1.0]");
	}
	else {
		pFStack->X = asin(pFStack->X);
		ConvertXFromRadians(pFStack, usFloatAngleType);
	}
}


/*** FArcCosOp
*
* INPUTS   pFStack
*
*/
void FArcCosOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	if (pFStack->X < -1.0 || pFStack->X > 1.0) {
		ErrorBox("arc cos domain is [-1.0..1.0]");
	}
	else {
		pFStack->X = acos(pFStack->X);
		ConvertXFromRadians(pFStack, usFloatAngleType);
	}
}


/*** FArcTanOp
*
* INPUTS   pFStack
*
*/
void FArcTanOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	pFStack->X = atan(pFStack->X);
	ConvertXFromRadians(pFStack, usFloatAngleType);
}


/*** FSinOp
*
* INPUTS   pFStack
*
*/
void FSinOp(PFSTACK pFStack) {
	if (fInverse) {
		FArcSinOp(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		ConvertXToRadians(pFStack, usFloatAngleType);
		
		pFStack->X = sin(pFStack->X);
	}
}


/*** FCosOp
*
* INPUTS   pFStack
*
*/
void FCosOp(PFSTACK pFStack) {
	if (fInverse) {
		FArcCosOp(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		ConvertXToRadians(pFStack, usFloatAngleType);
		
		pFStack->X = cos(pFStack->X);
	}
}


/*** FTanOp
*
* INPUTS   pFStack
*
*/
void FTanOp(PFSTACK pFStack) {
	if (fInverse) {
		FArcTanOp(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		ConvertXToRadians(pFStack, usFloatAngleType);
		
		pFStack->X = tan(pFStack->X);
	}
}


/*** FExp10Op
*
* INPUTS   pFStack
*
*/
void FExp10Op(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	pFStack->X = pow(10, pFStack->X);
}


/*** FExpOp
*
* INPUTS   pFStack
* SUMMARY  e^x
*
*/
void FExpOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	pFStack->X = exp(pFStack->X);
}


/*** FLogOp
*
* INPUTS   pFStack
*
*/
void FLogOp(PFSTACK pFStack) {
	if (fInverse) {
		FExp10Op(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		if (pFStack->X < 0.0)
			ErrorBox("Log(negative number) is undefined");
		else
			pFStack->X = log10(pFStack->X);
	}
}


/*** FLnOp
*
* INPUTS   pFStack
*
*/
void FLnOp(PFSTACK pFStack) {
	if (fInverse) {
		FExpOp(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		if (pFStack->X < 0.0)
			ErrorBox("Ln(negative number) is undefined");
		else
			pFStack->X = log(pFStack->X);
	}
}


/*** FSquareOp
*
* INPUTS   pFStack
* SUMMARY  x*x
*
*/
void FSquareOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	pFStack->X = pFStack->X * pFStack->X;
}


/*** FSqrtOp
*
* INPUTS   pFStack
*
*/
void FSqrtOp(PFSTACK pFStack) {
	if (fInverse) {
		FSquareOp(pFStack);
		fInverse = FALSE;
	}
	else {
		if (fEnter)
			FEnterOp(pFStack, pEntryString);
		
		if (pFStack->X >= 0.)
			pFStack->X = sqrt(pFStack->X);
		else
			ErrorBox("Cannot give Square Root of negative number.");
	}
}


/*** FYToXOp
*
* INPUTS   pFStack
*
*/
void FYToXOp(PFSTACK pFStack) {
	if (fEnter)
		FEnterOp(pFStack, pEntryString);
	
	if (fInverse) { 				// Compute X'th root of Y
		fInverse = FALSE;
		if (pFStack->X != 0.0) {
			pFStack->X = (1.0 / pFStack->X);
		}
		else {
			ErrorBox("Division by zero");
			return; 				// don't do the pow function
		}
	}
	
	if (0.0 == pFStack->Y) {
		if (0.0 == pFStack->X) {
			ErrorBox("0^0 is undefined");
			return;
		}
	}
	
	// BUGBUG: We maybe should do other input verification checks here.
	
	pFStack->X = pow(pFStack->Y, pFStack->X);
	FDropY(pFStack);
}


/*** FEnterOp -- Enter the number or Dup.
*
* ENTRY    - pFStack
*			 pEntryString
* EXIT	   - none
* SYNOPSIS - First, move the stack up.	Then, if fEnter, we should turn
*			 off fEnter and convert the entry string to float.
*
*/

void FEnterOp(PFSTACK pFStack, PUCHAR pEntryString)
{
	if (fEnter) {
		fEnter = FALSE;
		pFStack->X = atof(pEntryString);
	}
	else {
		pFStack->T = pFStack->Z;
		pFStack->Z = pFStack->Y;
		pFStack->Y = pFStack->X;
	}
}


/*** FPiOp -- Enter pi in X
*
* ENTER    - pFStack
*
*/
void FPiOp(PFSTACK pFStack)
{
	FEnterOp(pFStack, pEntryString);
	pFStack->X = PI_VALUE;
}


/*** FDropOp -- Drop the value in X.
*
* ENTRY    - pFStack
*
*/

void FDropOp(PFSTACK pFStack)
{
	pFStack->X = pFStack->Y;
	pFStack->Y = pFStack->Z;
	pFStack->Z = pFStack->T;
	pFStack->T = 0;
	fEnter = FALSE;
}


/*** FBackSpaceOp -- removes the right-most digit from X.
*
* ENTRY    - pEntryString
*			 fEnter
* EXIT	   - none
* SYNOPSIS - Removes the right-most digit from X, depending on the current
*			 base and integer type.
*
*/

void FBackSpaceOp(PUCHAR pEntryString, BOOL fEnter)
{
	size_t Length;
	
	if (fEnter && (Length = strlen(pEntryString))) {
		pEntryString[Length - 1] = '\0';
	}
}


/*** FClearStack -- Clears the stack to all Zero's.
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Puts a 0 in the entire stack.
*
*/

void FClearStack(PFSTACK pFStack)
{
	pFStack->X = 0;
	pFStack->Y = 0;
	pFStack->Z = 0;
	pFStack->T = 0;
}


/*** CalcIt -- Decide which operation to perform
*
* ENTRY    - pFStack
*			 dwOperation = the operation to perform
*			 hwnd = handle to calculator window
* EXIT	   - none
* SYNOPSIS - Delegates authority to one of the Op routines.
*
*/

void FCalcIt(PFSTACK pFStack, DWORD dwOperation, HWND hwnd)
{
	BOOLEAN		SwitchMode = FALSE;
	
	//	  DebugPrintf("CalcIt(%u)\n", dwOperation);
	
	switch(dwOperation) {
	case ENTER: 			FEnterOp(pFStack, pEntryString);
		break;
	case EEX_KEY:			FEExOp(pEntryString, fEnter);
		break;
	case SWAP_KEY:			FSwapOp(pFStack);
		break;
	case PLUS_KEY:			FAddOp(pFStack);
		break;
	case MINUS_KEY: 		FSubtractOp(pFStack);
		break;
	case UNARY_PLUS_KEY:	FUnaryAddOp(pFStack);
		break;
	case UNARY_MINUS_KEY:	FUnarySubtractOp(pFStack);
		break;
	case MULT_KEY:			FMultiplyOp(pFStack);
		break;
	case DIV_KEY:			FDivideOp(pFStack);
		break;
	case ONE_OVER_KEY:		FOneOverOp(pFStack);
		break;
	case NEGATE_KEY:		FNegOp(pFStack, pEntryString, fEnter);
		break;
	case BACKSPACE_KEY: 	FBackSpaceOp(pEntryString, fEnter);
		break;
	case DELETE_KEY:		FDropOp(pFStack);
		break;
	case ROLL_DOWN_KEY: 	FRollDownOp(pFStack);
		break;
	case INVERSE_KEY:		fInverse = !fInverse;
		break;
	case PI_KEY:			FPiOp(pFStack);
		break;
	case SIN_KEY:			FSinOp(pFStack);
		break;
	case COS_KEY:			FCosOp(pFStack);
		break;
	case TAN_KEY:			FTanOp(pFStack);
		break;
	case LOG_KEY:			FLogOp(pFStack);
		break;
	case LN_KEY:			FLnOp(pFStack);
		break;
	case SQRT_KEY:			FSqrtOp(pFStack);
		break;
	case YTOX_KEY:			FYToXOp(pFStack);
		break;
		
	case IDM_BINARY:		ChangeBase(2);
		SwitchMode = TRUE;
		break;
	case IDM_OCTAL: 		ChangeBase(8);
		SwitchMode = TRUE;
		break;
	case IDM_DECIMAL:		ChangeBase(10);
		SwitchMode = TRUE;
		break;
	case IDM_HEXADECIMAL:	ChangeBase(16);
		SwitchMode = TRUE;
		break;
		
	case IDM_DISPLAY_AUTO:
	case IDM_DISPLAY_FIX_0:
	case IDM_DISPLAY_FIX_1:
	case IDM_DISPLAY_FIX_2:
	case IDM_DISPLAY_FIX_3:
	case IDM_DISPLAY_FIX_4:
	case IDM_DISPLAY_FIX_5:
	case IDM_DISPLAY_FIX_6:
	case IDM_DISPLAY_FIX_7:
	case IDM_DISPLAY_FIX_8:
	case IDM_DISPLAY_FIX_9:
	case IDM_DISPLAY_FIX_10:
	case IDM_DISPLAY_FIX_11:
	case IDM_DISPLAY_FIX_12:
	case IDM_DISPLAY_FIX_13:
	case IDM_DISPLAY_FIX_14:
	case IDM_DISPLAY_FIX_15:
		
	case IDM_DISPLAY_SCI_0:
	case IDM_DISPLAY_SCI_1:
	case IDM_DISPLAY_SCI_2:
	case IDM_DISPLAY_SCI_3:
	case IDM_DISPLAY_SCI_4:
	case IDM_DISPLAY_SCI_5:
	case IDM_DISPLAY_SCI_6:
	case IDM_DISPLAY_SCI_7:
	case IDM_DISPLAY_SCI_8:
	case IDM_DISPLAY_SCI_9:
	case IDM_DISPLAY_SCI_10:
	case IDM_DISPLAY_SCI_11:
	case IDM_DISPLAY_SCI_12:
	case IDM_DISPLAY_SCI_13:
	case IDM_DISPLAY_SCI_14:
	case IDM_DISPLAY_SCI_15:
		
	case IDM_DISPLAY_ENG_0:
	case IDM_DISPLAY_ENG_1:
	case IDM_DISPLAY_ENG_2:
	case IDM_DISPLAY_ENG_3:
	case IDM_DISPLAY_ENG_4:
	case IDM_DISPLAY_ENG_5:
	case IDM_DISPLAY_ENG_6:
	case IDM_DISPLAY_ENG_7:
	case IDM_DISPLAY_ENG_8:
	case IDM_DISPLAY_ENG_9:
	case IDM_DISPLAY_ENG_10:
	case IDM_DISPLAY_ENG_11:
	case IDM_DISPLAY_ENG_12:
	case IDM_DISPLAY_ENG_13:
	case IDM_DISPLAY_ENG_14:
	case IDM_DISPLAY_ENG_15:
		break;
		
	case IDM_TRIG_DEGREE:
		usFloatAngleType = FLOAT_ANGLE_DEG;
		break;
	case IDM_TRIG_RADIAN:
		usFloatAngleType = FLOAT_ANGLE_RAD;
		break;
	case IDM_TRIG_GRAD:
		usFloatAngleType = FLOAT_ANGLE_GRAD;
		break;
		
	case IDM_COPY_X:
		// If we are in entry mode, string copy from the entry string
		// otherwise, convert the value in X.
		
		if (fEnter)
			StrClipboardCopy(pEntryString);
		else
			FClipboardCopy(FStack.X, usSignificantDigits, usFloatDisplayMode);
		break;
		
	case IDM_COPY_Y:
		FClipboardCopy(FStack.Y, usSignificantDigits, usFloatDisplayMode);
		break;
		
	case IDM_COPY_Z:
		FClipboardCopy(FStack.Z, usSignificantDigits, usFloatDisplayMode);
		break;
		
	case IDM_COPY_T:
		FClipboardCopy(FStack.T, usSignificantDigits, usFloatDisplayMode);
		break;
		
	case IDM_PASTE_X:
		if (fEnter)
			StrClipboardPaste(pEntryString);
		else
			FClipboardPaste(&FStack.X);
		break;
		
	case IDM_PASTE_Y:
		FClipboardPaste(&FStack.Y);
		break;
		
	case IDM_PASTE_Z:
		FClipboardPaste(&FStack.Z);
		break;
		
	case IDM_PASTE_T:
		FClipboardPaste(&FStack.T);
		break;
		
	case IDM_HELP_ABOUT:
		DialogBox(GetModuleHandle(NULL), "AboutBox", hwnd,
			About);
		break;
		
	case IDM_HELP_HELP_KEYS:
		fHelpKeysMode = TRUE;	// Get us into HelpKeysMode
		CheckMenuItem(GetMenu(hwnd), IDM_HELP_HELP_KEYS, MF_CHECKED);
		break;
		
	default:
		DebugPrintf("Don't recognize %u operation\n", dwOperation);
		ErrorFlag = TRUE;
	}
		

	//
	//  Switch calculator modes if the user stepped on the base menu.
	//  We've already "switched" bases.
	//
		
	if (SwitchMode) {
		bQuit = FALSE;
		usCalcMode = CALC_MODE_BINARY;
		PostMessage(hwnd, WM_COMMAND, IDM_CHANGE_MODE, 0);
		return; 	// don't redraw the stack display
	}

	RedrawDisplay(hDisplay);
}


/*** FHelpKeys -- Give a help popup on whatever key was pressed
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

BOOL FHelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
                UCHAR* szStringBuffer,
                DWORD cbStringBuffer )
{
    BOOL fRet = FALSE;
	DWORD StringSize;
	
	DebugPrintf("HelpKeys(%u)\n", dwOperation);
	
	if (IsBaseDigit((UCHAR)dwOperation, 10)) {
		dwOperation = ID_DIGIT;
	}
	
	if (dwOperation == ONE_OVER_KEY)
		dwOperation = SUBSTITUTE_ONE_OVER_KEY;
	else if (dwOperation == YTOX_KEY)
		dwOperation = SUBSTITUTE_YTOX_KEY;
	else if (dwOperation == LN_KEY)
		dwOperation = SUBSTITUTE_LN_KEY;
	else if (dwOperation == DECIMAL_KEY)
		dwOperation = SUBSTITUTE_DECIMAL_KEY;
    else if ( dwOperation == IDD_DISPLAY_FLOAT )
    {
        dwOperation = IDS_FLOAT;
    }
	
	switch(dwOperation) 
    {	
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
			szStringBuffer, cbStringBuffer )) == 0) 
        {
			strcpy_s(szStringBuffer, cbStringBuffer,
				"No help available for this key");
			StringSize = (DWORD)strlen(szStringBuffer);
		}
        else
        {
            fRet =  TRUE;
        }
		
//		MessageBox(hwnd, szStringBuffer, "RPN Help Keys Mode",
//			MB_OK | MB_ICONINFORMATION);
		break;
		
	}

    return fRet;
}


/*
* FNumTest
*
* INPUTS:  pEntryString = current entry string
*		   Char = character to add to entry string
*
* OUTPUTS: Returns TRUE if we have can add this character to the entry
*		   string and still have a valid FNUMBER.
*
*
* SUMMARY: A valid floating point number, X, follows these rules:
*		   1. mantissa digits <= DBL_DIG digits (currently 15)
*		   2. exponetnt digits < 3
*		   3. can be converted to a double (no funky error values allowed)
*		   4. Fits this reg-exp: [-][num][.][num][E[-]<num>]
*
*		   If the character is validated, we add the character to the string
*		   and return TRUE.
*		   Assumes that there is room for another character in the string.
*/

BOOL FNumTest(PUCHAR pEntryString, UCHAR Char) {
	size_t Length;
	PUCHAR pTemp;
	DWORD DotCount = 0;
	BOOL fValid = FALSE;
	size_t MantDigits = 0, ExpDigits = 0;
	PUCHAR pExp = NULL;
	FNUMBER X;
	
	
	//	  DebugPrintf("FNumTest: [%s] '%c'\n", pEntryString, Char);
	
	if (strlen(pEntryString) >= DISPLAY_WIDTH) {
		DebugPrintf("Invalid: String too long\n");
		return(FALSE);
	}
	
	// Add the character to the string.
	pEntryString[Length = strlen(pEntryString)] = Char;
	pEntryString[Length+1] = '\0';
	
	
	// Verify that the string fits our idea of a floating point number
	pTemp = pEntryString;
	if (*pTemp == '-')
		pTemp++;
	
	while (isdigit(*pTemp)) {
		pTemp++;
		MantDigits++;
		fValid = TRUE;
	}
	
	if (*pTemp == '.') {
		pTemp++;
		fValid = TRUE;
		while (isdigit(*pTemp)) {
			pTemp++;
			MantDigits++;
		}
	}
	
	if (*pTemp == 'E') {
		pTemp++;
		pExp = pTemp;
		fValid = TRUE;
		
		if (*pTemp == '-')
			pTemp++;
		
		while (isdigit(*pTemp)) {
			pTemp++;
			ExpDigits++;
		}
	}
	
	
	if (! fValid) {
		DebugPrintf("Invalid: %s at %u\n", pEntryString, pTemp-pEntryString);
	}
	else { // Other validation tests...
		// Verify the mantissa length <= FNUM_MANT_DIG
		if (MantDigits > FNUM_MANT_DIG) {
			fValid = FALSE;
			DebugPrintf("Invalid: %s mantissa too large\n", pEntryString);
		}
		
		// Check for more than one '.'
		pTemp = pEntryString;
		while (*pTemp != '\0')
			if (*(pTemp++) == '.')
				DotCount++;
			if (DotCount > 1) {
				fValid = FALSE;
				DebugPrintf("Invlaid: %s too many decimals\n", pEntryString);
			}
			
			if (pExp) {
				if (ExpDigits > FNUM_EXP_DIG) {
					fValid = FALSE;
					DebugPrintf("Invalid: %s exponent too many digits\n",
						pEntryString);
				}
			}
			
			// Verify that the value is in range
			// BUGBUG: Oh, yeah?  How you gonna do that?
			// How about this: do atof(pEntryString) and find out what you get?
			X = atof(pEntryString);
			
			if (X == 0) {	// suspicious... should it be zero?
				if (strpbrk(pEntryString, "123456789")) {
					fValid = FALSE;
					DebugPrintf("Invalid: %s number conversion yields 0\n");
				}
			}
			
#if ! FLOAT_BUG
			{
				int FNumClass;
				
				// Ask floating point package what we have here...
				switch (FNumClass = _fpclass(X)) {
				case _FPCLASS_SNAN:
				case _FPCLASS_QNAN:
					DebugPrintf("Invalid: %s Not A Number\n", pEntryString);
					fValid = FALSE;
					break;
					
				case _FPCLASS_NINF:
					DebugPrintf("Invalid: %s Negative Infinity\n", pEntryString);
					fValid = FALSE;
					break;
					
				case _FPCLASS_PINF:
					DebugPrintf("Invalid: %s Positive Infinity\n", pEntryString);
					fValid = FALSE;
					break;
					
				case _FPCLASS_NN:
				case _FPCLASS_ND:
				case _FPCLASS_NZ:
				case _FPCLASS_PZ:
				case _FPCLASS_PD:
				case _FPCLASS_PN:
					//				  DebugPrintf("Valid number: %s!\n", pEntryString);
					break;
					
				default:
					DebugPrintf("_fpclass --> 0x%x\n", FNumClass);
				}
			}
#endif
	}
	
	if (! fValid) {
		// remove the character from the entry string
		pEntryString[Length] = '\0';
		return(FALSE);
	}
	else
		return(TRUE);
}


/*
* FClipboardCopy
*
* INPUTS:  fNumber = number to copy
*		   usSignificantDigits = digits to dispaly
*		   usFloatDisplayMode = FIX, SCI, ENG
* OUTPUTS: TRUE on success
*
* SUMMARY:
*	   Copy the number to the clipboard in the current radix and number type.
*	   Copies as CF_TEXT format.
*
*/
BOOL FClipboardCopy(FNUMBER fNumber, USHORT usSignificantDigits,
					USHORT usFloatDisplayMode) {
	HGLOBAL hClipboardMem;
	PUCHAR	pchClipboard;
	UCHAR	StrNum[DISPLAY_WIDTH + 1];
	size_t	dwLength;
	
	
	FToA(fNumber, StrNum, sizeof(StrNum), usSignificantDigits, usFloatDisplayMode, FALSE);
	
	hClipboardMem = GlobalAlloc(GHND | GMEM_DDESHARE,
		(dwLength = strlen(StrNum)) + 1);
	
	if (! hClipboardMem) {
		DebugPrintf("Clipboard Copy couldn't allocate %u bytes of memory\n",
			dwLength);
		return(FALSE);;
	}
	
	if (!(pchClipboard = GlobalLock(hClipboardMem))) {
		DebugPrintf("Clipboard Copy GlobalLock couldn't lock memory\n");
		GlobalFree(hClipboardMem);
		return(FALSE);
	}
	
	memcpy(pchClipboard, StrNum, dwLength + 1);
	
	GlobalUnlock(hClipboardMem);
	OpenClipboard(hwndDialog);
	
	EmptyClipboard();
	
	// Note: SetClipboardData implicitly frees hClipboardMem.
	SetClipboardData(CF_TEXT, hClipboardMem);
	
	CloseClipboard();
	return(TRUE);
}


/*
* FClipboardPaste
*
* INPUTS:  pfNumber -> number to paste to
* OUTPUTS: TRUE on success
*
* SUMMARY:
*	   Copy the number from the clipboard to the number pointer.
*	   Only pastes the CF_TEXT clipboard format.
*
*/
BOOL FClipboardPaste(FNUMBER * pfNumber) {
	HGLOBAL hClipboardMem;
	PUCHAR	pClipboardMem;
	FNUMBER fNumber;
	BOOL	bReturn = TRUE;
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
		// window copy function puts there.)
		pTemp = pClipboardMem;
		while ((*pTemp > MAX_CONTROL_CHAR) && (*pTemp != ' '))
			pTemp++;
		
		*pTemp = '\0';
		
		if (FStrNumTest(pClipboardMem, &fNumber)) {
			// valid number in dwNumber
			*pfNumber = fNumber;
		}
		else {	// warn user that the clipboard doesn't contain a valid number
				ErrorBox("The clipboard does not contain a\n\
					valid floating point number.");
				bReturn = FALSE;
		}
		GlobalUnlock(hClipboardMem);
	}
	else {
	ErrorBox("The clipboard does not contain a number\n\
		in the correct format.");
	bReturn = FALSE;
	}
	
	CloseClipboard();
	return(bReturn);
}


/*
* FStrNumTest
*
* INPUTS:  lpNumStr -> string containing potential number
*		   pfNumber -> number returned
*
* OUTPUTS: reutrns TRUE if the string represents a valid number in the current
*		   radix and usIntegerType.
*
* SUMMARY: Builds the number, a character at a time, repeatedly calling
*		   FNumTest to validate each character.
*/
BOOL FStrNumTest(PUCHAR lpNumStr, FNUMBER * pfNumber) {
	DWORD dwNumber = 0;
	UCHAR fNumberStr[DISPLAY_WIDTH + 1];
	PUCHAR pNumberStr = fNumberStr;
	size_t dwStrLength = strlen(lpNumStr);
	BOOL fReturn = TRUE;
	DWORD i;
	
	
	fNumberStr[0] = '\0';
	for (i = 0; i < dwStrLength; i++) {
		if ((lpNumStr[i] == '-') || (lpNumStr[i] == '+') ||
			(toupper(lpNumStr[i])) == 'E') {
			// don't validate number here, it will fail.
			fNumberStr[i] = lpNumStr[i];
			fNumberStr[i+1] = '\0';
			continue;
		}
		
		// valid float digit (or .) ?
		if (IsFloatChar(lpNumStr[i])) {
			if (! FNumTest(pNumberStr, lpNumStr[i])) {
				fReturn = FALSE;
				break;
			}
		}
		else {
			fReturn = FALSE;
			break;
		}
	}
	
	if (fReturn)
		*pfNumber = atof(pNumberStr);
	
	return(fReturn);
}


/*
* StrClipboardCopy
*
* INPUTS:  pString = string to place in clipboard
* OUTPUTS: TRUE on success
*
* SUMMARY:
*	   Copy the string to the clipboard.
*	   Copies as CF_TEXT format.
*
*/
BOOL StrClipboardCopy(PUCHAR pString) {
	HGLOBAL hClipboardMem;
	PUCHAR	pchClipboard;
	size_t	dwLength;
	
	
	hClipboardMem = GlobalAlloc(GHND | GMEM_DDESHARE,
		(dwLength = strlen(pString)) + 1);
	
	if (! hClipboardMem) {
		DebugPrintf("Clipboard Copy couldn't allocate %u bytes of memory\n",
			dwLength);
		return(FALSE);;
	}
	
	if (!(pchClipboard = GlobalLock(hClipboardMem))) {
		DebugPrintf("Clipboard Copy GlobalLock couldn't lock memory\n");
		GlobalFree(hClipboardMem);
		return(FALSE);
	}
	
	memcpy(pchClipboard, pString, dwLength + 1);
	
	GlobalUnlock(hClipboardMem);
	OpenClipboard(hwndDialog);
	
	EmptyClipboard();
	
	// Note: SetClipboardData implicitly frees hClipboardMem.
	SetClipboardData(CF_TEXT, hClipboardMem);
	
	CloseClipboard();
	return(TRUE);
}


/*
* StrClipboardPaste
*
* INPUTS:  pString -> string to paste to
* OUTPUTS: TRUE on success
*
* SUMMARY:
*	   Copy the number string from the clipboard to the string pointer.
*	   The string must be valid as a floating point number.
*	   Will only paste up to DISPLAY_WIDTH characters.
*	   Only pastes the CF_TEXT clipboard format.
*
*/
BOOL StrClipboardPaste(PUCHAR pString) {
	HGLOBAL hClipboardMem;
	PUCHAR	pClipboardMem;
	FNUMBER fNumber;
	BOOL	bReturn = TRUE;
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
		// window copy function puts there.)
		pTemp = pClipboardMem;
		while (*pTemp > MAX_CONTROL_CHAR)
			pTemp++;
		
		*pTemp = '\0';
		
		if (FStrNumTest(pClipboardMem, &fNumber)) {
			// valid number in dwNumber
			memcpy(pString, pClipboardMem, DISPLAY_WIDTH);
			pString[DISPLAY_WIDTH] = '\0';
		}
		else {	// warn user that the clipboard doesn't contain a valid number
				ErrorBox("The clipboard does not contain a\n\
					valid floating point number.");
				bReturn = FALSE;
		}
		GlobalUnlock(hClipboardMem);
	}
	else {
	ErrorBox("The clipboard does not contain a number\n\
		in the correct format.");
	bReturn = FALSE;
	}
	
	CloseClipboard();
	return(bReturn);
}
