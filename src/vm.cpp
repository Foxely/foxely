#include <stdio.h>
#include <stdarg.h>
#include <chrono>
#include "common.h"
#include "debug.h"
#include "Parser.h"
#include "vm.hpp"
#include "scy/pluga/pluga.h"
#include "scy/pluga/sharedlibrary.h"
#include "scy/pluga/plugin_api.h"

// Value Fox_StringToValue(const char* str)
// {
// 	return OBJ_VAL(TakeString(str));
// }

void VM::Load()
{
	        // Set the plugin shared library location
        std::string path;
        path += "./modules/OS/";
#if WIN32
#ifdef _DEBUG
        path += "plugatestplugind.dll";
#else
        path += "plugatestplugin.dll";
#endif
#else
#ifdef _DEBUG
        path += "plugatestplugind.so";
#else
        path += "plugatestplugin.so";
#endif
#endif

        try {
            // Load the shared library
            std::cout << "Loading: " << path << std::endl;
            fox::SharedLibrary lib;
            lib.open(path);

            // Get plugin descriptor and exports
            fox::pluga::PluginDetails* info;
            lib.sym("exports", reinterpret_cast<void**>(&info));
            std::cout << "Plugin Info: "
                 << "\n\tAPI Version: " << info->apiVersion
                 << "\n\tFile Name: " << info->fileName
                 << "\n\tClass Name: " << info->className
                 << "\n\tPlugin Name: " << info->pluginName
                 << "\n\tPlugin Version: " << info->pluginVersion
				 << std::endl;

            // API version checking
            // if (info->apiVersion != SCY_PLUGIN_API_VERSION)
            //     throw std::runtime_error(util::format(
            //         "Plugin version mismatch. Expected %s, got %s.",
            //         SCY_PLUGIN_API_VERSION, info->apiVersion));

            // Instantiate the plugin
            auto plugin = reinterpret_cast<fox::pluga::IPlugin*>(info->initializeFunc());
			NativeMethods methods = plugin->GetMethods();
			DefineNativeClass(plugin->GetClassName(), methods);

//             // Call string accessor methods
//             plugin->setValue("abracadabra");
//             expect(strcmp(plugin->cValue(), "abracadabra") == 0);
// #if PLUGA_ENABLE_STL
//             expect(plugin->sValue() == "abracadabra");
// #endif

//             // Call command methods
//             expect(plugin->onCommand("options:set", "rendomdata", 10));
//             expect(plugin->lastError() == nullptr);
//             expect(plugin->onCommand("unknown:command", "rendomdata", 10) == false);
//             expect(strcmp(plugin->lastError(), "Unknown command") == 0);

//             // Call a C function
//             GimmeFiveFunc gimmeFive;
//             lib.sym("gimmeFive", reinterpret_cast<void**>(&gimmeFive));
//             expect(gimmeFive() == 5);

            // Close the plugin and free memory
			m_vLibraryImported.push_back(lib);
        } catch (std::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            // expect(false);
        }

        std::cout << "Ending" << std::endl;
}

VM::VM()
{
    std::cout << "VM" << std::endl;
	m_oParser.m_pVm = this;
	openUpvalues = NULL;
	stack[0] = Value();
    ResetStack();
	// DefineNative("clock", clockNative);

	// NativeMethods methods =
	// {
	// 	std::make_pair<std::string, Native>("which", std::make_pair<NativeFn, int>(whichNative, 0)),
	// 	std::make_pair<std::string, Native>("shell", std::make_pair<NativeFn, int>(shellNative, 1)),
	// 	std::make_pair<std::string, Native>("getenv", std::make_pair<NativeFn, int>(getEnvNative, 1)),
	// };
	// DefineNativeClass("os", methods);

	initString = NULL;
}

VM::~VM()
{
	initString = NULL;

	for (auto& lib : m_vLibraryImported)
	{
		lib.close();
	}
	
}

VM* VM::GetInstance()
{
	static VM m_oInstance;
	return &m_oInstance;
}

void VM::ResetStack()
{
    stackTop = stack;
	frameCount = 0;
}

void VM::Push(Value value)
{
    *stackTop = value;
    stackTop++;
}

Value VM::Pop()
{
    stackTop--;
    return *stackTop;
}

Value VM::Peek(int distance)
{
    return stackTop[-1 - distance];
}

