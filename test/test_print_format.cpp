#include "test_core.h"
#include "../src/print_format.h"
#include <cstdint>



thread_local PrintFormat::Info info;
thread_local uint64_t idx;

inline void gPrintFormatPreSuite() {

}

inline void gPrintFormatPostSuite() {
    //Arena::release(&arena);
}

inline void gPrintFormatPreCase() {
    idx = 0;
    info = PrintFormat::Info {};
}

inline void gPrintFormatPostCase() {
}

PrintFormat::Err parse(const char* fmt, uint64_t* idx) {
    *idx = 0;
    info = PrintFormat::Info {};
    return PrintFormat::parse(fmt, idx, &info);
}


inline Test::Case gPrintFormatCases[] = {

    // 1. Escaped Literal Percent ("%%")
    TEST_CASE(test_format_escaped_percent) {
        PrintFormat::Err err = parse("%", &idx);
        Test::assert(err, PrintFormat::Err::OK_ESCAPED);
        Test::assert(idx, 1);
    }},

    // 2. 1-Character Shortcuts (%v, %t, %x, %b, %r, %l, %+, %#)
    TEST_CASE(test_format_single_char_shortcuts) {
        // %v (Value)
        Test::assert(parse("v", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.printValue == true);

        // %t (Type)
        Test::assert(parse("t", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.printType == true);
        Test::assert(info.printValue == false);

        // %x (Hex)
        Test::assert(parse("x", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.hex == true);

        // %b (Binary)
        Test::assert(parse("b", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.binary == true);

        // %r (Right align) and %l (Left align)
        Test::assert(parse("R", &idx), PrintFormat::Err::OK);
        Test::assert(info.alignRight == true);

        Test::assert(parse("L", &idx), PrintFormat::Err::OK);
        Test::assert(info.alignLeft == true);

        // %+ (Show sign) and %# (Pretty)
        Test::assert(parse("+", &idx), PrintFormat::Err::OK);
        Test::assert(info.showSign == true);

        Test::assert(parse("#", &idx), PrintFormat::Err::OK);
        Test::assert(info.pretty == true);
    }},

    // 3. Text Immediately Attached to 1-Char Shortcut ("%vwhatever")
    TEST_CASE(test_format_attached_text_after_shortcut) {
        // "%vwhatever" -> Consumes exactly 1 char ('v'), idx points to 'w' at [1]
        PrintFormat::Err err = parse("vwhatever", &idx);
        Test::assert(err, PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.printValue == true);

        // "%t_suffix" -> Consumes 1 char ('t'), idx points to '_' at [1]
        err = parse("t_suffix", &idx);
        Test::assert(err, PrintFormat::Err::OK);
        Test::assert(idx, 1);
        Test::assert(info.printType == true);
    }},

    // 4. Full Block Format ("%{Options:Indent:MaxWidth:Precision}")
    TEST_CASE(test_format_full_block_parsing) {
        // 1. Full 4 sub-fields: "%{r+l:1:10:2}"
        Test::assert(parse("{R+L:1:10:2}", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 12); // Consumed all 12 characters up to '}'
        Test::assert(info.alignLeft == true);
        Test::assert(info.showSign == true);
        Test::assert(info.indent, 1);
        Test::assert(info.width, 10);
        Test::assert(info.precision, 2);

        // 2. Partial sub-fields: "%{x::16}" (hex, width 16, omitted indent & precision)
        Test::assert(parse("{x::16}", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 7);
        Test::assert(info.hex == true);
        Test::assert(info.indent, PrintFormat::DEFAULT_INT);
        Test::assert(info.width, 16);
        Test::assert(info.precision, PrintFormat::DEFAULT_INT);

        // 3. Width only: "%{::20}"
        Test::assert(parse("{::20}", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 6);
        Test::assert(info.width, 20);

        // 4. Options only: "%{t#}"
        Test::assert(parse("{t#}", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 4);
        Test::assert(info.printType == true);
        Test::assert(info.pretty == true);

        // 5. Empty block: "%{}"
        Test::assert(parse("{}", &idx), PrintFormat::Err::OK);
        Test::assert(idx, 2);
    }},

    // 5. Text Immediately Attached to Block ("%{r:1:10}whatever")
    TEST_CASE(test_format_attached_text_after_block) {
        // "%{r:1:10}whatever" -> Consumes 10 chars, idx points to 'w' at [10]
        PrintFormat::Err err = parse("{R:1:10}whatever", &idx);
        Test::assert(err, PrintFormat::Err::OK);
        Test::assert(idx, 8);
        Test::assert(info.alignRight == true);
        Test::assert(info.indent, 1);
        Test::assert(info.width, 10);
    }},

    // 6. Error Conditions & Diagnostics
    TEST_CASE(test_format_error_handling) {

        // 1. Invalid option character: %q
        Test::assert(parse("q", &idx), PrintFormat::Err::INVALID_OPTION);
        Test::assert(idx, 0); // Points directly at 'q'!

        // 2. Invalid option inside block: %{r+q:10}
        Test::assert(parse("{R+q:10}", &idx), PrintFormat::Err::INVALID_OPTION);
        Test::assert(idx, 3); // Points directly at 'q'!

        // 3. Unclosed block
        Test::assert(parse("{R:1:10", &idx), PrintFormat::Err::UNEXPECTED_SYMBOL);

        // 4. Non-digit character in integer subfield: %{r:abc:10}
        Test::assert(parse("{R:abc:10}", &idx), PrintFormat::Err::UNEXPECTED_SYMBOL);

        // 5. Incomplete trailing '%' at end of string: "%"
        Test::assert(parse("", &idx), PrintFormat::Err::INVALID_OPTION);
    }}

};



extern const Test::Suite gPrintFormatSuite = {
    "Print Format",
    NULL,
    gPrintFormatCases,
    sizeof(gPrintFormatCases) / sizeof(Test::Case),
    gPrintFormatPreCase,
    gPrintFormatPostCase,
    gPrintFormatPreSuite,
    gPrintFormatPostSuite,
};
