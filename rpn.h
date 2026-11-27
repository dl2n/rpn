/*-----------------------
RPN.H header file
-----------------------*/

#define DEBUG_MODE   FALSE

#define H_NULL  0       // Null handle

#define ID_RPN       1
#define ID_ICON      2
#define ID_ABOUT     3
#define ID_RPN_MENU  4

/*
* Menu identifiers
*/

#define IDM_ABOUT    258

#define IDM_BASE           260
#define IDM_BINARY         261
#define IDM_OCTAL          262
#define IDM_DECIMAL        263
#define IDM_HEXADECIMAL    264
#define IDM_FLOAT          265

#define IDM_WORD_SIZE      270
#define IDM_WORD_SIZE_8    271
#define IDM_WORD_SIZE_16   272
#define IDM_WORD_SIZE_32   273
#define IDM_WORD_SIZE_64   274

#define IDM_INTEGER_TYPE   280
#define IDM_INTEGER_TYPE_UNSIGNED   281
#define IDM_INTEGER_TYPE_1S         282
#define IDM_INTEGER_TYPE_2S         283

#define IDM_HELP_HELP_KEYS          290
#define IDM_HELP_ABOUT              291

#define IDM_MODE                    300
#define IDM_MODE_HEXCALC            301

#define IDM_DISPLAY                 310
#define IDM_DISPLAY_AUTO            311
#define IDM_DISPLAY_FIX             312
#define IDM_DISPLAY_FIX_0           313
#define IDM_DISPLAY_FIX_1           314
#define IDM_DISPLAY_FIX_2           315
#define IDM_DISPLAY_FIX_3           316
#define IDM_DISPLAY_FIX_4           317
#define IDM_DISPLAY_FIX_5           318
#define IDM_DISPLAY_FIX_6           319
#define IDM_DISPLAY_FIX_7           320
#define IDM_DISPLAY_FIX_8           321
#define IDM_DISPLAY_FIX_9           322
#define IDM_DISPLAY_FIX_10          323
#define IDM_DISPLAY_FIX_11          324
#define IDM_DISPLAY_FIX_12          325
#define IDM_DISPLAY_FIX_13          326
#define IDM_DISPLAY_FIX_14          327
#define IDM_DISPLAY_FIX_15          328

#define IDM_DISPLAY_SCI             330
#define IDM_DISPLAY_SCI_0           331
#define IDM_DISPLAY_SCI_1           332
#define IDM_DISPLAY_SCI_2           333
#define IDM_DISPLAY_SCI_3           334
#define IDM_DISPLAY_SCI_4           335
#define IDM_DISPLAY_SCI_5           336
#define IDM_DISPLAY_SCI_6           337
#define IDM_DISPLAY_SCI_7           338
#define IDM_DISPLAY_SCI_8           339
#define IDM_DISPLAY_SCI_9           340
#define IDM_DISPLAY_SCI_10          341
#define IDM_DISPLAY_SCI_11          342
#define IDM_DISPLAY_SCI_12          343
#define IDM_DISPLAY_SCI_13          344
#define IDM_DISPLAY_SCI_14          345
#define IDM_DISPLAY_SCI_15          346

#define IDM_DISPLAY_ENG             350
#define IDM_DISPLAY_ENG_0           351
#define IDM_DISPLAY_ENG_1           352
#define IDM_DISPLAY_ENG_2           353
#define IDM_DISPLAY_ENG_3           354
#define IDM_DISPLAY_ENG_4           355
#define IDM_DISPLAY_ENG_5           356
#define IDM_DISPLAY_ENG_6           357
#define IDM_DISPLAY_ENG_7           358
#define IDM_DISPLAY_ENG_8           359
#define IDM_DISPLAY_ENG_9           360
#define IDM_DISPLAY_ENG_10          361
#define IDM_DISPLAY_ENG_11          362
#define IDM_DISPLAY_ENG_12          363
#define IDM_DISPLAY_ENG_13          364
#define IDM_DISPLAY_ENG_14          365
#define IDM_DISPLAY_ENG_15          366

#define IDM_TRIG                    370
#define IDM_TRIG_DEGREE             371
#define IDM_TRIG_RADIAN             372
#define IDM_TRIG_GRAD               373

#define IDM_CHANGE_MODE             400


#define IDM_COPY_X                  410
#define IDM_COPY_Y                  411
#define IDM_COPY_Z                  412
#define IDM_COPY_T                  413
#define IDM_PASTE_X                 414
#define IDM_PASTE_Y                 415
#define IDM_PASTE_Z                 416
#define IDM_PASTE_T                 417

