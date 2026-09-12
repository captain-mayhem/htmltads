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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>

/* RPC calling-convention decoration used on IDataObject/IDropTarget method
   parameters throughout the shared headers (e.g. "IDataObject __RPC_FAR *") */
#define __RPC_FAR

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
/* always 32 bits on Windows (LLP64), unlike a bare "long" under Linux's
   LP64 model (64 bits) - see the HRESULT note below for why this matters */
typedef int32_t   LONG;
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
/* Windows' LONG/HRESULT are always 32 bits even in 64-bit builds (LLP64);
   Linux's "long" is 64 bits (LP64), so a plain "long" here would silently
   treat every negative-as-32-bit HRESULT constant (E_FAIL, E_NOINTERFACE,
   ...) as a positive, "successful" 64-bit value - use a fixed-width type
   instead. Found the hard way: CTadsApp::get_my_docs_path() (tadsapp.cpp)
   calling a vtable method through a null IMalloc* because SUCCEEDED()
   read a failed SHGetSpecialFolderLocation() as having succeeded. */
typedef int32_t   HRESULT;
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
 *   COM / OLE.  IDataObject, IStream and IDispatch are forward declarations
 *   only, so pointer-to-interface parameters in shared headers parse - the
 *   real definitions come with migration.md item O (gate the Web UI behind
 *   TADS_WEBUI_ENABLED) or stay Windows-only (tadsole.cpp, tadsistr.h).
 *
 *   IUnknown/IDropSource/IDropTarget are real abstract classes, not just
 *   forward declarations, because CTadsWin - always compiled, not gated
 *   behind any flag - derives from IDropSource and IDropTarget directly
 *   (tadswin.h) to implement OLE drag-and-drop (migration.md 5.1's note on
 *   CoInitialize/CoUninitialize staying unconditional).  None of this is
 *   ever exercised off Windows: RegisterDragDrop()/DoDragDrop()/
 *   CoCreateInstance() are #ifdef _WIN32-only call sites, so no real COM
 *   caller ever reaches these vtables - they only need to satisfy the
 *   compiler and linker.  Method layout mirrors real oleidl.h so the
 *   existing multiple-inheritance-without-virtual-IUnknown pattern in
 *   tadswin.cpp (single QueryInterface/AddRef/Release override satisfying
 *   both bases) still works exactly as it does on Windows.
 */
struct IDataObject;
struct IStream;
struct IDispatch;
typedef struct _GUID
    { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef const GUID &REFIID;
typedef const GUID &REFCLSID;
inline bool operator==(const GUID &a, const GUID &b)
{
    return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3
        && memcmp(a.Data4, b.Data4, sizeof(a.Data4)) == 0;
}
inline bool operator!=(const GUID &a, const GUID &b) { return !(a == b); }

typedef struct tagPOINTL { LONG x, y; } POINTL;

struct IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
    virtual ~IUnknown() { }
};

struct IDropSource: public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE
        QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) = 0;
    virtual HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect) = 0;
};

struct IDropTarget: public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE
        DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
                  DWORD *pdwEffect) = 0;
    virtual HRESULT STDMETHODCALLTYPE
        DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) = 0;
    virtual HRESULT STDMETHODCALLTYPE DragLeave() = 0;
    virtual HRESULT STDMETHODCALLTYPE
        Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt,
             DWORD *pdwEffect) = 0;
};

/* IDropTargetHelper - only ever reached through CoCreateInstance(), which is
   an #ifdef _WIN32-only call site (tadswin.cpp), so drop_target_helper_ is
   always null off Windows and these methods never actually run. */
struct IDropTargetHelper: public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE
        DragEnter(HWND hwndTarget, IDataObject *pDataObject, POINT *ppt,
                  DWORD dwEffect) = 0;
    virtual HRESULT STDMETHODCALLTYPE DragLeave() = 0;
    virtual HRESULT STDMETHODCALLTYPE DragOver(POINT *ppt, DWORD dwEffect) = 0;
    virtual HRESULT STDMETHODCALLTYPE
        Drop(IDataObject *pDataObject, POINT *ppt, DWORD dwEffect) = 0;
    virtual HRESULT STDMETHODCALLTYPE Show(BOOL fShow) = 0;
};

/*
 *   FORMATETC/STGMEDIUM/IDataObject - just enough of the real OLE clipboard
 *   transfer types for CHtmlSysWin_win32_Input::DragEnter()/Drop()
 *   (htmlgui.cpp), which are always compiled (they implement CTadsWin's
 *   IDropTarget side, receiving a drop). IDataObject here declares only the
 *   two methods that code actually calls (QueryGetData/GetData) - the real,
 *   full COM IDataObject (with SetData/EnumFormatEtc/...) is implemented by
 *   CTadsDataObjText (tadsole.h), which stays Windows-only (it backs OLE
 *   drag-and-drop *source* support, migration.md 5.3) and is never included
 *   off Windows, so there's no clash between the two.  As with
 *   IDropTargetHelper, nothing ever calls into these methods for real off
 *   Windows, since drop_target_register() never registers a real OS drop
 *   target there.
 */
typedef WORD CLIPFORMAT;
#define CF_TEXT 1
#define DVASPECT_CONTENT 1
#define TYMED_HGLOBAL 1
#define DROPEFFECT_NONE   0
#define DROPEFFECT_COPY   1
#define DROPEFFECT_MOVE   2
#define DROPEFFECT_LINK   4
#define DROPEFFECT_SCROLL 0x80000000

typedef struct tagFORMATETC
{
    CLIPFORMAT cfFormat;
    void *ptd;
    DWORD dwAspect;
    LONG lindex;
    DWORD tymed;
} FORMATETC;

typedef struct tagSTGMEDIUM
{
    DWORD tymed;
    HGLOBAL hGlobal;
    IUnknown *pUnkForRelease;
} STGMEDIUM;

struct IDataObject: public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *pformatetc) = 0;
    virtual HRESULT STDMETHODCALLTYPE
        GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) = 0;
};

