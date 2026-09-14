// Unit tests for the localisation tables: completeness, layout budgets, the character set the
// pixel fonts can draw, and the id arithmetic the scenes and the campaign table rely on.
#include "doctest.h"

#include "game/strings_ids.h"
#include "game/strings_lines.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

#define MOKU_STR_NAME(id) #id,
const char* const NAMES[] = { STRING_LIST(MOKU_STR_NAME) };
#undef MOKU_STR_NAME

constexpr int COUNT = static_cast<int>(Str::COUNT);
static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == COUNT, "one name per id");

const Language LANGS[2] = { Language::EN, Language::FR };
const char* const LANG_NAMES[2] = { "EN", "FR" };

// Layout budget per category, keyed by id prefix (longest match wins). Characters, not bytes:
// the 8 px variable-width font averages ~4.3 px per character, so these are the widths of the
// boxes in the mockups. See docs/decisions/strings.md.
// `wrapped` marks the categories the game breaks at runtime (render::wrap) rather than at the
// newlines the author wrote: for those, the line count and line length here mean nothing and the
// pixel-width cases at the bottom of this file are the real check.
struct Budget { const char* prefix; int lines; int chars; bool wrapped = false; };
const Budget BUDGETS[] = {
    { "HUDL_", 1, 12 },   // HUD / row labels          88 px column
    { "HUDV_", 1, 10 },   // HUD values
    { "VAL_", 1, 12 },    // option and setting values
    { "MENU_", 1, 14 },   // menu items
    { "TITLE_", 1, 20 },  // screen titles
    { "HINT_", 1, 20 },   // button hints inside a column
    { "NAV_", 1, 40 },    // full-width navigation lines
    { "BAN_", 1, 12 },    // event banners, font_16
    { "CHAP_", 1, 14 },   // chapter labels on the map
    { "MNAME_", 1, 16 },  // mission names
    { "WHO_", 1, 12 },    // speaker names
    { "VOICE_", 1, 10 },  // helper voice prefixes
    { "OPPT_", 1, 20 },   // opponent titles
    { "OPP_", 1, 8 },     // opponent names
    { "OBJ_", 3, 40, true },   // objective card: wrapped into the HUD and the briefing
    { "RANKH_", 2, 26 },  // rank hint card
    { "DLG_", 3, 26 },    // tutor / rival dialogue box
    { "MSG_", 2, 34 },    // messages on full-width panels
    { "BLURB_", 2, 34 },  // opponent blurbs
    { "LVLB_", 3, 18 },   // AI level blurbs (72 px column)
    { "SAY_", 2, 24, true },   // opponent speech bubbles, wrapped
    { "INTJ_", 2, 22, true },  // helper interjection bubbles, wrapped
};

const Budget* budget_for(const char* name)
{
    const Budget* best = nullptr;
    for (const Budget& b : BUDGETS) {
        const size_t n = std::strlen(b.prefix);
        if (std::strncmp(name, b.prefix, n) == 0 && (best == nullptr || n > std::strlen(best->prefix))) {
            best = &b;
        }
    }
    return best;
}

// Every glyph the two pixel fonts contain (docs/decisions/fonts.md): printable ASCII plus these.
const char* const EXTRA_GLYPHS =
    "ÀÂÇÉÈÊËÎÏÔÙÛÜŒàâçéèêëîïôùûüœ’«»…·★☆▲▼▶◀×←↑→↓▮▯©";

std::vector<uint32_t> decode_utf8(const char* s, bool& ok)
{
    std::vector<uint32_t> out;
    ok = true;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    while (*p) {
        uint32_t cp = 0;
        int extra = 0;
        if (*p < 0x80) { cp = *p; extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1Fu; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0Fu; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07u; extra = 3; }
        else { ok = false; return out; }
        ++p;
        for (int i = 0; i < extra; ++i, ++p) {
            if ((*p & 0xC0) != 0x80) { ok = false; return out; }
            cp = (cp << 6) | (*p & 0x3Fu);
        }
        out.push_back(cp);
    }
    return out;
}

std::vector<std::vector<uint32_t>> lines_of(const char* s, bool& ok)
{
    std::vector<std::vector<uint32_t>> lines(1);
    for (uint32_t cp : decode_utf8(s, ok)) {
        if (cp == '\n') lines.emplace_back();
        else lines.back().push_back(cp);
    }
    return lines;
}

