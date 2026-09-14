// String ids. The tables strings_en.h / strings_fr.h list the texts in the SAME order via the
// STRING_LIST X-macro. Rule: never index tables directly; use tr(Str::ID).
#pragma once
#include <cstdint>

// STRING_LIST(X) is defined in strings_list.h (one X(ID) per string). Text tables define
// X(ID) as the literal. mission_text lists live in mission_text_list.h with the same scheme.
#include "strings_list.h"

enum class Str : uint16_t {
#define X(id) id,
    STRING_LIST(X)
#undef X
    COUNT
};

enum class Language : uint8_t { EN = 0, FR = 1 };

const char* tr(Str id);                 // current language
const char* tr(Str id, Language lang);
void set_language(Language lang);
Language current_language();
