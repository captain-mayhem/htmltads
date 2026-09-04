/*
 *   tadslicensedlg.cpp - ImGui-native "License Information" dialog (guit3)
 *
 *   See tadslicensedlg.h for the overall design.  The license text comes
 *   from the same IDX_LICENSE_TEXT "TEXTFILE" resource (../notes3/license.txt,
 *   embedded via ../win32/htmlt3.rc) the old Win32 LicenseDlg loaded into its
 *   EDITTEXT control - still a plain Win32 resource lookup since guit3 is
 *   Windows-only for now (see migration.md 5.4/B for the eventual
 *   embedded-byte-array plan that will replace this).
 */

#include <windows.h>
#include <string>

#include <imgui/imgui.h>

#include "tadslicensedlg.h"
#include "tadsapp.h"
#include "tadsfont.h"     /* CTadsFont::get_dpi_scale() - see migration.md 3.5a */
#include "htmlres.h"      /* IDX_LICENSE_TEXT */

namespace
{
    struct LicenseDlgState
    {
        /* open() was called; waiting for the next render() to show it */
        bool pending = false;

        /* the dialog is currently showing (OpenPopup has been issued) */
        bool is_open = false;

        /* license text, loaded lazily on first open() and cached thereafter */
        std::string text;
        bool text_loaded = false;
    };

    LicenseDlgState s_dlg;

    /* load the license text from its resource, if we haven't already */
    void load_license_text()
    {
        s_dlg.text_loaded = true;

        HINSTANCE inst = CTadsApp::get_app()->get_instance();
        HRSRC hres = FindResource(
            inst, MAKEINTRESOURCE(IDX_LICENSE_TEXT), "TEXTFILE");
        if (hres == 0)
            return;

        HGLOBAL hgl = LoadResource(inst, hres);
        if (hgl == 0)
            return;

        void *mem = LockResource(hgl);
        DWORD len = SizeofResource(inst, hres);
        if (mem == 0 || len == 0)
            return;

        /*
         *   Build the string from the exact resource size rather than
         *   relying on the resource bytes being null-terminated (the old
         *   Win32 code handed 'mem' straight to EM_REPLACESEL as if it were
         *   a C string, which happened to work but wasn't guaranteed by the
         *   resource format).
         */
        s_dlg.text.assign((const char *)mem, len);
    }

    /*
     *   Draw the dialog's popup and contents for the current frame.
     */
    void draw_frame()
    {
        const char *popup_id = "License Information###TadsLicenseDlg";

        bool just_opened = s_dlg.pending;
        if (s_dlg.pending)
        {
            ImGui::OpenPopup(popup_id);
            s_dlg.pending = false;
            s_dlg.is_open = true;
        }

        /* scale the fixed pixel size for the display - see migration.md 3.5a */
        const float s = CTadsFont::get_dpi_scale();

        ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        /* same design size as the old DLG_LICENSE resource (400x300) */
        if (just_opened)
            ImGui::SetNextWindowSize(ImVec2(400 * s, 300 * s), ImGuiCond_Appearing);

        bool p_open = true;
        if (!ImGui::BeginPopupModal(popup_id, &p_open, ImGuiWindowFlags_NoResize))
            return;

        if (!p_open)
        {
            /* the window's own [x] close button was clicked */
            ImGui::EndPopup();
            s_dlg.is_open = false;
            return;
        }

        /* the read-only license text, filling everything above the OK button */
        float ok_row_ht = ImGui::GetFrameHeightWithSpacing();
        ImGui::InputTextMultiline("##LicenseText",
            const_cast<char *>(s_dlg.text.c_str()), s_dlg.text.size() + 1,
            ImVec2(-1.0f, -ok_row_ht), ImGuiInputTextFlags_ReadOnly);

        /* centered OK button, matching the old dialog's DEFPUSHBUTTON */
        float ok_wid = 50.0f * s;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ok_wid) * 0.5f);
        if (ImGui::Button("OK", ImVec2(ok_wid, 0))
            || ImGui::IsKeyPressed(ImGuiKey_Escape)
            || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            s_dlg.is_open = false;
            return;
        }

        ImGui::EndPopup();
    }
}

/* ------------------------------------------------------------------------ */

void CTadsLicenseDlg::open()
{
    if (!s_dlg.text_loaded)
        load_license_text();

    s_dlg.pending = true;
}

void CTadsLicenseDlg::render()
{
    if (!s_dlg.pending && !s_dlg.is_open)
        return;

    draw_frame();
}
