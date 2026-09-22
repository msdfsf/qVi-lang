#include "../io.h"

namespace CLI {

    #define OPTION_SYMBOL '-'

    struct Value {
        Value* next;
        const char* data;
    };

    struct Option {
        const int   id;
        const char* tag;
        const char* name;
        const char* help;

        Value value    = { 0 };
        bool  hasValue = false;

        bool  encountered = false;
    };



    inline void _printIndented(IO::Stream* stream, const char* str, int indent) {
        int i = 0;
        int s = 0;
        while (1) {
            const char ch = str[i];
            if (ch == '\0' || ch == '\n') {
                IO::writef(stream, "%*c%.*s\n", indent, ' ', i - s, str + s);
                s = i + 1;
                if (ch == '\0') return;
            }
            i++;
        }
    }

    inline void printOptionHelp(IO::Stream* stream, Option* options, const int optionCount, const int indent = 14) {
        for (int i = 0; i < optionCount; i++) {
            Option* opt = options + i;
            IO::writef(stream, "  %s", opt->name);
            _printIndented(stream, opt->help,  indent);
        }
    }

    // Return number of values encountered
    inline int parseOptionValues(const char** argv, const int argc, Value* value) {
        if (!value || argc <= 0 || (argc > 0 && argv[0][0] == OPTION_SYMBOL)) {
            return 0;
        }

        value->next = 0; // TODO: need to think, how and who will provide memory
        value->data = argv[0];
        return 1;
    }

    // Returns position (index) in 'argv' that wasn't formatted as option.
    // If all 'argv' list is contains valid options, returns 'argc'.
    // All unknown options are silently skipped.
    inline int parseOptions(Option* optv, const int optc, const char** argv, const int argc) {
        for (int i = 0; i < argc; i++) {
            const char* arg = argv[i];

            if (arg[0] != OPTION_SYMBOL || arg[1] == '\0') {
                return i;
            }

            const char* flag = arg + 1;
            if (flag[0] == OPTION_SYMBOL) flag++;

            Option* matched = NULL;
            for (int i = 0; i < optc; i++) {
                Option* opt = optv + i;
                if ((opt->tag && strcmp(flag, opt->tag) == 0) ||
                    (opt->name && strcmp(flag, opt->name) == 0)) {
                    matched = opt;
                    break;
                }
            }

            if (matched) {
                matched->encountered = true;
                const int n = parseOptionValues(argv + i + 1, argc - i - 1, &matched->value);
                if (n > 0) matched->hasValue = true;
                i += n;
            } else {
                // TODO: we may want to collect these flags, so user can print them
                const int n = parseOptionValues(argv + i + 1, argc - i - 1, 0);
                i += n;
            }
        }

        return argc;
    }

}