bool IsFalsey(Value value)
{
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void VM::RuntimeError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &frames[i];
		ObjectFunction* function = frame->closure->function;
		// -1 because the IP is sitting on the next instruction to be
		// executed.
		size_t instruction = frame->ip - function->chunk.m_vCode.begin() - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.m_vLines[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			fprintf(stderr, "%s()\n", function->name->string.c_str());
		}
	}

    ResetStack();
}

bool VM::Call(ObjectClosure* closure, int argCount)
{
	// Check the number of args pass to the function call
	if (argCount != closure->function->arity) {
		RuntimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
		return false;
	}

	if (frameCount == FRAMES_MAX) {
		RuntimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &frames[frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.m_vCode.begin();

	frame->slots = stackTop - argCount - 1;
	return true;
}

bool VM::CallValue(Value callee, int argCount)
{
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
		// case OBJ_FUNCTION:
		// 	return Call(AS_FUNCTION(callee), argCount);
		case OBJ_NATIVE:
		{
			ObjectNative* objNative = (ObjectNative *) AS_OBJ(callee);
			if (objNative->arity != argCount)
			{
				RuntimeError("Expected %d arguments but got %d.", objNative->arity, argCount);
				return false;
			}
			NativeFn native = AS_NATIVE(callee);
			Value result = native(argCount, stackTop - argCount);
			stackTop -= argCount + 1;
			Push(result);
			return true;
		}
		case OBJ_BOUND_METHOD:
		{
			ObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
			stackTop[-argCount - 1] = bound->receiver;
			return Call(bound->method, argCount);
		}
		case OBJ_CLASS:
		{
			ObjectClass* klass = AS_CLASS(callee);
			stackTop[-argCount - 1] = OBJ_VAL(new ObjectInstance(klass));
			Value initializer;
			if (klass->methods.Get(initString, initializer)) {
				return Call(AS_CLOSURE(initializer), argCount);
			} else if (argCount != 0) {
				RuntimeError("Expected 0 arguments but got %d.", argCount);
				return false;
			}
			return true;
		}
		case OBJ_CLOSURE:
        	return Call(AS_CLOSURE(callee), argCount);
		default:
			// Non-callable object type.
			break;
		}
	}

	RuntimeError("Can only call functions and classes.");
	return false;
}

ObjectUpvalue* VM::CaptureUpvalue(Value* local)
{
	ObjectUpvalue* prevUpvalue = NULL;
	ObjectUpvalue* upvalue = openUpvalues;

	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local) return upvalue;
	ObjectUpvalue* createdUpvalue = new ObjectUpvalue(local);
	if (prevUpvalue == NULL) {
    	openUpvalues = createdUpvalue;
	} else {
	  	prevUpvalue->next = createdUpvalue;
	}
	return createdUpvalue;
}

void VM::CloseUpvalues(Value* last)
{
	while (openUpvalues != NULL && openUpvalues->location >= last)
	{
	    ObjectUpvalue* upvalue = openUpvalues;
	    upvalue->closed = *upvalue->location;
	    upvalue->location = &upvalue->closed;
	    openUpvalues = upvalue->next;
	}
}

void VM::DefineNative(const std::string& name, NativeFn function)
{
	Push(OBJ_VAL(CopyString(name)));
	Push(OBJ_VAL(new ObjectNative(function)));
	globals.Set(AS_STRING(stack[0]), stack[1]);
	Pop();
	Pop();
}

void VM::DefineNativeClass(const std::string& name, NativeMethods& functions)
{
	Push(OBJ_VAL(CopyString(name)));
	Push(OBJ_VAL(new ObjectNativeClass(AS_STRING(stack[0]))));
	globals.Set(AS_STRING(stack[0]), stack[1]);
	ObjectNativeClass* klass = AS_NATIVE_CLASS(Pop());
	Pop();
	Push(OBJ_VAL(klass));
	for (auto& it : functions)
	{
		NativeFn func = it.second.first;
		int arity = it.second.second;

		Push(OBJ_VAL(CopyString(it.first)));
		Push(OBJ_VAL(new ObjectNative(func, arity)));

		klass->methods.Set(AS_STRING(stack[1]), stack[2]);

		Pop();
		Pop();
	}
	Pop();
}

bool VM::InvokeFromClass(ObjectClass* klass, ObjectString* name, int argCount)
{
	Value method;
	if (!klass->methods.Get(name, method)) {
		RuntimeError("Undefined property '%s'.", name->string.c_str());
		return false;
	}

	return Call(AS_CLOSURE(method), argCount);
}

