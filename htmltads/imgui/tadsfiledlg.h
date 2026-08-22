/*
 *   tadsfiledlg.h - ImGui-native file open/save dialog (guit3)
 *
 *   Replaces the Win32 GetOpenFileName()/GetSaveFileName() common dialogs
 *   with a self-contained ImGui file browser (directory navigation,
 *   filtering, a filename field, and an overwrite-confirmation for Save).
 *
 *   Two entry points are provided, matching the two situations call sites
 *   are in elsewhere in this port (see tadswin_message_box() in tadswin.h
 *   for the precedent this follows):
 *
 *   - open()/render() is for callers running inside an ImGui frame (e.g. a
 *     menu or button click handler, possibly nested inside another modal
 *     popup, such as the Options dialog's Game Chest tab).  open() just
 *     queues the dialog and records a completion callback; render(),
 *     called once per frame from CHtmlSys_mainwin::do_render() at the same
 *     root ID-stack depth as the other top-level popups (see the
 *     OpenPopup()/BeginPopup() ID-stack-matching note in htmlgui.cpp's
 *     render_context_menu()), draws it and invokes the callback exactly
 *     once when the dialog is dismissed - on the frame after open() was
 *     called, same as CHtmlPreferences::open_options_dialog().
 *
 *   - open_blocking() is for callers running outside any ImGui frame (e.g.
 *     very early startup, before event_loop() begins) - it runs its own
 *     local GLFW/ImGui frame loop on the given window, blocking the caller
 *     until the dialog closes, exactly like tadswin_message_box() does.
 */

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <functional>

#ifndef TADSFILEDLG_H
#define TADSFILEDLG_H

/* dialog mode */
enum TadsFileDlgMode
{
    TADSFILEDLG_OPEN,
    TADSFILEDLG_SAVE
};

class CTadsFileDialog
{
public:
    /*
     *   Queue a file dialog to be shown starting on the next frame.
     *
     *   mode - TADSFILEDLG_OPEN or TADSFILEDLG_SAVE
     *
     *   title - dialog title/caption
     *
     *   filter - Win32-style multi-string filter, e.g.
     *       "Description\0*.ext;*.ext2\0Description2\0*.*\0\0" - the same
     *       format used by OPENFILENAME::lpstrFilter, so existing filter
     *       strings can be passed through unchanged; null/empty defaults
     *       to a single "All Files (*.*)" entry
     *
     *   initial_path - starting folder and/or filename.  May be a bare
     *       directory (the name field starts blank), a full path to a
     *       file that may or may not exist yet (the directory and name
     *       parts are split apart automatically), or null/empty (defaults
     *       to the current working directory, name field blank)
     *
     *   must_exist - for Open mode, require the chosen file to already
     *       exist (ignored for Save mode, which always allows a new
     *       filename; choosing an existing file in Save mode triggers an
     *       overwrite confirmation instead)
     *
     *   callback - invoked exactly once when the dialog closes, with the
     *       chosen full path, or null if the user cancelled
     */
    static void open(TadsFileDlgMode mode, const char *title,
                     const char *filter, const char *initial_path,
                     bool must_exist,
                     std::function<void(const char *filename)> callback);

    /*
     *   Draw the dialog for the current frame, if one is pending or open.
     *   Safe to call unconditionally every frame; a no-op when no dialog
     *   is open.
     */
    static void render();

    /*
     *   Blocking variant, for use outside any ImGui frame.  Runs its own
     *   local GLFW/ImGui frame loop on 'window' until the user picks a
     *   file or cancels.  Returns true and fills 'result_buf' with the
     *   chosen full path on success; returns false (result_buf untouched)
     *   on Cancel.  If 'window' is null (no GLFW window exists yet), falls
     *   back to the native GetOpenFileName()/GetSaveFileName() common
     *   dialog, mirroring tadswin_message_box()'s fallback for the same
     *   situation.
     *
     *   'render_background', if given, is called once per frame (after
     *   NewFrame(), before the dialog itself is drawn) to draw whatever
     *   should appear behind/around the dialog - e.g. the main window's own
     *   do_render(), so an already-running game stays visible and the
     *   dialog looks like a floating modal instead of covering the whole
     *   screen.  If 'render_background' itself calls CTadsFileDialog::
     *   render() as part of its own per-frame rendering (as
     *   CHtmlSys_mainwin::do_render() does), that call draws this dialog,
     *   so open_blocking() will *not* separately call draw_frame() in that
     *   case - only one of the two ever draws the dialog for a given frame.
     *   If 'render_background' is omitted, the loop just clears the screen
     *   and draws the dialog by itself, as before (fine when there's
     *   nothing behind it yet, e.g. the very first "choose a game" prompt
     *   at startup).
     */
    static bool open_blocking(GLFWwindow *window, TadsFileDlgMode mode,
                              const char *title, const char *filter,
                              const char *initial_path, bool must_exist,
                              char *result_buf, size_t result_buf_size,
                              std::function<void()> render_background
                                  = std::function<void()>());
};

#endif /* TADSFILEDLG_H */