std::string where(int i, int lang) { return std::string(NAMES[i]) + " [" + LANG_NAMES[lang] + "]"; }

}  // namespace

TEST_CASE("both tables are complete and hold no empty text")
{
    REQUIRE(COUNT > 400);
    for (int i = 0; i < COUNT; ++i) {
        for (int l = 0; l < 2; ++l) {
            const char* s = tr(static_cast<Str>(i), LANGS[l]);
            INFO(where(i, l));
            REQUIRE(s != nullptr);
            CHECK(s[0] != '\0');
            CHECK(std::strcmp(s, "?") != 0);       // the out-of-range marker
        }
    }
}

TEST_CASE("every id belongs to a category and fits its box")
{
    for (int i = 0; i < COUNT; ++i) {
        const Budget* b = budget_for(NAMES[i]);
        INFO(NAMES[i]);
        REQUIRE(b != nullptr);                     // unknown prefix: give the id a category
        for (int l = 0; l < 2; ++l) {
            bool ok = false;
            const char* s = tr(static_cast<Str>(i), LANGS[l]);
            const auto lines = lines_of(s, ok);
            INFO(where(i, l) << " = " << s);
            CHECK(ok);
            CHECK(static_cast<int>(lines.size()) <= b->lines);
            for (const auto& line : lines) {
                if (!b->wrapped) {
                    CHECK(static_cast<int>(line.size()) <= b->chars);
                }
                CHECK_FALSE(line.empty());                       // no blank line inside a text
                if (!line.empty()) {
                    CHECK(line.front() != ' ');                  // no leading space
                    CHECK(line.back() != ' ');                   // no trailing space
                }
            }
        }
    }
}

TEST_CASE("only characters the pixel fonts can draw")
{
    bool ok = false;
    const std::vector<uint32_t> extra = decode_utf8(EXTRA_GLYPHS, ok);
    REQUIRE(ok);
    for (int i = 0; i < COUNT; ++i) {
        for (int l = 0; l < 2; ++l) {
            const char* s = tr(static_cast<Str>(i), LANGS[l]);
            const std::vector<uint32_t> cps = decode_utf8(s, ok);
            INFO(where(i, l) << " = " << s);
            REQUIRE(ok);
            for (uint32_t cp : cps) {
                if (cp == '\n') continue;                        // explicit line break
                if (cp >= 0x20 && cp < 0x7F) {
                    CHECK(cp != '\'');                           // use the typographic apostrophe
                    CHECK(cp != '"');                            // use « » instead
                    continue;
                }
                bool found = false;
                for (uint32_t e : extra) found = found || e == cp;
                INFO("unsupported code point " << cp);
                CHECK(found);
            }
        }
    }
}

TEST_CASE("the French table is a translation, not a copy")
{
    int same = 0;
    std::vector<std::string> identical;
    for (int i = 0; i < COUNT; ++i) {
        const char* en = tr(static_cast<Str>(i), Language::EN);
        const char* fr = tr(static_cast<Str>(i), Language::FR);
        if (std::strcmp(en, fr) == 0) { ++same; identical.push_back(NAMES[i]); }
    }
    // Proper nouns (INDI, REX, KOMI, SEKI, the opponents) and symbols (2×A, ▼ A) stay put.
    INFO("identical entries: " << same << " of " << COUNT);
    CHECK(same * 100 < COUNT * 15);
    // ... and the long texts must all be translated.
    for (int i = 0; i < COUNT; ++i) {
        const char* en = tr(static_cast<Str>(i), Language::EN);
        if (std::strlen(en) < 20) continue;
        INFO(NAMES[i] << " is identical in both languages");
        CHECK(std::strcmp(en, tr(static_cast<Str>(i), Language::FR)) != 0);
    }
}

TEST_CASE("language selection")
{
    const Language before = current_language();
    set_language(Language::EN);
    CHECK(current_language() == Language::EN);
    CHECK(std::strcmp(tr(Str::MENU_CAMPAIGN), "CAMPAIGN") == 0);
    set_language(Language::FR);
    CHECK(current_language() == Language::FR);
    CHECK(std::strcmp(tr(Str::MENU_CAMPAIGN), "CAMPAGNE") == 0);
    CHECK(std::strcmp(tr(Str::MENU_CAMPAIGN, Language::EN), "CAMPAIGN") == 0);   // explicit wins
    set_language(before);
    // An id outside the table is reported, not a crash or a null pointer.
    CHECK(std::strcmp(tr(static_cast<Str>(COUNT)), "?") == 0);
    CHECK(std::strcmp(tr(static_cast<Str>(COUNT + 500), Language::FR), "?") == 0);
}

