#pragma once

#include "file_system.h"
#include "array_list.h"



namespace Interpreter { struct ExeBlock; }
namespace Extern { struct Library; }

struct AstContext;
struct AstRegistry;

namespace Reg {

    struct Unit {
        AstContext*  ast;
        AstRegistry* reg;

        Interpreter::ExeBlock* exe;

        // contains Extern::Library*
        DArray::Container* libs;
    };

    Unit* get(FileSystem::Handle file);
    void invalidate(FileSystem::Handle file);

    void addLibrary(Unit* unit, Extern::Library* lib);

}
