/*
 *   tadsfinddlg.h - ImGui-native "Find" dialog (guit3)
 *
 *   Replaces the Win32 CTadsDialogFind modal (w32fndlg.h/.cpp, DLG_FIND
 *   resource) used by CHtmlSys_mainwin::get_find_text() for the Edit >
 *   Find Text on Current Page... command.  Only the plain Find dialog is
 *   ported here - CTadsDialogFindReplace/CTadsDialogFindRegex (w32fndlg.h)
 *   and their DLG_REPLACE/DLG_REGEXFIND resources belong to the Workbench
 *   debugger/editor, which is out of scope for the guit3 client port (see
 *   the "MDI" decision in migration.md); w32fndlg.cpp/.h are left compiled
 *   but unused, same as the old Options property-sheet code.
 *
 *   Like CTadsFileDialog (tadsfiledlg.h), this follows the deferred
 *   pending-flag pattern: open() is safe to call from deep inside an
 *   in-progress ImGui frame (a menu click or keyboard shortcut handled
 *   during CHtmlSys_mainwin::do_render()), and just records the request and
 *   a completion callback; render(), called once per frame from
 *   do_render(), is what actually calls ImGui::OpenPopup()/BeginPopupModal()
 *   and invokes the callback when the dialog is dismissed - on the frame
 *   after open() was called. This dialog has no blocking/open_blocking()
 *   entry point because its one call site (do_find(), routed through the
 *   CHtmlSysWin_win32_owner::get_find_text() callback interface) only ever
 *   runs from inside the main event loop's frame, never before it starts.
 */

#include <functional>

#ifndef TADSFINDDLG_H
#define TADSFINDDLG_H

class CTadsFindDialog
{
public:
    /*
     *   Queue the Find dialog to be shown starting on the next frame.
     *
     *   initial_text - text to pre-fill the "Find what" field with (the
     *       caller's last search string, or empty)
     *
     *   exact_case, start_at_top, wrap, dir - the caller's persisted
     *       option settings (from the previous search), used to
     *       initialize the dialog's checkboxes/radio buttons
     *
     *   callback - invoked exactly once when the dialog closes.  On
     *       "Find Next", called with the (non-null) search text and the
     *       current option settings.  On Cancel or the popup's own close
     *       button, called with a null text pointer and all other
     *       arguments zeroed.
     */
    static void open(const char *initial_text, int exact_case,
                     int start_at_top, int wrap, int dir,
                     std::function<void(const char *findstr, int exact_case,
                                        int start_at_top, int wrap,
                                        int dir)> callback);

    /*
     *   Draw the dialog for the current frame, if one is pending or open.
     *   Safe to call unconditionally every frame; a no-op when no dialog
     *   is open.
     */
    static void render();
};

#endif /* TADSFINDDLG_H */
