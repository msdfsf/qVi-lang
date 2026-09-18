#pragma once
#include "print_format.h"
#include "lexer.h"
#include <cstdint>



namespace PrintFormat {

    bool parseOption(char ch, Info* info) {
        switch (ch) {
            case 'v': info->printValue = true;  break;
            case 'x': info->hex        = true;  info->printValue = true; break;
            case 'b': info->binary     = true;  info->printValue = true; break;
            case 'r': info->raw        = true;  info->printValue = true; break;
            case 't': info->printType  = true;  break;
            case 'R': info->alignRight = true;  break;
            case 'L': info->alignLeft  = true;  break;
            case '+': info->showSign   = true;  info->printValue = true; break;
            case '#': info->pretty     = true;  break;
            default:  return false;
        }

        return true;
    }

    const uint64_t INVALID_INT = -2LL;
    uint64_t parseInt(const char* str, uint64_t* idx) {
        uint64_t startIdx = *idx;

        uint64_t num = Lex::parseInt(str, idx);
        const char ch = str[*idx];
        if (ch != ':' && ch != '}') {
            return INVALID_INT;
        }

        if (startIdx == *idx) {
            (*idx)++;
            return -1;
        } else {
            (*idx)++;
            return num;
        }
    }

    // TODO: proper errors
    Err parse(const char* fmt, uint64_t* idx, Info* info) {
        char ch = fmt[*idx];
        if (ch == '%') {
            (*idx)++;
            return Err::OK_ESCAPED;
        } else if (ch == '{') {
            (*idx)++;

            while (1) {
                const char ch = fmt[*idx];
                if (ch == '\0') {
                    return Err::UNEXPECTED_END;
                }

                if (ch == ':' || ch == '}') {
                    (*idx)++;
                    break;
                }

                if (!parseOption(ch, info)) {
                    return Err::INVALID_OPTION;
                }

                (*idx)++;
            }
            if (fmt[*idx - 1] == '}') return Err::OK;

            uint64_t num;

            num = parseInt(fmt, idx);
            if (num == INVALID_INT) return Err::UNEXPECTED_SYMBOL;
            info->indent = num;
            if (fmt[*idx - 1] == '}') return Err::OK;

            num = parseInt(fmt, idx);
            if (num == INVALID_INT) return Err::UNEXPECTED_SYMBOL;
            info->width = num;
            if (fmt[*idx - 1] == '}') return Err::OK;

            num = parseInt(fmt, idx);
            if (num == INVALID_INT) return Err::UNEXPECTED_SYMBOL;
            info->precision = num;
            if (fmt[*idx - 1] != '}') return Err::UNEXPECTED_SYMBOL;

            return Err::OK;
        } else {
            info->printValue = false;
            if (!parseOption(ch, info)) {
                return Err::INVALID_OPTION;
            }

            (*idx)++;
            return Err::OK;
        }
    }

}
