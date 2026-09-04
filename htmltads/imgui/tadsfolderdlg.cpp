/*
 *   tadsfolderdlg.cpp - ImGui-native folder picker dialog (guit3)
 *
 *   See tadsfolderdlg.h for the overall design. This file holds the
 *   dialog's entire working state as file-local statics, since only one
 *   instance can ever be showing at a time (it's a modal). The directory-
 *   scanning/navigation logic (FindFirstFileA/FindNextFileA, GetFullPathNameA
 *   resolution, the editable path bar, the "Up" button) mirrors
 *   CTadsFileDialog (tadsfiledlg.cpp) closely, minus the filename field and
 *   file-type filter and with the listing restricted to directories only.
 */

#include <windows.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#include <imgui/imgui.h>

#include "tadsfolderdlg.h"
#include "tadsfont.h"        /* CTadsFont::get_dpi_scale() - see migration.md 3.5a */

namespace
{
    /* the dialog's complete working state */
    struct FolderDlgState
    {
        /* open() was called; waiting for the next render() to show it */
        bool pending = false;

        /* the dialog is currently showing (OpenPopup has been issued) */
        bool is_open = false;

        std::string prompt;
        std::string caption;

        std::string cur_dir;
        std::vector<std::string> entries;     /* subdirectory names only */
        int sel_idx = -1;
        bool need_refresh = false;

        char path_buf[MAX_PATH] = { 0 };

        std::string error_msg;

        std::function<void(const char *folder)> callback;
    };

    FolderDlgState s_dlg;

    /*
     *   Strip a Win32 "&Letter" mnemonic marker out of a label, same
     *   convention render_menu_bar() uses in htmlgui.cpp ("&&" -> "&", a
     *   lone "&" dropped) - this build doesn't parse mnemonics, so an
     *   unstripped "&" would otherwise show up as a literal character.
     */
    std::string strip_mnemonic(const char *s)
    {
        std::string out;
        if (s != 0)
        {
            for (const char *p = s; *p != '\0'; ++p)
            {
                if (*p == '&')
                {
                    if (*(p + 1) == '&')
                    {
                        out += '&';
                        ++p;
                    }
                    /* else: lone '&' mnemonic marker - drop it */
                }
                else
                    out += *p;
            }
        }
        return out;
    }

    /* navigate to a (possibly relative) directory; defers the listing refresh */
    void navigate_to(const std::string &dir)
    {
        char full[MAX_PATH];
        if (!dir.empty() && GetFullPathNameA(dir.c_str(), MAX_PATH, full, 0) != 0)
            s_dlg.cur_dir = full;
        else
            s_dlg.cur_dir = dir;

        /* strip a trailing backslash, except for a bare drive root ("C:\") */
        if (s_dlg.cur_dir.size() > 3 && s_dlg.cur_dir.back() == '\\')
            s_dlg.cur_dir.pop_back();

        s_dlg.need_refresh = true;
        s_dlg.error_msg.clear();
    }

