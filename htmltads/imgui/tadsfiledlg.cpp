/*
 *   tadsfiledlg.cpp - ImGui-native file open/save dialog (guit3)
 *
 *   See tadsfiledlg.h for the overall design.  This file holds the dialog's
 *   entire working state as file-local statics, since only one instance can
 *   ever be showing at a time (it's a modal).
 */

#include <windows.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "tadsfiledlg.h"
#include "tadsapp.h"
#include "tadsfont.h"        /* CTadsFont::get_dpi_scale() - see migration.md 3.5a */

namespace
{
    /* one file/directory entry in the current directory listing */
    struct FileDlgEntry
    {
        std::string name;
        bool is_dir;
    };

    /* one "Description\0*.pattern\0" group parsed out of a Win32-style filter */
    struct FilterGroup
    {
        std::string desc;
        std::string pattern;
    };

    /* the dialog's complete working state */
    struct FileDlgState
    {
        /* open() was called; waiting for the next render() to show it */
        bool pending = false;

        /* the dialog is currently showing (OpenPopup has been issued) */
        bool is_open = false;

        TadsFileDlgMode mode = TADSFILEDLG_OPEN;
        bool must_exist = true;
        std::string title;

        std::vector<FilterGroup> filters;
        int filter_idx = 0;

        std::string cur_dir;
        std::vector<FileDlgEntry> entries;
        int sel_idx = -1;
        bool need_refresh = false;

        char name_buf[MAX_PATH] = { 0 };
        char path_buf[MAX_PATH] = { 0 };

        std::string error_msg;
        bool show_overwrite_confirm = false;

        std::function<void(const char *filename)> callback;
    };

    FileDlgState s_dlg;

    /* parse a Win32-style "Desc\0Pattern\0Desc2\0Pattern2\0\0" filter string */
    void parse_filters(const char *filter)
    {
        s_dlg.filters.clear();

        if (filter != 0)
        {
            const char *p = filter;
            while (*p != '\0')
            {
                const char *desc = p;
                p += strlen(p) + 1;
                if (*p == '\0')
                    break;

                const char *pat = p;
                p += strlen(p) + 1;

                FilterGroup g;
                g.desc = desc;
                g.pattern = pat;
                s_dlg.filters.push_back(g);
            }
        }

        if (s_dlg.filters.empty())
        {
            FilterGroup g;
            g.desc = "All Files (*.*)";
            g.pattern = "*.*";
            s_dlg.filters.push_back(g);
        }

        s_dlg.filter_idx = 0;
    }

    /* split a path into directory and filename parts */
    void split_initial_path(const char *initial_path, std::string &dir,
                            std::string &name)
    {
        dir.clear();
        name.clear();

        if (initial_path == 0 || *initial_path == '\0')
            return;

        DWORD attr = GetFileAttributesA(initial_path);
        if (attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            /* it's an existing directory - use it as-is, no filename */
            dir = initial_path;
            return;
        }

        const char *slash = strrchr(initial_path, '\\');
        if (slash == 0)
            slash = strrchr(initial_path, '/');

        if (slash != 0)
        {
            dir.assign(initial_path, slash - initial_path);
            name = slash + 1;
        }
        else
        {
            /* no path separator - it's a bare filename in the cwd */
            name = initial_path;
        }
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

    /* re-scan the current directory into s_dlg.entries, applying the filter */
    void refresh_listing()
    {
        s_dlg.entries.clear();
        s_dlg.sel_idx = -1;
        s_dlg.need_refresh = false;

        std::string pattern = s_dlg.cur_dir;
        if (!pattern.empty() && pattern.back() != '\\')
            pattern += '\\';
        pattern += "*";

        const std::string &spec = s_dlg.filters[s_dlg.filter_idx].pattern;

        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (strcmp(fd.cFileName, ".") == 0)
                    continue;

                bool is_dir =
                    (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                /* only files are filtered by the type spec - dirs always show */
                if (!is_dir && !PathMatchSpecA(fd.cFileName, spec.c_str()))
                    continue;

                FileDlgEntry e;
                e.name = fd.cFileName;
                e.is_dir = is_dir;
                s_dlg.entries.push_back(e);
            }
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }

        std::sort(s_dlg.entries.begin(), s_dlg.entries.end(),
            [](const FileDlgEntry &a, const FileDlgEntry &b)
            {
                if (a.is_dir != b.is_dir)
                    return a.is_dir;
                return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
            });
    }

