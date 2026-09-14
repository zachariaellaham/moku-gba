// Splitting localised text into drawable lines.
//
// Texts in the string tables carry their own line breaks as '\n' (the tables are written to fit
// the boxes in the mockups; no scene re-wraps them). bn::sprite_text_generator raises BN_ERROR on
// any control character, so a multi-line string must NEVER be passed to it whole:
//
//     TextLine lines[3];
//     int n = text_lines(tr(Str::DLG_07_BEFORE), lines, 3);
//     for (int i = 0; i < n; ++i)
//         generator.generate(x, y + i * 10, bn::string_view(lines[i].data, lines[i].size), sprites);
#pragma once

struct TextLine {
    const char* data;   // first byte of the line, NOT null-terminated at its end
    int size;           // bytes (not characters) up to the '\n' or the end of the text
};

// Splits text on '\n' into at most max_lines pieces and returns how many were written.
// A null or empty text yields 0 lines. Text past max_lines lines is dropped.
int text_lines(const char* text, TextLine* out, int max_lines);

// Measures one piece of text; ctx is passed through from wrap_text (e.g. a text generator).
using TextWidthFn = int (*)(const char* data, int size, void* ctx);

// Greedy word wrap for a box narrower than the text was written for - the play HUD draws the
// briefing objective in its 84 px column. The text is copied into buffer with its own '\n'
// breaks turned into spaces and re-wrapped to max_width; the returned lines point into buffer
// (which must outlive them) and never hold a control character. A word that does not fit alone
// gets a line of its own. Returns the number of lines written; the rest of the text is dropped.
//
//     int w(const char* d, int n, void* ctx)
//     { return static_cast<bn::sprite_text_generator*>(ctx)->width(bn::string_view(d, n)); }
//     char buffer[96]; TextLine lines[3];
//     int n = wrap_text(tr(Str::OBJ_03), 84, w, &generator, buffer, 96, lines, 3);
int wrap_text(const char* text, int max_width, TextWidthFn width, void* ctx,
              char* buffer, int buffer_size, TextLine* out, int max_lines);