/*
 *   Global memory API.  do_copy() (htmlgui.cpp) is a live, always-executed
 *   clipboard path off Windows too - it allocates a GMEM_FIXED block (a
 *   plain buffer, per the Win32 contract for that flag - no "handle"
 *   indirection), converts it, and frees it - so unlike the COM interfaces
 *   above, these need to actually work, not just compile.
 */
#define GMEM_FIXED     0x0000
#define GMEM_MOVEABLE  0x0002
#define GMEM_ZEROINIT  0x0040
#define GHND           (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GMEM_DDESHARE  0x2000
/* IID "values" for CTadsWin::QueryInterface()'s iid comparisons
   (tadswin.cpp) - never matched by a real caller off Windows, so any
   distinct values work; Data1 alone is enough to tell them apart via
   operator==() above. */
static const GUID IID_IUnknown     = { 1, 0, 0, { 0 } };
static const GUID IID_IDropSource  = { 2, 0, 0, { 0 } };
static const GUID IID_IDropTarget  = { 3, 0, 0, { 0 } };
static const GUID IID_IDropTargetHelper = { 4, 0, 0, { 0 } };
static const GUID CLSID_DragDropHelper  = { 5, 0, 0, { 0 } };

inline HGLOBAL GlobalAlloc(UINT flags, size_t size)
{
    void *p = malloc(size != 0 ? size : 1);
    if (p != 0 && (flags & GMEM_ZEROINIT) != 0)
        memset(p, 0, size);
    return p;
}
inline void *GlobalLock(HGLOBAL h) { return h; }
inline BOOL GlobalUnlock(HGLOBAL) { return 1; }
inline HGLOBAL GlobalFree(HGLOBAL h) { free(h); return 0; }

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

/* HRESULT values - used only by the always-dead-off-Windows COM code above */
#define S_OK           ((HRESULT)0L)
#define S_FALSE        ((HRESULT)1L)
#define E_NOINTERFACE  ((HRESULT)0x80004002L)
#define E_FAIL         ((HRESULT)0x80004005L)
#define E_NOTIMPL      ((HRESULT)0x80004001L)
#define SUCCEEDED(hr)  ((HRESULT)(hr) >= 0)
#define FAILED(hr)     ((HRESULT)(hr) < 0)

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
#define WM_NCCREATE          0x0081
#define WM_NCDESTROY         0x0082
#define WM_MDIREFRESHMENU    0x0234
#define WM_CONTEXTMENU       0x007B
#define WM_ERASEBKGND        0x0014
#define WM_QUERYDRAGICON     0x0037
#define WM_LBUTTONDBLCLK     0x0203
#define WM_LBUTTONUP         0x0202
#define WM_RBUTTONDOWN       0x0204
#define WM_RBUTTONUP         0x0205
#define WM_NCLBUTTONDOWN     0x00A1
#define WM_NCLBUTTONDBLCLK   0x00A3
#define WM_NCLBUTTONUP       0x00A2
#define WM_NCRBUTTONDOWN     0x00A4
#define WM_NCRBUTTONUP       0x00A5
#define WM_NCMOUSEMOVE       0x00A0
#define WM_MOUSEMOVE         0x0200
#define WM_CAPTURECHANGED    0x0215
#define WM_HOTKEY            0x0312
#define WM_SYSCHAR           0x0106
#define WM_SYSKEYDOWN        0x0104
#define WM_VSCROLL           0x0115
#define WM_HSCROLL           0x0114
#define WM_QUERYNEWPALETTE   0x030F
#define WM_PALETTECHANGED    0x0311
#define WM_CTLCOLORSTATIC    0x0138
#define WM_CTLCOLOREDIT      0x0133
#define WM_SETTINGCHANGE     0x001A
#define WM_SYSCOLORCHANGE    0x0015

/*
 *   Everything below this point supports code that is provably dead off
 *   Windows: guit3 never creates a real HWND (migration.md 3.4/3.4a), so
 *   none of tadswin.cpp's native window-procedure/GDI/menu-API bodies or
 *   the OLE drag-and-drop registration calls are ever reached there - they
 *   just need to satisfy the compiler and linker.  Values match the real
 *   Win32 constants/layouts where that's essentially free (so a stray
 *   working caller would behave the same either way); functions are inline
 *   no-ops that report harmless failure/empty results.
 */

/* virtual-key / mouse-key-state codes */
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_LBUTTON  0x01
#define VK_MBUTTON  0x04
#define VK_RBUTTON  0x02
#define MK_LBUTTON  0x0001
#define MK_RBUTTON  0x0002
#define MK_SHIFT    0x0004
#define MK_CONTROL  0x0008
#define MK_MBUTTON  0x0010

/* TrackPopupMenu flags */
#define TPM_LEFTALIGN  0x0000
#define TPM_TOPALIGN   0x0000

/* WM_ACTIVATE wParam low word */
#define WA_INACTIVE 0
#define WA_ACTIVE   1

/* GetSystemMetrics indices */
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37

/* system menu command */
#define SC_KEYMENU 0xF100

/* toolbar (Comctl32) messages/styles - dead native-toolbar code, migration.md 3.1 */
#define TB_BUTTONCOUNT       (0x0400 + 24)
#define TB_GETBUTTON         (0x0400 + 23)
#define TB_GETSTATE          (0x0400 + 18)
#define TB_SETSTATE          (0x0400 + 17)
#define TBSTYLE_SEP           0x0001
#define TBSTATE_CHECKED       0x01
#define TBSTATE_PRESSED       0x02
#define TBSTATE_ENABLED       0x04
#define TBSTATE_INDETERMINATE 0x10
typedef struct tagTBBUTTON
{
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    BYTE bReserved[2];
    DWORD_PTR dwData;
    INT_PTR iString;
} TBBUTTON;

