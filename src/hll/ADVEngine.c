/* Copyright (C) 2026 Rance03 compatibility contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

/*
 * ADVEngine
 * ---------
 * AliceSoft library used by Rance03 to introspect the game own VM code
 * (the in-game CADVEngine class implements a VM function browser/stepper
 * on top of it).
 *
 * The functions are implemented on top of the real AIN data, using the
 * libsys4 disassembler. Functions that would require runtime stack access
 * (GetArgumentValue_*) or delegate creation (CreateDelegateObjectID) are
 * stubbed out with reasonable return values.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system4/ain.h"
#include "system4/dasm.h"
#include "system4/instructions.h"
#include "system4/string.h"

#include "xsystem4.h"
#include "hll.h"

static int loaded_fno = -1;
static uint32_t *step_addrs = NULL;
static int nr_steps = 0;
static uint32_t *if_addrs = NULL;
static int nr_ifs = 0;

static void ADVEngine_clear_state(void)
{
	loaded_fno = -1;
	free(step_addrs);
	step_addrs = NULL;
	nr_steps = 0;
	free(if_addrs);
	if_addrs = NULL;
	nr_ifs = 0;
}

static int ADVEngine_resolve(struct string *name)
{
	if (!name)
		return -1;
	return ain_get_function(ain, name->text);
}

// Analyse a function: collect the address of every instruction ("step"),
// and the address of every IF instruction ("if").
static bool ADVEngine_analyse(int fno)
{
	if (fno < 0 || fno >= ain->nr_functions)
		return false;
	if (fno == loaded_fno)
		return true;

	ADVEngine_clear_state();

	// find the end of the function = address of the next function
	uint32_t start = ain->functions[fno].address;
	uint32_t end = ain->code_size;
	for (int i = 0; i < ain->nr_functions; i++) {
		if (i == fno)
			continue;
		int32_t a = ain->functions[i].address;
		if (a >= (int32_t)start && (uint32_t)a < end)
			end = a;
	}

	// collect step addresses
	struct dasm d;
	dasm_init(&d, ain);
	dasm_jump(&d, start);
	int cap = 0;
	while (!dasm_eof(&d) && dasm_addr(&d) < end) {
		if (nr_steps >= cap) {
			cap = cap ? cap*2 : 64;
			step_addrs = xrealloc(step_addrs, cap * sizeof(uint32_t));
		}
		uint32_t addr = dasm_addr(&d);
		int op = dasm_opcode(&d);
		if (op == IFZ || op == IFNZ) {
			if_addrs = xrealloc(if_addrs, (nr_ifs+1) * sizeof(uint32_t));
			if_addrs[nr_ifs++] = addr;
		}
		step_addrs[nr_steps++] = addr;
		dasm_next(&d);
	}
	dasm_close(&d);
	loaded_fno = fno;
	return true;
}

static bool ADVEngine_analyse_by_name(struct string *name)
{
	return ADVEngine_analyse(ADVEngine_resolve(name));
}

static bool ADVEngine_valid_step(int step)
{
	return ADVEngine_analyse(loaded_fno) && step >= 0 && step < nr_steps;
}

// The opcode of the instruction at the given step.
static int ADVEngine_opcode_at(int step)
{
	if (!ADVEngine_valid_step(step))
		return -1;
	struct dasm d;
	dasm_init(&d, ain);
	dasm_jump(&d, step_addrs[step]);
	int op = dasm_opcode(&d);
	dasm_close(&d);
	return op;
}

static bool ADVEngine_Load(struct string *FunctionName)
{
	int fno = ADVEngine_resolve(FunctionName);
	if (fno < 0) {
		WARNING("ADVEngine.Load: no such function: %s", FunctionName ? display_sjis0(FunctionName->text) : "(null)");
		return false;
	}
	return ADVEngine_analyse(fno);
}

static int ADVEngine_Release(void)
{
	ADVEngine_clear_state();
	return 0;
}

static int ADVEngine_NumofFunctionName(void)
{
	return ain->nr_functions;
}

static int ADVEngine_GetFunctionName(struct string **FunctionName, int Index)
{
	if (Index < 0 || Index >= ain->nr_functions)
		return 0;
	if (*FunctionName)
		free_string(*FunctionName);
	*FunctionName = cstr_to_string(ain->functions[Index].name);
	return 1;
}

static bool ADVEngine_AnalyseIf(struct string *FunctionName)
{
	return ADVEngine_analyse_by_name(FunctionName);
}

static int ADVEngine_NumofIf(struct string *FunctionName)
{
	if (!ADVEngine_analyse_by_name(FunctionName))
		return 0;
	return nr_ifs;
}

static int ADVEngine_GetIf(struct string **If, struct string *FunctionName, int Index)
{
	if (!ADVEngine_analyse_by_name(FunctionName) || Index < 0 || Index >= nr_ifs)
		return 0;
	if (*If)
		free_string(*If);
	char buf[64];
	snprintf(buf, sizeof(buf), "IF (0x%08x)", if_addrs[Index]);
	*If = cstr_to_string(buf);
	return 1;
}

static bool ADVEngine_AnalyseFunction(struct string *FunctionName, struct string *IfData)
{
	// XXX: the IfData argument (selected branch condition) is not used; we
	//      always analyse the whole function.
	return ADVEngine_analyse_by_name(FunctionName);
}

static int ADVEngine_NumofStep(struct string *FunctionName)
{
	if (!ADVEngine_analyse_by_name(FunctionName))
		return 0;
	return nr_steps;
}

static int ADVEngine_GetStepText(struct string **Text, struct string *FunctionName, int Step)
{
	if (!ADVEngine_analyse_by_name(FunctionName) || Step < 0 || Step >= nr_steps)
		return 0;
	if (*Text)
		free_string(*Text);
	char buf[128];
	int op = ADVEngine_opcode_at(Step);
	if (op >= 0 && op < NR_OPCODES)
		snprintf(buf, sizeof(buf), "%04d: %s", Step, instructions[op].name);
	else
		snprintf(buf, sizeof(buf), "%04d: (opcode %d)", Step, op);
	*Text = cstr_to_string(buf);
	return 1;
}

static int ADVEngine_OpenFunctionFile(struct string *FunctionName, int Step)
{
	// The original library reads the function source from a .fnf file.
	// Compiled games don ship function files.
	WARNING("ADVEngine.OpenFunctionFile: function files not supported (%s, step %d)",
			FunctionName ? display_sjis0(FunctionName->text) : "(null)", Step);
	return 0;
}

static int ADVEngine_GetCodeType(struct string *FunctionName, int Step)
{
	// The code type is the VM opcode of the instruction at the given step.
	if (!ADVEngine_analyse_by_name(FunctionName) || Step < 0 || Step >= nr_steps)
		return -1;
	return ADVEngine_opcode_at(Step);
}

static int ADVEngine_GetMessage(struct string **Message, struct string *FunctionName, int Step)
{
	if (!ADVEngine_valid_step(Step))
		return 0;
	struct dasm d;
	dasm_init(&d, ain);
	dasm_jump(&d, step_addrs[Step]);
	int op = dasm_opcode(&d);
	int32_t msg = (op == _MSG && dasm_nr_args(&d) > 0) ? dasm_arg(&d, 0) : -1;
	dasm_close(&d);
	if (msg < 0 || msg >= ain->nr_messages || !ain->messages[msg])
		return 0;
	if (*Message)
		free_string(*Message);
	*Message = string_dup(ain->messages[msg]);
	return 1;
}

static bool ADVEngine_BindFunction(struct string *FunctionName, int Step)
{
	if (!ADVEngine_valid_step(Step))
		return false;
	int op = ADVEngine_opcode_at(Step);
	return op == CALLFUNC || op == CALLMETHOD || op == CALLFUNC2;
}

static int ADVEngine_GetFuncName(struct string **FuncName, struct string *FunctionName, int Step)
{
	if (!ADVEngine_valid_step(Step))
		return 0;
	struct dasm d;
	dasm_init(&d, ain);
	dasm_jump(&d, step_addrs[Step]);
	int op = dasm_opcode(&d);
	int fno = -1;
	if ((op == CALLFUNC || op == CALLMETHOD) && dasm_nr_args(&d) > 0)
		fno = dasm_arg(&d, 0);
	dasm_close(&d);
	if (fno < 0 || fno >= ain->nr_functions) {
		WARNING("ADVEngine.GetFuncName: step %d of %s is not a function call",
				Step, display_sjis0(FunctionName->text));
		return 0;
	}
	if (*FuncName)
		free_string(*FuncName);
	*FuncName = cstr_to_string(ain->functions[fno].name);
	return 1;
}

static int ADVEngine_CreateDelegateObjectID(struct string *FunctionName, int Step)
{
	// XXX: creating a delegate for an arbitrary VM function requires access
	//      to the function type (FNCT) tables; not implemented.
	WARNING("ADVEngine.CreateDelegateObjectID: not implemented (step %d)", Step);
	return -1;
}

static bool ADVEngine_GetArgumentValue_int(int *Value, struct string *FunctionName, int Step, int Index)
{
	// XXX: argument values live on the VM stack at runtime; static
	//      extraction is not possible.
	WARNING("ADVEngine.GetArgumentValue_int: not implemented");
	return false;
}

static bool ADVEngine_GetArgumentValue_float(float *Value, struct string *FunctionName, int Step, int Index)
{
	WARNING("ADVEngine.GetArgumentValue_float: not implemented");
	return false;
}

static bool ADVEngine_GetArgumentValue_bool(int *Value, struct string *FunctionName, int Step, int Index)
{
	WARNING("ADVEngine.GetArgumentValue_bool: not implemented");
	return false;
}

static bool ADVEngine_GetArgumentValue_string(struct string **Value, struct string *FunctionName, int Step, int Index)
{
	WARNING("ADVEngine.GetArgumentValue_string: not implemented");
	return false;
}

HLL_LIBRARY(ADVEngine,
	    HLL_EXPORT(Load, ADVEngine_Load),
	    HLL_EXPORT(Release, ADVEngine_Release),
	    HLL_EXPORT(NumofFunctionName, ADVEngine_NumofFunctionName),
	    HLL_EXPORT(GetFunctionName, ADVEngine_GetFunctionName),
	    HLL_EXPORT(AnalyseIf, ADVEngine_AnalyseIf),
	    HLL_EXPORT(NumofIf, ADVEngine_NumofIf),
	    HLL_EXPORT(GetIf, ADVEngine_GetIf),
	    HLL_EXPORT(AnalyseFunction, ADVEngine_AnalyseFunction),
	    HLL_EXPORT(NumofStep, ADVEngine_NumofStep),
	    HLL_EXPORT(GetStepText, ADVEngine_GetStepText),
	    HLL_EXPORT(OpenFunctionFile, ADVEngine_OpenFunctionFile),
	    HLL_EXPORT(GetCodeType, ADVEngine_GetCodeType),
	    HLL_EXPORT(GetMessage, ADVEngine_GetMessage),
	    HLL_EXPORT(BindFunction, ADVEngine_BindFunction),
	    HLL_EXPORT(GetFuncName, ADVEngine_GetFuncName),
	    HLL_EXPORT(CreateDelegateObjectID, ADVEngine_CreateDelegateObjectID),
	    HLL_EXPORT(GetArgumentValue_int, ADVEngine_GetArgumentValue_int),
	    HLL_EXPORT(GetArgumentValue_float, ADVEngine_GetArgumentValue_float),
	    HLL_EXPORT(GetArgumentValue_bool, ADVEngine_GetArgumentValue_bool),
	    HLL_EXPORT(GetArgumentValue_string, ADVEngine_GetArgumentValue_string));