bool VM::Invoke(ObjectString* name, int argCount)
{
	Value receiver = Peek(argCount);

	if (!IS_INSTANCE(receiver) && !IS_NATIVE_CLASS(receiver)) {
		RuntimeError("Only instances && module have methods.");
		return false;
	}

	Value value;
	if (IS_INSTANCE(receiver))
	{
		ObjectInstance* instance = AS_INSTANCE(receiver);

		if (instance->fields.Get(name, value)) {
			stackTop[-argCount - 1] = value;
			return CallValue(value, argCount);
		}
		return InvokeFromClass(instance->klass, name, argCount);
	}
	else if (IS_NATIVE_CLASS(receiver))
	{
		ObjectNativeClass* instance = AS_NATIVE_CLASS(receiver);
		Value method;
		if (!instance->methods.Get(name, method)) {
			RuntimeError("Undefined property '%s'.", name->string.c_str());
			return false;
		}
		return CallValue(method, argCount);
	}
}

InterpretResult VM::interpret(const char* source)
{
	initString = CopyString("init");
	Load();

    Chunk oChunk;
    ObjectFunction *function = Compile(m_oParser, source, &oChunk);

    if (function == NULL)
        return (INTERPRET_COMPILE_ERROR);

    Push(OBJ_VAL(function));
	ObjectClosure* closure = new ObjectClosure(function);
    Pop();
    Push(OBJ_VAL(closure));
    CallValue(OBJ_VAL(closure), 0);
	InterpretResult result = run();

    return result;
}