TEST_CASE("mission text is laid out as 20 blocks of 7 ids")
{
    constexpr int STRIDE = 7;   // name, objective, rank hint, before, hint, win, fail
    for (int m = 1; m <= 20; ++m) {
        const int base = static_cast<int>(Str::MNAME_01) + (m - 1) * STRIDE;
        INFO("mission " << m);
        CHECK(std::string(NAMES[base]).rfind("MNAME_", 0) == 0);
        CHECK(std::string(NAMES[base + 1]).rfind("OBJ_", 0) == 0);
        CHECK(std::string(NAMES[base + 2]).rfind("RANKH_", 0) == 0);
        CHECK(std::string(NAMES[base + 3]).find("_BEFORE") != std::string::npos);
        CHECK(std::string(NAMES[base + 4]).find("_HINT") != std::string::npos);
        CHECK(std::string(NAMES[base + 5]).find("_WIN") != std::string::npos);
        CHECK(std::string(NAMES[base + 6]).find("_FAIL") != std::string::npos);
    }
    CHECK(static_cast<int>(Str::MNAME_20) - static_cast<int>(Str::MNAME_01) == 19 * STRIDE);
    CHECK(static_cast<int>(Str::DLG_07_BEFORE) == static_cast<int>(Str::MNAME_07) + 3);
    // The five chained tsumego of mission 18: name, objective, before, hint, win.
    constexpr int T_STRIDE = 5;
    CHECK(static_cast<int>(Str::MNAME_T18_5) - static_cast<int>(Str::MNAME_T18_1) == 4 * T_STRIDE);
    CHECK(static_cast<int>(Str::OBJ_T18_1) == static_cast<int>(Str::MNAME_T18_1) + 1);
    CHECK(static_cast<int>(Str::DLG_T18_1_WIN) == static_cast<int>(Str::MNAME_T18_1) + 4);
    // One rival line per mission from 15 on.
    CHECK(static_cast<int>(Str::DLG_RIVAL_M20) - static_cast<int>(Str::DLG_RIVAL_M15) == 5);
}

TEST_CASE("tables indexed by an enum value are contiguous and in enum order")
{
    // save::Settings / missions.h orders: unlocks 1..5, AI levels 0..4, opponents 0..4,
    // helpers 0..2, chapters 1..5.
    CHECK(static_cast<int>(Str::MSG_UNLOCK_19_MASTER) - static_cast<int>(Str::MSG_UNLOCK_SANDBOX_9_EASY) == 4);
    CHECK(static_cast<int>(Str::VAL_LVL_MASTER) - static_cast<int>(Str::VAL_LVL_VERY_EASY) == 4);
    CHECK(static_cast<int>(Str::LVLB_MASTER) - static_cast<int>(Str::LVLB_VERY_EASY) == 4);
    CHECK(static_cast<int>(Str::OPP_TENGEN) - static_cast<int>(Str::OPP_PEBBLE) == 4);
    CHECK(static_cast<int>(Str::OPPT_TENGEN) - static_cast<int>(Str::OPPT_PEBBLE) == 4);
    CHECK(static_cast<int>(Str::BLURB_TENGEN) - static_cast<int>(Str::BLURB_PEBBLE) == 4);
    CHECK(static_cast<int>(Str::WHO_REX) - static_cast<int>(Str::WHO_SEN) == 2);
    CHECK(static_cast<int>(Str::VOICE_REX) - static_cast<int>(Str::VOICE_INDI) == 1);
    CHECK(static_cast<int>(Str::CHAP_5) - static_cast<int>(Str::CHAP_1) == 4);
    CHECK(static_cast<int>(Str::VAL_SKIN_DINO) - static_cast<int>(Str::VAL_SKIN_CLASSIC) == 2);
    CHECK(static_cast<int>(Str::VAL_PLAYER_2) - static_cast<int>(Str::VAL_PLAYER_1) == 1);
    // Opponent lines: 5 characters x 6 events, in this order.
    const char* const EVENTS[6] = { "_INTRO", "_CAPTURE", "_CAPTURED", "_WIN", "_LOSE", "_ATARI" };
    for (int o = 0; o < 5; ++o) {
        for (int e = 0; e < 6; ++e) {
            const int i = static_cast<int>(Str::SAY_PEBBLE_INTRO) + o * 6 + e;
            INFO("opponent " << o << " event " << e << " = " << NAMES[i]);
            CHECK(std::string(NAMES[i]).rfind("SAY_", 0) == 0);
            CHECK(std::string(NAMES[i]).find(EVENTS[e]) != std::string::npos);
        }
    }
    // Helper interjections: 3 helpers x 5 events, in this order.
    const char* const HELPERS[3] = { "SEN", "INDI", "REX" };
    const char* const HELPER_EVENTS[5] = { "_ATARI", "_CAPTURE", "_CLEAR", "_FAIL", "_THINK" };
    for (int h = 0; h < 3; ++h) {
        for (int e = 0; e < 5; ++e) {
            const int i = static_cast<int>(Str::INTJ_SEN_ATARI) + h * 5 + e;
            INFO("helper " << h << " event " << e << " = " << NAMES[i]);
            CHECK(std::string(NAMES[i]).find(HELPERS[h]) != std::string::npos);
            CHECK(std::string(NAMES[i]).find(HELPER_EVENTS[e]) != std::string::npos);
        }
    }
}