/* native menu (User32) APIs - dead, replaced by the ImGui menu bar (3.1) */
#define MF_BYPOSITION  0x00000400
#define MF_CHECKED     0x00000008
#define MF_UNCHECKED   0x00000000
#define MF_ENABLED     0x00000000
#define MF_GRAYED      0x00000001
#define MIIM_STATE   0x00000001
#define MIIM_ID      0x00000002
#define MIIM_TYPE    0x00000010
#define MIIM_FTYPE   0x00000100
#define MIIM_STRING  0x00000040
#define MFT_RADIOCHECK 0x00000200
#define MFS_DEFAULT    0x00001000
typedef struct tagMENUITEMINFOA
{
    UINT cbSize;
    UINT fMask;
    UINT fType;
    UINT fState;
    UINT wID;
    HMENU hSubMenu;
    HICON hbmpChecked;
    HICON hbmpUnchecked;
    ULONG_PTR dwItemData;
    LPSTR dwTypeData;
    UINT cch;
    HBITMAP hbmpItem;
} MENUITEMINFOA, MENUITEMINFO;

/* window-creation structures - dead, no real CreateWindowEx() call exists */
#define CW_USEDEFAULT ((int)0x80000000)
typedef struct tagWINDOWPOS
{
    HWND hwnd, hwndInsertAfter;
    int x, y, cx, cy;
    UINT flags;
} WINDOWPOS, *LPWINDOWPOS;
typedef struct tagCREATESTRUCTA
{
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy, cx, y, x;
    LONG style;
    LPCSTR lpszName;
    LPCSTR lpszClass;
    DWORD dwExStyle;
} CREATESTRUCTA, CREATESTRUCT, *LPCREATESTRUCT;
typedef struct tagMDICREATESTRUCTA
{
    LPCSTR szClass;
    LPCSTR szTitle;
    HANDLE hOwner;
    int x, y, cx, cy;
    DWORD style;
    LPARAM lParam;
} MDICREATESTRUCTA, MDICREATESTRUCT;
typedef struct tagMEASUREITEMSTRUCT
{
    UINT CtlType, CtlID, itemID;
    UINT itemWidth, itemHeight;
    DWORD_PTR itemData;
} MEASUREITEMSTRUCT;
typedef struct tagDRAWITEMSTRUCT
{
    UINT CtlType, CtlID, itemID, itemAction, itemState;
    HWND hwndItem;
    HDC hDC;
    RECT rcItem;
    DWORD_PTR itemData;
} DRAWITEMSTRUCT;

/* rebar/imagelist (Comctl32) structures - dead native toolbar code */
typedef struct tagREBARBANDINFOA { UINT cbSize; UINT fMask; int cxHeader; } REBARBANDINFOA, REBARBANDINFO, REBARBANDINFOW;
#define CCSIZEOF_STRUCT(structname, member) \
    (offsetof(structname, member) + sizeof(((structname *)0)->member))
typedef struct tagIMAGELISTDRAWPARAMS { DWORD cbSize; } IMAGELISTDRAWPARAMS;
#define IMAGELISTDRAWPARAMS_V3_SIZE sizeof(IMAGELISTDRAWPARAMS)
#define CDSIZEOF_STRUCT(structname, member) CCSIZEOF_STRUCT(structname, member)

#define MAKEPOINTS(l) (*((POINTS *)&(l)))

inline char *_vsnprintf(char *buf, size_t n, const char *fmt, va_list args)
    { return (char *)(intptr_t)vsnprintf(buf, n, fmt, args); }

/* File-open common dialog - dead: tadsfiledlg.cpp's live path never reaches
   the raw GetOpenFileName()/GetSaveFileName() fallback off Windows. */
#define OFN_HIDEREADONLY    0x00000004
#define OFN_ENABLESIZING    0x00800000
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_FILEMUSTEXIST   0x00001000
#define OFN_PATHMUSTEXIST   0x00000800
typedef UINT_PTR (APIENTRY *LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagOFNA
{
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter;
    LPSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPSTR lpstrFile;
    DWORD nMaxFile;
    LPSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCSTR lpstrInitialDir;
    LPCSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset, nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCSTR lpTemplateName;
} OPENFILENAMEA, OPENFILENAME;
inline BOOL GetOpenFileName(OPENFILENAME *) { return 0; }
inline BOOL GetSaveFileName(OPENFILENAME *) { return 0; }

/* LOGFONT font-family bits (used inline building a default LOGFONT) */
#define VARIABLE_PITCH 2
#define FF_ROMAN (1 << 4)

/* GetVersionEx() dwPlatformId values (win_sys_id_ is always left 0 off
   Windows - see CTadsApp::CTadsApp(), tadsapp.cpp) */
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT      2

/* interlocked increment/decrement + a critical section, backed for real by
   GCC atomics / pthreads - tadsmidi.h's CTadsMidiFilePlayer double-buffer
   refcounting/locking (the file implementing it, tadsmidi.cpp, is itself
   Windows-only pending a portable MIDI synth - migration.md 3.7 - but the
   header's class declaration is included unconditionally). */
inline LONG InterlockedIncrement(LONG *v) { return __sync_add_and_fetch(v, 1); }
inline LONG InterlockedDecrement(LONG *v) { return __sync_sub_and_fetch(v, 1); }
#include <pthread.h>
typedef pthread_mutex_t CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
    { pthread_mutex_init(cs, 0); }
inline void DeleteCriticalSection(CRITICAL_SECTION *cs)
    { pthread_mutex_destroy(cs); }
inline void EnterCriticalSection(CRITICAL_SECTION *cs)
    { pthread_mutex_lock(cs); }
inline void LeaveCriticalSection(CRITICAL_SECTION *cs)
    { pthread_mutex_unlock(cs); }
#define MM_MOM_OPEN  (0x3C0 + 1)
#define MM_MOM_CLOSE (0x3C0 + 2)
#define MM_MOM_DONE  (0x3C0 + 3)
#define MOM_DONE     MM_MOM_DONE

/* native window/menu/GDI API - dead no-op stand-ins */
inline BOOL DestroyWindow(HWND) { return 0; }
inline LONG SetWindowLong(HWND, int, LONG) { return 0; }
inline LONG GetWindowLong(HWND, int) { return 0; }
inline LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR) { return 0; }
inline LONG_PTR GetWindowLongPtr(HWND, int) { return 0; }
inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline LRESULT DefFrameProc(HWND, HWND, UINT, WPARAM, LPARAM) { return 0; }
inline LRESULT DefMDIChildProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline BOOL SetProp(HWND, LPCSTR, HANDLE) { return 0; }
inline HANDLE GetProp(HWND, LPCSTR) { return 0; }
inline HMENU GetMenu(HWND) { return 0; }
inline HMENU GetSystemMenu(HWND, BOOL) { return 0; }
inline short GetKeyState(int) { return 0; }
inline HWND GetParent(HWND) { return 0; }
inline LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline BOOL GetWindowRect(HWND, RECT *r) { if (r) *r = RECT(); return 0; }
inline BOOL MapWindowPoints(HWND, HWND, POINT *, UINT) { return 0; }
inline BOOL InvalidateRect(HWND, const RECT *, BOOL) { return 0; }
inline BOOL IsIconic(HWND) { return 0; }
inline int MessageBox(HWND, LPCSTR, LPCSTR, UINT) { return 0; }
inline int GetSystemMetrics(int) { return 0; }
inline UINT GetDoubleClickTime() { return 500; }
inline int LoadString(HINSTANCE, UINT, LPSTR buf, int n)
    { if (buf && n > 0) buf[0] = 0; return 0; }
