/*
 *   Copyright (c) 2026 by the TADS 3 authors.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsplat.h - platform type-compatibility layer for guit3
Function
  guit3's headers (tadswin.h, htmlgui.h, htmlpref.h, ...) still declare
  functions and structures in terms of Win32 types - HWND, RECT, DWORD,
  COLORREF, LOGFONT, SCROLLINFO and so on - even though the code behind
  those declarations no longer talks to the Windows GUI (the handles are
  opaque tokens now, the SCROLLINFO store is ours, etc.; see migration.md
  sections 3.4/3.4a).  Rather than renaming ~26,000 call sites, this one
  header supplies those types.

  On Windows it is nothing but "#include <windows.h>", so the Windows build
  is byte-for-byte identical to before.  Off Windows it defines just the
  slice of the Win32 surface that guit3's *headers* name; the .cpp files
  still "#include <windows.h>" directly and compile Windows-only until the
  three build gates come down (migration.md M4), at which point this
  header's non-Windows branch is completed against real compiler output.

  This is migration.md's work item A1 (section 5.4).  Its companion, A2, is
  the set of os_* call hooks (the os_font_family_is_present() precedent in
  tadsfont.h / guifont.cpp) that route the remaining Win32 *calls* through
  named single-purpose backends.
Notes

Modified
  2026 - A1 of the guit3 platform-independence plan
*/

#ifndef TADSPLAT_H
#define TADSPLAT_H

#if defined(_WIN32)

/*
 *   Windows build: this header is a pure forwarder.  Nothing below this
 *   point is compiled, so the Win32 translation is exactly what it always
 *   was.
 */
#include <windows.h>

#else /* !_WIN32 */

/* ------------------------------------------------------------------------ */
/*
 *   Non-Windows compatibility shim.
 *
 *   INCOMPLETE BY DESIGN.  M2/A1 only needs the seam to exist; the first
 *   real off-Windows compile (M4) is what surfaces the exact remaining
 *   gaps, and they get filled in then.  Audio/MIDI (tadswav.h, tadscsnd.h,
 *   tadsmidi.h) and COM/OLE (tadscom.h, tadsole.h, tadswebctl.h) types are
 *   deliberately only stubbed to "parses as a pointer" here - those
 *   subsystems are gated Windows-only well past M2 (migration.md items I,
 *   O and phase two).
 */

#include <stddef.h>
#include <stdint.h>

/* calling-convention / SAL decorations - no-ops off Windows */
#define CALLBACK
#define WINAPI
#define APIENTRY
#define STDMETHODCALLTYPE
#define WINUSERAPI
#define CDECL

/* ------------------------------------------------------------------------ */
/*
 *   Scalar types
 */
typedef unsigned long   DWORD;
typedef unsigned short   WORD;
typedef unsigned char   BYTE;
typedef unsigned int   UINT;
typedef int   BOOL;
typedef int   INT;
typedef long   LONG;
typedef unsigned long   ULONG;
typedef long long   LONGLONG;
typedef unsigned long long ULONGLONG;
typedef unsigned int   DWORD32;
typedef intptr_t   INT_PTR;
typedef uintptr_t   UINT_PTR;
typedef intptr_t   LONG_PTR;
typedef uintptr_t   ULONG_PTR;
typedef uintptr_t   DWORD_PTR;
typedef uintptr_t   WPARAM;
typedef intptr_t   LPARAM;
typedef intptr_t   LRESULT;
typedef unsigned long   COLORREF;
typedef unsigned short   ATOM;
typedef wchar_t   WCHAR;
typedef char   TCHAR;
typedef void          *LPVOID;
typedef const void   *LPCVOID;
typedef char          *LPSTR;
typedef const char   *LPCSTR;
typedef TCHAR          *LPTSTR;
typedef const TCHAR   *LPCTSTR;
typedef long   HRESULT;
typedef size_t   SIZE_T;

/* ------------------------------------------------------------------------ */
/*
 *   Handles.  Every one of these is an opaque token in guit3 (migration.md
 *   3.4a) - never dereferenced, only compared against 0 and passed around -
 *   so a single void* alias covers all of them.
 */