TEST_CASE("copy that other screens build on")
{
    // The capture banner takes a runtime suffix and must still fit the 12 character banner box.
    for (int l = 0; l < 2; ++l) {
        const std::string banner = std::string(tr(Str::BAN_CAPTURE, LANGS[l])) + " +99";
        bool ok = false;
        INFO(banner);
        CHECK(static_cast<int>(decode_utf8(banner.c_str(), ok).size()) <= 12);
        CHECK(ok);
    }
    // The atari one-liner gets " E4." appended: it stays inside the 34 character message box.
    for (int l = 0; l < 2; ++l) {
        for (Str id : { Str::MSG_ATARI_BLACK, Str::MSG_ATARI_WHITE }) {
            const std::string line = std::string(tr(id, LANGS[l])) + " T19.";
            bool ok = false;
            INFO(line);
            CHECK(static_cast<int>(decode_utf8(line.c_str(), ok).size()) <= 34);
            CHECK(ok);
        }
    }
    // Mission 7 is the briefing in the mockups: the names are the ones drawn there.
    CHECK(std::strcmp(tr(Str::MNAME_07, Language::EN), "LAST BREATH") == 0);
    CHECK(std::strcmp(tr(Str::MNAME_07, Language::FR), "DERNIER SOUFFLE") == 0);
    CHECK(std::strcmp(tr(Str::WHO_SEN, Language::EN), "MASTER SEN") == 0);
    CHECK(std::strcmp(tr(Str::TITLE_MISSION_CLEAR, Language::EN), "MISSION CLEAR") == 0);
}

TEST_CASE("text_lines splits every text into the pieces a scene draws")
{
    // bn::sprite_text_generator raises BN_ERROR on a control character, so every scene draws
    // text through text_lines(); no line it returns may contain a '\n'.
    for (int i = 0; i < COUNT; ++i) {
        const Budget* b = budget_for(NAMES[i]);
        REQUIRE(b != nullptr);
        for (int l = 0; l < 2; ++l) {
            const char* s = tr(static_cast<Str>(i), LANGS[l]);
            TextLine lines[4] = {};
            const int n = text_lines(s, lines, 4);
            INFO(where(i, l) << " = " << s);
            CHECK(n >= 1);
            CHECK(n <= b->lines);
            int total = 0;
            for (int k = 0; k < n; ++k) {
                CHECK(lines[k].size > 0);
                CHECK(std::memchr(lines[k].data, '\n', static_cast<size_t>(lines[k].size)) == nullptr);
                total += lines[k].size;
            }
            CHECK(total == static_cast<int>(std::strlen(s)) - (n - 1));   // one '\n' between lines
        }
    }
}