inline BOOL GetMenuItemInfo(HMENU, UINT, BOOL, MENUITEMINFO *) { return 0; }
inline BOOL SetMenuItemInfo(HMENU, UINT, BOOL, const MENUITEMINFO *) { return 0; }
inline int GetMenuItemCount(HMENU) { return 0; }
inline UINT GetMenuItemID(HMENU, int) { return (UINT)-1; }
inline BOOL CheckMenuItem(HMENU, UINT, UINT) { return 0; }
inline BOOL EnableMenuItem(HMENU, UINT, UINT) { return 0; }
inline BOOL HideCaret(HWND) { return 0; }
inline BOOL ShowCaret(HWND) { return 0; }
inline HDC BeginPaint(HWND, PAINTSTRUCT *ps)
    { if (ps) *ps = PAINTSTRUCT(); return 0; }
inline BOOL EndPaint(HWND, const PAINTSTRUCT *) { return 0; }
inline HDC CreateCompatibleDC(HDC) { return 0; }
inline HBITMAP CreateCompatibleBitmap(HDC, int, int) { return 0; }
inline HGDIOBJ SelectObject(HDC, HGDIOBJ) { return 0; }
inline HPALETTE SelectPalette(HDC, HPALETTE, BOOL) { return 0; }
inline UINT RealizePalette(HDC) { return 0; }
inline BOOL DeleteDC(HDC) { return 0; }
inline BOOL DeleteObject(HGDIOBJ) { return 0; }
#define SRCCOPY 0x00CC0020
#define WHITE_BRUSH 0
inline BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD) { return 0; }
inline HGDIOBJ GetStockObject(int) { return 0; }
inline BOOL FillRect(HDC, const RECT *, HBRUSH) { return 0; }

/* OLE drag-and-drop registration - dead, see IDropTarget/IDropSource above;
   returning failure is exactly what makes CTadsWin::drop_target_register()
   correctly skip registering off Windows. */
#define CLSCTX_INPROC_SERVER 1
inline HRESULT RegisterDragDrop(HWND, IDropTarget *) { return E_FAIL; }
inline HRESULT RevokeDragDrop(HWND) { return E_FAIL; }
inline HRESULT CoCreateInstance(REFCLSID, IUnknown *, DWORD, REFIID, void **ppv)
    { if (ppv) *ppv = 0; return E_FAIL; }
#define DRAGDROP_S_DROP               ((HRESULT)0x00040100L)
#define DRAGDROP_S_CANCEL             ((HRESULT)0x00040101L)
#define DRAGDROP_S_USEDEFAULTCURSORS  ((HRESULT)0x00040102L)
inline HRESULT DoDragDrop(IDataObject *, IDropSource *, DWORD, DWORD *)
    { return DRAGDROP_S_CANCEL; }

/* notification codes / popup-menu tracking / GDI - dead native code */
#define NM_SETFOCUS  (-7)
#define NM_KILLFOCUS (-8)
#define TPM_RETURNCMD 0x0100
inline BOOL GetCursorPos(POINT *pt) { if (pt) { pt->x = 0; pt->y = 0; } return 0; }
inline int TrackPopupMenu(HMENU, UINT, int, int, int, HWND, const RECT *)
    { return 0; }
inline HWND GetDesktopWindow() { return 0; }
inline HDC GetDC(HWND) { return 0; }
inline int ReleaseDC(HWND, HDC) { return 0; }
#define BITSPIXEL 12
#define PLANES    14
inline int GetDeviceCaps(HDC, int) { return 0; }
#define DIB_RGB_COLORS 0
inline int GetDIBits(HDC, HBITMAP, UINT, UINT, void *, BITMAPINFO *, UINT)
    { return 0; }
inline int StretchDIBits(HDC, int, int, int, int, int, int, int, int,
                          const void *, const BITMAPINFO *, UINT, DWORD)
    { return 0; }
inline DWORD GetSysColor(int) { return 0; }
inline BOOL GetTextExtentPoint32(HDC, LPCSTR, int, SIZE *sz)
    { if (sz) { sz->cx = 0; sz->cy = 0; } return 0; }
inline BOOL GetTextExtentExPoint(HDC, LPCSTR, int, int, int *, int *, SIZE *sz)
    { if (sz) { sz->cx = 0; sz->cy = 0; } return 0; }

inline BOOL ShowWindow(HWND, int) { return 0; }
inline BOOL UpdateWindow(HWND) { return 0; }
inline BOOL ScrollWindow(HWND, int, int, const RECT *, const RECT *) { return 0; }
inline BOOL GetClientRect(HWND, RECT *r) { if (r) *r = RECT(); return 0; }
inline BOOL PtInRect(const RECT *, POINT) { return 0; }
inline HWND CreateWindowEx(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
                           HWND, HMENU, HINSTANCE, LPVOID)
    { return 0; }
