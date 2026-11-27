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
static void FCalcIt(DWORD dwOperation, HWND hwnd);
BOOL FHelpKeys( DWORD dwOperation, HANDLE hInst, HWND hwnd ,
                UCHAR* szStringBuffer, DWORD cbStringBuffer );
extern void DebugPrintf(const char * format, ...);
static int IsFloatChar(UCHAR ch);
static void FEnterOp(FLOAT_STATE *state, BOOL *entering);
static BOOL FNumTest(PUCHAR pEntryString, UCHAR Char);
extern void ErrorBox(const char * format, ...);
BOOL FClipboardCopy(FNUMBER fNumber, USHORT usSignificantDigits,
					USHORT usFloatDisplayMode);
BOOL FClipboardPaste(FNUMBER * pfNumber);
BOOL FStrNumTest(PUCHAR lpNumStr, FNUMBER * pfNumber);
BOOL StrClipboardCopy(PUCHAR pString);
BOOL StrClipboardPaste(PUCHAR pString);


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
		FCalcIt((DWORD)wParam, hwnd);
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

static int IsFloatChar(UCHAR ch)
{
	if (isdigit(ch))
		return(TRUE);
	if (ch == '.')
		return(TRUE);
	
	return(FALSE);
}


// While editing, change the exponent sign if present, otherwise the mantissa.
static BOOL FNegOp(FLOAT_STATE *state, BOOL entering)
{
	if (entering) {
		char *entry = (char *)state->Entry;
		char *exponent = strchr(entry, 'E');
		char *suffix = exponent ? exponent + 1 : entry;
		size_t bytesWithTerminator = strlen(suffix) + 1;
		if (*suffix == '-') {
			memmove(suffix, suffix + 1, bytesWithTerminator - 1);
		} else {
			// A full clipboard entry has no room to move its terminator right.
			if (strlen(entry) >= DISPLAY_WIDTH)
				return FALSE;
			memmove(suffix + 1, suffix, bytesWithTerminator);
			*suffix = '-';
		}
	} else {
		state->Stack.X = -state->Stack.X;
	}
	return TRUE;
}


// EEx toggles the exponent portion of an active entry. An empty
// mantissa starts at 1 so the user can enter a power of ten directly.
static BOOL FEExOp(PUCHAR pEntryString, BOOL fEnter) {
	PUCHAR pTemp;
	size_t Length;
	
	if (fEnter) {  // nop if not in entry mode
		if (pTemp = strchr(pEntryString, 'E')) {
			// already has an exponent, truncate it.
			*pTemp = '\0';
		}
		else { // Add an 'E' to the end of the string.
			Length = strlen(pEntryString);
			if (Length >= DISPLAY_WIDTH)
				return FALSE;
			if (!Length) {
				// if there is no mantissa, provide a 1
				pEntryString[Length++] = '1';
			}
			
			pEntryString[Length++] = 'E';
			pEntryString[Length] = '\0';
		}
	}
	return TRUE;
}


/*** FDivideOp -- (Y / X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Divide Y by X into X.	 Move stack down, clear fEnter.
*
*/

static void FDivideOp(PFSTACK pFStack, CALCULATOR_REPORT_ERROR reportError, void *context)
{
	if (pFStack->X != 0.0) {
		pFStack->X = pFStack->Y / pFStack->X;
		DropFloatY(pFStack);
	}
	else
		reportError(context, "Division by zero");
}


/*** FOneOverOp -- (1 / X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Replace X with its reciprocal without dropping Y.
*
*/

static void FOneOverOp(PFSTACK pFStack, CALCULATOR_REPORT_ERROR reportError, void *context)
{
	if (pFStack->X != 0.0) {
		pFStack->X = 1.0 / pFStack->X;
	}
	else
		reportError(context, "Division by zero");
}


/*** FAddOp -- (X + Y) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - Add Y to X and drop Y.
*
*/

static void FAddOp(PFSTACK pFStack)
{
	pFStack->X += pFStack->Y;
	DropFloatY(pFStack);
}


/*** FSubtractOp -- (Y - X) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Subtract X from Y and drop Y.
* WARNINGS -
* EFFECTS  -
*
*/

