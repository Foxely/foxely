#include <iostream>
#include <stdexcept>
#include <string.h>
#include <fstream>
#include <streambuf>

#include "foxely.h"
#include "library/library.h"

Value importNative(VM* oVM, int argCount, Value* args)
{
    Fox_FixArity(oVM, argCount, 1);
    // if (!oVM->imports.Set(AS_STRING(args[0]), NIL_VAL))
    //     return NIL_VAL;
    std::ifstream t(Fox_ValueToCString(args[0]));
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    t.close();

    Chunk oChunk;
	ObjectFunction *function = Compile(oVM->m_oParser, str.c_str(), &oChunk);
    if (function == NULL)
        return NIL_VAL;
    oVM->Push(OBJ_VAL(function));
    ObjectClosure *closure = oVM->gc.New<ObjectClosure>(oVM, function);
    oVM->Pop();
    oVM->CallFunction(closure, 0);
    CallFrame *frame = &oVM->frames[oVM->frameCount - 1];
    frame = &oVM->frames[oVM->frameCount - 1];
    return NIL_VAL;
}

void DefineModuleModule(VM* oVM)
{
    NativeMethods methods =
	{
		std::make_pair<std::string, NativeFn>("load", importNative),
	};
    oVM->DefineModule("module");
    oVM->DefineClass("module", "module", methods);
}