/*
 *   tadsfolderdlg.h - ImGui-native folder picker dialog (guit3)
 *
 *   Replaces the Win32 CTadsDialogFolderSel2 modal (foldsel.h/foldsel2.cpp,
 *   IDD_FOLDER_SELECTOR2 resource) - a directory-only browser with an
 *   editable path field, used by the Options dialog's Starting tab ("Browse"
 *   next to the initial game folder). foldsel.h/foldsel2.cpp are left
 *   compiled but unused, same "harmless dead code" reasoning as the old
 *   Options property-sheet code and w32fndlg.cpp/.h.
 *
 *   Like CTadsFileDialog (tadsfiledlg.h) and CTadsFindDialog
 *   (tadsfinddlg.h), this follows the deferred pending-flag pattern: open()
 *   is safe to call from deep inside an in-progress ImGui frame (the
 *   Starting tab's Browse button click), and just records the request and a
 *   completion callback; render() is what actually calls
 *   ImGui::OpenPopup()/BeginPopupModal() and invokes the callback when the
 *   dialog is dismissed - on the frame after open() was called.
 *
 *   Unlike CTadsFileDialog/CTadsFindDialog, render() must be called from
 *   *inside* the Options dialog's own still-open BeginPopupModal block
 *   (CHtmlPreferences::render_options_dialog(), right before its
 *   EndPopup()), not from the top-level popup list in
 *   CHtmlSys_mainwin::do_render(). This dialog's only call site is the
 *   Starting tab's Browse button, which only ever fires while the Options
 *   modal is already open, so the folder picker always needs to nest on top
 *   of it - and Dear ImGui decides a popup's nesting level from the
 *   ID/window stack depth *at the moment OpenPopup() is called*, not from
 *   whichever modal happened to be open most recently. Calling render() at
 *   the root level (matching CTadsFileDialog/CTadsFindDialog's own
 *   convention) calls OpenPopup() with an empty BeginPopupStack - Dear ImGui
 *   then treats the folder picker as a *new top-level (level 0) popup*,
 *   which silently truncates and replaces the Options modal's own
 *   level-0 entry in the global popup stack instead of stacking on top of
 *   it. The visible symptom, confirmed by a real (not just traced) Cancel
 *   click: clicking Cancel/Select Folder in the folder picker closed the
 *   entire Options dialog along with it, not just the folder picker. Nesting
 *   the render() call inside render_options_dialog()'s active
 *   BeginPopupModal block makes BeginPopupStack.Size 1 (Options) at the
 *   point OpenPopup() fires, so the folder picker opens as a proper level-1
 *   popup stacked on top of Options, and closing it only pops that level.
 *   If a future caller ever needs this dialog *not* nested under another
 *   modal, it would need its own top-level render() call site instead of
 *   reusing this one - don't just add a second call from
 *   CHtmlSys_mainwin::do_render() alongside the existing one, since that
 *   would call OpenPopup()/BeginPopupModal() twice in the same frame.
 *   No blocking/open_blocking() entry point is needed - its one call site
 *   only ever runs from inside the Options dialog, which only ever shows
 *   once event_loop() is already running.
 *
 *   Unlike CTadsFileDialog, there's no filename field or file-type filter -
 *   the listing shows only subdirectories of the current directory, and
 *   "Select Folder" accepts the current directory itself. As with the
 *   original dialog, the path field may be edited to name a folder that
 *   doesn't exist yet; navigating into an existing directory (via the
 *   listing or by typing/Enter) just changes the current directory, it
 *   doesn't require the final accepted path to already exist.
 */

#include <functional>

#ifndef TADSFOLDERDLG_H
#define TADSFOLDERDLG_H

class CTadsFolderDialog
{
public:
    /*
     *   Queue a folder picker to be shown starting on the next frame.
     *
     *   prompt - a short instruction shown above the directory listing
     *       (e.g. "Initial \"Open\" Folder:"); a leading Win32 "&"
     *       mnemonic marker, if present, is stripped automatically, since
     *       this build doesn't parse mnemonics (see render_menu_bar()'s
     *       strip_mnemonic() in htmlgui.cpp for the same convention)
     *
     *   caption - dialog title/caption
     *
     *   initial_folder - starting directory; a bare/relative path is
     *       resolved against the current working directory, and
     *       null/empty defaults to the current working directory
     *
     *   callback - invoked exactly once when the dialog closes, with the
     *       chosen full folder path, or null if the user cancelled
     */
    static void open(const char *prompt, const char *caption,
                     const char *initial_folder,
                     std::function<void(const char *folder)> callback);

    /*
     *   Draw the dialog for the current frame, if one is pending or open.
     *   Safe to call unconditionally every frame; a no-op when no dialog
     *   is open.
     */
    static void render();
};

#endif /* TADSFOLDERDLG_H */