#define IDM_VIEW_KEYPAD             420


#define IDD_DISPLAY  257
#define IDD_STATUS   258
#define IDD_ABOUT    259

#define IDD_PAINT    260

#define IDD_DISPLAY_FLOAT   261
#define IDD_STATUS_FLOAT    262

#define IDD_CLEAR_ERROR		500


#define MAX_CONTROL_CHAR		0x20			// SPC

#define BACKSPACE				8
#define SWAP_KEY				9			// TAB
#define ENTER					13
#define ROLL_DOWN				82          // R
#define NOT_KEY					126         // ~
#define DELETE_KEY				127			// DEL
#define PLUS_KEY				43          // '+'
#define MINUS_KEY				45          // '-'
#define MULT_KEY				42          // '*'
#define DIV_KEY					47          // '/'
#define MOD_KEY					37          // '%'
#define AND_KEY					38          // '&'
#define OR_KEY					124         // '|'
#define XOR_KEY					94          // '^'
#define LEFT_SHIFT_KEY			60          // '<'
#define RIGHT_SHIFT_KEY			62          // '>'
#define NEGATE_KEY				83          // 'S'
#define BACKSPACE_KEY			8           // '\b'
#define ROLL_DOWN_KEY			82          // 'R'
#define KILO_KEY				75          // 'K'
#define TOGGLE_BASE_KEY			76          // 'L'

#define UNARY_LEFT_SHIFT_KEY	44			// ',' - unshifted '<'
#define UNARY_RIGHT_SHIFT_KEY	46			// '.' - unshifted '>'
#define UNARY_PLUS_KEY			93			// ']' - immediately below + key
#define UNARY_MINUS_KEY			91			// '[' - immediately below - key

//
//  Keybindings only in the FLOAT calculator
//

#define EEX_KEY					69          // 'E'
#define ONE_OVER_KEY			37          // '%'
#define SUBSTITUTE_ONE_OVER_KEY 137			// special case since 37 is used in binary calc
#define INVERSE_KEY				27          // ESC
#define PI_KEY					80          // 'P'
#define YTOX_KEY				94          // '^'
#define SUBSTITUTE_YTOX_KEY		194         // special case since 94 is used in binary calc
#define SQRT_KEY				81          // 'Q'
#define SIN_KEY					73          // 'I'
#define COS_KEY					67          // 'C'
#define TAN_KEY					84          // 'T'
#define LN_KEY					76          // 'L'
#define SUBSTITUTE_LN_KEY		176         // special case since 76 is used in binary calc
#define LOG_KEY					71          // 'G'
#define DECIMAL_KEY				46          // '.'
#define SUBSTITUTE_DECIMAL_KEY	146			// special case since 46 is used in binary calc


#define DISPLAY_WIDTH       35
#define KILO              1024

#define FNUM_MAX            DBL_MAX
#define FNUM_MIN            DBL_MIN
#define FNUM_MANT_DIG       DBL_DIG
#define FNUM_EXP_DIG        3


#define FLOAT_ANGLE_DEG     1
#define FLOAT_ANGLE_RAD     2
#define FLOAT_ANGLE_GRAD    3

#define FLOAT_DISPLAY_AUTO  0
#define FLOAT_DISPLAY_FIX   1
#define FLOAT_DISPLAY_SCI   2
#define FLOAT_DISPLAY_ENG   3
#define FLOAT_SIGNIFICANT_DIGITS 10

#define PI_VALUE            (3.1415926535897932384)

#define RAD_PER_GRAD    ((2.0 * PI_VALUE) / 400.0)    // BUGBUG: Check 400 grads/circle?
#define RAD_PER_DEG     ((2.0 * PI_VALUE) / 360.0)
#define GRAD_PER_RAD    (400.0 / (2.0 * PI_VALUE))    // BUGBUG: Check 400 grads/circle?
#define DEG_PER_RAD     (360.0 / (2.0 * PI_VALUE))

#ifndef MAXULONGLONG
#define MAXULONGLONG    0xffffffffffffffff
#endif
#ifndef MAXLONGLONG
#define MAXLONGLONG     0x7fffffffffffffff
#endif
#ifndef MINLONGLONG
#define MINLONGLONG     0x8000000000000000
#endif

#ifndef MAXULONG
#define MAXULONG        0xffffffff
#endif
#ifndef MAXUSHORT
#define MAXUSHORT       0xffff
#endif
#ifndef MAXUCHAR
#define MAXUCHAR        0xff
#endif

#define MaxWordSize(WordSize) (WordSize == 64? MAXULONGLONG:    \
	WordSize == 32? MAXULONG:        \
	WordSize == 16? MAXUSHORT:       \