static void FSubtractOp(PFSTACK pFStack)
{
	pFStack->X = pFStack->Y - pFStack->X;
	DropFloatY(pFStack);
}


/*** FUnaryAddOp -- (X + 1) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - Add 1 to X.
*
*/

static void FUnaryAddOp(PFSTACK pFStack)
{
	pFStack->X += 1.0;
}


/*** FUnarySubtractOp -- (X - 1) --> X
*
* ENTRY    - pFStack
* EXIT	   - none
* SYNOPSIS - Subtract 1 from X.
* WARNINGS -
* EFFECTS  -
*
*/

static void FUnarySubtractOp(PFSTACK pFStack)
{
	pFStack->X -= 1.0;
}


/*** FMultiplyOp -- (X * Y) --> X
*
* ENTRY    - pFStack
* EXIT	   -
* SYNOPSIS - X * Y to X, move stack down, set fEnter to FALSE
*
*/

static void FMultiplyOp(PFSTACK pFStack)
{
	pFStack->X *= pFStack->Y;
	DropFloatY(pFStack);
}


/*** ConvertXToRadians
* INPUTS	pFStack
*			usFloatAngleType
*/
static void ConvertXToRadians(PFSTACK pFStack, USHORT usFloatAngleType) {
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
static void ConvertXFromRadians(PFSTACK pFStack, USHORT usFloatAngleType) {
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
static void FArcSinOp(PFSTACK pFStack, USHORT angleType, CALCULATOR_REPORT_ERROR reportError, void *context) {
	if (pFStack->X < -1.0 || pFStack->X > 1.0) {
		reportError(context, "arc sin domain is [-1.0..1.0]");
	}
	else {
		pFStack->X = asin(pFStack->X);
		ConvertXFromRadians(pFStack, angleType);
	}
}


/*** FArcCosOp
*
* INPUTS   pFStack
*
*/
static void FArcCosOp(PFSTACK pFStack, USHORT angleType, CALCULATOR_REPORT_ERROR reportError, void *context) {
	if (pFStack->X < -1.0 || pFStack->X > 1.0) {
		reportError(context, "arc cos domain is [-1.0..1.0]");
	}
	else {
		pFStack->X = acos(pFStack->X);
		ConvertXFromRadians(pFStack, angleType);
	}
}


/*** FArcTanOp
*
* INPUTS   pFStack
*
*/
static void FArcTanOp(PFSTACK pFStack, USHORT angleType) {
	pFStack->X = atan(pFStack->X);
	ConvertXFromRadians(pFStack, angleType);
}


/*** FSinOp
*
* INPUTS   pFStack
*
*/
static void FSinOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FArcSinOp(pFStack, state->AngleType, reportError, context);
		state->Inverse = FALSE;
	}
	else {
		ConvertXToRadians(pFStack, state->AngleType);
		
		pFStack->X = sin(pFStack->X);
	}
}


/*** FCosOp
*
* INPUTS   pFStack
*
*/
static void FCosOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FArcCosOp(pFStack, state->AngleType, reportError, context);
		state->Inverse = FALSE;
	}
	else {
		ConvertXToRadians(pFStack, state->AngleType);
		
		pFStack->X = cos(pFStack->X);
	}
}


/*** FTanOp
*
* INPUTS   pFStack
*
*/
static void FTanOp(FLOAT_STATE *state) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FArcTanOp(pFStack, state->AngleType);
		state->Inverse = FALSE;
	}
	else {
		ConvertXToRadians(pFStack, state->AngleType);
		
		pFStack->X = tan(pFStack->X);
	}
}


/*** FExp10Op
*
* INPUTS   pFStack
*
*/
static void FExp10Op(PFSTACK pFStack) {
	pFStack->X = pow(10, pFStack->X);
}


/*** FExpOp
*
* INPUTS   pFStack
* SUMMARY  e^x
*
*/
static void FExpOp(PFSTACK pFStack) {
	pFStack->X = exp(pFStack->X);
}


/*** FLogOp
*
* INPUTS   pFStack
*
*/
static void FLogOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FExp10Op(pFStack);
		state->Inverse = FALSE;
	}
	else {
		if (pFStack->X < 0.0)
			reportError(context, "Log(negative number) is undefined");
		else
			pFStack->X = log10(pFStack->X);
	}
}


