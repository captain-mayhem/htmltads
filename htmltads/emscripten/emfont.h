#ifndef EMFONT_H
#define EMFONT_H

#include "htmlsys.h"

class CHtmlSysFont_emscripten : public CHtmlSysFont{
public:
    CHtmlSysFont_emscripten();
    ~CHtmlSysFont_emscripten();

    /* get metrics */
    void get_font_metrics(class CHtmlFontMetrics *);

    /* is this a fixed-pitch font? */
    int is_fixed_pitch() { return is_fixed_pitch_; }

    /* get the font's em size */
    int get_em_size() { return em_size_; }

private:
    /* am I a fixed-pitch (monospaced) font? */
    int is_fixed_pitch_ = FALSE;

    /* my em size, in pxels */
    int em_size_ = 18;
};

#endif