#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsstat.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsstat.cpp - status line implementation
Function

Notes

Modified
  10/26/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <imgui/imgui.h>

#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Status line
 */

CTadsStatusline::CTadsStatusline(CTadsWin * /*parent*/, int /*sizebox*/,
                                  int /*id*/)
{
    /* create our main default part */
    parts_ = main_part_ = new CTadsStatusPart();

    /* start with a reasonable default height, until our first render() */
    height_ = 24.0f;
    last_width_ = 0.0f;

    /* lay out a single, full-width part to start with */
    int edges[1] = { -1 };
    set_parts(1, edges);

    /* register with the application object */
    CTadsApp::get_app()->register_statusline(this);
}

CTadsStatusline::~CTadsStatusline()
{
    /* unregister ourselves with the application object */
    CTadsApp::get_app()->unregister_statusline(this);

    /* delete all of our parts */
    while (parts_ != 0)
    {
        /* unlink the first part */
        CTadsStatusPart *cur = parts_;
        parts_ = cur->nxt_;

        /* delete this part */
        delete cur;
    }
}

/*
 *   Update the status line.  Run through the list of sources until we
 *   find a message, then display that message.
 */
void CTadsStatusline::update()
{
    /* go through each part */
    int partno = 0;
    for (CTadsStatusPart *part = parts_ ; part != 0 ;
         part = part->nxt_, ++partno)
    {
        /* no message yet for this part */
        textchar_t *msg = 0;
        int caller_deletes = FALSE;

        /* go through each source until we find a message for this part */
        CTadsStatusSourceListitem *cur;
        for (cur = part->sources_ ; cur != 0 ; cur = cur->nxt_)
        {
            /* ask this item for a message; if we found one, stop looking */
            msg = cur->item_->get_status_message(&caller_deletes);
            if (msg != 0)
                break;
        }

        /*
         *   display the message - owner-drawn parts aren't supported by
         *   the ImGui status line (nothing in the app actually uses them),
         *   so just treat that as an empty message
         */
        set_part_text(partno,
                       (msg == 0 || msg == CTadsStatusSource::OWNER_DRAW)
                       ? "" : msg);

        /* we're done with the message - if we're to delete it, do so */
        if (caller_deletes)
            th_free(msg);
    }
}

/*
 *   Add a part
 */
void CTadsStatusline::add_part(CTadsStatusPart *newpart, int before_index)
{
    /* link the part to the statusline object */
    newpart->stat_ = this;

    /* find the location in the list where we're inserting the new item */
    CTadsStatusPart *cur, *prv;
    for (prv = 0, cur = parts_ ;
         cur != 0 && before_index > 0 ;
         prv = cur, cur = cur->nxt_, --before_index) ;

    /* link in the new item */
    newpart->nxt_ = cur;
    if (prv != 0)
        prv->nxt_ = newpart;
    else
        parts_ = newpart;

    /* figure the new layout */
    notify_parent_resize();
}

/*
 *   Notify the status line that the parent window has been resized
 */
void CTadsStatusline::notify_parent_resize(float width)
{
    /* if a width was given, remember it; otherwise reuse the last one */
    if (width >= 0)
        last_width_ = width;
    else
        width = last_width_;

    /* count the parts */
    int part_cnt = 0;
    CTadsStatusPart *part;
    for (part = parts_ ; part != 0 ; part = part->nxt_, ++part_cnt) ;
    if (part_cnt == 0)
        return;

    /* create the part width array */
    std::vector<int> widths(part_cnt);

    /* run through the parts and calculate the fixed and proportional widths */
    int i, fixed_width = 0, pro_width = 0, pro_cnt = 0;
    for (part = parts_, i = 0 ; part != 0 ; part = part->nxt_, ++i)
    {
        /* calculate this part's width, and store it in the widths array */
        int wid = widths[i] = part->calc_width();

        /*
         *   if it's positive, it's a fixed width in pixels; otherwise it's a
         *   proportional share of the leftover space
         */
        if (wid >= 0)
            fixed_width += wid;
        else
        {
            pro_width += wid;
            ++pro_cnt;
        }
    }

    /* figure the leftover space available for the proportional widths */
    int avail = (int)width - fixed_width;
    int rem = avail;

    /* divvy up the leftover space among the proportional items */
    for (i = 0 ; i < part_cnt ; ++i)
    {
        /* if this is a proportional item, allocate space */
        if (widths[i] < 0 && pro_width != 0)
        {
            /*
             *   If this is the last proportional item, simply allocate it
             *   all of the remaining proportional space; this avoids being
             *   off by a pixel one way or the other due to rounding.  If
             *   it's not the last proportional item, allocate its pro rata
             *   space.
             */
            widths[i] = (pro_cnt == 1 ? rem : (widths[i] * avail) / pro_width);

            /* deduct this allocation from the remaining space */
            rem -= widths[i];

            /* count this proportional item */
            --pro_cnt;
        }
    }

    /* convert the widths to right-edge positions for set_parts() */
    for (i = 1 ; i < part_cnt ; ++i)
        widths[i] += widths[i-1];

    /* set the new layout */
    set_parts(part_cnt, widths.data());

    /* notify each source of the change in its part's position */
    int left = 0;
    for (part = parts_, i = 0 ; part != 0 ; part = part->nxt_, ++i)
    {
        RECT rc;
        rc.left = left;
        rc.right = widths[i];
        rc.top = 0;
        rc.bottom = (int)height_;
        left = widths[i];

        for (CTadsStatusSourceListitem *src = part->sources_ ;
             src != 0 ; src = src->nxt_)
            src->item_->status_change_pos(&rc);
    }
}

