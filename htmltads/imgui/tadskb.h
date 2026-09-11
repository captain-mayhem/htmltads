/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadskb.h - TADS Windows Keyboard interface
Function
  This provides some services related to the Windows keyboard, particularly
  for generating and parsing the names of Windows virtual keys.
Notes
  
Modified
  10/27/06 MJRoberts  - Creation
*/

#ifndef TADSKB_H
#define TADSKB_H

#include "tadshtml.h"

/* shift key bits */
#define CTKB_SHIFT   0x0001
#define CTKB_CTRL    0x0002
#define CTKB_ALT     0x0004

/*
 *   keyboard utilities class
 *
 *   'vkey' throughout this class is a canonical key code (os_key_t, guios.h)
 *   - a GLFW_KEY_* value - not a Windows VK_xxx code.  It was ported from
 *   VK_xxx to the canonical enum as part of migration.md 5.4/L; the two
 *   remaining keyboard-layout queries (unshifted char for a key, key for a
 *   character) go through guios.h's os_key_to_char()/os_char_to_key().
 */
class CTadsKeyboard
{
public:
    CTadsKeyboard();
    ~CTadsKeyboard();

    /* get the name of a key */
    int get_key_name(
        textchar_t *buf, size_t buflen, int vkey, int shiftkeys);

    /* parse a key name */
    int parse_key_name(
        const textchar_t **keykname, size_t *keylen,
        int *vkey, int *shiftkeys);

    /* canonicalize a key name */
    int canonicalize_key_name(textchar_t *keyname);

protected:
    /* hash tables to and from key names */
    class CHtmlHashTable *hash_keyname;
    class CHtmlHashTable *hash_vkey;

    /*
     *   Shifted key mapping.  For each canonical key code K (GLFW_KEY_*,
     *   up to GLFW_KEY_LAST), this contains the character obtained from
     *   Shift+K, if any.
     */
    char shiftmap[512];
};

#endif /* TADSKB_H */
