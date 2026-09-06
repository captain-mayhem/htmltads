/*
 *   tadssettings_w32.cpp - Win32 (registry) backend for the guit3 settings
 *   store (tadssettings.h)
 *
 *   Every function here is the corresponding CTadsRegistry method (tadsreg.cpp)
 *   lifted unchanged, with only the two things the neutral interface hides
 *   removed: the explicit base-key argument (always HKEY_CURRENT_USER at the
 *   call sites) and the create-disposition out-parameter (never read).  The
 *   value read/write helpers are byte-for-byte the originals.  A future
 *   non-Windows port supplies its own file implementing tadssettings.h from an
 *   INI/JSON file; CMake picks exactly one backend per build.  See
 *   migration.md 5.4 item C.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSSETTINGS_H
#include "tadssettings.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Open a key (relative to HKEY_CURRENT_USER), creating it if requested.
 */
tads_settings_key_t CTadsSettings::open_key(const textchar_t *path, int create)
{
    LONG err;
    HKEY ret_key;
    DWORD disposition;

    if (create)
    {
        err = RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, 0, 0, KEY_ALL_ACCESS,
                             0, &ret_key, &disposition);
    }
    else
    {
        err = RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_ALL_ACCESS,
                           &ret_key);
    }

    /* return the key if we were successful */
    return (err == ERROR_SUCCESS) ? (tads_settings_key_t)ret_key : 0;
}

/*
 *   Close a key
 */
void CTadsSettings::close_key(tads_settings_key_t key)
{
    RegCloseKey((HKEY)key);
}

/*
 *   Get a key's value (private helper - matches CTadsRegistry::query_key)
 */
static int reg_query_key(tads_settings_key_t key, const textchar_t *valname,
                         textchar_t *buf, size_t *bufsiz)
{
    DWORD typ;
    DWORD datasiz;

    /* query the value into the buffer */
    datasiz = (DWORD)*bufsiz;
    if (RegQueryValueEx((HKEY)key, valname, 0, &typ, (BYTE *)buf, &datasiz)
        != ERROR_SUCCESS || typ != REG_SZ)
        return 1;

    /* set the size that we read */
    *bufsiz = (size_t)datasiz;

    /* success */
    return 0;
}

/*
 *   Get a key's value as a long
 */
long CTadsSettings::query_key_long(tads_settings_key_t key,
                                   const textchar_t *valname)
{
    char buf[128];
    size_t bufsiz = sizeof(buf);

    /* get the key text */
    if (reg_query_key(key, valname, buf, &bufsiz))
        return 0;

    /* parse the string into a long */
    return get_atol(buf);
}

/*
 *   Get a key's value as a boolean
 */
int CTadsSettings::query_key_bool(tads_settings_key_t key,
                                  const textchar_t *valname)
{
    char buf[128];
    size_t bufsiz = sizeof(buf);

    /* get the key text */
    if (reg_query_key(key, valname, buf, &bufsiz))
        return 0;

    /* if it starts with 'y' or 'Y', it's true, otherwise it's false */
    return (buf[0] == 'y' || buf[0] == 'Y');
}

/*
 *   Get a key as a string
 */
size_t CTadsSettings::query_key_str(tads_settings_key_t key,
                                    const textchar_t *valname,
                                    textchar_t *buf, size_t bufsiz)
{
    /* get the key text */
    if (reg_query_key(key, valname, buf, &bufsiz))
    {
        /* no such value - clear the buffer and indicate an empty string */
        buf[0] = '\0';
        return 0;
    }

    /* return the length, minus the null terminator */
    return bufsiz - 1;
}

/*
 *   Get a key value as binary data
 */
size_t CTadsSettings::query_key_binary(tads_settings_key_t key,
                                       const textchar_t *valname,
                                       void *buf, size_t bufsiz)
{
    DWORD typ;
    DWORD datasiz;

    datasiz = (DWORD)bufsiz;
    if (RegQueryValueEx((HKEY)key, valname, 0, &typ, (BYTE *)buf, &datasiz)
        != ERROR_SUCCESS || typ != REG_BINARY)
        return 0;

    /* return the length of the buffer read */
    return (size_t)datasiz;
}