InterpretResult VM::run()
{
	CallFrame* frame = &frames[frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.m_oConstants.m_vValues[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(Peek(0)) || !IS_NUMBER(Peek(1))) { \
        RuntimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(Pop()); \
      double a = AS_NUMBER(Pop()); \
      Push(valueType(a op b)); \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = stack; slot < stackTop; slot++)
        {
            printf("[ ");
            PrintValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.m_vCode.begin()));
#endif
// auto startExec = std::chrono::steady_clock::now();
        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
            case OP_NIL:    Push(NIL_VAL); break;
            case OP_TRUE:   Push(BOOL_VAL(true)); break;
            case OP_FALSE:  Push(BOOL_VAL(false)); break;
			case OP_POP: Pop(); break;

			case OP_GET_UPVALUE:
			{
		        uint8_t slot = READ_BYTE();
		        Push(*frame->closure->upValues[slot]->location);
		        break;
		    }

			case OP_SET_UPVALUE:
			{
		        uint8_t slot = READ_BYTE();
		        *frame->closure->upValues[slot]->location = Peek(0);
		        break;
		    }

			case OP_GET_LOCAL:
			{
				uint8_t slot = READ_BYTE();
				Push(frame->slots[slot]);
				break;
			}

			case OP_SET_LOCAL:
			{
				uint8_t slot = READ_BYTE();
				frame->slots[slot] = Peek(0);
				break;
			}

			case OP_GET_GLOBAL: {
				ObjectString* name = READ_STRING();
				Value value;
				if (!globals.Get(name, value)) {
					RuntimeError("Undefined variable '%s'.", name->string.c_str());
					return INTERPRET_RUNTIME_ERROR;
				}
				Push(value);
				break;
			}

			case OP_DEFINE_GLOBAL: {
				ObjectString* name = READ_STRING();
				globals.Set(name, Peek(0));
				Pop();
				break;
			}

			case OP_SET_GLOBAL: {
				ObjectString* name = READ_STRING();
				if (globals.Set(name, Peek(0))) {
					globals.Delete(name);
					RuntimeError("Undefined variable '%s'.", name->string.c_str());
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_GET_PROPERTY:
			{
				if (!IS_INSTANCE(Peek(0)))
				{
					RuntimeError("Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectInstance* instance = AS_INSTANCE(Peek(0));
				ObjectString* name = READ_STRING();

				Value value;
				if (instance->fields.Get(name, value)) {
					Pop(); // Instance.
					Push(value);
					break;
				}
				
				if (!BindMethod(instance->klass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SET_PROPERTY:
			{
				if (!IS_INSTANCE(Peek(1))) {
					RuntimeError("Only instances have fields.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjectInstance* instance = AS_INSTANCE(Peek(1));
				instance->fields.Set(READ_STRING(), Peek(0));

				Value value = Pop();
				Pop();
				Push(value);
				break;
			}

            case OP_EQUAL: {
                Value b = Pop();
                Value a = Pop();
                Push(BOOL_VAL(ValuesEqual(a, b)));
                break;
            }

            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD:
            {
                if (IS_STRING(Peek(0)) && IS_STRING(Peek(1))) {
                    Concatenate();
                } else if (IS_NUMBER(Peek(0)) && IS_NUMBER(Peek(1))) {
                    double b = AS_NUMBER(Pop());
                    double a = AS_NUMBER(Pop());
                    Push(NUMBER_VAL(a + b));
                } else {
                    RuntimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB:    BINARY_OP(NUMBER_VAL, -); break;
            case OP_MUL:    BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIV:    BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                Push(BOOL_VAL(IsFalsey(Pop())));
            break;
            case OP_NEGATE:
            {
                if (!IS_NUMBER(Peek(0))) {
                    RuntimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Push(NUMBER_VAL(-AS_NUMBER(Pop())));
                break;
            }
			case OP_PRINT:
			{
				PrintValue(Pop());
				std::cout << std::endl;
				break;
			}

			case OP_JUMP:
			{
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}

			case OP_JUMP_IF_FALSE:
			{
				uint16_t offset = READ_SHORT();
				if (IsFalsey(Peek(0))) frame->ip += offset;
				break;
			}

			case OP_LOOP:
			{
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}

			case OP_CALL:
			{
				int argCount = READ_BYTE();
				if (!CallValue(Peek(argCount), argCount))
					return INTERPRET_RUNTIME_ERROR;
				frame = &frames[frameCount - 1];
				break;
			}

			case OP_CLASS:
				Push(OBJ_VAL(new ObjectClass(READ_STRING())));
				break;

			case OP_INHERIT:
			{
				Value superclass = Peek(1);
				
				if (!IS_CLASS(superclass)) {
					RuntimeError("Superclass must be a class.");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjectClass* subclass = AS_CLASS(Peek(0));
				subclass->methods.AddAll(AS_CLASS(superclass)->methods);
				Pop(); // Subclass.
				break;
			}
			
			case OP_METHOD:
				DefineMethod(READ_STRING());
				break;

			case OP_INVOKE:
			{
				ObjectString* method = READ_STRING();
				int argCount = READ_BYTE();
				if (!Invoke(method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &frames[frameCount - 1];
				break;
			}

			case OP_SUPER_INVOKE:
			{
				ObjectString* method = READ_STRING();
				int argCount = READ_BYTE();
				ObjectClass* superclass = AS_CLASS(Pop());
				if (!InvokeFromClass(superclass, method, argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &frames[frameCount - 1];
				break;
			}

			case OP_CLOSURE:
			{
		        ObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
		        ObjectClosure* closure = new ObjectClosure(function);
		        Push(OBJ_VAL(closure));

				for (int i = 0; i < closure->upvalueCount; i++) {
		          uint8_t isLocal = READ_BYTE();
		          uint8_t index = READ_BYTE();
		          if (isLocal) {
		            closure->upValues[i] = CaptureUpvalue(frame->slots + index);
		          } else {
		            closure->upValues[i] = frame->closure->upValues[index];
		          }
		        }
		        break;
	      	}

			case OP_CLOSE_UPVALUE:
		        CloseUpvalues(stackTop - 1);
		        Pop();
		        break;
			
			case OP_GET_SUPER:
			{
				ObjectString* name = READ_STRING();
				ObjectClass* superclass = AS_CLASS(Pop());
				if (!BindMethod(superclass, name)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

            case OP_RETURN:
			{
				Value result = Pop();
				CloseUpvalues(frame->slots);
				frameCount--;
				if (frameCount == 0) {
					Pop();
					return INTERPRET_OK;
				}

				stackTop = frame->slots;
				Push(result);

				frame = &frames[frameCount - 1];
				break;
			}

            case OP_CONST:
            {
                Value constant = READ_CONSTANT();
                Push(constant);
                break;
            }
        }
				// auto endExec = std::chrono::steady_clock::now();
				// disassembleInstruction(frame->function->chunk, (int)(frame->ip - frame->function->chunk.m_vCode.begin()));
				// std::cout << "Instruction Time: " << std::chrono::duration <double> (endExec - startExec).count() << " s" << std::endl;
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

void VM::Concatenate()
{
    ObjectString* b = AS_STRING(Peek(0));
    ObjectString* a = AS_STRING(Peek(1));
	Pop();
	Pop();

    ObjectString* result = TakeString(a->string + b->string);
    Push(OBJ_VAL(result));
}

void VM::AddToRoots()
{
	for (Value* slot = stack; slot < stackTop; slot++)
	{
	    AddValueToRoot(*slot);
	}

	for (int i = 0; i < frameCount; i++)
	{
    	AddObjectToRoot((Object *)frames[i].closure);
  	}

	for (ObjectUpvalue* upvalue = openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
		AddObjectToRoot((Object*)upvalue);
	}

	AddTableToRoot(globals);
	AddCompilerToRoots();
	AddObjectToRoot((Object *)initString);
}

void VM::AddTableToRoot(Table& table)
{
	for (int i = 0; i < table.m_iCapacity; i++) {
		Entry& entry = table.m_vEntries[i];
		AddObjectToRoot((Object *)entry.m_pKey);
		AddValueToRoot(entry.m_oValue);
	}
}

void VM::AddValueToRoot(Value value)
{
	if (!IS_OBJ(value))
		return;
	AddObjectToRoot(AS_OBJ(value));
}

void VM::AddObjectToRoot(Object* object)
{
	if (object == NULL)
		return;
#ifdef DEBUG_LOG_GC
	  printf("%p added to root ", (void*)object);
	  PrintValue(OBJ_VAL(object));
	  printf("\n");
#endif
	GC::Gc.AddRoot(object);

	BlackenObject(object);
	strings.RemoveWhite();
}

void VM::AddCompilerToRoots()
{
	Compiler* compiler = m_oParser.currentCompiler;
	while (compiler != NULL)
	{
		AddObjectToRoot((Object *)compiler->function);
		compiler = compiler->enclosing;
	}
}

void VM::AddArrayToRoot(ValueArray* array)
{
	for (int i = 0; i < array->m_vValues.size(); i++) {
		AddValueToRoot(array->m_vValues[i]);
	}
}

void VM::BlackenObject(Object* object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void*)object);
	PrintValue(OBJ_VAL(object));
	printf("\n");
#endif
	switch (object->type)
	{
		case OBJ_INSTANCE:
		{
			ObjectInstance* instance = (ObjectInstance *)object;
			AddObjectToRoot((Object *)instance->klass);
			AddTableToRoot(instance->fields);
		break;
		}
		case OBJ_BOUND_METHOD: {
			ObjectBoundMethod* bound = (ObjectBoundMethod*)object;
			AddValueToRoot(bound->receiver);
			AddObjectToRoot((Object*)bound->method);
			break;
		}
		case OBJ_CLASS:
		{
			ObjectClass* klass = (ObjectClass*)object;
			AddObjectToRoot((Object *)klass->name);
			AddTableToRoot(klass->methods);
			break;
		}
		case OBJ_CLOSURE:
		{
			ObjectClosure* closure = (ObjectClosure*)object;
			AddObjectToRoot((Object *)closure->function);
			for (int i = 0; i < closure->upvalueCount; i++) {
				AddObjectToRoot((Object *)closure->upValues[i]);
			}
			break;
		}
		case OBJ_FUNCTION:
		{
			ObjectFunction* function = (ObjectFunction *)object;
			AddObjectToRoot((Object *)function->name);
			AddArrayToRoot(&function->chunk.m_oConstants);
			break;
	    }
		case OBJ_UPVALUE:
			AddValueToRoot(((ObjectUpvalue *)object)->closed);
			break;
		case OBJ_NATIVE:
		case OBJ_STRING:
			break;
	}
}

void VM::DefineMethod(ObjectString* name)
{
	Value method = Peek(0);
	ObjectClass* klass = AS_CLASS(Peek(1));
	klass->methods.Set(name, method);
	Pop();
}

bool VM::BindMethod(ObjectClass* klass, ObjectString* name)
{
	Value method;
	if (!klass->methods.Get(name, method)) {
		RuntimeError("Undefined property '%s'.", name->string.c_str());
		return false;
	}

	ObjectBoundMethod* bound = new ObjectBoundMethod(Peek(0), AS_CLOSURE(method));
	Pop();
	Push(OBJ_VAL(bound));
	return true;
}