// Format Specification:
//
// 1. "%%"                                   -> Escaped literal '%'
// 2. "%<option>"                            -> One Option shortcut (%v, %t, %x, %r, %l)
// 3. "%{Options:Indent:MaxWidth:Precision}" -> Full format block (e.g. "%{r+:1:10}")
//
// Options (1-character type-contextual flags, combinable in any order):
//   v : Value (default)
//   t : Type name
//   R : Right-aligned
//   L : Left-aligned
//   + : Force sign (+/-)
//   x : Hexadecimal
//   b : Binary
//   r : Raw
//   # : Pretty
//
// - All sub-fields are optional: "%{x::16}" (hex, width 16), "%{::20}" (width 20).
// - String requires no escaping of '{' or '}'
//

#pragma once
#include <cstdint>



namespace PrintFormat {

    constexpr int DEFAULT_INT = -1;

    struct Info {
        bool printValue       = true;  // 'v' (default)
        bool printType        = false; // 't'
        bool alignRight       = false; // 'R'
        bool alignLeft        = false; // 'L'
        bool showSign         = false; // '+'
        bool hex              = false; // 'x'
        bool binary           = false; // 'b'
        bool raw              = false; // 'r'
        bool pretty           = false; // '#'
        bool crop             = false; // '<'

        // -1 omitted
        int indent            = DEFAULT_INT;
        int width             = DEFAULT_INT;
        int precision         = DEFAULT_INT;
    };

    enum Err {
        OK,
        OK_ESCAPED,
        INCOMPLETE_FORMAT_SPECIFIER,
        INVALID_OPTION,
        UNEXPECTED_SYMBOL,
        UNEXPECTED_END
    };

    Err parse(const char* fmt, uint64_t* idx, Info* info);

    inline const char* getErrorDescription(PrintFormat::Err err) {
        switch (err) {
            case PrintFormat::INVALID_OPTION:
                return "Invalid format option flag.";
            case PrintFormat::UNEXPECTED_SYMBOL:
                return "Expected an integer or ':' in format subfield.";
            case PrintFormat::UNEXPECTED_END:
                return "Unclosed format block; expected '}' before end of string.";
            case PrintFormat::INCOMPLETE_FORMAT_SPECIFIER:
                return "Incomplete format specifier; unexpected trailing '%'.";
            default:
                return "Syntax error in format specifier.";
        }
    }

}
