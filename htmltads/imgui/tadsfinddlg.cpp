/*
 *   tadsfinddlg.cpp - ImGui-native "Find" dialog (guit3)
 *
 *   See tadsfinddlg.h for the overall design.  This file holds the
 *   dialog's entire working state (including the search-history list, the
 *   equivalent of CTadsDialogFind::past_strings_) as file-local statics,
 *   since only one instance can ever be showing at a time (it's a modal).
 */

#include <cstring>
#include <string>
#include <vector>

#include <imgui/imgui.h>

#include "tadsfinddlg.h"
#include "tadsfont.h"        /* CTadsFont::get_dpi_scale() - see migration.md 3.5a */

namespace
{
    /* the dialog's complete working state */
    struct FindDlgState
    {
        /* open() was called; waiting for the next render() to show it */
        bool pending = false;

        /* the dialog is currently showing (OpenPopup has been issued) */
        bool is_open = false;

        char text_buf[512] = { 0 };

        bool exact_case = false;
        bool wrap = false;
        bool start_at_top = false;
        int dir = 1;                            /* 1 = forward, -1 = backward */

        /* past search strings, most recent first, capped at 10 */
        std::vector<std::string> history;

        /*
         *   Size the popup auto-fits to, captured fresh every frame while
         *   settle_frames is still counting down (see draw_frame()) and
         *   then pinned exactly there for good.  A freshly-opened
         *   AlwaysAutoResize window can't get its size right on the very
         *   first frame - ContentSizeIdeal reflects the *previous* frame's
         *   layout, which doesn't exist yet for a window that didn't exist
         *   last frame - so capturing only the opening frame's size (as an
         *   earlier version of this code did) locks in a too-small size
         *   forever. Letting it auto-fit for a few frames first before
         *   freezing avoids that without reintroducing a visible
         *   grow/shrink after the dialog has already settled.
         */
        ImVec2 fixed_size = ImVec2(0, 0);
        int settle_frames = 0;

        std::function<void(const char *findstr, int exact_case,
                           int start_at_top, int wrap, int dir)> callback;
    };

    FindDlgState s_dlg;

    /* close the dialog without a match; invoke the callback with a null text */
    void finish_cancel()
    {
        auto cb = s_dlg.callback;
        s_dlg.is_open = false;
        s_dlg.pending = false;
        s_dlg.callback = nullptr;
        if (cb)
            cb(0, 0, 0, 0, 0);
    }

    /* the "Find Next" button (or Enter in the text field) was activated */
    void accept()
    {
        std::string text = s_dlg.text_buf;

        /*
         *   Save the search text in the history list - if it's already
         *   there (case-insensitively), move it to the front rather than
         *   adding a duplicate, same as CTadsDialogFind::save_find_text().
         */
        for (size_t i = 0; i < s_dlg.history.size(); ++i)
        {
            if (_stricmp(s_dlg.history[i].c_str(), text.c_str()) == 0)
            {
                s_dlg.history.erase(s_dlg.history.begin() + i);
                break;
            }
        }
        s_dlg.history.insert(s_dlg.history.begin(), text);
        if (s_dlg.history.size() > 10)
            s_dlg.history.resize(10);

        auto cb = s_dlg.callback;
        int exact_case = s_dlg.exact_case;
        int start_at_top = s_dlg.start_at_top;
        int wrap = s_dlg.wrap;
        int dir = s_dlg.dir;

        s_dlg.is_open = false;
        s_dlg.pending = false;
        s_dlg.callback = nullptr;

        if (cb)
            cb(text.c_str(), exact_case, start_at_top, wrap, dir);
    }