MAXUCHAR)

#define MaxSignedWordSize(WordSize) (WordSize == 64? MAXLONGLONG:    \
	WordSize == 32? MAXLONG:        \
	WordSize == 16? MAXSHORT:       \
MAXCHAR)

#define MinSignedWordSize(WordSize) (WordSize == 64? MINLONGLONG:    \
	WordSize == 32? MINLONG:        \
	WordSize == 16? MINSHORT:       \
MINCHAR)

#define ID_DIGIT            0

/*
* String table values
*/

#define MAX_STRING_SIZE     255
#define IDS_HELP_PREFIX     0x200
#define IDS_ENTER_KEY       (ENTER | IDS_HELP_PREFIX)
#define IDS_SWAP_KEY        (SWAP_KEY | IDS_HELP_PREFIX)
#define IDS_PLUS_KEY        (PLUS_KEY | IDS_HELP_PREFIX)
#define IDS_MINUS_KEY       (MINUS_KEY | IDS_HELP_PREFIX)
#define IDS_MULT_KEY        (MULT_KEY | IDS_HELP_PREFIX)
#define IDS_DIV_KEY         (DIV_KEY | IDS_HELP_PREFIX)
#define IDS_MOD_KEY         (MOD_KEY | IDS_HELP_PREFIX)
#define IDS_AND_KEY         (AND_KEY | IDS_HELP_PREFIX)
#define IDS_OR_KEY          (OR_KEY | IDS_HELP_PREFIX)
#define IDS_XOR_KEY         (XOR_KEY | IDS_HELP_PREFIX)
#define IDS_LEFT_SHIFT_KEY  (LEFT_SHIFT_KEY | IDS_HELP_PREFIX)
#define IDS_RIGHT_SHIFT_KEY (RIGHT_SHIFT_KEY | IDS_HELP_PREFIX)
#define IDS_NEGATE_KEY      (NEGATE_KEY | IDS_HELP_PREFIX)
#define IDS_NOT_KEY         (NOT_KEY | IDS_HELP_PREFIX)
#define IDS_BACKSPACE_KEY   (BACKSPACE_KEY | IDS_HELP_PREFIX)
#define IDS_DELETE_KEY      (DELETE_KEY | IDS_HELP_PREFIX)
#define IDS_ROLL_DOWN_KEY   (ROLL_DOWN_KEY | IDS_HELP_PREFIX)
#define IDS_KILO_KEY        (KILO_KEY | IDS_HELP_PREFIX)
#define IDS_TOGGLE_BASE_KEY (TOGGLE_BASE_KEY | IDS_HELP_PREFIX)
#define IDS_BINARY          (IDM_BINARY | IDS_HELP_PREFIX)
#define IDS_OCTAL           (IDM_OCTAL | IDS_HELP_PREFIX)
#define IDS_DECIMAL         (IDM_DECIMAL | IDS_HELP_PREFIX)
#define IDS_HEXADECIMAL     (IDM_HEXADECIMAL | IDS_HELP_PREFIX)
#define IDS_WORD_SIZE_8     (IDM_WORD_SIZE_8 | IDS_HELP_PREFIX)
#define IDS_WORD_SIZE_16    (IDM_WORD_SIZE_16 | IDS_HELP_PREFIX)
#define IDS_WORD_SIZE_32    (IDM_WORD_SIZE_32 | IDS_HELP_PREFIX)
#define IDS_WORD_SIZE_64    (IDM_WORD_SIZE_64 | IDS_HELP_PREFIX)
#define IDS_INTEGER_TYPE_UNSIGNED   (IDM_INTEGER_TYPE_UNSIGNED | IDS_HELP_PREFIX)
#define IDS_INTEGER_TYPE_1S (IDM_INTEGER_TYPE_1S | IDS_HELP_PREFIX)
#define IDS_INTEGER_TYPE_2S (IDM_INTEGER_TYPE_2S | IDS_HELP_PREFIX)
#define IDS_FLOAT           (IDM_FLOAT | IDS_HELP_PREFIX)
#define IDS_HELP_ABOUT      (IDM_HELP_ABOUT | IDS_HELP_PREFIX)
#define IDS_DIGIT           (ID_DIGIT | IDS_HELP_PREFIX)
#define IDS_EEX_KEY         (EEX_KEY | IDS_HELP_PREFIX)
#define IDS_ONE_OVER_KEY    (SUBSTITUTE_ONE_OVER_KEY | IDS_HELP_PREFIX)
#define IDS_INVERSE_KEY     (INVERSE_KEY | IDS_HELP_PREFIX)
#define IDS_PI_KEY          (PI_KEY | IDS_HELP_PREFIX)
#define IDS_YTOX_KEY        (SUBSTITUTE_YTOX_KEY | IDS_HELP_PREFIX)
#define IDS_SQRT_KEY        (SQRT_KEY | IDS_HELP_PREFIX)
#define IDS_SIN_KEY         (SIN_KEY | IDS_HELP_PREFIX)
#define IDS_COS_KEY         (COS_KEY | IDS_HELP_PREFIX)
#define IDS_TAN_KEY         (TAN_KEY | IDS_HELP_PREFIX)
#define IDS_LN_KEY          (SUBSTITUTE_LN_KEY | IDS_HELP_PREFIX)
#define IDS_LOG_KEY         (LOG_KEY | IDS_HELP_PREFIX)
#define IDS_DECIMAL_KEY     (SUBSTITUTE_DECIMAL_KEY | IDS_HELP_PREFIX)

