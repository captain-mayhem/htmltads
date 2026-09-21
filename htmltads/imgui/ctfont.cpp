/*
 *   ctfont.cpp - macOS (CoreText) backend for the guit3 font platform hooks
 *   declared in tadsfont.h (os_font_family_is_present() /
 *   os_font_data_for_name()).
 *
 *   The macOS counterpart of guifont.cpp's Win32 implementation (fcfont.cpp
 *   is the Linux one) - GDI's EnumFontFamiliesEx() and
 *   CreateFontIndirect()+GetFontData() become CTFontManagerCopyAvailable-
 *   FontFamilyNames() and CTFontCreateWithName()+CTFontCopyAttribute(...URL).
 *   CMake selects exactly one backend per build - see
 *   htmltads/imgui/CMakeLists.txt and migration.md 5.4/G.  Pure C++ against
 *   CoreText/CoreFoundation's C API - no Objective-C needed, so this
 *   compiles as a plain .cpp.
 *
 *   UNVERIFIED: there is no Mac build of guit3 and no macOS toolchain
 *   available to this change (migration.md M4 is what makes a real non-
 *   Windows build possible at all), so unlike fcfont.cpp - which was
 *   syntax-checked against a real fontconfig install - this file has only
 *   been checked by inspection against the CoreText API.  Revisit once
 *   there's an actual Mac build to compile and run it against.
 */

#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>
#include <imgui/imgui.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   os_font_family_is_present() - is a font family with this name actually
 *   installed?  CTFontManagerCopyAvailableFontFamilyNames() is the direct
 *   macOS equivalent of enumerating installed families: unlike creating a
 *   CTFont/CTFontDescriptor by name (which always succeeds by substituting
 *   the system fallback font), it only lists families that genuinely exist.
 */
int os_font_family_is_present(const char *fontname, size_t len)
{
    CFStringRef target = CFStringCreateWithBytes(
        kCFAllocatorDefault, (const UInt8 *)fontname, (CFIndex)len,
        kCFStringEncodingUTF8, false);
    if (target == 0)
        return 0;

    CFArrayRef families = CTFontManagerCopyAvailableFontFamilyNames();
    int present = 0;
    if (families != 0)
    {
        CFIndex n = CFArrayGetCount(families);
        for (CFIndex i = 0 ; i < n ; ++i)
        {
            CFStringRef fam = (CFStringRef)CFArrayGetValueAtIndex(families, i);
            if (CFStringCompare(fam, target, kCFCompareCaseInsensitive)
                == kCFCompareEqualTo)
            {
                present = 1;
                break;
            }
        }
        CFRelease(families);
    }

    CFRelease(target);
    return present;
}


/* ------------------------------------------------------------------------ */
/*
 *   os_font_data_for_name() - resolve a family/weight/italic request to the
 *   raw bytes of the TrueType/OpenType file CoreText's matcher picks for it.
 *
 *   Goes through CTFontCreateCopyWithSymbolicTraits() rather than building a
 *   weight/slant attribute dictionary from scratch: it asks CoreText for the
 *   bold and/or italic variant of the named family in exactly the same
 *   "give me the closest match, and tell me if there isn't one" shape
 *   CreateFontIndirect() has on Windows.  A boolean bold flag is coarser
 *   than the Win32 lfWeight scale (100-900), but every live call site in
 *   guit3 only ever requests plain or bold weights (see
 *   CHtmlSysWin_win32::get_font()), so nothing finer is needed.
 *
 *   'charset' (a Win32 xxx_CHARSET value) has no CoreText equivalent - it
 *   selects a legacy code page's glyph subset, not a family or style - so
 *   it's intentionally unused here, matching fcfont.cpp.
 */
unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int charset, size_t *data_size)
{
    if (name == 0)
        name = "";

    CFStringRef cfname = CFStringCreateWithCString(
        kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    if (cfname == 0)
        return 0;

    CTFontRef base = CTFontCreateWithName(cfname, 12.0, 0);
    CFRelease(cfname);
    if (base == 0)
        return 0;

    CTFontSymbolicTraits want = 0;
    if (italic)
        want |= kCTFontTraitItalic;
    if (weight >= 700 /* FW_BOLD */)
        want |= kCTFontTraitBold;

    CTFontRef styled = base;
    if (want != 0)
    {
        CTFontRef variant = CTFontCreateCopyWithSymbolicTraits(
            base, 12.0, 0, want, kCTFontTraitItalic | kCTFontTraitBold);
        if (variant != 0)
        {
            CFRelease(base);
            styled = variant;
        }
        /* no such bold/italic variant - fall back to the plain face, same
           as CreateFontIndirect() substituting when an exact style is
           unavailable */
    }

    unsigned char *buffer = 0;
    CFURLRef url = (CFURLRef)CTFontCopyAttribute(styled, kCTFontURLAttribute);
    if (url != 0)
    {
        char path[4096];
        if (CFURLGetFileSystemRepresentation(url, true,
                                             (UInt8 *)path, sizeof(path)))
        {
            /*
             *   Read the whole file into an ImGui::MemAlloc()'d buffer, the
             *   same allocator the Win32 backend uses - the caller hands
             *   this straight to ImFontAtlas::AddFontFromMemoryTTF() (which
             *   takes ownership) or frees it with ImGui::MemFree() if
             *   unused.  Font-collection (.ttc) face selection isn't
             *   captured here, same caveat as fcfont.cpp.
             */
            FILE *fp = fopen(path, "rb");
            if (fp != 0)
            {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                if (size > 0)
                {
                    buffer = (unsigned char *)ImGui::MemAlloc((size_t)size);
                    if (fread(buffer, 1, (size_t)size, fp) == (size_t)size)
                    {
                        *data_size = (size_t)size;
                    }
                    else
                    {
                        ImGui::MemFree(buffer);
                        buffer = 0;
                    }
                }

                fclose(fp);
            }
        }
        CFRelease(url);
    }

    CFRelease(styled);
    return buffer;
}


/* ------------------------------------------------------------------------ */
/*
 *   os_enum_font_families() - report every installed family once, in the
 *   ENUMLOGFONTEX/NEWTEXTMETRIC shape Win32's EnumFontFamiliesEx() uses, so
 *   CHtmlPreferences::cust_refresh_font_lists() (htmlpref.cpp) can sort
 *   families into its serif/sans/script/typewriter buckets the same way on
 *   every platform - see fcfont.cpp's own os_enum_font_families() for the
 *   full rationale (that comment applies here too, CoreText APIs standing
 *   in for fontconfig/FreeType).
 *
 *   CTFontManagerCopyAvailableFontFamilyNames() gives the family list
 *   directly (same call os_font_family_is_present() above already uses).
 *   For each family, kCTFontSymbolicTrait on a throwaway CTFontDescriptor
 *   gives both fields in one shot: kCTFontTraitMonoSpace maps straight to
 *   FF_MODERN, matching fcfont.cpp treating any monospaced family as
 *   "typewriter" outright; failing that, the trait's stylistic-class nibble
 *   (kCTFontTraitClassMask, i.e. bits 28-31) is CoreText's own copy of the
 *   OS/2 table's sFamilyClass high byte, classified with the same mapping
 *   fcfont.cpp falls back to.
 *
 *   CAVEAT (untested on real macOS fonts): fcfont.cpp's own testing against
 *   this repo's actual Linux font set found sFamilyClass left unset (0) by
 *   nearly every real TrueType/OpenType family, and had to add a PANOSE-
 *   based classification (also read from the OS/2 table, but a different
 *   field - see fcfont.cpp's os_enum_font_families() comment) to get
 *   useful coverage. CoreText doesn't expose PANOSE through a public
 *   symbolic-trait API the way it does the class nibble, so this backend
 *   only has the weaker signal; if a real Mac build (§5.4/G) shows most
 *   families coming back FF_DONTCARE the way fcfont.cpp's did before that
 *   fix, the same PANOSE bytes are reachable here too via
 *   CTFontCopyTable(kCTFontTableOS2) - parse the raw OS/2 table the same
 *   way FT_Get_Sfnt_Table()'s TT_OS2 struct does.
 */
namespace {

int ct_traits_to_family(CTFontDescriptorRef desc)
{
    CFDictionaryRef traits = (CFDictionaryRef)CTFontDescriptorCopyAttribute(
        desc, kCTFontTraitsAttribute);
    if (traits == 0)
        return 0;

    int family = 0;
    CFNumberRef symNum = (CFNumberRef)CFDictionaryGetValue(
        traits, kCTFontSymbolicTrait);
    uint32_t sym = 0;
    if (symNum != 0 && CFNumberGetValue(symNum, kCFNumberSInt32Type, &sym))
    {
        if (sym & kCTFontTraitMonoSpace)
        {
            family = FF_MODERN;
        }
        else
        {
            switch (sym & kCTFontTraitClassMask)
            {
            case kCTFontClassOldStyleSerifs:
            case kCTFontClassTransitionalSerifs:
            case kCTFontClassModernSerifs:
            case kCTFontClassClarendonSerifs:
            case kCTFontClassSlabSerifs:
            case kCTFontClassFreeformSerifs:
                family = FF_ROMAN;
                break;

            case kCTFontClassSansSerif:
                family = FF_SWISS;
                break;

            case kCTFontClassOrnamentals:
                family = FF_DECORATIVE;
                break;

            case kCTFontClassScripts:
                family = FF_SCRIPT;
                break;

            default:
                family = 0;
                break;
            }
        }
    }

    CFRelease(traits);
    return family;
}

} // namespace

void os_enum_font_families(unsigned int charset_id, FONTENUMPROC callback,
                           LPARAM lparam)
{
    CFArrayRef families = CTFontManagerCopyAvailableFontFamilyNames();
    if (families == 0)
        return;

    CFIndex n = CFArrayGetCount(families);
    for (CFIndex i = 0 ; i < n ; ++i)
    {
        CFStringRef fam = (CFStringRef)CFArrayGetValueAtIndex(families, i);

        ENUMLOGFONTEX elf;
        memset(&elf, 0, sizeof(elf));
        if (!CFStringGetCString(fam, elf.elfLogFont.lfFaceName,
                                sizeof(elf.elfLogFont.lfFaceName),
                                kCFStringEncodingUTF8))
            continue;

        int fam_family = 0;
        CTFontDescriptorRef desc =
            CTFontDescriptorCreateWithNameAndSize(fam, 12.0);
        if (desc != 0)
        {
            fam_family = ct_traits_to_family(desc);
            CFRelease(desc);
        }
        elf.elfLogFont.lfPitchAndFamily = (BYTE)fam_family;

        NEWTEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        tm.tmCharSet = (BYTE)charset_id;
        if (fam_family != FF_MODERN)
            tm.tmPitchAndFamily |= TMPF_FIXED_PITCH;

        if (!callback(&elf, &tm, 0, lparam))
            break;
    }

    CFRelease(families);
}
