/*
 *   guires_data.h - embedded byte-array resources for the non-Windows guit3
 *   build (guios_portable.cpp's item B backend)
 *
 *   Windows pulls the toolbar icon strip and the license text out of the
 *   compiled-in .exe resources (IDB_TERP_TOOLBAR / IDX_LICENSE_TEXT -
 *   win32/runtbar.bmp and notes3/license.txt); there is no resource
 *   compiler off Windows, so this file embeds the same two source files
 *   directly as byte arrays, mechanically generated with `xxd -i` from the
 *   originals and never hand-edited.  See migration.md 5.4/B.
 */

#ifndef GUIRES_DATA_H
#define GUIRES_DATA_H

/* win32/runtbar.bmp - the 304x15 4bpp toolbar icon strip bitmap, verbatim */
extern const unsigned char g_runtbar_bmp_data[];
extern const unsigned int g_runtbar_bmp_size;

/* notes3/license.txt - the license text shown by the License dialog */
extern const unsigned char g_license_txt_data[];
extern const unsigned int g_license_txt_size;

#endif /* GUIRES_DATA_H */