/*
 *   Handle a WM_MENUSELECT message
 */
void CTadsStatusline::menu_select_msg(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    /* presume we won't need to update anything */
    int upd = FALSE;

    /* run through all parts */
    for (CTadsStatusPart *part = parts_ ; part != 0 ; part = part->nxt_)
    {
        /* run through all sources in this part */
        for (CTadsStatusSourceListitem *src = part->sources_ ;
             src != 0 ; src = src->nxt_)
        {
            /* give this source a chance to handle it */
            upd |= src->item_->do_status_menuselect(wparam, lparam);
        }
    }

    /* if necessary, do an update */
    if (upd)
        update();
}

/*
 *   Set the part layout - mirrors the old SB_SETPARTS message
 */
void CTadsStatusline::set_parts(int count, const int *right_edges)
{
    if (count < 0)
        count = 0;

    part_edges_.assign(right_edges, right_edges + count);

    /* resize the text array, preserving text for parts that still exist */
    part_texts_.resize(count);
}

/*
 *   Set a single part's displayed text - mirrors the old SB_SETTEXT
 *   message
 */
void CTadsStatusline::set_part_text(int index, const textchar_t *text)
{
    if (index < 0)
        return;

    if ((size_t)index >= part_texts_.size())
        part_texts_.resize(index + 1);

    part_texts_[index] = (text != 0 ? text : "");
}

/*
 *   Draw the status line for the current frame
 */
void CTadsStatusline::render(float x, float y, float width)
{
    /* remember the width we're laying out for */
    last_width_ = width;

    /* a single text line, plus a little padding above and below */
    height_ = ImGui::GetFrameHeightWithSpacing();

    ImVec2 wp(x, y);

    /*
     *   Draw directly into the foreground draw list, on top of every
     *   window, rather than opening our own ImGui window: this is a fixed
     *   piece of chrome that should always be visible and look the same
     *   (classic status-bar grey), regardless of the app's theme, of
     *   window z-order, or of any style push/pop imbalance elsewhere in
     *   the not-yet-fully-ported window tree.
     */
    ImDrawList *draw_list = ImGui::GetForegroundDrawList();

    const ImU32 bg_col = IM_COL32(212, 212, 212, 255);
    const ImU32 text_col = IM_COL32(0, 0, 0, 255);
    const ImU32 border_col = IM_COL32(128, 128, 128, 255);

    draw_list->AddRectFilled(ImVec2(wp.x, wp.y),
                              ImVec2(wp.x + width, wp.y + height_), bg_col);

    /* draw a thin separator above the status line */
    draw_list->AddLine(ImVec2(wp.x, wp.y), ImVec2(wp.x + width, wp.y),
                        border_col);

    int left = 0;
    for (size_t i = 0 ; i < part_edges_.size() ; ++i)
    {
        int right = (part_edges_[i] < 0 ? (int)width : part_edges_[i]);
        if (right > (int)width)
            right = (int)width;

        /* clip this part's text to its slot so long text doesn't overrun */
        draw_list->PushClipRect(ImVec2(wp.x + left, wp.y),
                                 ImVec2(wp.x + right, wp.y + height_), true);
        const std::string &text =
            (i < part_texts_.size() ? part_texts_[i] : std::string());
        draw_list->AddText(ImVec2(wp.x + left + 4.0f, wp.y + 2.0f), text_col,
                            text.c_str());
        draw_list->PopClipRect();

        /* draw a separator between this part and the next one */
        if (i > 0)
            draw_list->AddLine(ImVec2(wp.x + left, wp.y + 1.0f),
                                ImVec2(wp.x + left, wp.y + height_ - 1.0f),
                                border_col);

        left = right;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Status source interface statics
 */

textchar_t CTadsStatusSource::OWNER_DRAW[1] = "";
