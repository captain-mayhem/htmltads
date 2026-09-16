/*
 *   tadslicensedlg.h - ImGui-native "License Information" dialog (guit3)
 *
 *   Replaces the Win32 LicenseDlg (a CTadsDialog subclass defined inline in
 *   htmlgui.cpp, driving the DLG_LICENSE resource) shown from
 *   CHtmlSys_abouttadswin::show_license_dlg() when the "License" link on the
 *   About HTML TADS box is clicked - a native modal dialog resource, which
 *   guit3's event loop never pumps, so the link silently did nothing (see
 *   migration.md 5.4/N).
 *
 *   Like CTadsFindDialog (tadsfinddlg.h), this follows the deferred
 *   pending-flag pattern: open() just records the request, and render(),
 *   called once per frame from CHtmlSys_mainwin::do_render(), is what
 *   actually calls ImGui::OpenPopup()/BeginPopupModal() - on the frame after
 *   open() was called. do_render() runs on every iteration of the event
 *   loop, including the recursive ones CHtmlSys_abouttadswin::run_dlg()
 *   enters while the About box itself is up, so the popup correctly shows on
 *   top of About's own nested loop too.
 */

#ifndef TADSLICENSEDLG_H
#define TADSLICENSEDLG_H

class CTadsLicenseDlg
{
public:
    /* queue the License dialog to be shown starting on the next frame */
    static void open();

    /*
     *   Draw the dialog for the current frame, if one is pending or open.
     *   Safe to call unconditionally every frame; a no-op when no dialog is
     *   open.
     */
    static void render();
};

#endif /* TADSLICENSEDLG_H */
