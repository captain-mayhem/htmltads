/*
 *   tadssettings.h - guit3 persistent settings store
 *
 *   A neutral, windows.h-free home for the small key/value settings store the
 *   preferences layer needs: the app-wide options, the per-theme ("profile")
 *   settings trees, and the per-game theme associations.  It replaces the
 *   direct use of CTadsRegistry (tadsreg.cpp) and the raw RegEnumKeyEx /
 *   RegDeleteKey / RegEnumValue calls scattered through htmlpref.cpp and
 *   htmlgui.cpp.
 *
 *   The interface is deliberately the same open / query / set /
 *   enumerate-subkeys / enumerate-values / delete shape the registry code
 *   already had - in particular the *enumeration* primitives are first-class
 *   here, because the theme-profile feature is built on "list the child keys
 *   of Settings\Profiles" and a flat key/value file would not give that for
 *   free.
 *
 *   One backend is selected per platform by CMake.  The Win32 backend
 *   (tadssettings_w32.cpp) is CTadsRegistry lifted verbatim, so the Windows
 *   build is byte-identical; the non-Windows backend
 *   (tadssettings_portable.cpp) is an INI-style file under
 *   $XDG_CONFIG_HOME / ~/Library/Preferences - migration.md's M3 work,
 *   landed but unverified until there's a real non-Windows build to run it
 *   on (M4).  See section 5.4 item C.  This mirrors the
 *   os_font_family_is_present() / guios.h precedent.
 */

#ifndef TADSSETTINGS_H
#define TADSSETTINGS_H

#include <stddef.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif


/*
 *   An opaque handle to an open settings key.  On Windows this is really an
 *   HKEY; neutral code must not assume anything about it beyond "null means
 *   failure".
 */
typedef struct tads_settings_key_opaque *tads_settings_key_t;


/*
 *   Result of a single enumeration step (enum_subkeys / enum_str_values).
 */
enum tads_settings_enum_t
{
    /* an entry was returned in the caller's buffer(s) */
    TADS_SETTINGS_ENUM_OK = 1,

    /* there is no entry at this index - stop enumerating */
    TADS_SETTINGS_ENUM_END = 0,

    /* an entry exists here but isn't representable (e.g. a non-string
       registry value); skip it and try the next index */
    TADS_SETTINGS_ENUM_SKIP = -1
};


class CTadsSettings
{
public:
    /*
     *   Open the settings key at the given backslash-delimited path, relative
     *   to the current user's settings root (Windows: HKEY_CURRENT_USER).  If
     *   'create' is true the key and any missing parents are created;
     *   otherwise a missing key yields a null return.  Close the result with
     *   close_key().
     */
    static tads_settings_key_t open_key(const textchar_t *path, int create);

    /* close a key opened with open_key() */
    static void close_key(tads_settings_key_t key);

    /* is a value with the given name set under this key? */
    static int value_exists(tads_settings_key_t key, const textchar_t *valname);

    /* read a value; the query_key_* family matches CTadsRegistry exactly */
    static long query_key_long(tads_settings_key_t key,
                               const textchar_t *valname);
    static size_t query_key_str(tads_settings_key_t key,
                                const textchar_t *valname,
                                textchar_t *buf, size_t bufsiz);
    static int query_key_bool(tads_settings_key_t key,
                              const textchar_t *valname);
    static size_t query_key_binary(tads_settings_key_t key,
                                   const textchar_t *valname,
                                   void *buf, size_t bufsiz);

    /* write a value */
    static void set_key_long(tads_settings_key_t key,
                             const textchar_t *valname, long val);
    static void set_key_str(tads_settings_key_t key, const textchar_t *valname,
                            const textchar_t *str, size_t len);
    static void set_key_bool(tads_settings_key_t key,
                             const textchar_t *valname, int val);
    static void set_key_binary(tads_settings_key_t key,
                               const textchar_t *valname,
                               void *buf, size_t bufsiz);

    /*
     *   Enumerate this key's immediate child keys.  Copy the 'idx'th subkey
     *   name (0-based) into 'buf' and return TADS_SETTINGS_ENUM_OK; return
     *   TADS_SETTINGS_ENUM_END once 'idx' is past the last subkey.  The set of
     *   subkeys must not be modified during a scan.
     */
    static int enum_subkeys(tads_settings_key_t key, unsigned int idx,
                            textchar_t *buf, size_t bufsiz);

    /*
     *   Enumerate this key's string values.  Copy the 'idx'th value's name
     *   into 'namebuf' and its string data into 'valbuf'.  Returns
     *   TADS_SETTINGS_ENUM_OK on success, TADS_SETTINGS_ENUM_END at the end of
     *   the scan, or TADS_SETTINGS_ENUM_SKIP if the value at this index is not
     *   a string (the caller should just move on to the next index).
     */
    static int enum_str_values(tads_settings_key_t key, unsigned int idx,
                               textchar_t *namebuf, size_t namebufsiz,
                               textchar_t *valbuf, size_t valbufsiz);

    /*
     *   Delete the key at the given path (same path convention as open_key()).
     *   The key must have no child keys of its own.  Returns zero on success,
     *   non-zero on failure.
     */
    static int delete_key(const textchar_t *path);
};

#endif /* TADSSETTINGS_H */