    /* re-scan the current directory into s_dlg.entries (subdirectories only) */
    void refresh_listing()
    {
        s_dlg.entries.clear();
        s_dlg.sel_idx = -1;
        s_dlg.need_refresh = false;

        std::string pattern = s_dlg.cur_dir;
        if (!pattern.empty() && pattern.back() != '\\')
            pattern += '\\';
        pattern += "*";

        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (strcmp(fd.cFileName, ".") == 0)
                    continue;

                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    continue;

                s_dlg.entries.push_back(fd.cFileName);
            }
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }

        std::sort(s_dlg.entries.begin(), s_dlg.entries.end(),
            [](const std::string &a, const std::string &b)
            { return _stricmp(a.c_str(), b.c_str()) < 0; });
    }

    /* close the dialog and invoke the completion callback, if any */
    void finish(const char *result)
    {
        auto cb = s_dlg.callback;
        s_dlg.is_open = false;
        s_dlg.pending = false;
        s_dlg.callback = nullptr;
        if (cb)
            cb(result);
    }

    /*
     *   Draw the dialog's popup and contents for the current frame. Assumes
     *   ImGui::NewFrame() has already been called for this frame.
     */
    void draw_frame()
    {
        /*
         *   Use a "%s###StableID" title so the visible title bar shows the
         *   caller's own caption (which varies per call site), while the
         *   popup's actual ImGui ID stays fixed across frames - same trick
         *   CTadsFileDialog uses for its own title.
         */
        std::string popup_id = s_dlg.caption + "###TadsFolderDialog";

        bool just_opened = s_dlg.pending;
        if (s_dlg.pending)
        {
            ImGui::OpenPopup(popup_id.c_str());
            s_dlg.pending = false;
            s_dlg.is_open = true;
            refresh_listing();
        }

        /* scale the fixed pixel sizes for the display - see migration.md 3.5a */
        const float s = CTadsFont::get_dpi_scale();

        ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(480 * s, 420 * s), ImGuiCond_Appearing);

        bool p_open = true;
        if (!ImGui::BeginPopupModal(popup_id.c_str(), &p_open,
            ImGuiWindowFlags_NoCollapse))
            return;

        if (!p_open)
        {
            /* the window's own [x] close button was clicked */
            ImGui::EndPopup();
            finish(0);
            return;
        }

        if (!s_dlg.prompt.empty())
        {
            ImGui::TextWrapped("%s", s_dlg.prompt.c_str());
            ImGui::Spacing();
        }

        /* "Up" button + editable current-directory path */
        if (ImGui::Button("Up"))
        {
            size_t slash = s_dlg.cur_dir.find_last_of('\\');
            if (slash != std::string::npos)
            {
                std::string parent = (slash <= 2)
                    ? s_dlg.cur_dir.substr(0, 3)
                    : s_dlg.cur_dir.substr(0, slash);
                navigate_to(parent);
            }
        }
        ImGui::SameLine();
        strncpy(s_dlg.path_buf, s_dlg.cur_dir.c_str(),
                sizeof(s_dlg.path_buf) - 1);
        s_dlg.path_buf[sizeof(s_dlg.path_buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1);

        /*
         *   Auto-focus the path field the frame the dialog opens, same
         *   convention CTadsFindDialog uses for its own primary text field
         *   - without this, keyboard focus stays wherever it was before
         *   (e.g. the Options dialog's last-focused control).
         */
        if (just_opened)
            ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##TadsFolderDlgPath", s_dlg.path_buf,
            sizeof(s_dlg.path_buf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            navigate_to(s_dlg.path_buf);
        }

        if (s_dlg.need_refresh)
            refresh_listing();

        /* the subdirectory listing */
        ImGui::BeginChild("TadsFolderDlgList", ImVec2(0, -68 * s), true);
        for (int i = 0; i < (int)s_dlg.entries.size(); ++i)
        {
            const std::string &name = s_dlg.entries[i];
            std::string label = "[Dir]  " + name;
            bool selected = (i == s_dlg.sel_idx);
            if (ImGui::Selectable(label.c_str(), selected,
                ImGuiSelectableFlags_AllowDoubleClick))
            {
                s_dlg.sel_idx = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    navigate_to(s_dlg.cur_dir + "\\" + name);
                    break;
                }
            }
        }
        ImGui::EndChild();

        if (!s_dlg.error_msg.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                               s_dlg.error_msg.c_str());

        ImGui::Separator();

        const float btn_w = 110.0f * s;
        if (ImGui::Button("Select Folder", ImVec2(btn_w, 0)))
        {
            if (s_dlg.cur_dir.empty())
                s_dlg.error_msg = "Please enter a folder name.";
            else
            {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                finish(s_dlg.cur_dir.c_str());
                return;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))
            || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            finish(0);
            return;
        }

        ImGui::EndPopup();
    }
}

/* ------------------------------------------------------------------------ */

void CTadsFolderDialog::open(const char *prompt, const char *caption,
    const char *initial_folder,
    std::function<void(const char *folder)> callback)
{
    s_dlg.prompt = strip_mnemonic(prompt);
    s_dlg.caption = (caption != 0 ? caption : "Select Folder");
    s_dlg.callback = callback;
    s_dlg.error_msg.clear();

    std::string dir;
    if (initial_folder != 0 && *initial_folder != '\0')
        dir = initial_folder;
    else
    {
        char cwd[MAX_PATH];
        GetCurrentDirectoryA(sizeof(cwd), cwd);
        dir = cwd;
    }
    navigate_to(dir);

    s_dlg.pending = true;
}

void CTadsFolderDialog::render()
{
    if (!s_dlg.pending && !s_dlg.is_open)
        return;

    draw_frame();
}