#define IDS_UNARY_LEFT_SHIFT    (UNARY_LEFT_SHIFT_KEY | IDS_HELP_PREFIX)
#define IDS_UNARY_RIGHT_SHIFT   (UNARY_RIGHT_SHIFT_KEY | IDS_HELP_PREFIX)
#define IDS_UNARY_PLUS			(UNARY_PLUS_KEY | IDS_HELP_PREFIX)
#define IDS_UNARY_MINUS			(UNARY_MINUS_KEY | IDS_HELP_PREFIX)

#define IDS_TRIG_DEGREE     (IDM_TRIG_DEGREE | IDS_HELP_PREFIX)
#define IDS_TRIG_RADIAN     (IDM_TRIG_RADIAN | IDS_HELP_PREFIX)
#define IDS_TRIG_GRAD       (IDM_TRIG_GRAD | IDS_HELP_PREFIX)

#define IDS_COPY_X          (IDM_COPY_X | IDS_HELP_PREFIX)
#define IDS_COPY_Y          (IDM_COPY_Y | IDS_HELP_PREFIX)
#define IDS_COPY_Z          (IDM_COPY_Z | IDS_HELP_PREFIX)
#define IDS_COPY_T          (IDM_COPY_T | IDS_HELP_PREFIX)
#define IDS_PASTE_X         (IDM_PASTE_X | IDS_HELP_PREFIX)
#define IDS_PASTE_Y         (IDM_PASTE_Y | IDS_HELP_PREFIX)
#define IDS_PASTE_Z         (IDM_PASTE_Z | IDS_HELP_PREFIX)
#define IDS_PASTE_T         (IDM_PASTE_T | IDS_HELP_PREFIX)
#define IDS_VIEW_KEYPAD     (IDM_VIEW_KEYPAD | IDS_HELP_PREFIX)



#define RPN_ICON            500
#define RPN_DIALOG          600
#define RPNF_DIALOG         601
#define RPN_CLASS           "Rpn"
#define RPNF_CLASS          "RpnF"
#define RPN_MENU            800
#define RPNF_MENU           801
#define RPN_ACCELERATORS    802

/*
* Values for usIntegerType
*/
#define INTEGER_TYPE_UNSIGNED    0
#define INTEGER_TYPE_1S_COMP     1
#define INTEGER_TYPE_2S_COMP     2

#define CLIENT_CLASS  "RPN"
#define DISPLAY_CLASS "Display"
#define STATUS_CLASS "Status"
#define ABOUT_CLASS "About"

typedef ULONGLONG NUMBER, *PNUMBER;
typedef LONGLONG SIGNED_NUMBER, *PSIGNED_NUMBER;
typedef double FNUMBER, *PFNUMBER;

typedef struct _STACK {
	NUMBER   X;
	NUMBER   Y;
	NUMBER   Z;
	NUMBER   T;
} STACK, * PSTACK;

typedef struct _FSTACK {
    FNUMBER X;
    FNUMBER Y;
    FNUMBER Z;
    FNUMBER T;
} FSTACK, *PFSTACK;

void SwapIntegerRegisters(PSTACK stack);
void SwapFloatRegisters(PFSTACK stack);
void RollIntegerRegisters(PSTACK stack);
void RollFloatRegisters(PFSTACK stack);
void DuplicateIntegerX(PSTACK stack);
void DuplicateFloatX(PFSTACK stack);
void DropIntegerX(PSTACK stack);
void DropFloatX(PFSTACK stack);
void DropIntegerY(PSTACK stack);
void DropFloatY(PFSTACK stack);