inline BOOL SetMenu(HWND, HMENU) { return 0; }
inline LONG GetClassLong(HWND, int) { return 0; }
inline HANDLE GlobalGetAtomName(ATOM, LPSTR buf, int n)
    { if (buf && n > 0) buf[0] = 0; return 0; }
inline int lstrcmpi(LPCSTR a, LPCSTR b) { return strcasecmp(a, b); }
inline BOOL RemoveProp(HWND, LPCSTR) { return 0; }
inline LRESULT CallWindowProc(WNDPROC, HWND, UINT, WPARAM, LPARAM) { return 0; }

/* Windows hook API (SetWindowsHookEx et al) - dead, only used to intercept a
   native message pump that doesn't exist off Windows */
#define WH_MSGFILTER (-1)
#define WH_CBT 5
typedef LRESULT (CALLBACK *HOOKPROC)(int, WPARAM, LPARAM);
typedef void *HHOOK;
inline unsigned long GetCurrentThreadId() { return 0; }
inline HHOOK SetWindowsHookEx(int, HOOKPROC, HINSTANCE, unsigned long) { return 0; }
inline BOOL UnhookWindowsHookEx(HHOOK) { return 0; }
inline LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }
#define HCBT_CREATEWND 3
#define GCL_STYLE (-26)
#define CS_IME 0x00010000
typedef struct tagCBT_CREATEWNDA
{
    CREATESTRUCT *lpcs;
    HWND hwndInsertAfter;
} CBT_CREATEWNDA, CBT_CREATEWND, *LPCBT_CREATEWND;

#define SPI_GETWHEELSCROLLLINES 104
inline BOOL SystemParametersInfo(UINT, UINT, void *pv, UINT)
    { if (pv) *(UINT *)pv = 3; return 1; }
#define WHEEL_DELTA 120
#define MSGF_SCROLLBAR 5
#define WM_MBUTTONUP 0x0208
#define SM_CXVSCROLL 2
#define SM_CYHSCROLL 3
#define SIZE_RESTORED  0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SIZE_MAXSHOW   3
#define SIZE_MAXHIDE   4
#define GWLP_WNDPROC (-4)