/*** FLnOp
*
* INPUTS   pFStack
*
*/
static void FLnOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FExpOp(pFStack);
		state->Inverse = FALSE;
	}
	else {
		if (pFStack->X < 0.0)
			reportError(context, "Ln(negative number) is undefined");
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
static void FSquareOp(PFSTACK pFStack) {
	pFStack->X = pFStack->X * pFStack->X;
}


/*** FSqrtOp
*
* INPUTS   pFStack
*
*/
static void FSqrtOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) {
		FSquareOp(pFStack);
		state->Inverse = FALSE;
	}
	else {
		if (pFStack->X >= 0.)
			pFStack->X = sqrt(pFStack->X);
		else
			reportError(context, "Cannot give Square Root of negative number.");
	}
}


/*** FYToXOp
*
* INPUTS   pFStack
*
*/
static void FYToXOp(FLOAT_STATE *state, CALCULATOR_REPORT_ERROR reportError, void *context) {
	PFSTACK pFStack = &state->Stack;
	if (state->Inverse) { // Inverse power uses the reciprocal exponent.
		state->Inverse = FALSE;
		if (pFStack->X != 0.0) {
			pFStack->X = (1.0 / pFStack->X);
		}
		else {
			reportError(context, "Division by zero");
			return; 				// don't do the pow function
		}
	}
	
	if (0.0 == pFStack->Y) {
		if (0.0 == pFStack->X) {
			reportError(context, "0^0 is undefined");
			return;
		}
	}
	
	// BUGBUG: We maybe should do other input verification checks here.
	
	pFStack->X = pow(pFStack->Y, pFStack->X);
	DropFloatY(pFStack);
}


// Enter commits an active entry in place; otherwise it duplicates X.
static void FEnterOp(FLOAT_STATE *state, BOOL *entering)
{
	PFSTACK pFStack = &state->Stack;
	if (*entering) {
		*entering = FALSE;
		pFStack->X = atof((const char *)state->Entry);
	}
	else {
		DuplicateFloatX(pFStack);
	}
}


static void CommitFloatEntry(FLOAT_STATE *state, BOOL *entering)
{
	if (*entering)
		FEnterOp(state, entering);
}


/*** FDropOp -- Drop the value in X.
*
* ENTRY    - pFStack
*
*/

/*** FBackSpaceOp -- removes the right-most digit from X.
*
* ENTRY    - pEntryString
*			 fEnter
* EXIT	   - none
* SYNOPSIS - Removes the last character of an active float entry.
*
*/

static void FBackSpaceOp(PUCHAR pEntryString, BOOL fEnter)
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