TEST_CASE("text_lines edge cases")
{
    TextLine lines[4] = {};
    CHECK(text_lines(nullptr, lines, 4) == 0);
    CHECK(text_lines("", lines, 4) == 0);
    CHECK(text_lines("abc", lines, 0) == 0);
    CHECK(text_lines("abc", nullptr, 4) == 0);
    REQUIRE(text_lines("one", lines, 4) == 1);
    CHECK(std::string(lines[0].data, static_cast<size_t>(lines[0].size)) == "one");
    REQUIRE(text_lines("one\ntwo\nthree", lines, 4) == 3);
    CHECK(std::string(lines[2].data, static_cast<size_t>(lines[2].size)) == "three");
    CHECK(text_lines("one\ntwo\nthree", lines, 2) == 2);          // extra lines are dropped
    REQUIRE(text_lines("\n\nonly\n", lines, 4) == 1);             // empty pieces are skipped
    CHECK(std::string(lines[0].data, static_cast<size_t>(lines[0].size)) == "only");
}

namespace {

// A stand-in for bn::sprite_text_generator::width(): 6 px per character, like the font's digits.
int six_px_per_char(const char* data, int size, void* ctx)
{
    (void)ctx;
    bool ok = false;
    const std::string text(data, static_cast<size_t>(size));
    return 6 * static_cast<int>(decode_utf8(text.c_str(), ok).size());
}

}  // namespace