typedef struct tagWINDOWPLACEMENT
{
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT;
inline BOOL GetWindowPlacement(HWND, WINDOWPLACEMENT *wp)
    { if (wp) *wp = WINDOWPLACEMENT(); return 0; }

/* Dialog control / common-dialog / font-enumeration API - dead, backs only
   tadsdlg.h's native property-sheet infrastructure (superseded by the ImGui
   dialogs, migration.md 3.3/5.3) and a couple of other never-taken paths. */
inline HWND GetDlgItem(HWND, int) { return 0; }
inline HWND SetFocus(HWND) { return 0; }
#define DT_SINGLELINE 0x0020
inline int DrawText(HDC, LPCSTR, int, RECT *, UINT) { return 0; }
#define PSM_QUERYSIBLINGS (0x0400 + 115)
typedef BOOL (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagPROPSHEETPAGEA
{
    DWORD dwSize;
    DWORD dwFlags;
    HINSTANCE hInstance;
    LPCSTR pszTemplate;
    LPCSTR pszIcon;
    LPCSTR pszTitle;
    DLGPROC pfnDlgProc;
    LPARAM lParam;
    void *pfnCallback;
} PROPSHEETPAGEA, PROPSHEETPAGE;
typedef struct tagPROPSHEETHEADERA
{
    DWORD dwSize;
    DWORD dwFlags;
    HWND hwndParent;
    HINSTANCE hInstance;
    LPCSTR pszCaption;
    UINT nPages;
    PROPSHEETPAGE *ppsp;
} PROPSHEETHEADERA, PROPSHEETHEADER;
typedef struct tagDELETEITEMSTRUCT
{
    UINT CtlType, CtlID, itemID;
    HWND hwndItem;
    DWORD_PTR itemData;
} DELETEITEMSTRUCT;
typedef struct tagCOMPAREITEMSTRUCT
{
    UINT CtlType, CtlID;
    HWND hwndItem;
    UINT itemID1;
    DWORD_PTR itemData1;
    UINT itemID2;
    DWORD_PTR itemData2;
} COMPAREITEMSTRUCT;
typedef struct tagENUMLOGFONTEXA { LOGFONT elfLogFont; } ENUMLOGFONTEXA, ENUMLOGFONTEX;
typedef struct tagNEWTEXTMETRICA
{
    LONG tmHeight;
    BYTE tmCharSet;
    BYTE tmPitchAndFamily;
} NEWTEXTMETRICA, NEWTEXTMETRIC;
#define TMPF_FIXED_PITCH 0x01
#define FF_SWISS  (2 << 4)
#define FF_SCRIPT (4 << 4)
#define FF_MODERN (3 << 4)
#define FF_DECORATIVE (5 << 4)
typedef int (CALLBACK *FONTENUMPROC)(void *, void *, DWORD, LPARAM);
inline int EnumFontFamiliesEx(HDC, LOGFONT *, FONTENUMPROC, LPARAM, DWORD)
    { return 0; }
typedef struct tagTOOLTIPTEXTA
    { NMHDR hdr; LPSTR lpszText; char szText[80]; HINSTANCE hinst; }
    TOOLTIPTEXTA, TOOLTIPTEXT;
#define MA_NOACTIVATE 3

/* Menu / font / accelerator API - dead, native-menu and GDI-font leftovers
   replaced by ImGui equivalents (migration.md 3.1/5.4/G) */
inline UINT GetACP() { return CP_ACP; }
inline HFONT CreateFontIndirect(const LOGFONT *) { return 0; }
#define DEFAULT_GUI_FONT 17
#define ODT_MENU 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define FW_NORMAL 400
#define FW_BOLD   700
#define LF_FACESIZE 32
typedef struct tagACCEL { BYTE fVirt; WORD key; WORD cmd; } ACCEL;
inline HACCEL CreateAcceleratorTable(ACCEL *, int) { return 0; }
inline BOOL DestroyAcceleratorTable(HACCEL) { return 0; }
inline HACCEL LoadAccelerators(HINSTANCE, LPCSTR) { return 0; }
inline BOOL IsDialogMessage(HWND, MSG *) { return 0; }
inline BOOL TranslateMessage(const MSG *) { return 0; }
inline LRESULT DispatchMessage(const MSG *) { return 0; }
inline int TranslateAccelerator(HWND, HACCEL, MSG *) { return 0; }
inline HDC GetWindowDC(HWND) { return 0; }
inline BOOL GetTextMetrics(HDC, TEXTMETRIC *tm)
    { if (tm) *tm = TEXTMETRIC(); return 0; }
inline int GetTextFace(HDC, int, LPSTR buf) { if (buf) buf[0] = 0; return 0; }
inline HWND SetCapture(HWND) { return 0; }
inline BOOL ReleaseCapture() { return 0; }
inline HWND GetCapture() { return 0; }
inline BOOL IsWindowEnabled(HWND) { return 0; }
inline HMENU GetSubMenu(HMENU, int) { return 0; }
#define MFS_DISABLED 0x00000003
#define MFS_GRAYED   0x00000003
#define MFT_STRING    0x00000000
#define MFT_BITMAP    0x00000004
#define MFT_SEPARATOR 0x00000800
#define MAKEWPARAM(lo, hi) ((WPARAM)MAKELONG(lo, hi))
inline LPSTR CharNextExA(WORD, LPCSTR p, DWORD) { return (LPSTR)p; }
inline LPSTR CharPrevExA(WORD, LPCSTR start, LPCSTR p, DWORD)
    { return (LPSTR)(p > start ? p - 1 : start); }
inline BOOL PostMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline UINT RegisterWindowMessage(LPCSTR) { return 0; }

/* virtual-key codes not already covered above */
#define VK_MENU     0x12
#define VK_CAPITAL  0x14
#define VK_LWIN     0x5B
#define VK_RWIN     0x5C
#define VK_LSHIFT   0xA0
#define VK_RSHIFT   0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU    0xA4
#define VK_RMENU    0xA5

/* non-client-metrics / shell-folder API - dead (Options dialog font default,
   "My Documents" lookup - not exercised without a real GUI font enumerator
   or shell off Windows) */
typedef struct tagNONCLIENTMETRICSA
{
    UINT cbSize;
    int iBorderWidth, iScrollWidth, iScrollHeight, iCaptionWidth,
        iCaptionHeight;
    LOGFONT lfCaptionFont;
    int iSmCaptionWidth, iSmCaptionHeight;
    LOGFONT lfSmCaptionFont;
    int iMenuWidth, iMenuHeight;
    LOGFONT lfMenuFont;
    LOGFONT lfStatusFont;
    LOGFONT lfMessageFont;
} NONCLIENTMETRICSA, NONCLIENTMETRICS;
#define SPI_GETNONCLIENTMETRICS 41
struct IMalloc: public IUnknown
{
    virtual void *Alloc(size_t) = 0;
    virtual void Free(void *) = 0;
};
typedef struct tagITEMIDLIST { BYTE mkid[2]; } ITEMIDLIST;
#define CSIDL_PERSONAL 0x0005
inline HRESULT SHGetSpecialFolderLocation(HWND, int, ITEMIDLIST **idl)
    { if (idl) *idl = 0; return E_FAIL; }
inline BOOL SHGetPathFromIDList(const ITEMIDLIST *, LPSTR buf)
    { if (buf) buf[0] = 0; return 0; }
inline HRESULT SHGetMalloc(IMalloc **m) { if (m) *m = 0; return E_FAIL; }

/* window-enumeration / dialog-box API - dead, backs only tadsdlg.h's
   native property-sheet/dialog-box infrastructure (migration.md 3.3/5.3) */
inline HWND GetTopWindow(HWND) { return 0; }
#define GW_HWNDNEXT 2
#define GW_OWNER 4
inline HWND GetWindow(HWND, UINT) { return 0; }
inline BOOL EnableWindow(HWND, BOOL) { return 0; }
inline HWND CreateDialogParam(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM)
    { return 0; }
inline int DialogBoxParam(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM)
    { return 0; }
#define WM_INITDIALOG 0x0110
#define WM_DELETEITEM 0x002D
#define WM_COMPAREITEM 0x0039
inline BOOL MoveWindow(HWND, int, int, int, int, BOOL) { return 0; }
inline BOOL EndDialog(HWND, INT_PTR) { return 0; }
#define CB_ADDSTRING     (0x0140 + 3)
#define CB_SETITEMDATA   (0x0140 + 26)
#define CB_RESETCONTENT  (0x0140 + 11)
#define CB_FINDSTRING    (0x0140 + 12)
#define CB_SETCURSEL     (0x0140 + 14)
#define OFN_ENABLEHOOK 0x00000020
#define OFN_EXPLORER   0x00080000
#define CDN_INITDONE ((UINT)-800)
#define PSN_SETACTIVE ((0U-2U)-0x0201)
#define PSM_CHANGED   (0x0400 + 102)
#define PSM_UNCHANGED (0x0400 + 103)
#define DWLP_MSGRESULT 0

/* GetTickCount() - CTadsCaret's blink timing (tadscar.cpp) polls this for
   real every frame (there's no WM_TIMER dispatch off Windows to fire a
   real timer callback), so unlike SetTimer/KillTimer below this needs to
   actually work, matching the std::chrono::steady_clock replacement
   already used for os_get_sys_clock_ms() elsewhere (migration.md 5.4/D). */
#include <chrono>
inline DWORD GetTickCount()
{
    using namespace std::chrono;
    return (DWORD)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}
inline UINT GetCaretBlinkTime() { return 530; }
typedef void (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
inline UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC) { return 0; }
inline BOOL KillTimer(HWND, UINT_PTR) { return 0; }
inline void SetRect(RECT *r, int l, int t, int rt, int b)
    { if (r) { r->left = l; r->top = t; r->right = rt; r->bottom = b; } }
inline BOOL InvertRect(HDC, const RECT *) { return 0; }

/* GDI drawing / palette / device-context API - dead, backs only leftover
   native drawing paths (palette creation, native popup menus, tooltips,
   version-resource lookup, ...) that ImGui rendering and the ported dialogs
   replaced (migration.md 3.1/3.2/5.3). */
typedef struct tagPALETTEENTRY
    { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY;
typedef struct tagLOGPALETTE
    { WORD palVersion, palNumEntries; PALETTEENTRY palPalEntry[1]; } LOGPALETTE;
inline HPALETTE CreatePalette(const LOGPALETTE *) { return 0; }
typedef struct tagLOGBRUSH { UINT lbStyle; COLORREF lbColor; LONG lbHatch; } LOGBRUSH;
#define BS_SOLID 0
inline HBRUSH CreateBrushIndirect(const LOGBRUSH *) { return 0; }
#define PS_NULL 5
inline HPEN CreatePen(int, int, COLORREF) { return 0; }
#define BLACK_PEN 7
#define WHITE_PEN 6
#define LTGRAY_BRUSH 1
inline BOOL LineTo(HDC, int, int) { return 0; }
inline BOOL MoveToEx(HDC, int, int, POINT *) { return 0; }
inline BOOL Polygon(HDC, const POINT *, int) { return 0; }
#define ETO_CLIPPED 0x0004
inline BOOL ExtTextOut(HDC, int, int, UINT, const RECT *, LPCSTR, UINT,
                       const int *)
    { return 0; }
#define OPAQUE 2
#define TRANSPARENT 1
inline COLORREF SetBkColor(HDC, COLORREF) { return 0; }
inline int SetBkMode(HDC, int) { return 0; }
inline COLORREF SetTextColor(HDC, COLORREF) { return 0; }
inline BOOL FrameRect(HDC, const RECT *, HBRUSH) { return 0; }
inline BOOL OffsetRect(RECT *r, int dx, int dy)
    { if (r) { r->left += dx; r->right += dx; r->top += dy; r->bottom += dy; }
      return 0; }
inline HBRUSH GetSysColorBrush(int) { return 0; }
#define COLOR_3DLIGHT 22
#define CS_SAVEBITS 0x0800
inline BOOL ClientToScreen(HWND, POINT *) { return 0; }
inline HWND WindowFromPoint(POINT) { return 0; }
inline BOOL IsWindowVisible(HWND) { return 0; }
inline BOOL BringWindowToTop(HWND) { return 0; }
inline HWND GetFocus() { return 0; }
inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return 0; }
#define SWP_NOACTIVATE 0x0010
#define SWP_NOMOVE     0x0002
#define SWP_NOSIZE     0x0001
#define HWND_TOP ((HWND)0)
inline BOOL SetWindowText(HWND, LPCSTR) { return 0; }
typedef struct tagWNDCLASSEXA { UINT cbSize; } WNDCLASSEXA, WNDCLASSEX;
inline BOOL GetClassInfo(HINSTANCE, LPCSTR, WNDCLASS *) { return 0; }
inline ATOM RegisterClass(const WNDCLASS *) { return 0; }
inline HMENU CreatePopupMenu() { return 0; }
inline BOOL DeleteMenu(HMENU, UINT, UINT) { return 0; }
inline BOOL ModifyMenu(HMENU, UINT, UINT, UINT_PTR, LPCSTR) { return 0; }
inline BOOL InsertMenuItem(HMENU, UINT, BOOL, const MENUITEMINFO *) { return 0; }
#define MF_BYCOMMAND 0x00000000
#define MF_STRING    0x00000000
inline int GetMenuString(HMENU, UINT, LPSTR buf, int n, UINT)
    { if (buf && n > 0) buf[0] = 0; return 0; }
#define MFS_CHECKED 0x00000008
#define MFS_ENABLED 0x00000000
#define MIIM_DATA 0x00000020
inline HMENU LoadMenu(HINSTANCE, LPCSTR) { return 0; }
inline BOOL DestroyMenu(HMENU) { return 0; }
inline BOOL CheckDlgButton(HWND, int, UINT) { return 0; }
inline UINT IsDlgButtonChecked(HWND, int) { return 0; }
#define BST_CHECKED   1
#define BST_UNCHECKED 0

/* registry API - dead, backs only the pre-M2 native registry preference
   store (tadssettings_w32.cpp is the live Windows backend; guit3 off
   Windows uses tadssettings_portable.cpp's INI file instead, migration.md
   5.4/C) and a couple of vestigial direct RegEnumKeyEx call sites
   (htmlpref.cpp/htmlgui.cpp) not yet routed through that interface. */
typedef void *HKEY;
#define HKEY_CURRENT_USER ((HKEY)1)
#define KEY_ALL_ACCESS 0x000F003F
#define ERROR_SUCCESS 0L
inline long RegCloseKey(HKEY) { return ERROR_SUCCESS; }
inline long RegCreateKeyEx(HKEY, LPCSTR, DWORD, LPSTR, DWORD, DWORD, void *,
                           HKEY *out, DWORD *)
    { if (out) *out = 0; return 1; }
inline long RegEnumKeyEx(HKEY, DWORD, LPSTR buf, DWORD *n, DWORD *, LPSTR,
                         DWORD *, void *)
    { if (buf && n && *n > 0) buf[0] = 0; return 1; }

/* process / crash-handler API - dead off Windows: exc_handler() (guimain.cpp)
   is real Win32 SEH and stays Windows-only regardless (migration.md 5.4/M). */
inline HANDLE GetCurrentProcess() { return 0; }
inline BOOL TerminateProcess(HANDLE, UINT) { return 0; }
struct EXCEPTION_POINTERS { void *dummy; };
typedef long (CALLBACK *LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS *);
inline LPTOP_LEVEL_EXCEPTION_FILTER
    SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER) { return 0; }

/* version-resource API - dead, guit3 has no embedded VERSIONINFO resource
   off Windows (no .rc compiler - migration.md 5.1/5.4/B) */
inline DWORD GetFileVersionInfoSize(LPCSTR, DWORD *) { return 0; }
inline BOOL GetFileVersionInfo(LPCSTR, DWORD, DWORD, void *) { return 0; }
typedef struct tagVS_FIXEDFILEINFO
    { DWORD dwFileVersionMS, dwFileVersionLS; } VS_FIXEDFILEINFO;
inline BOOL VerQueryValue(const void *, LPCSTR, void **buf, UINT *n)
    { if (buf) *buf = 0; if (n) *n = 0; return 0; }

/* Comctl32 toolbar/tooltip API - dead, replaced by the ImGui toolbar and
   tooltips (migration.md 3.1) */
typedef struct tagINITCOMMONCONTROLSEX { DWORD dwSize, dwICC; } INITCOMMONCONTROLSEX;
#define ICC_WIN95_CLASSES 0x000000FF
#define ICC_COOL_CLASSES  0x00000400
inline BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX *) { return 0; }
inline HWND CreateToolbarEx(HWND, DWORD, UINT, int, HINSTANCE, UINT,
                            const TBBUTTON *, int, int, int, int, int, UINT)
    { return 0; }
