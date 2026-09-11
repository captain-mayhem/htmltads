#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadskb.cpp - Windows keyboard utilities
Function
  
Notes
  
Modified
  10/27/06 MJRoberts  - Creation
*/

#include <GLFW/glfw3.h>

#include "tadshtml.h"
#include "htmlhash.h"
#include "tadskb.h"
#include "guios.h"

/* ------------------------------------------------------------------------ */
/*
 *   Simple hash entry for a keyname-to-vkey mapping 
 */
class KeynameEntry: public CHtmlHashEntryCI
{
public:
    KeynameEntry(const textchar_t *name, int vkey)
        : CHtmlHashEntryCI(name, get_strlen(name), FALSE)
    {
        this->vkey = vkey;
        this->shift = 0;
    }

    KeynameEntry(const textchar_t *name, int vkey, int shift)
        : CHtmlHashEntryCI(name, get_strlen(name), TRUE)
    {
        this->vkey = vkey;
        this->shift = shift;
    }

    int vkey;
    int shift;
};

/* ------------------------------------------------------------------------ */
/*
 *   construction 
 */
CTadsKeyboard::CTadsKeyboard()
{
    struct keyname_t
    {
        int vkey;
        const char *name;
    };

    /*
     *   The standard keys, by canonical (GLFW_KEY_*) code.  VK_SELECT,
     *   VK_PRINT, VK_EXECUTE and VK_HELP were dropped from the original
     *   VK_xxx table here - GLFW has no equivalent constants for them and,
     *   being rarely-implemented OEM keys even on Windows, no caller
     *   exercised them.
     */
    static const keyname_t keyname[] =
    {
        { GLFW_KEY_BACKSPACE, "Backspace" },
        { GLFW_KEY_TAB, "Tab" },
        { GLFW_KEY_ENTER, "Enter" },
        { GLFW_KEY_PAUSE, "Pause" },
        { GLFW_KEY_ESCAPE, "Esc" },
        { GLFW_KEY_SPACE, "Space" },
        { GLFW_KEY_PAGE_UP, "PageUp" },
        { GLFW_KEY_PAGE_DOWN, "PageDown" },
        { GLFW_KEY_END, "End" },
        { GLFW_KEY_HOME, "Home" },
        { GLFW_KEY_LEFT, "Left" },
        { GLFW_KEY_UP, "Up" },
        { GLFW_KEY_RIGHT, "Right" },
        { GLFW_KEY_DOWN, "Down" },
        { GLFW_KEY_PRINT_SCREEN, "PrintScreen" },
        { GLFW_KEY_INSERT, "Insert" },
        { GLFW_KEY_DELETE, "Delete" },
        { GLFW_KEY_KP_0, "Num0" },
        { GLFW_KEY_KP_1, "Num1" },
        { GLFW_KEY_KP_2, "Num2" },
        { GLFW_KEY_KP_3, "Num3" },
        { GLFW_KEY_KP_4, "Num4" },
        { GLFW_KEY_KP_5, "Num5" },
        { GLFW_KEY_KP_6, "Num6" },
        { GLFW_KEY_KP_7, "Num7" },
        { GLFW_KEY_KP_8, "Num8" },
        { GLFW_KEY_KP_9, "Num9" },
        { GLFW_KEY_KP_MULTIPLY, "Num*" },
        { GLFW_KEY_KP_ADD, "Num+" },
        { GLFW_KEY_KP_SUBTRACT, "Num-" },
        { GLFW_KEY_KP_DECIMAL, "Num." },
        { GLFW_KEY_KP_DIVIDE, "Num/" },
        { GLFW_KEY_F1, "F1" },
        { GLFW_KEY_F2, "F2" },
        { GLFW_KEY_F3, "F3" },
        { GLFW_KEY_F4, "F4" },
        { GLFW_KEY_F5, "F5" },
        { GLFW_KEY_F6, "F6" },
        { GLFW_KEY_F7, "F7" },
        { GLFW_KEY_F8, "F8" },
        { GLFW_KEY_F9, "F9" },
        { GLFW_KEY_F10, "F10" },
        { GLFW_KEY_F11, "F11" },
        { GLFW_KEY_F12, "F12" },
        { GLFW_KEY_F13, "F13" },
        { GLFW_KEY_F14, "F14" },
        { GLFW_KEY_F15, "F15" },
        { GLFW_KEY_F16, "F16" },
        { GLFW_KEY_F17, "F17" },
        { GLFW_KEY_F18, "F18" },
        { GLFW_KEY_F19, "F19" },
        { GLFW_KEY_F20, "F20" },
        { GLFW_KEY_F21, "F21" },
        { GLFW_KEY_F22, "F22" },
        { GLFW_KEY_F23, "F23" },
        { GLFW_KEY_F24, "F24" },

        { 0, 0 }
    };
    const keyname_t *k;

    /*
     *   The keyboard layout doesn't assign definitive key codes to
     *   punctuation keys - these key assignments vary by keyboard.  So, we
     *   need to ask the OS for the current mappings to these keys, via
     *   os_char_to_key() (guios.h).
     */
    static const char xch[] = "`~-_=+[{]}|;:'\"\\,<.>/?:!@#$%^&*()";
    const char *p;

    /* build the hash table for mapping key names to virtual key codes */
    hash_keyname = new CHtmlHashTable(128, new CHtmlHashFuncCI());
    for (k = keyname ; k->name != 0 ; ++k)
        hash_keyname->add(new KeynameEntry(k->name, k->vkey));

    /* build the hash table for mapping virtual key codes to key names */
    hash_vkey = new CHtmlHashTable(128, new CHtmlHashFuncUInt());
    for (k = keyname ; k->name != 0 ; ++k)
        hash_vkey->add(new CHtmlHashEntryUInt(k->vkey, (void *)k->name));

    /* add the keys with varying canonical-key mappings */
    memset(shiftmap, 0, sizeof(shiftmap));
    for (p = xch ; *p != 0 ; ++p)
    {
        /* get the canonical key that generates this character */
        int shift = 0;
        int vkey = os_char_to_key(*p, &shift);

        /* if there's no key for it, skip it */
        if (vkey == 0)
            continue;

        /* the key name is just the ascii character value */
        char nm[2] = { *p, '\0' };

        /*
         *   Add it to the name-to-vkey table.  We only need these on the
         *   input side, because on the output side we ask the OS to do the
         *   mapping from key to character code.
         */
        hash_keyname->add(new KeynameEntry(nm, vkey, shift));

        /* 
         *   if it's a Shift+ key, note the mapping from the vkey to the
         *   shifted version of the character - this lets us reverse the
         *   mapping later to ask what character we get from Shift+X 
         */
        if (shift == CTKB_SHIFT)
            shiftmap[vkey] = *p;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   deletion 
 */
CTadsKeyboard::~CTadsKeyboard()
{
    /* delete our key name hash tables */
    delete hash_keyname;
    delete hash_vkey;
}

/* ------------------------------------------------------------------------ */
/*
 *   Canonicalize a key name.  This puts the modifiers (Ctrl, Shift, Alt) in
 *   the standard order, and makes sure that the capitalization matches the
 *   generated names.  We simply overwrite the name in the buffer, since we
 *   can't make the name any longer.  
 */
int CTadsKeyboard::canonicalize_key_name(textchar_t *keyname)
{
    textchar_t *outbuf;
    size_t outrem;
    textchar_t *src, *dst;
    
    /* allocate a buffer of the same size as the input */
    outrem = get_strlen(keyname) + 1;
    outbuf = new textchar_t[outrem];
    
    /* keep going until we run out of keys */
    for (src = keyname, dst = outbuf ; ; )
    {
        int vkey, shift;
        size_t len;
        
        /* find the next key name */
        for ( ; isspace(*src) ; ++src) ;

        /* stop if we're done */
        if (*src == '\0')
            break;

        /* if there's another key before this, add a space to the output */
        if (dst != outbuf && outrem > 1)
            *dst++ = ' ', --outrem;

        /* parse this name */
        len = strlen(src);
        if (!parse_key_name((const textchar_t **)&src, &len, &vkey, &shift))
            return FALSE;

        /* 
         *   re-generate the name into the destination point in the buffer -
         *   it can't get any longer, so at worst this will overwrite the
         *   same text 
         */
        if (!get_key_name(dst, outrem, vkey, shift))
            return FALSE;

        /* advance past this output */
        len = strlen(dst);
        outrem -= len;
        dst += len;
    }

    /* null-terminate the output */
    if (outrem != 0)
        *dst = '\0';
    else
        *(dst-1) = '\0';

    /* copy the output back over the input */
    strcpy(keyname, outbuf);

    /* done with the output buffer */
    delete [] outbuf;

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   parse a key name 
 */
int CTadsKeyboard::parse_key_name(
    const textchar_t **keyname, size_t *keylen, int *vkey, int *shift)
{
    const textchar_t *start;
    const textchar_t *p;
    size_t rem = *keylen;
    KeynameEntry *entry;

    /* presume we won't find a match */
    *vkey = 0;
    *shift = 0;

    /* skip leading spaces */
    for (p = *keyname ; rem != 0 && isspace(*p) ; ++p, --rem) ;

    /* parse modifiers */
    for (*shift = 0 ; ; )
    {
        if (rem >= 5 && memicmp(p, "Ctrl+", 5) == 0)
        {
            *shift |= CTKB_CTRL;
            p += 5;
            rem -= 5;
        }
        else if (rem >= 6 && memicmp(p, "Shift+", 6) == 0)
        {
            *shift |= CTKB_SHIFT;
            p += 6;
            rem -= 6;
        }
        else if (rem >= 4 && memicmp(p, "Alt+", 4) == 0)
        {
            *shift |= CTKB_ALT;
            p += 4;
            rem -= 4;
        }
        else
        {
            /* done with modifiers */
            break;
        }
    }

    /* find the end of the name token */
    for (start = p++, --rem ; rem != 0 && !isspace(*p) ; ++p, --rem) ;

    /* find the name token in our hash table */
    entry = (KeynameEntry *)hash_keyname->find(start, p - start);
    if (entry != 0)
    {
        /* found it - use the key code from the entry */
        *vkey = entry->vkey;

        /* add the shift keys from the entry */
        *shift |= entry->shift;
    }
    else if (p - start == 1)
    {
        /* check for letters and digits */
        if (isalpha(*start))
            *vkey = toupper(*start);
        else if (isdigit(*start))
            *vkey = *start;
    }

    /* position the string pointer at the end of the key name */
    *keyname = p;
    *keylen = rem;

    /* we were successful if we found a non-zero vkey */
    return (*vkey != 0);
}

/* ------------------------------------------------------------------------ */
/* 
 *   add a key name to a buffer under construction 
 */
static int add_key_name(textchar_t **buf, size_t *buflen,
                        const textchar_t *str)
{
    size_t len = get_strlen(str);

    /* if we don't have room, return failure */
    if (*buflen < len + 1)
        return FALSE;

    /* copy the string and null terminator */
    memcpy(*buf, str, len + 1);

    /* advance past it in the buffer */
    *buf += len;
    *buflen -= len;

    /* success */
    return TRUE;
}

/* 
 *   add a shift key name to a buffer under construction 
 */
static int add_shift_key_name(textchar_t **buf, size_t *buflen,
                              int shiftbit, const textchar_t *str)
{
    /* 
     *   if we have this shift-key bit, add the name; otherwise just return
     *   success, since there's nothing we need to add 
     */
    return (shiftbit ? add_key_name(buf, buflen, str) : TRUE);
}

/*
 *   generate the name of a key 
 */
int CTadsKeyboard::get_key_name(textchar_t *buf, size_t buflen,
                                int vkey, int shiftkeys)
{
    CHtmlHashEntryUInt *entry;
    const textchar_t *name = 0;
    textchar_t nbuf[5];
    unsigned int ch;

    /* get the mapping to an ordinary character, if any */
    ch = (unsigned int)os_key_to_char(vkey);

    /* look up the key in our table */
    entry = (CHtmlHashEntryUInt *)hash_vkey->find(
        (const textchar_t *)&vkey, sizeof(vkey));

    /*
     *   If there's an entry for the key in our key table, use that.
     *   
     *   Otherwise, if the key is shifted, and the shifted key translates to
     *   an ordinary ascii character, take out the shift and just use the
     *   shifted character.  For example, for "Shift+8" on the US keyboard,
     *   we'd use simply "*" as the key name.
     *   
     *   Otherwise, if the vkey translates to an ordinary ascii character,
     *   use the ordinary character as the key name.  
     */
    if (entry != 0)
    {
        /* use the name from the table */
        name = (textchar_t *)entry->val_;
    }
    else if ((shiftkeys & CTKB_SHIFT) != 0 && shiftmap[vkey] != 0)
    {
        /* take out the shift */
        shiftkeys &= ~CTKB_SHIFT;

        /* use the shifted character as the name */
        nbuf[0] = (textchar_t)toupper(shiftmap[vkey]);
        nbuf[1] = '\0';
        name = nbuf;
    }
    else if (ch > 32 && ch < 127)
    {
        /* this is the name */
        nbuf[0] = (textchar_t)toupper(ch);
        nbuf[1] = '\0';
        name = nbuf;
    }

    /* if we didn't find a name, we can't generate the key name */
    if (name == 0)
        return FALSE;

    /* generate shift keys */
    if (!add_shift_key_name(&buf, &buflen, shiftkeys & CTKB_CTRL, "Ctrl+"))
        return FALSE;
    if (!add_shift_key_name(&buf, &buflen, shiftkeys & CTKB_SHIFT, "Shift+"))
        return FALSE;
    if (!add_shift_key_name(&buf, &buflen, shiftkeys & CTKB_ALT, "Alt+"))
        return FALSE;

    /* add the key name */
    return add_key_name(&buf, &buflen, name);
}
