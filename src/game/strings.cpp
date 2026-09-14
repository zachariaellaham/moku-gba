// Localised text lookup. Both language tables are built from the same X-macro lists, so a
// missing, extra or misplaced row is a compile error, and tr() is a single array index.
#include "game/strings_ids.h"
#include "game/strings_lines.h"

#include "game/strings_en.h"
#include "game/strings_fr.h"
#include "game/mission_text_en.h"
#include "game/mission_text_fr.h"

namespace {

constexpr int COUNT = static_cast<int>(Str::COUNT);

#define MOKU_STR_TEXT(id, text) text,
const char* const TABLE_EN[COUNT] = { UI_TEXT_EN(MOKU_STR_TEXT) MISSION_TEXT_EN(MOKU_STR_TEXT) };
const char* const TABLE_FR[COUNT] = { UI_TEXT_FR(MOKU_STR_TEXT) MISSION_TEXT_FR(MOKU_STR_TEXT) };
#undef MOKU_STR_TEXT

// Every row names its id, so the order of the text tables can be checked against enum Str
// here instead of trusted. These arrays exist only inside the static_asserts below.
#define MOKU_STR_ID(id, text) static_cast<uint16_t>(Str::id),
constexpr uint16_t IDS_EN[] = { UI_TEXT_EN(MOKU_STR_ID) MISSION_TEXT_EN(MOKU_STR_ID) };
constexpr uint16_t IDS_FR[] = { UI_TEXT_FR(MOKU_STR_ID) MISSION_TEXT_FR(MOKU_STR_ID) };
#undef MOKU_STR_ID

constexpr int size_of(const uint16_t* first, const uint16_t* last) { return static_cast<int>(last - first); }
constexpr bool in_table_order(const uint16_t* ids, int n) {
    for (int i = 0; i < n; ++i) {
        if (ids[i] != static_cast<uint16_t>(i)) return false;   // row i holds a different id
    }
    return n == COUNT;
}

static_assert(size_of(IDS_EN, IDS_EN + sizeof(IDS_EN) / sizeof(IDS_EN[0])) == COUNT,
              "strings_en.h + mission_text_en.h must have exactly one row per Str id");
static_assert(size_of(IDS_FR, IDS_FR + sizeof(IDS_FR) / sizeof(IDS_FR[0])) == COUNT,
              "strings_fr.h + mission_text_fr.h must have exactly one row per Str id");
static_assert(in_table_order(IDS_EN, sizeof(IDS_EN) / sizeof(IDS_EN[0])),
              "English rows are not in STRING_LIST order (compare strings_en.h with strings_list.h)");
static_assert(in_table_order(IDS_FR, sizeof(IDS_FR) / sizeof(IDS_FR[0])),
              "French rows are not in STRING_LIST order (compare strings_fr.h with strings_list.h)");

Language g_language = Language::EN;

}  // namespace

const char* tr(Str id, Language lang) {
    const unsigned index = static_cast<unsigned>(id);
    if (index >= static_cast<unsigned>(COUNT)) return "?";      // never happens for a valid Str
    return lang == Language::FR ? TABLE_FR[index] : TABLE_EN[index];
}

const char* tr(Str id) { return tr(id, g_language); }

void set_language(Language lang) { g_language = lang; }

Language current_language() { return g_language; }

int text_lines(const char* text, TextLine* out, int max_lines)
{
    if (text == nullptr || out == nullptr || max_lines <= 0) return 0;
    int count = 0;
    const char* line = text;
    for (const char* p = text; ; ++p) {
        if (*p != '\n' && *p != '\0') continue;
        const int size = static_cast<int>(p - line);
        if (size > 0) {
            out[count].data = line;
            out[count].size = size;
            ++count;
        }
        if (*p == '\0' || count == max_lines) break;
        line = p + 1;
    }
    return count;
}

int wrap_text(const char* text, int max_width, TextWidthFn width, void* ctx,
              char* buffer, int buffer_size, TextLine* out, int max_lines)
{
    if (text == nullptr || out == nullptr || width == nullptr || buffer == nullptr) return 0;
    if (max_lines <= 0 || buffer_size <= 1) return 0;

    // Copy into the caller's buffer with every separator turned into a space: the spans handed
    // back (and measured) then hold no control character, whatever the text's own breaks were.
    // Copying runs whole UTF-8 sequences, so a text too long for the buffer is cut between
    // characters and never leaves half an accent behind.
    int size = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0'; ) {
        int length = 1;
        if ((*p & 0xE0u) == 0xC0u) length = 2;
        else if ((*p & 0xF0u) == 0xE0u) length = 3;
        else if ((*p & 0xF8u) == 0xF0u) length = 4;
        if (size + length > buffer_size - 1) break;
        if (*p == '\n') { buffer[size++] = ' '; ++p; continue; }
        for (int i = 0; i < length && *p != '\0'; ++i) buffer[size++] = static_cast<char>(*p++);
    }
    buffer[size] = '\0';

    int count = 0;
    char* line = nullptr;                 // first character of the line being filled
    int line_size = 0;
    char* p = buffer;
    while (*p != '\0') {
        while (*p == ' ') ++p;                                   // skip the separators
        if (*p == '\0') break;
        char* word = p;
        while (*p != '\0' && *p != ' ') ++p;
        const int word_size = static_cast<int>(p - word);
        if (line == nullptr) {
            line = word;
            line_size = word_size;
            continue;
        }
        const int grown = static_cast<int>(word + word_size - line);   // line + space + word
        if (width(line, grown, ctx) <= max_width) {
            line_size = grown;
            continue;
        }
        out[count].data = line;
        out[count].size = line_size;
        if (++count == max_lines) return count;
        line = word;
        line_size = word_size;
    }
    if (line != nullptr) {
        out[count].data = line;
        out[count].size = line_size;
        ++count;
    }
    return count;
}