    /* build the full path from the current directory + name field */
    std::string full_selected_path()
    {
        std::string p = s_dlg.cur_dir;
        if (!p.empty() && p.back() != '\\')
            p += '\\';
        p += s_dlg.name_buf;
        return p;
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

    /* the OK/Save/Open button (or Enter in the name field) was activated */
    void try_accept()
    {
        if (s_dlg.name_buf[0] == '\0')
        {
            s_dlg.error_msg = "Please enter a file name.";
            return;
        }

        std::string full = full_selected_path();
        DWORD attr = GetFileAttributesA(full.c_str());
        bool exists = (attr != INVALID_FILE_ATTRIBUTES);
        bool is_dir = exists && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (is_dir)
        {
            /* typed/selected a directory - browse into it instead */
            navigate_to(full);
            return;
        }

        if (s_dlg.mode == TADSFILEDLG_OPEN)
        {
            if (s_dlg.must_exist && !exists)
            {
                s_dlg.error_msg = "File not found.";
                return;
            }

            ImGui::CloseCurrentPopup();
            finish(full.c_str());
        }
        else
        {
            if (exists)
            {
                s_dlg.show_overwrite_confirm = true;
                return;
            }

            ImGui::CloseCurrentPopup();
            finish(full.c_str());
        }
    }

    /*
     *   Draw the dialog's popup and contents for the current frame.  Shared
     *   by the deferred (render()) and blocking (open_blocking()) entry
     *   points; assumes ImGui::NewFrame() has already been called for this
     *   frame.
     */
    void draw_frame()
    {
        /*
         *   Use a "%s###StableID" title so the visible title bar shows the
         *   caller's own dialog title (which varies per call site), while
         *   the popup's actual ImGui ID stays fixed across frames - same
         *   trick CHtmlPreferences::render_customize_theme_dialog() uses
         *   for its per-profile title.
         */
        std::string popup_id = s_dlg.title + "###TadsFileDialog";

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
        ImGui::SetNextWindowSize(ImVec2(560 * s, 440 * s), ImGuiCond_Appearing);

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
        if (ImGui::InputText("##TadsFileDlgPath", s_dlg.path_buf,
            sizeof(s_dlg.path_buf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            navigate_to(s_dlg.path_buf);
        }

        if (s_dlg.need_refresh)
            refresh_listing();

        /* the directory listing */
        ImGui::BeginChild("TadsFileDlgList", ImVec2(0, -84 * s), true);
        for (int i = 0; i < (int)s_dlg.entries.size(); ++i)
        {
            FileDlgEntry &e = s_dlg.entries[i];
            std::string label = (e.is_dir ? "[Dir]  " : "       ") + e.name;
            bool selected = (i == s_dlg.sel_idx);
            if (ImGui::Selectable(label.c_str(), selected,
                ImGuiSelectableFlags_AllowDoubleClick))
            {
                s_dlg.sel_idx = i;
                bool dbl = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                if (e.is_dir)
                {
                    if (dbl)
                    {
                        navigate_to(s_dlg.cur_dir + "\\" + e.name);
                        break;
                    }
                }
                else
                {
                    strncpy(s_dlg.name_buf, e.name.c_str(),
                            sizeof(s_dlg.name_buf) - 1);
                    s_dlg.name_buf[sizeof(s_dlg.name_buf) - 1] = '\0';
                    s_dlg.error_msg.clear();
                    if (dbl)
                    {
                        try_accept();
                        break;
                    }
                }
            }
        }
        ImGui::EndChild();

        /* filename field */
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("File name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##TadsFileDlgName", s_dlg.name_buf,
            sizeof(s_dlg.name_buf), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            try_accept();
        }

        /* file-type filter, if more than one group was given */
        if (s_dlg.filters.size() > 1)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Files of type:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##TadsFileDlgFilter",
                s_dlg.filters[s_dlg.filter_idx].desc.c_str()))
            {
                for (int i = 0; i < (int)s_dlg.filters.size(); ++i)
                {
                    bool sel = (i == s_dlg.filter_idx);
                    if (ImGui::Selectable(s_dlg.filters[i].desc.c_str(), sel))
                    {
                        s_dlg.filter_idx = i;
                        refresh_listing();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (!s_dlg.error_msg.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                               s_dlg.error_msg.c_str());

        ImGui::Separator();

        const char *ok_label =
            (s_dlg.mode == TADSFILEDLG_SAVE ? "Save" : "Open");
        const float btn_w = 90.0f * s;
        if (ImGui::Button(ok_label, ImVec2(btn_w, 0)))
            try_accept();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w, 0))
            || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            finish(0);
        }

        /* overwrite-confirmation, nested on top of the dialog (Save mode) */
        if (s_dlg.show_overwrite_confirm)
        {
            ImGui::OpenPopup("Overwrite?###TadsFileDlgOverwrite");
            s_dlg.show_overwrite_confirm = false;
        }
        if (ImGui::BeginPopupModal("Overwrite?###TadsFileDlgOverwrite", 0,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("\"%s\" already exists.\nDo you want to replace it?",
                        s_dlg.name_buf);
            ImGui::Spacing();
            if (ImGui::Button("Yes", ImVec2(btn_w, 0)))
            {
                std::string full = full_selected_path();
                ImGui::CloseCurrentPopup();
                ImGui::CloseCurrentPopup();
                finish(full.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(btn_w, 0))
                || ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::EndPopup();
    }
}

/* ------------------------------------------------------------------------ */

void CTadsFileDialog::open(TadsFileDlgMode mode, const char *title,
    const char *filter, const char *initial_path, bool must_exist,
    std::function<void(const char *filename)> callback)
{
    s_dlg.mode = mode;
    s_dlg.title = (title != 0 ? title
                   : (mode == TADSFILEDLG_SAVE ? "Save File" : "Open File"));
    s_dlg.must_exist = must_exist;
    s_dlg.callback = callback;
    s_dlg.show_overwrite_confirm = false;
    s_dlg.error_msg.clear();

    parse_filters(filter);

    std::string dir, name;
    split_initial_path(initial_path, dir, name);
    if (dir.empty())
    {
        char cwd[MAX_PATH];
        GetCurrentDirectoryA(sizeof(cwd), cwd);
        dir = cwd;
    }
    navigate_to(dir);

    strncpy(s_dlg.name_buf, name.c_str(), sizeof(s_dlg.name_buf) - 1);
    s_dlg.name_buf[sizeof(s_dlg.name_buf) - 1] = '\0';

    s_dlg.pending = true;
}

void CTadsFileDialog::render()
{
    if (!s_dlg.pending && !s_dlg.is_open)
        return;

    draw_frame();
}

bool CTadsFileDialog::open_blocking(GLFWwindow *window, TadsFileDlgMode mode,
    const char *title, const char *filter, const char *initial_path,
    bool must_exist, char *result_buf, size_t result_buf_size,
    std::function<void()> render_background)
{
    /* with no GLFW window yet, fall back to the native common dialog */
    if (window == 0)
    {
        /* the initial path may be a bare dir or a full path; split it */
        std::string dir, name;
        split_initial_path(initial_path, dir, name);
        strncpy(result_buf, name.c_str(), result_buf_size - 1);
        result_buf[result_buf_size - 1] = '\0';

        OPENFILENAME5 info;
        info.hwndOwner = 0;
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrFilter = filter;
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = 0;
        info.lpstrFile = result_buf;
        info.nMaxFile = (DWORD)result_buf_size;
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrInitialDir = (dir.empty() ? 0 : dir.c_str());
        info.lpstrTitle = title;
        info.Flags = OFN_HIDEREADONLY | OFN_ENABLESIZING;
        if (mode == TADSFILEDLG_SAVE)
            info.Flags |= OFN_OVERWRITEPROMPT;
        else if (must_exist)
            info.Flags |= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;

        return (mode == TADSFILEDLG_SAVE
                ? GetSaveFileName((OPENFILENAME *)&info)
                : GetOpenFileName((OPENFILENAME *)&info)) != 0;
    }

    bool done = false;
    std::string result;
    bool got_result = false;

    open(mode, title, filter, initial_path, must_exist,
        [&](const char *filename)
        {
            done = true;
            if (filename != 0)
            {
                result = filename;
                got_result = true;
            }
        });

    while (!done && !glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (render_background)
        {
            /*
             *   the caller's own per-frame rendering - this is expected to
             *   draw this dialog itself (via its own CTadsFileDialog::
             *   render() call), so don't also call draw_frame() below
             */
            render_background();
        }
        else
        {
            /* nothing behind the dialog - just draw the dialog itself */
            draw_frame();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (got_result)
    {
        strncpy(result_buf, result.c_str(), result_buf_size - 1);
        result_buf[result_buf_size - 1] = '\0';
        return true;
    }

    return false;
}