void CTadsSettings::set_key_long(tads_settings_key_t key,
                                 const textchar_t *valname, long val)
{
    textchar_t buf[128];

    /* generate a string for the number and save the string */
    sprintf(buf, "%ld", val);
    RegSetValueEx((HKEY)key, valname, 0, REG_SZ,
                  (BYTE *)buf, get_strlen(buf) + sizeof(textchar_t));
}

void CTadsSettings::set_key_str(tads_settings_key_t key,
                                const textchar_t *valname,
                                const textchar_t *str, size_t len)
{
    CStringBuf buf(str, len);
    RegSetValueEx((HKEY)key, valname, 0, REG_SZ,
                  (BYTE *)buf.get(), len + sizeof(textchar_t));
}

void CTadsSettings::set_key_bool(tads_settings_key_t key,
                                 const textchar_t *valname, int val)
{
    textchar_t *valstr;
    size_t vallen;

    if (val)
    {
        valstr = "Yes";
        vallen = 4 * sizeof(textchar_t);
    }
    else
    {
        valstr = "No";
        vallen = 3 * sizeof(textchar_t);
    }

    RegSetValueEx((HKEY)key, valname, 0, REG_SZ, (BYTE *)valstr, vallen);
}

void CTadsSettings::set_key_binary(tads_settings_key_t key,
                                   const textchar_t *valname,
                                   void *buf, size_t bufsiz)
{
    RegSetValueEx((HKEY)key, valname, 0, REG_BINARY, (BYTE *)buf, bufsiz);
}


/*
 *   Check to see if a value is set
 */
int CTadsSettings::value_exists(tads_settings_key_t key,
                                const textchar_t *valname)
{
    DWORD datasize;
    DWORD datatype;

    return (RegQueryValueEx((HKEY)key, valname, 0, &datatype, 0, &datasize)
            == ERROR_SUCCESS);
}


/* ------------------------------------------------------------------------ */
/*
 *   Enumerate child keys - the RegEnumKeyEx loop that opt_refresh_profile_list()
 *   (htmlpref.cpp) and render_themes_menu_items() (htmlgui.cpp) each had inline.
 */
int CTadsSettings::enum_subkeys(tads_settings_key_t key, unsigned int idx,
                                textchar_t *buf, size_t bufsiz)
{
    DWORD len = (DWORD)bufsiz;
    FILETIME ft;

    return RegEnumKeyEx((HKEY)key, idx, buf, &len, 0, 0, 0, &ft)
               == ERROR_SUCCESS
           ? TADS_SETTINGS_ENUM_OK
           : TADS_SETTINGS_ENUM_END;
}

/*
 *   Enumerate string values - the RegEnumValue loop from rename_profile_refs()
 *   (htmlgui.cpp), which skipped (rather than stopped at) non-REG_SZ values.
 */
int CTadsSettings::enum_str_values(tads_settings_key_t key, unsigned int idx,
                                   textchar_t *namebuf, size_t namebufsiz,
                                   textchar_t *valbuf, size_t valbufsiz)
{
    DWORD nmlen = (DWORD)namebufsiz;
    DWORD vallen = (DWORD)valbufsiz;
    DWORD typ;

    if (RegEnumValue((HKEY)key, idx, namebuf, &nmlen, 0, &typ,
                     (BYTE *)valbuf, &vallen) != ERROR_SUCCESS)
        return TADS_SETTINGS_ENUM_END;

    return (typ == REG_SZ)
           ? TADS_SETTINGS_ENUM_OK
           : TADS_SETTINGS_ENUM_SKIP;
}


/* ------------------------------------------------------------------------ */
/*
 *   Delete a (leaf) key.  The one live caller - the Options dialog's "Delete
 *   Theme" button (htmlpref.cpp) - used the shallow RegDeleteKey directly
 *   rather than CTadsRegistry::delete_key's recursive form, so this preserves
 *   that exactly: a profile key holds only values, never child keys.
 */
int CTadsSettings::delete_key(const textchar_t *path)
{
    return RegDeleteKey(HKEY_CURRENT_USER, path) != ERROR_SUCCESS;
}