CALCULATOR_RESULT FloatCommand(FLOAT_STATE *state, DWORD dwOperation, BOOL *entering,
                              BOOL *error, CALCULATOR_REPORT_ERROR reportError, void *context)
{
	PFSTACK pFStack = &state->Stack;
	UCHAR ch = (UCHAR)dwOperation;

	// The first digit lifts the stack. Until commit, editing changes only
	// the entry string; X still contains the previous numeric value.
	if (IsFloatChar(ch)) {
		if (!*entering) {
			FEnterOp(state, entering);
			state->Entry[0] = '\0';
			*entering = TRUE;
		}
		return FNumTest(state->Entry, ch) ? CALCULATOR_REDRAW : CALCULATOR_ENTRY_REJECTED;
	}
	
	switch(dwOperation) {
	case ENTER: 			FEnterOp(state, entering);
		break;
	case EEX_KEY:			return FEExOp(state->Entry, *entering) ? CALCULATOR_REDRAW : CALCULATOR_ENTRY_REJECTED;
	case SWAP_KEY:			CommitFloatEntry(state, entering);
		SwapFloatRegisters(pFStack);
		break;
	case PLUS_KEY:			CommitFloatEntry(state, entering);
		FAddOp(pFStack);
		break;
	case MINUS_KEY: 		CommitFloatEntry(state, entering);
		FSubtractOp(pFStack);
		break;
	case UNARY_PLUS_KEY:	CommitFloatEntry(state, entering);
		FUnaryAddOp(pFStack);
		break;
	case UNARY_MINUS_KEY:	CommitFloatEntry(state, entering);
		FUnarySubtractOp(pFStack);
		break;
	case MULT_KEY:			CommitFloatEntry(state, entering);
		FMultiplyOp(pFStack);
		break;
	case DIV_KEY:			CommitFloatEntry(state, entering);
		FDivideOp(pFStack, reportError, context);
		break;
	case ONE_OVER_KEY:		CommitFloatEntry(state, entering);
		FOneOverOp(pFStack, reportError, context);
		break;
	case NEGATE_KEY:		return FNegOp(state, *entering) ? CALCULATOR_REDRAW : CALCULATOR_ENTRY_REJECTED;
	case BACKSPACE_KEY: 	FBackSpaceOp(state->Entry, *entering);
		break;
	case DELETE_KEY:		DropFloatX(pFStack);
		*entering = FALSE;
		break;
	case ROLL_DOWN_KEY: 	CommitFloatEntry(state, entering);
		RollFloatRegisters(pFStack);
		break;
	case INVERSE_KEY:		state->Inverse = !state->Inverse;
		break;
	case PI_KEY:			FEnterOp(state, entering);
		pFStack->X = PI_VALUE;
		break;
	case SIN_KEY:			CommitFloatEntry(state, entering);
		FSinOp(state, reportError, context);
		break;
	case COS_KEY:			CommitFloatEntry(state, entering);
		FCosOp(state, reportError, context);
		break;
	case TAN_KEY:			CommitFloatEntry(state, entering);
		FTanOp(state);
		break;
	case LOG_KEY:			CommitFloatEntry(state, entering);
		FLogOp(state, reportError, context);
		break;
	case LN_KEY:			CommitFloatEntry(state, entering);
		FLnOp(state, reportError, context);
		break;
	case SQRT_KEY:			CommitFloatEntry(state, entering);
		FSqrtOp(state, reportError, context);
		break;
	case YTOX_KEY:			CommitFloatEntry(state, entering);
		FYToXOp(state, reportError, context);
		break;
		
	// Legacy display menu IDs are accepted but never changed float settings.
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
		state->AngleType = FLOAT_ANGLE_DEG;
		break;
	case IDM_TRIG_RADIAN:
		state->AngleType = FLOAT_ANGLE_RAD;
		break;
	case IDM_TRIG_GRAD:
		state->AngleType = FLOAT_ANGLE_GRAD;
		break;

	default:
		*error = TRUE;
		return CALCULATOR_UNKNOWN_COMMAND;
	}

	return CALCULATOR_REDRAW;
}

static void FCalcIt(DWORD dwOperation, HWND hwnd)
{
	FLOAT_STATE *state = &Calculator.Float;

	switch (dwOperation) {
	case IDM_COPY_X:
		// If we are in entry mode, string copy from the entry string
		// otherwise, convert the value in X.
		
		if (Calculator.Entering)
			StrClipboardCopy(state->Entry);
		else
			FClipboardCopy(state->Stack.X, state->SignificantDigits, state->DisplayMode);
		break;
		
	case IDM_COPY_Y:
		FClipboardCopy(state->Stack.Y, state->SignificantDigits, state->DisplayMode);
		break;
		
	case IDM_COPY_Z:
		FClipboardCopy(state->Stack.Z, state->SignificantDigits, state->DisplayMode);
		break;
		
	case IDM_COPY_T:
		FClipboardCopy(state->Stack.T, state->SignificantDigits, state->DisplayMode);
		break;
		
	case IDM_PASTE_X:
		if (Calculator.Entering)
			StrClipboardPaste(state->Entry);
		else
			FClipboardPaste(&state->Stack.X);
		break;
		
	case IDM_PASTE_Y:
		FClipboardPaste(&state->Stack.Y);
		break;
		
	case IDM_PASTE_Z:
		FClipboardPaste(&state->Stack.Z);
		break;
		
	case IDM_PASTE_T:
		FClipboardPaste(&state->Stack.T);
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
		RunCalculatorCommand(hwnd, dwOperation);
		return;
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

static BOOL FNumTest(PUCHAR pEntryString, UCHAR Char) {
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