typedef struct tagTBBUTTONINFOA { UINT cbSize, dwMask; int idCommand; }
    TBBUTTONINFOA, TBBUTTONINFO;
#define TBIF_COMMAND 0x00000020
#define TB_GETBUTTONINFO   (0x0400 + 63)
#define TB_GETITEMRECT     (0x0400 + 29)
#define TB_AUTOSIZE        (0x0400 + 33)
#define TB_CHANGEBITMAP    (0x0400 + 43)
#define TB_DELETEBUTTON    (0x0400 + 22)
#define TB_SETEXTENDEDSTYLE (0x0400 + 84)
#define TBSTYLE_BUTTON      0x0000
#define TBSTYLE_DROPDOWN    0x0008
#define TBSTYLE_FLAT        0x0800
#define TBSTYLE_TOOLTIPS    0x0100
#define TBSTYLE_EX_DRAWDDARROWS 0x00000001
#define TBDDRET_DEFAULT 0
#define TBN_DROPDOWN (-702)
typedef struct tagNMTOOLBAR { NMHDR hdr; int iItem; } NMTOOLBAR;
#define NM_RCLICK (-5)
typedef struct tagTOOLINFOA
    { UINT cbSize; HWND hwnd; RECT rect; UINT_PTR uId; LPSTR lpszText; }
    TOOLINFOA, TOOLINFO;
