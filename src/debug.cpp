#include <stdio.h>

#include "debug.h"
#include "value.hpp"
#include "object.hpp"

static int invokeInstruction(const char *name, Chunk& chunk, int offset) {
    uint8_t constant = chunk.m_vCode[offset + 1];
    uint8_t argCount = chunk.m_vCode[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
   	PrintValue(chunk.m_oConstants.m_vValues[constant]);
    printf("'\n");
    return offset + 3;
}

static int jumpInstruction(const char *name, int sign, Chunk& chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk.m_vCode[offset + 1] << 8);
    jump |= chunk.m_vCode[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int byteInstruction(const char *name, Chunk& chunk, int offset) {
    uint8_t slot = chunk.m_vCode[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

// Print a Simple (1 byte) Instruction like 'OP_RETURN'
static int simpleInstruction(const char* name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

// Print a Constant Instruction (2 bytes)
static int constantInstruction(const char* name, Chunk& chunk, int offset)
{
	uint8_t constant = chunk.m_vCode[offset + 1];
	printf("%-16s %4d '", name, constant);
	PrintValue(chunk.m_oConstants.m_vValues[constant]);
	printf("'\n");
	return offset + 2;
}

void disassembleChunk(Chunk& chunk, const char* name)
{
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk.m_vCode.size();) {
        offset = disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(Chunk& chunk, int offset)
{
    printf("%04d ", offset);

    if (offset > 0 && chunk.m_vLines[offset] == chunk.m_vLines[offset - 1])
        printf("   | ");
    else
        printf("%4d ", chunk.m_vLines[offset]);

    uint8_t instruction = chunk.m_vCode[offset];
    switch (instruction)
	{
		case OP_CONST:
			return constantInstruction("OP_CONST", chunk, offset);
		case OP_NIL:
			return simpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);

		case OP_ARRAY:
			return constantInstruction("OP_ARRAY", chunk, offset);
		case OP_ADD_LIST:
			return constantInstruction("OP_ADD_LIST", chunk, offset);
		case OP_SLICE:
			return constantInstruction("OP_SLICE", chunk, offset);
		case OP_MAP:
			return constantInstruction("OP_MAP", chunk, offset);
		case OP_ADD_MAP:
			return constantInstruction("OP_ADD_MAP", chunk, offset);
		
		case OP_SUBSCRIPT:
			return constantInstruction("OP_SUBSCRIPT", chunk, offset);
		case OP_SUBSCRIPT_ASSIGN:
			return simpleInstruction("OP_SUBSCRIPT_ASSIGN", offset);

		case OP_POP:
			return simpleInstruction("OP_POP", offset);

		case OP_GET_LOCAL:
			return byteInstruction("OP_GET_LOCAL", chunk, offset);
		case OP_SET_LOCAL:
			return byteInstruction("OP_SET_LOCAL", chunk, offset);

		case OP_GET_GLOBAL:
			return constantInstruction("OP_GET_GLOBAL", chunk, offset);
		case OP_DEFINE_GLOBAL:
			return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			return constantInstruction("OP_SET_GLOBAL", chunk, offset);

		case OP_GET_UPVALUE:
			return byteInstruction("OP_GET_UPVALUE", chunk, offset);
		case OP_SET_UPVALUE:
			return byteInstruction("OP_SET_UPVALUE", chunk, offset);

		case OP_GET_PROPERTY:
			return constantInstruction("OP_GET_PROPERTY", chunk, offset);
		case OP_SET_PROPERTY:
			return constantInstruction("OP_SET_PROPERTY", chunk, offset);

		case OP_GET_SUPER:
			return constantInstruction("OP_GET_SUPER", chunk, offset);

		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);

		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUB:
			return simpleInstruction("OP_SUB", offset);
		case OP_MUL:
			return simpleInstruction("OP_MUL", offset);
		case OP_DIV:
			return simpleInstruction("OP_DIV", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);

		case OP_PRINT:
			return simpleInstruction("OP_PRINT", offset);
		case OP_PRINT_REPL:
			return simpleInstruction("OP_PRINT_REPL", offset);

		case OP_JUMP:
			return jumpInstruction("OP_JUMP", 1, chunk, offset);
		case OP_JUMP_IF_FALSE:
			return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
		case OP_LOOP:
			return jumpInstruction("OP_LOOP", -1, chunk, offset);
		case OP_CALL:
			return byteInstruction("OP_CALL", chunk, offset);
		case OP_INVOKE:
			return invokeInstruction("OP_INVOKE", chunk, offset);
		case OP_SUPER_INVOKE:
			return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
		case OP_CLOSURE:
        {
			offset++;
			uint8_t constant = chunk.m_vCode[offset++];
			printf("%-16s %4d ", "OP_CLOSURE", constant);
			PrintValue(chunk.m_oConstants.m_vValues[constant]);
			printf("\n");
			ObjectFunction* function = Fox_AsFunction(chunk.m_oConstants.m_vValues[constant]);
			for (int j = 0; j < function->upValueCount; j++)
            {
				int is_local = chunk.m_vCode[offset++];
				int index = chunk.m_vCode[offset++];
				printf("%04d      |                     %s %d\n",
					offset - 2, is_local ? "local" : "upvalue", index);
			}
			return offset;
		}
		case OP_CLOSE_UPVALUE:
			return simpleInstruction("OP_CLOSE_UPVALUE", offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		case OP_CLASS:
			return constantInstruction("OP_CLASS", chunk, offset);
		case OP_INHERIT:
			return simpleInstruction("OP_INHERIT", offset);
		case OP_METHOD:
			return constantInstruction("OP_METHOD", chunk, offset);
		case OP_OPERATOR:
			return constantInstruction("OP_OPERATOR", chunk, offset);
		case OP_IMPORT:
			return constantInstruction("OP_IMPORT", chunk, offset);
		case OP_END_MODULE:
			return simpleInstruction("OP_END_MODULE", offset);
		case OP_END:
			return simpleInstruction("OP_END", offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
    }
}