TEST_CASE("wrap_text re-wraps text for a narrower box")
{
    char buffer[128];
    TextLine lines[4];

    // One line per word when nothing fits.
    REQUIRE(wrap_text("alpha beta gamma", 1, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 4) == 3);
    CHECK(std::string(lines[1].data, static_cast<size_t>(lines[1].size)) == "beta");

    // Greedy fill: 60 px holds ten characters.
    const int n = wrap_text("one two three four", 60, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 4);
    REQUIRE(n == 2);
    CHECK(std::string(lines[0].data, static_cast<size_t>(lines[0].size)) == "one two");
    CHECK(std::string(lines[1].data, static_cast<size_t>(lines[1].size)) == "three four");

    // The text's own breaks become spaces and are re-wrapped, and no line keeps a control char.
    REQUIRE(wrap_text("one\ntwo three", 120, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 4) == 1);
    CHECK(std::string(lines[0].data, static_cast<size_t>(lines[0].size)) == "one two three");

    // Lines that do not fit in max_lines are dropped, never written past the end.
    lines[2] = { nullptr, 0 };
    CHECK(wrap_text("alpha beta gamma", 1, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 2) == 2);
    CHECK(lines[2].data == nullptr);

    // Rubbish in, nothing out.
    CHECK(wrap_text(nullptr, 60, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 4) == 0);
    CHECK(wrap_text("text", 60, nullptr, nullptr, buffer, sizeof(buffer), lines, 4) == 0);
    CHECK(wrap_text("text", 60, six_px_per_char, nullptr, buffer, 1, lines, 4) == 0);
    CHECK(wrap_text("text", 60, six_px_per_char, nullptr, buffer, sizeof(buffer), lines, 0) == 0);

    // A buffer too small cuts between characters, never inside a UTF-8 sequence.
    char small[8];
    REQUIRE(wrap_text("étoile filante", 200, six_px_per_char, nullptr, small, sizeof(small), lines, 4) == 1);
    const std::string cut(lines[0].data, static_cast<size_t>(lines[0].size));
    bool ok = false;
    decode_utf8(cut.c_str(), ok);
    CHECK(ok);
    CHECK(cut.size() <= 7);

    // Every objective fits the 84 px HUD column in three lines (measured with the real font
    // metrics in tools/../docs/decisions/strings.md; 6 px per character is the pessimistic case
    // used here, so this only checks that the wrapper always terminates with something to draw).
    for (int i = 0; i < COUNT; ++i) {
        if (std::string(NAMES[i]).rfind("OBJ_", 0) != 0) continue;
        for (int l = 0; l < 2; ++l) {
            const int count = wrap_text(tr(static_cast<Str>(i), LANGS[l]), 84, six_px_per_char,
                                        nullptr, buffer, sizeof(buffer), lines, 4);
            INFO(where(i, l));
            CHECK(count >= 1);
            for (int k = 0; k < count; ++k) {
                CHECK(lines[k].size > 0);
                CHECK(std::memchr(lines[k].data, '\n', static_cast<size_t>(lines[k].size)) == nullptr);
            }
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Pixel widths. The budgets above are in characters, which is a good proxy but not a proof: a line
// of capitals is wider than a line of lowercase. include/moku_font_widths.h holds the same width
// tables the GBA draws with, so these cases measure the real thing.
// -------------------------------------------------------------------------------------------------

#include "moku_font_widths.h"

namespace {

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;

    for (char c : text) {
        if (c == '\n') {
            out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    out.push_back(current);
    return out;
}

int px(const std::string& text)
{
    bool unknown = false;
    const int width = fonts::moku_text_width(text.c_str(), fonts::moku_font_8_widths, &unknown);
    CHECK_FALSE(unknown);
    return width;
}

// render::wrap: break at spaces so no line is wider than max_width, at most max_lines lines.
std::vector<std::string> wrap_px(const std::string& text, int max_width, int max_lines)
{
    std::vector<std::string> out;

    for (const std::string& paragraph : split_lines(text)) {
        std::string line;
        std::size_t i = 0;

        while (i < paragraph.size() && static_cast<int>(out.size()) < max_lines) {
            std::size_t space = paragraph.find(' ', i);
            const std::string word = paragraph.substr(i, space == std::string::npos ? space : space - i);
            const std::string candidate = line.empty() ? word : line + " " + word;

            if (!line.empty() && px(candidate) > max_width) {
                out.push_back(line);
                line = word;
            } else {
                line = candidate;
            }

            if (space == std::string::npos) {
                break;
            }

            i = space + 1;
        }

        if (!line.empty() && static_cast<int>(out.size()) < max_lines) {
            out.push_back(line);
        }
    }

    return out;
}

}  // namespace

TEST_CASE("every string fits its box in pixels, in both languages")
{
    // Width of the box each category is drawn in, from the scene that draws it.
    struct PixelBudget { const char* prefix; int width; int lines; };
    const PixelBudget PIXEL_BUDGETS[] = {
        { "SAY_",  118, 2 },   // the opponent's speech bubble: four 32 px parts, 5 px of padding
        { "INTJ_", 118, 2 },   // the helper's pop-in uses the same bubble
        { "BAN_",   96, 1 },   // the banner plate is two 64 px halves, minus its border
        { "HUDL_",  88, 1 },   // the HUD label column
        { "MENU_", 140, 1 },   // a menu row, minus the cursor
        { "OBJ_",   84, 3 },   // the HUD objective block, HUD_WIDTH in play_scene.cpp
        { "DLG_",  168, 3 },   // the dialogue box, right of the 48 px portrait well
    };

    for (int i = 0; i < COUNT; ++i) {
        const PixelBudget* budget = nullptr;

        for (const PixelBudget& candidate : PIXEL_BUDGETS) {
            if (std::string(NAMES[i]).rfind(candidate.prefix, 0) == 0) {
                budget = &candidate;
            }
        }

        if (!budget) {
            continue;
        }

        for (int l = 0; l < 2; ++l) {
            const std::string text = tr(static_cast<Str>(i), LANGS[l]);
            const auto lines = wrap_px(text, budget->width, budget->lines);
            INFO(where(i, l) << " = " << text << " -> " << lines.size() << " line(s)");

            // Nothing may be dropped: what wrapping produces has to be the whole string back.
            std::string joined;

            for (const auto& line : lines) {
                joined += line;
                CHECK(px(line) <= budget->width);
            }

            std::string stripped;

            for (char c : text) {
                if (c != ' ' && c != '\n') {
                    stripped += c;
                }
            }

            std::string joined_stripped;

            for (char c : joined) {
                if (c != ' ' && c != '\n') {
                    joined_stripped += c;
                }
            }

            CHECK(joined_stripped == stripped);
        }
    }
}

TEST_CASE("mission objectives also fit the briefing box")
{
    // The briefing draws the objective in two lines of 112 px, and render::wrap writes into
    // bn::string<32>, so no line may be longer than 31 characters either.
    for (int i = 0; i < COUNT; ++i) {
        if (std::string(NAMES[i]).rfind("OBJ_", 0) != 0) {
            continue;
        }

        for (int l = 0; l < 2; ++l) {
            const std::string text = tr(static_cast<Str>(i), LANGS[l]);
            const auto lines = wrap_px(text, 112, 2);
            INFO(where(i, l) << " = " << text);
            CHECK(lines.size() <= 2u);

            std::string joined;

            for (const auto& line : lines) {
                joined += line;
                CHECK(px(line) <= 112);
                CHECK(line.size() <= 31u);
            }

            std::string a, b;

            for (char c : text) { if (c != ' ' && c != '\n') a += c; }
            for (char c : joined) { if (c != ' ' && c != '\n') b += c; }

            CHECK(a == b);
        }
    }
}