typedef void   *HANDLE;
typedef void   *HWND;
typedef void   *HDC;
typedef void   *HMENU;
typedef void   *HACCEL;
typedef void   *HCURSOR;
typedef void   *HICON;
typedef void   *HINSTANCE;
typedef void   *HMODULE;
typedef void   *HFONT;
typedef void   *HGDIOBJ;
typedef void   *HBITMAP;
typedef void   *HBRUSH;
typedef void   *HPEN;
typedef void   *HRGN;
typedef void   *HPALETTE;
typedef void   *HGLOBAL;
typedef void   *HLOCAL;
typedef void   *HKEY;
typedef void   *HHOOK;
typedef void   *HRSRC;
typedef void   *HDROP;
typedef void   *HIMAGELIST;
typedef void   *HTREEITEM;
typedef void   *HWAVEOUT;
typedef void   *HMIDISTRM;
typedef void   *HMMIO;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef int (CALLBACK *FARPROC)();
typedef int (CALLBACK *PROC)();

/* ------------------------------------------------------------------------ */
/*
 *   Plain structures.  Members match the Win32 layout the code actually
 *   reads.
 */
typedef struct tagPOINT   { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE   { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT
    { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct _POINTS   { short x, y; } POINTS;

typedef struct tagSCROLLINFO
{
    UINT cbSize;
    UINT fMask;
    int nMin;
    int nMax;
    UINT nPage;
    int nPos;
    int nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;

typedef struct tagLOGFONTA
{
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    char lfFaceName[32];
} LOGFONTA, LOGFONT, *LPLOGFONT;

typedef struct tagMSG
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *LPMSG;

typedef struct tagNMHDR
{
    HWND hwndFrom;
    UINT_PTR idFrom;
    UINT code;
} NMHDR, *LPNMHDR;

typedef struct tagPAINTSTRUCT
{
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagWNDCLASSA
{
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, WNDCLASS, *LPWNDCLASS;

typedef struct tagTEXTMETRICA
{
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    BYTE tmFirstChar;
    BYTE tmLastChar;
    BYTE tmDefaultChar;
    BYTE tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
} TEXTMETRICA, TEXTMETRIC, *LPTEXTMETRIC;

typedef struct _FILETIME
    { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME
{
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
typedef union _LARGE_INTEGER
    { struct { DWORD LowPart; LONG HighPart; } u; LONGLONG QuadPart; } LARGE_INTEGER;

typedef struct _OSVERSIONINFOA
{
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char szCSDVersion[128];
} OSVERSIONINFOA, OSVERSIONINFO;

typedef struct tagRGBQUAD
    { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth, biHeight;
    WORD biPlanes, biBitCount;
    DWORD biCompression, biSizeImage;
    LONG biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAPINFO
    { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *LPBITMAPINFO;

/*
 *   Audio / MIDI - stubs only.  tadswav.cpp, tadscsnd.cpp and tadsmidi.cpp
 *   are Windows-only until migration.md item I / phase two replaces the
 *   Win32 decoder file I/O and the midiStream* sequencer.
 */
typedef UINT MMRESULT;
typedef struct tWAVEFORMATEX
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct pcmwaveformat_tag
{
    struct { WORD wf_tag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec;
             WORD nBlockAlign; } wf;
    WORD wBitsPerSample;
} PCMWAVEFORMAT;
typedef struct wavehdr_tag
{
    LPSTR lpData;
    DWORD dwBufferLength, dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags, dwLoops;
    struct wavehdr_tag *lpNext;
    DWORD_PTR reserved;
} WAVEHDR;
typedef struct midihdr_tag
{
    LPSTR lpData;
    DWORD dwBufferLength, dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags;
    struct midihdr_tag *lpNext;
    DWORD_PTR reserved;
    DWORD dwOffset;
    DWORD_PTR dwReserved[8];
} MIDIHDR, *LPMIDIHDR;

/*
 *   COM / OLE - forward declarations only, so pointer-to-interface
 *   parameters in shared headers parse.  The real definitions come with
 *   migration.md item O (gate the Web UI / drag-and-drop behind
 *   TADS_WEBUI_ENABLED); off Windows those headers are not compiled.
 */
struct IUnknown;
struct IDataObject;
struct IStream;
struct IDropTarget;
struct IDropSource;
struct IDispatch;
typedef struct _GUID
    { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef const GUID &REFIID;
typedef const GUID &REFCLSID;

/* ------------------------------------------------------------------------ */
/*
 *   Macros
 */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define RGB(r, g, b) \
    ((COLORREF)(((BYTE)(r)) | (((WORD)((BYTE)(g))) << 8) \
                | (((DWORD)((BYTE)(b))) << 16)))
#define GetRValue(rgb) ((BYTE)((rgb) & 0xff))
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xff))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xff))

#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define LOWORD(l) ((WORD)((ULONG_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((ULONG_PTR)(l) >> 16) & 0xffff))
#define MAKELONG(lo, hi) \
    ((LONG)(((WORD)(lo)) | (((DWORD)((WORD)(hi))) << 16)))
#define MAKELPARAM(lo, hi) ((LPARAM)MAKELONG(lo, hi))

/* ------------------------------------------------------------------------ */
/*
 *   Constants still named from header-inline code.  Values are the
 *   ABI-stable Win32 numbers so a mixed Windows/non-Windows include of both
 *   this header and <windows.h> in one TU can't disagree.
 */

/* window styles */
#define WS_OVERLAPPED      0x00000000L
#define WS_POPUP           0x80000000L
#define WS_CHILD           0x40000000L
#define WS_CLIPSIBLINGS    0x04000000L
#define WS_CLIPCHILDREN    0x02000000L
#define WS_CAPTION         0x00C00000L
#define WS_BORDER          0x00800000L
#define WS_DLGFRAME        0x00400000L
#define WS_VSCROLL         0x00200000L
#define WS_HSCROLL         0x00100000L
#define WS_SYSMENU         0x00080000L
#define WS_THICKFRAME      0x00040000L
#define WS_SIZEBOX         WS_THICKFRAME
#define WS_GROUP           0x00020000L
#define WS_TABSTOP         0x00010000L
#define WS_MINIMIZEBOX     0x00020000L
#define WS_MAXIMIZEBOX     0x00010000L
#define WS_OVERLAPPEDWINDOW \
    (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME \
     | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

/* extended window styles */
#define WS_EX_DLGMODALFRAME 0x00000001L
#define WS_EX_TOPMOST       0x00000008L
#define WS_EX_TOOLWINDOW    0x00000080L
#define WS_EX_WINDOWEDGE    0x00000100L
#define WS_EX_CLIENTEDGE    0x00000200L
#define WS_EX_MDICHILD      0x00000040L

/* class styles */
#define CS_VREDRAW   0x0001
#define CS_HREDRAW   0x0002
#define CS_DBLCLKS   0x0008
#define CS_OWNDC     0x0020

/* GetWindowLong indices */
#define GWL_WNDPROC   (-4)
#define GWL_STYLE     (-16)
#define GWL_EXSTYLE   (-20)

/* ShowWindow codes */
#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_NORMAL          1
#define SW_SHOWMINIMIZED   2
#define SW_SHOWMAXIMIZED   3
#define SW_MAXIMIZE        3
#define SW_SHOWNOACTIVATE  4
#define SW_SHOW            5
#define SW_MINIMIZE        6
#define SW_RESTORE        9

/* scrollbar constants */
#define SB_HORZ       0
#define SB_VERT       1
#define SB_CTL        2
#define SB_BOTH       3
#define SB_LINEUP     0
#define SB_LINELEFT   0
#define SB_LINEDOWN   1
#define SB_LINERIGHT  1
#define SB_PAGEUP     2
#define SB_PAGELEFT   2
#define SB_PAGEDOWN   3
#define SB_PAGERIGHT  3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK 5
#define SB_TOP        6
#define SB_LEFT       6
#define SB_BOTTOM     7
#define SB_RIGHT      7
#define SB_ENDSCROLL  8

/* SCROLLINFO fMask flags */
#define SIF_RANGE       0x0001
#define SIF_PAGE        0x0002
#define SIF_POS         0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS    0x0010
#define SIF_ALL         (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)

/* status-bar messages (SB_SETPARTS/SB_SETTEXT collide with scrollbar SB_*;
 * the status-bar values are WM_USER-relative and only used symbolically) */
#define WM_USER         0x0400
#define SB_SETTEXTA     (WM_USER + 1)
#define SB_SETPARTS     (WM_USER + 4)
#ifndef SB_SETTEXT
#define SB_SETTEXT      SB_SETTEXTA
#endif

/* MessageBox flags and returns */
#define MB_OK              0x00000000L
#define MB_OKCANCEL        0x00000001L
#define MB_YESNOCANCEL     0x00000003L
#define MB_YESNO           0x00000004L
#define MB_ICONERROR       0x00000010L
#define MB_ICONQUESTION    0x00000020L
#define MB_ICONWARNING     0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_DEFBUTTON2      0x00000100L
#define IDOK      1
#define IDCANCEL  2
#define IDABORT   3
#define IDRETRY   4
#define IDIGNORE  5
#define IDYES     6
#define IDNO      7

/* standard cursor ids */
#define IDC_ARROW    MAKEINTRESOURCE(32512)
#define IDC_IBEAM    MAKEINTRESOURCE(32513)
#define IDC_WAIT     MAKEINTRESOURCE(32514)
#define IDC_CROSS    MAKEINTRESOURCE(32515)
#define IDC_HAND     MAKEINTRESOURCE(32649)

/* GetSysColor indices */
#define COLOR_WINDOW        5
#define COLOR_WINDOWTEXT    8
#define COLOR_APPWORKSPACE  12
#define COLOR_HIGHLIGHT     13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_3DFACE        15
#define COLOR_BTNFACE       15
#define COLOR_3DSHADOW      16
#define COLOR_BTNSHADOW     16
#define COLOR_3DHILIGHT     20
#define COLOR_BTNHIGHLIGHT  20

/* character sets / code pages */
#define ANSI_CHARSET     0
#define DEFAULT_CHARSET  1
#define SYMBOL_CHARSET   2
#define CP_ACP           0
#define CP_UTF8          65001

/*
 *   Window messages.  Off Windows there is no message pump (migration.md
 *   section 3.4); these exist only so header-inline switch/case code that
 *   was mechanically carried over from the Win32 sources still compiles.
 */
#define WM_NULL              0x0000
#define WM_CREATE            0x0001
#define WM_DESTROY           0x0002
#define WM_MOVE              0x0003
#define WM_SIZE              0x0005
#define WM_ACTIVATE          0x0006
#define WM_SETFOCUS          0x0007
#define WM_KILLFOCUS         0x0008
#define WM_PAINT             0x000F
#define WM_CLOSE             0x0010
#define WM_QUIT              0x0012
#define WM_SHOWWINDOW        0x0018
#define WM_ACTIVATEAPP       0x001C
#define WM_SETCURSOR         0x0020
#define WM_MOUSEACTIVATE     0x0021
#define WM_GETMINMAXINFO     0x0024
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED  0x0047
#define WM_NOTIFY            0x004E
#define WM_NCACTIVATE        0x0086
#define WM_KEYDOWN           0x0100
#define WM_KEYUP             0x0101
#define WM_CHAR              0x0102
#define WM_SYSCOMMAND        0x0112
#define WM_TIMER             0x0113
#define WM_COMMAND           0x0111
#define WM_INITMENU          0x0116
#define WM_INITMENUPOPUP     0x0117
#define WM_MENUSELECT        0x011F
#define WM_MENUCHAR          0x0120
#define WM_MEASUREITEM       0x002C
#define WM_DRAWITEM          0x002B
#define WM_ENTERSIZEMOVE     0x0231
#define WM_EXITSIZEMOVE      0x0232
#define WM_LBUTTONDOWN       0x0201
#define WM_MOUSEWHEEL        0x020A
#define WM_CHILDACTIVATE     0x0022
#define WM_SHOW              WM_SHOWWINDOW
#define WM_MDIACTIVATE       0x0222
#define WM_MDIDESTROY        0x0221

#endif /* !_WIN32 */

#endif /* TADSPLAT_H */