#define TTM_ACTIVATE     (0x0400 + 1)
#define TTM_NEWTOOLRECT  (0x0400 + 52)
#define TTN_NEEDTEXT     (-530)
#define TTN_SHOW         (-521)
#define TTN_POP          (-522)

/* font-metrics API - dead (charset enumeration for GDI font selection,
   superseded by fontconfig/CoreText - migration.md 5.4/G) */
typedef struct tagCHARSETINFO { UINT ciCharset, ciACP; } CHARSETINFO;
#define EASTEUROPE_CHARSET 238
#define CP_SYMBOL 42
#define TCI_SRCCODEPAGE 2
inline BOOL TranslateCharsetInfo(DWORD *, CHARSETINFO *ci, DWORD)
    { if (ci) *ci = CHARSETINFO(); return 0; }
#define FF_DONTCARE 0
#define FIXED_PITCH 1

/* keyboard/mouse-event API - dead, guit3 reads input through GLFW/ImGui,
   not raw Win32 messages (migration.md 3.4) */
typedef short SHORT;
inline SHORT GetAsyncKeyState(int) { return 0; }
#define MOUSEEVENTF_LEFTDOWN  0x0002
#define MOUSEEVENTF_RIGHTDOWN 0x0008
inline void mouse_event(DWORD, DWORD, DWORD, DWORD, DWORD_PTR) { }
inline void PostQuitMessage(int) { }
inline BOOL PeekMessage(MSG *, HWND, UINT, UINT, UINT) { return 0; }
#define PM_NOREMOVE 0x0000
#define PM_REMOVE   0x0001
#define WM_KEYFIRST     0x0100
#define WM_KEYLAST      0x0109
#define WM_DEADCHAR     0x0103
#define WM_SYSDEADCHAR  0x0107
#define WM_SYSKEYUP     0x0105
#define VK_BACK    0x08
#define VK_CANCEL  0x03
#define VK_DELETE  0x2E
#define VK_DOWN    0x28
#define VK_END     0x23
#define VK_ESCAPE  0x1B
#define VK_F1  0x70
#define VK_F2  0x71
#define VK_F3  0x72
#define VK_F4  0x73
#define VK_F5  0x74
#define VK_F6  0x75
#define VK_F7  0x76
#define VK_F8  0x77
#define VK_F9  0x78
#define VK_F10 0x79
#define VK_HELP  0x2F
#define VK_HOME  0x24
#define VK_INSERT 0x2D
#define VK_LEFT  0x25
#define VK_NEXT  0x22
#define VK_PRIOR 0x21
#define VK_RIGHT 0x27
#define VK_SCROLL 0x91
#define VK_UP    0x26

/* system-menu commands - dead */
#define SC_MAXIMIZE 0xF030
#define SC_MINIMIZE 0xF020
#define SC_RESTORE  0xF120
#define SC_SIZE     0xF000
#define GW_CHILD 5

inline UINT RegisterClipboardFormat(LPCSTR) { return 0; }
inline BOOL SetCurrentDirectory(LPCSTR) { return 0; }
#define MB_TASKMODAL 0x00002000L
inline HRESULT CoInitialize(void *) { return S_OK; }
inline void CoUninitialize() { }

#define HWND_BROADCAST ((HWND)(intptr_t)0xffff)
#define _snprintf snprintf
#define MB_ICONEXCLAMATION MB_ICONWARNING
inline DWORD GetModuleFileName(HINSTANCE, LPSTR buf, DWORD n)
    { if (buf && n > 0) buf[0] = 0; return 0; }

#endif /* !_WIN32 */

#endif /* TADSPLAT_H */
