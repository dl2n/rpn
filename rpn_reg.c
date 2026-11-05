/*
* RPN_REG.c  RPN Registry functions
*
* History:
*	 08/25/97	 BruceK   Registry storage of calculator state
*/

#include <windows.h>
#include "rpn.h"

const TCHAR szRegKey[] = "Software\\BruceK\\RPN";


DWORD RegSaveLong(HKEY hKey, LPTSTR szName, ULONG lData) {
	DWORD dwErr;
	
	dwErr = RegSetValueEx(hKey, szName, 0, REG_DWORD, (LPBYTE)&lData, sizeof(lData));
	
	return(dwErr);
}

DWORD RegSaveShort(HKEY hKey, LPTSTR szName, USHORT sData) {
	return(RegSaveLong(hKey, szName, (ULONG)sData));
}

DWORD RegSaveBinary(HKEY hKey, LPTSTR szName, LPBYTE pData, ULONG cbData) {
	DWORD dwErr;
	
	dwErr = RegSetValueEx(hKey, szName, 0, REG_BINARY, pData, cbData);
	
	return(dwErr);
}


DWORD RegGetShort(HKEY hKey, LPTSTR szName, USHORT * psData) {
	DWORD dwData = 0;
	DWORD cbData = sizeof(dwData);
	DWORD dwErr;
	DWORD dwType = 0;
	
	dwErr = RegQueryValueEx(hKey, szName, 0, &dwType, (LPBYTE)&dwData, &cbData);
	
	*psData = (USHORT)dwData;
	
	return(dwErr);
}

DWORD RegGetLong(HKEY hKey, LPTSTR szName, ULONG * plData) {
	DWORD dwErr;
	DWORD cbData = sizeof(*plData);
	DWORD dwType = 0;
	
	dwErr = RegQueryValueEx(hKey, szName, 0, &dwType, (LPBYTE)plData, &cbData);
	
	return(dwErr);
}


DWORD RegGetBinary(HKEY hKey, LPTSTR szName, LPBYTE pData, ULONG cbData) {
	DWORD dwErr;
	DWORD dwType = 0;
	
	dwErr = RegQueryValueEx(hKey, szName, 0, &dwType, pData, &cbData);
	
	return(dwErr);
}


void SaveRPNState(void) {
	HKEY hKey = NULL;
	DWORD dwErr;
	DWORD dwDisposition;
	
	if (! (dwErr = RegCreateKeyEx(HKEY_CURRENT_USER, szRegKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition))) {
		RegSaveShort(hKey, "Radix", Radix);
		RegSaveShort(hKey, "WordSize", WordSize);
		RegSaveShort(hKey, "IntegerType", usIntegerType);
		RegSaveShort(hKey, "LastRadix", LastRadix);
		RegSaveBinary(hKey, "Binary Stack", (LPBYTE)&Stack, sizeof(Stack));
		// RegSaveLong(hKey, "fEnter", fEnter);
		RegSaveShort(hKey, "CalcMode", usCalcMode);
		RegSaveLong(hKey, "WindowX", dwWindowX);
		RegSaveLong(hKey, "WindowY", dwWindowY);
		RegSaveBinary(hKey, "Float Stack", (LPBYTE)&FStack, sizeof(FStack));
		// UCHAR pEntryString[DISPLAY_WIDTH + 1] = "";
		RegSaveShort(hKey, "SignificantDigits", usSignificantDigits);
		RegSaveShort(hKey, "FloatDisplayMode", usFloatDisplayMode);
		RegSaveShort(hKey, "FloatAngleType", usFloatAngleType);
		
		RegCloseKey(hKey);
	}
}

void RestoreRPNState(void) {
	HKEY hKey = NULL;
	DWORD dwErr;
	
	if (! (dwErr = RegOpenKeyEx(HKEY_CURRENT_USER, szRegKey, 0, KEY_READ, &hKey))) {
		RegGetShort(hKey, "Radix", &Radix);
		RegGetShort(hKey, "WordSize", &WordSize);
		RegGetShort(hKey, "IntegerType", &usIntegerType);
		RegGetShort(hKey, "LastRadix", &LastRadix);
		RegGetBinary(hKey, "Binary Stack", (LPBYTE)&Stack, sizeof(Stack));
		// RegGetLong(hKey, "fEnter", &fEnter);
		RegGetShort(hKey, "CalcMode", &usCalcMode);
		RegGetLong(hKey, "WindowX", &dwWindowX);
		RegGetLong(hKey, "WindowY", &dwWindowY);
		RegGetBinary(hKey, "Float Stack", (LPBYTE)&FStack, sizeof(FStack));
		// UCHAR pEntryString[DISPLAY_WIDTH + 1] = "";
		RegGetShort(hKey, "SignificantDigits", &usSignificantDigits);
		RegGetShort(hKey, "FloatDisplayMode", &usFloatDisplayMode);
		RegGetShort(hKey, "FloatAngleType", &usFloatAngleType);
		
		RegCloseKey(hKey);
		
		//
		//  Sanity checks
		//
		
		switch (WordSize) {
		case 32:
		case 16:
		case 8:
			break;
		default:
			WordSize = 64;
			break;
		}
		
		switch (Radix) {
		case 10:
		case 8:
		case 2:
			break;
		default:
			Radix = 16;
			break;
		}
		
		switch (LastRadix) {
		case 10:
		case 8:
		case 2:
			break;
		default:
			LastRadix = 16;
			break;
		}
		
		switch (usIntegerType) {
		case INTEGER_TYPE_2S_COMP:
			break;
		default:
			usIntegerType = INTEGER_TYPE_UNSIGNED;
			break;
		}
	}
}