typedef struct _INTEGER_STATE {
    USHORT Radix;
    USHORT WordSize;
    USHORT IntegerType;
    USHORT LastRadix;
    STACK Stack;
} INTEGER_STATE;

char *IntToA(NUMBER value, char *buffer, ULONG length, USHORT radix,
             USHORT integerType, USHORT wordSize, BOOLEAN adorn);
BOOL StrNumTest(PUCHAR text, USHORT radix, USHORT integerType, USHORT wordSize,
                PNUMBER number);
BOOL IsBaseDigit(UCHAR ch, USHORT radix);
// Paste text is caller-owned and must be released with free().
char *ExtractPasteToken(const char *text, size_t capacity);
char *ReadClipboardToken(HWND owner);
BOOL WriteClipboardText(HWND owner, const char *text);

typedef struct _FLOAT_STATE {
    FSTACK Stack;
    UCHAR Entry[DISPLAY_WIDTH + 1];
    USHORT SignificantDigits;
    USHORT DisplayMode;
    USHORT AngleType;
    BOOL Inverse;
} FLOAT_STATE;

char *FToA(FNUMBER value, char *buffer, ULONG length, USHORT significantDigits,
           USHORT displayMode, BOOLEAN adorn);

// These values are also used by the existing registry format.
typedef enum _CALC_MODE {
    CALC_MODE_BINARY = 1,
    CALC_MODE_FLOAT = 2
} CALC_MODE;

typedef struct _CALCULATOR_STATE {
    INTEGER_STATE Integer;
    FLOAT_STATE Float;
    CALC_MODE Mode;
    // Both modes see the same flags, even when switching during entry.
    BOOL Entering;
    BOOL Error;
} CALCULATOR_STATE;

typedef struct _DISPLAY_LINES {
    char Text[5][DISPLAY_WIDTH + 1];  // Status, T, Z, Y, X
} DISPLAY_LINES;

// Compose all five rows without GDI. A row that cannot be formatted is
// empty and makes the result FALSE; the other rows remain available.
BOOL ComposeDisplayLines(const CALCULATOR_STATE *state, DISPLAY_LINES *lines);

typedef enum _CALCULATOR_RESULT {
    CALCULATOR_REDRAW,
    CALCULATOR_ENTRY_REJECTED,
    CALCULATOR_UNKNOWN_COMMAND,
    CALCULATOR_MODE_CHANGED
} CALCULATOR_RESULT;

typedef void (*CALCULATOR_REPORT_ERROR)(void *context, const char *message);

extern CALCULATOR_STATE Calculator;
extern const CALCULATOR_STATE CalculatorDefaults;
// Route a calculation or mode command without a window. The required
// callback reports modal errors synchronously, at the same point as the UI.
CALCULATOR_RESULT CalculatorCommand(CALCULATOR_STATE *state, DWORD command,
                                   CALCULATOR_REPORT_ERROR reportError, void *context);
CALCULATOR_RESULT IntegerCommand(INTEGER_STATE *state, DWORD command, BOOL *entering,
                                BOOL *error, CALCULATOR_REPORT_ERROR reportError, void *context);
CALCULATOR_RESULT FloatCommand(FLOAT_STATE *state, DWORD command, BOOL *entering,
                              BOOL *error, CALCULATOR_REPORT_ERROR reportError, void *context);
void RunCalculatorCommand(HWND hwnd, DWORD command);

extern BOOL fHelpKeysMode;
extern BOOL fKeypadHidden;
extern HWND hDisplay;
extern HWND hwndDialog;
extern DWORD cyChar;
extern HANDLE hInst;
extern BOOL fFirstTime;
extern BOOL bQuit;
extern DWORD dwWindowX;
extern DWORD dwWindowY;

typedef struct _RPN_WINDOW_STATE {
    DWORD X;
    DWORD Y;
    BOOL KeypadHidden;
} RPN_WINDOW_STATE;

DWORD SaveRPNStateToKey(HKEY key, const CALCULATOR_STATE *state, const RPN_WINDOW_STATE *window);
// Restore into startup defaults. A missing value keeps its default; malformed
// values keep their defaults and set the display error flag.
void RestoreRPNStateFromKey(HKEY key, CALCULATOR_STATE *state, RPN_WINDOW_STATE *window);
DWORD SaveRPNState(void);
void RestoreRPNState(void);
void RedrawDisplay(HWND hwnd);
void SetKeypadVisible(HWND hwnd, BOOL fShow);
void CALLBACK ClearErrorTimer(HWND hwnd, UINT message, UINT_PTR id, DWORD time);

void FWmCommandProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void FWmNotifyProc( HWND hwnd, int idCtrl, LPNMHDR pnmh );