    /*
     *   Draw the dialog's popup and contents for the current frame.
     */
    void draw_frame()
    {
        const char *popup_id = "Find###TadsFindDialog";

        bool just_opened = s_dlg.pending;
        if (s_dlg.pending)
        {
            ImGui::OpenPopup(popup_id);
            s_dlg.pending = false;
            s_dlg.is_open = true;

            /*
             *   Let the window auto-fit for a handful of frames before
             *   pinning its size - see the settle_frames comment above.
             */
            s_dlg.settle_frames = 4;
        }

        /* scale the fixed pixel sizes for the display - see migration.md 3.5a */
        const float s = CTadsFont::get_dpi_scale();

        ImGuiViewport *vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        /*
         *   While settling, auto-fit every frame and keep recapturing
         *   fixed_size, since the first frame or two after opening still
         *   under-measures (ContentSizeIdeal lags a frame behind actual
         *   content for a window that didn't exist last frame). Once
         *   settled, pin the window to the last captured size instead of
         *   re-auto-fitting, so it doesn't keep resizing itself frame to
         *   frame after it's already reached its natural size.
         */
        bool settling = s_dlg.settle_frames > 0;
        ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoResize;
        if (settling)
        {
            win_flags |= ImGuiWindowFlags_AlwaysAutoResize;
            if (just_opened)
                ImGui::SetNextWindowSize(ImVec2(360 * s, 0), ImGuiCond_Appearing);
        }
        else if (s_dlg.fixed_size.x > 0.0f)
        {
            ImGui::SetNextWindowSize(s_dlg.fixed_size, ImGuiCond_Always);
        }

        bool p_open = true;
        if (!ImGui::BeginPopupModal(popup_id, &p_open, win_flags))
            return;

        if (settling)
        {
            s_dlg.fixed_size = ImGui::GetWindowSize();
            --s_dlg.settle_frames;
        }

        if (!p_open)
        {
            /* the window's own [x] close button was clicked */
            ImGui::EndPopup();
            finish_cancel();
            return;
        }

        ImGui::TextUnformatted("Find what:");

        bool has_history = !s_dlg.history.empty();
        /* -1 = "stretch to edge" sentinel; -38 is a pixel inset that must scale */
        ImGui::SetNextItemWidth(has_history ? -38.0f * s : -1.0f);

        /*
         *   Auto-focus the text field the frame the dialog opens, same as
         *   the native dialog's default control - without this, keyboard
         *   focus stays wherever it was before (e.g. the game's own
         *   command line), and CHtmlSys_mainwin::event_loop()'s
         *   io.WantTextInput guard would then leave typed characters going
         *   to the game instead of this field.
         */
        if (just_opened)
            ImGui::SetKeyboardFocusHere();
        bool enter_pressed = ImGui::InputText("##FindWhat", s_dlg.text_buf,
            sizeof(s_dlg.text_buf), ImGuiInputTextFlags_EnterReturnsTrue);

        if (has_history)
        {
            ImGui::SameLine();
            if (ImGui::ArrowButton("##FindHistory", ImGuiDir_Down))
                ImGui::OpenPopup("FindHistoryPopup");

            if (ImGui::BeginPopup("FindHistoryPopup"))
            {
                for (const std::string &h : s_dlg.history)
                {
                    if (ImGui::Selectable(h.c_str()))
                    {
                        strncpy(s_dlg.text_buf, h.c_str(),
                                sizeof(s_dlg.text_buf) - 1);
                        s_dlg.text_buf[sizeof(s_dlg.text_buf) - 1] = '\0';
                    }
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Checkbox("Match case", &s_dlg.exact_case);
        ImGui::Checkbox("Wrap around at end of text", &s_dlg.wrap);
        ImGui::Checkbox("Start at top", &s_dlg.start_at_top);

        ImGui::Spacing();
        ImGui::SeparatorText("Direction");
        if (ImGui::RadioButton("Backward", s_dlg.dir == -1))
            s_dlg.dir = -1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Forward", s_dlg.dir == 1))
            s_dlg.dir = 1;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool can_find = s_dlg.text_buf[0] != '\0';
        ImGui::BeginDisabled(!can_find);
        if (ImGui::Button("Find Next") || (enter_pressed && can_find))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
            accept();
            return;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            finish_cancel();
            return;
        }

        ImGui::EndPopup();
    }
}

/* ------------------------------------------------------------------------ */

void CTadsFindDialog::open(const char *initial_text, int exact_case,
    int start_at_top, int wrap, int dir,
    std::function<void(const char *findstr, int exact_case,
                       int start_at_top, int wrap, int dir)> callback)
{
    strncpy(s_dlg.text_buf, initial_text != 0 ? initial_text : "",
            sizeof(s_dlg.text_buf) - 1);
    s_dlg.text_buf[sizeof(s_dlg.text_buf) - 1] = '\0';

    s_dlg.exact_case = (exact_case != 0);
    s_dlg.wrap = (wrap != 0);
    s_dlg.start_at_top = (start_at_top != 0);
    s_dlg.dir = (dir < 0 ? -1 : 1);

    s_dlg.callback = callback;
    s_dlg.pending = true;
}

void CTadsFindDialog::render()
{
    if (!s_dlg.pending && !s_dlg.is_open)
        return;

    draw_frame();
}
