#include "SafeWrite.h"
#include "Hooks_Script.h"
#include "GameForms.h"
#include "GameScript.h"
#include "ScriptUtils.h"
#include "CommandTable.h"
#include <stack>
#include <string>

// a size of ~1KB should be enough for a single line of code
char s_ExpressionParserAltBuffer[0x500] = {0};

#if RUNTIME


#if RUNTIME_VERSION == RUNTIME_VERSION_1_4_0_525

const UInt32 ExtractStringPatchAddr = 0x005ADDCA;	// ExtractArgs: follow first jz inside loop then first case of following switch: last call in case.
const UInt32 ExtractStringRetnAddr  = 0x005ADDE3;

static const UInt32 kResolveRefVarPatchAddr		= 0x005AC530;	// Second jnz, just after reference to TlsData. In second to last call before main switch in ExtractArgs. (Last in F03: 0x0517549)
static const UInt32 kResolveNumericVarPatchAddr = 0x005A9168;	//		From previous sub, third call before the end. ( second to last call in Fallout3:0X0051682D)
static const UInt32 kEndOfLineCheckPatchAddr	= 0;	// not yet supported at run-time

// incremented on each recursive call to Activate, limit of 5 hard-coded
static UInt32* kActivationRecurseDepth = (UInt32*)0x011CA424;

static const UInt32 kExpressionParserBufferOverflowHookAddr_1 = 0x005935D0;	// find ref to aInfixtopostfixError, then enter previous call. In sub, first reference to buffer at -0X48
static const UInt32 kExpressionParserBufferOverflowRetnAddr_1 = 0x005935D7;

static const UInt32 kExpressionParserBufferOverflowHookAddr_2 = 0x005937DE;	// same sub, second reference to same buffer
static const UInt32 kExpressionParserBufferOverflowRetnAddr_2 = 0x005937E5;

//static const UInt32 kExtractArgsNumArgsHookAddr = 0x004FB4B2;	// Needed in F3, not in FalloutNV
static const UInt32 kExtractArgsEndProcAddr = 0x005AE0FA;		// returns true from ExtractArgs(), F3: 0x00518014
static const UInt32 kExtractArgsReadNumArgsPatchAddr = 0x005ACCD4;	// FalloutNV uses a different sequence, see the end of this file
static const UInt32 kExtractArgsReadNumArgsRetnAddr = 0x005ACCE9;	// FalloutNV uses a different sequence, see the end of this file
		// Fallout3: static const UInt32 kExtractArgsReadNumArgsPatchAddr = 0x0051796F
static const UInt32 kExtractArgsNoArgsPatchAddr = 0x005ACD07;			// jle kExtractArgsEndProcAddr (if num args == 0)	F3: 0x0051798A

static const UInt32 kScriptRunner_RunHookAddr = 0x005E0D51;	// Start from Script::Execute, second call after pushing "all" arguments, take 3rd call from the end (present twice)
static const UInt32 kScriptRunner_RunRetnAddr = kScriptRunner_RunHookAddr + 5;
static const UInt32 kScriptRunner_RunCallAddr = 0x00702FC0;			// overwritten call
static const UInt32 kScriptRunner_RunEndProcAddr = 0x005E113A;		// retn 0x20

#elif RUNTIME_VERSION == RUNTIME_VERSION_1_4_0_525ng

const UInt32 ExtractStringPatchAddr = 0x005ADF7A;	// ExtractArgs: follow first jz inside loop then first case of following switch: last call in case.
const UInt32 ExtractStringRetnAddr  = 0x005ADF93;

static const UInt32 kResolveRefVarPatchAddr		= 0x005AC6E0; // First jnz, just after reference to TlsData. In second to last call before main switch in ExtractArgs. (Last in F03: 0x0517549)
static const UInt32 kResolveNumericVarPatchAddr = 0x005A9298;	//	First jnz,	From previous sub, third call before the end. ( second to last call in Fallout3:0X0051682D)
static const UInt32 kEndOfLineCheckPatchAddr	= 0;	// not yet supported at run-time

// incremented on each recursive call to Activate, limit of 5 hard-coded
static UInt32* kActivationRecurseDepth = (UInt32*)0x011CA424;

static const UInt32 kExpressionParserBufferOverflowHookAddr_1 = 0x005937C0;	// find ref to aInfixtopostfixError, then enter second previous call. In sub, first reference to buffer at -0X48
static const UInt32 kExpressionParserBufferOverflowRetnAddr_1 = 0x005937C7;

static const UInt32 kExpressionParserBufferOverflowHookAddr_2 = 0x005939CE;	// same sub, second reference to same buffer
static const UInt32 kExpressionParserBufferOverflowRetnAddr_2 = 0x005939D5;

//static const UInt32 kExtractArgsNumArgsHookAddr = 0x004FB4B2;	// Needed in F3, not in FalloutNV
static const UInt32 kExtractArgsEndProcAddr = 0x005AE2AA;		// returns true from ExtractArgs(), F3: 0x00518014
static const UInt32 kExtractArgsReadNumArgsPatchAddr = 0x005ACE84;	// FalloutNV uses a different sequence, see the end of this file
static const UInt32 kExtractArgsReadNumArgsRetnAddr = 0x005ACE99;	// FalloutNV uses a different sequence, see the end of this file
		// Fallout3: static const UInt32 kExtractArgsReadNumArgsPatchAddr = 0x0051796F
static const UInt32 kExtractArgsNoArgsPatchAddr = 0x005ACEB7;			// jle kExtractArgsEndProcAddr (if num args == 0)	F3: 0x0051798A

static const UInt32 kScriptRunner_RunHookAddr = 0x005E0C61;	// Start from Script::Execute, second call after pushing "all" arguments, take 3rd call from the end (present twice)
static const UInt32 kScriptRunner_RunRetnAddr = kScriptRunner_RunHookAddr + 5;
static const UInt32 kScriptRunner_RunCallAddr = 0x00702F10;			// overwritten call
static const UInt32 kScriptRunner_RunEndProcAddr = 0x005E104A;		// retn 0x20

#else
#error
#endif

static UInt32 __stdcall DoExtractString(char* scriptData, UInt32 dataLen, char* dest, ScriptEventList* eventList)
{
	// copy the string
	memcpy(dest, scriptData, dataLen);
	UInt32 newDataLen = dataLen;
	
	if (newDataLen && dest[0] == '$' && eventList && eventList->m_script)	// variable name
	{
		VariableInfo* varInfo = NULL;
		if ((newDataLen < 0x100) && (dest[newDataLen]))
			dest[newDataLen] = 0;
		varInfo = eventList->m_script->GetVariableByName(dest + 1);
		if (varInfo)
		{
			ScriptEventList::Var* var;
			var = eventList->GetVariable(varInfo->idx);
			if (var)
			{
				StringVar* strVar;
				strVar = g_StringMap.Get(var->data);
				if (strVar)
					if (strVar->GetLength() < 0x100) {		// replace string with contents of string var
						strcpy_s(dest, strVar->GetLength() + 1, strVar->GetCString());
						newDataLen = strVar->GetLength();
					}
			}
		}
	}			// "%e" becomes an empty string
	else if (newDataLen == 2 && dest[0] == '%' && toupper(dest[1]) == 'E') {
		newDataLen = 0;
	}
	// force zero terminated string
	if ((newDataLen < 0x100) && (dest[newDataLen]))
		dest[newDataLen] = 0;
	return dataLen;
}

static __declspec(naked) void ExtractStringHook(void)
{
	// This hooks immediately before a call to memcpy().
	// Original code copies a string literal from script data to buffer
	// If string is of format $localVariableName, replace literal with contents of string var

	__asm {
		// Grab the args to memcpy()
		pushad

		mov eax, [ebp+0x020]	// arg20 = ScriptEventList
		push eax
		push edx				// destination buffer
		movzx eax, [ebp-0xE4]	// var_E4 = dataLen
		push eax				// dataLen
		push ecx				// scriptData
		call DoExtractString
// NEUTRALISED		lea ecx, [ebp-0xE4]
// NEUTRALISED		mov [ecx], ax			// update var_E4 = dataLen

		popad
		add esp, 0Ch			// skipped op

		jmp [ExtractStringRetnAddr]
	}
}

static __declspec(naked) void ExpressionParserBufferOverflowHook_1(void)
{
	__asm {
		lea	edx, s_ExpressionParserAltBuffer		// swap buffers
		push edx
		lea eax, [ebp-0x54]
		jmp	[kExpressionParserBufferOverflowRetnAddr_1]
	}
}

static __declspec(naked) void ExpressionParserBufferOverflowHook_2(void)
{
	__asm {
		lea eax, s_ExpressionParserAltBuffer
		push eax
		mov ecx, [ebp-0x58]
		jmp	[kExpressionParserBufferOverflowRetnAddr_2]
	}
}

void Hook_Script_Init()
{
	WriteRelJump(ExtractStringPatchAddr, (UInt32)&ExtractStringHook);

	// patch the "apple bug"
	// game caches information about the most recently retrieved RefVariable for the current executing script
	// if same refIdx requested twice in a row returns previously returned ref without
	// bothering to check if form stored in ref var has changed
	// this fixes it by overwriting a conditional jump with an unconditional one
	SafeWrite8(kResolveRefVarPatchAddr, 0xEB);

	// game also caches information about the most recently retrieved local numeric variable for
	// currently executing script. Causes issues with function scripts. As above, overwrite conditional jump with unconditional
	SafeWrite8(kResolveNumericVarPatchAddr, 0xEB);

	// hook code in the vanilla expression parser's subroutine to fix the buffer overflow
	WriteRelJump(kExpressionParserBufferOverflowHookAddr_1, (UInt32)&ExpressionParserBufferOverflowHook_1);
	WriteRelJump(kExpressionParserBufferOverflowHookAddr_2, (UInt32)&ExpressionParserBufferOverflowHook_2);

	// hook ExtractArgs() to handle commands normally compiled with Cmd_Default_Parse which were instead compiled with Cmd_Expression_Parse
	ExtractArgsOverride::Init_Hooks();

	// Following report of functions failing to be called, with at least one reported case of issue with threading: in case it is needed.
	::InitializeCriticalSection(&csGameScript);
}

void ResetActivationRecurseDepth()
{
	// reset to circumvent the hard-coded limit
	//not found	*kActivationRecurseDepth = 0;
}

#else	// CS-stuff

#include "PluginManager.h"

static const UInt32 kEndOfLineCheckPatchAddr = 0x005C7EBC;	// find aGameMode, down +0x1C, follow offset, follow first jle, the following jnb	(F3:0x005C16A2)
static const UInt32 kCopyStringArgHookAddr	 = 0x005C7A70;	// find function referencing aInvalidOwnerSF, follow the jz before the switch  67 cases, take first choice of following case (F3:0x005C038C)

static const UInt32 kBeginScriptCompilePatchAddr = 0x005C9859;	// calls CompileScript()	// Find reference to aScriptDDS, then follow the 5th call below. It is the second call in this proc (F3:0x005C3099)
static const UInt32 kBeginScriptCompileCallAddr  = 0x005C96E0;	// bool __fastcall CompileScript(unk, unk, Script*, ScriptBuffer*)	(F3:0x005C96E0)
static const UInt32 kBeginScriptCompileRetnAddr	 = 0x005C985E;	// next op. (F3:0x005C309E)

static const UInt32 kExpressionParserBufferOverflowHookAddr_1 = 0x005B1AD9;	// First call before reference to aInfixToPostfix, then first reference to var_44	(F3:0x005B7049)
static const UInt32 kExpressionParserBufferOverflowRetnAddr_1 = 0x005B1ADE;	// Two ops below	(F3:0x005B704E)

static const UInt32 kExpressionParserBufferOverflowHookAddr_2 = 0x005B1C6C;	// Same proc, next reference to var-44	(F3:0x005B71F5)
static const UInt32 kExpressionParserBufferOverflowRetnAddr_2 = 0x005B1C73;	// Two ops below also	(F3:0x005B71FC)

static __declspec(naked) void ExpressionParserBufferOverflowHook_1(void)
{
	__asm {
		lea	eax, s_ExpressionParserAltBuffer
		push eax
		jmp	[kExpressionParserBufferOverflowRetnAddr_1]
	}
}

static __declspec(naked) void ExpressionParserBufferOverflowHook_2(void)
{
	__asm {
		lea	edx, s_ExpressionParserAltBuffer
		mov	byte ptr [edi], 0x20
		jmp	[kExpressionParserBufferOverflowRetnAddr_2]
	}
}

#endif	// RUNTIME

// Patch compiler check on end of line when calling commands from within other commands
// TODO: implement for run-time compiler
// Hook differs for 1.0/1.2 CS versions as the patched code differs
#if EDITOR

void PatchEndOfLineCheck(bool bDisableCheck)
{

	if (bDisableCheck)
		SafeWrite8(kEndOfLineCheckPatchAddr, 0xEB);		// unconditional (short) jump
	else
		SafeWrite8(kEndOfLineCheckPatchAddr, 0x73);		// conditional jnb (short)
}

#else

void PatchEndOfLineCheck(bool bDisableCheck)
{
	// ###TODO: implement for run-time
}

#endif

static bool s_bParsingExpression = false;

bool ParsingExpression()
{
	return s_bParsingExpression;
}

bool ParseNestedFunction(CommandInfo* cmd, ScriptLineBuffer* lineBuf, ScriptBuffer* scriptBuf)
{
	// disable check for end of line
	PatchEndOfLineCheck(true);

	s_bParsingExpression = true;
	bool bParsed = cmd->parse(cmd->numParams, cmd->params, lineBuf, scriptBuf);
	s_bParsingExpression = false;

	// re-enable EOL check
	PatchEndOfLineCheck(false);

	return bParsed;
}

static std::stack<UInt8*> s_loopStartOffsets;

void RegisterLoopStart(UInt8* offsetPtr)
{
	s_loopStartOffsets.push(offsetPtr);
}

bool HandleLoopEnd(UInt32 offsetToEnd)
{
	if (!s_loopStartOffsets.size())
		return false;

	UInt8* startPtr = s_loopStartOffsets.top();
	s_loopStartOffsets.pop();

	*((UInt32*)startPtr) = offsetToEnd;
	return true;
}


#if EDITOR

namespace CompilerOverride {
	static Mode s_currentMode = kOverride_BlockType;
	static const UInt16 kOpcode_Begin = 0x0010;
	static const UInt16 kOpcode_Dot = 0x001C;

	static bool __stdcall DoCmdDefaultParseHook(UInt32 numParams, ParamInfo* params, ScriptLineBuffer* lineBuf, ScriptBuffer* scriptBuf)
	{
		// record the mode so ExtractArgs hook knows what to expect
		lineBuf->Write16(s_currentMode);

		// parse args
		ExpressionParser parser(scriptBuf, lineBuf);
		bool bResult = parser.ParseArgs(params, numParams, false);
		return bResult;
	}

	static __declspec(naked) void Hook_Cmd_Default_Parse(void)
	{
		static UInt32 numParams;
		static ParamInfo* params;
		static ScriptLineBuffer* lineBuf;
		static ScriptBuffer* scriptBuf;
		static UInt8 result;

		__asm {
			// grab args
			mov	eax, [esp+4]
			mov	[numParams], eax
			mov	eax, [esp+8]
			mov	[params], eax
			mov	eax, [esp+0xC]
			mov [lineBuf], eax
			mov eax, [esp+0x10]
			mov [scriptBuf], eax

			// call override parse routine
			pushad
			push scriptBuf
			push lineBuf
			push params
			push numParams
			call DoCmdDefaultParseHook

			// store result
			mov	 [result], al

			// clean up
			popad

			// return result
			xor eax, eax
			mov al, [result]
			retn
		}
	}

	void __stdcall ToggleOverride(bool bOverride)
	{
		static const UInt32 patchLoc = 0x005C67E0;	// editor default parse routine	(g_defaultParseCommand in CommandTable)	(F3:0x005C01F0)
		
		// ToggleOverride() only gets invoked when we parse a begin or end statement, so set mode accordingly
		s_currentMode = kOverride_BlockType;

		// overwritten instructions
		static const UInt8 s_patchedInstructions[5] = { 0x81, 0xEC, 0x30, 0x02, 0x00 };	// same first five bytes as Oblivion
		if (bOverride) {			
			WriteRelJump(patchLoc, (UInt32)&Hook_Cmd_Default_Parse);
		}
		else {		
			for (UInt32 i = 0; i < sizeof(s_patchedInstructions); i++) {
				SafeWrite8(patchLoc+i, s_patchedInstructions[i]);
			}
		}
	}
	
	static const UInt32 stricmpAddr = 0x00C5C61A;			// doesn't clean stack						// the proc called below	(F3:0x00BBBDCF)
	static const UInt32 stricmpPatchAddr_1 = 0x005C8D49;	// stricmp(cmdInfo->longName, blockName)	// proc containing reference to aSyntaxError__2, second call above	(F3:0x005C2581)
	static const UInt32 stricmpPatchAddr_2 = 0x005C8D5D;	// stricmp(cmdInfo->altName, blockName)		// same proc, first call above that same reference (F3:0x005C2595)

	// stores offset into ScriptBuffer's data buffer at which to store the jump offset for the current block
	static const UInt32* kBeginBlockDataOffsetPtr = (const UInt32*)0x00ED9D54;		// same proc, address moved into before reference to aInvalidBlockTy (mov kBeginBlockDataOffsetPtr, ecx) (F3:0x00FC6D38)

	// target of an overwritten jump after parsing of block args
	// copies block args to ScriptBuffer after doing some bounds-checking
	static const UInt32 kCopyBlockArgsAddr = 0x005C92C9;							// same proc, jump before reference to aSyntaxError__3	(F3:0x005C2D5D)

	// address of an overwritten call to a void function(void) which does nothing.
	// after ScriptLineBuffer data has been copied to ScriptBuffer and CompileLine() is about to return true
	static const UInt32 nullCallAddr = 0x005C95EB;									// same proc, last call to nullsub(_3) before the endp	(F3:0x005C2E29, nullsub_19)

	// address at which the block len is calculated when compiling 'end' statement
	static const UInt32 storeBlockLenHookAddr = 0x005C8F8A;							// same proc of course, 3 ops BEFORE next addr (mov ecx, [eax])	(F3:0x005C27C3)
	static const UInt32 storeBlockLenRetnAddr = 0x005C8F90;							// same proc, mov kBeginBlockDataOffsetPtr, ebx after reference to aSyntaxError__3	(F3:0x005C27C9)

	static bool s_overridden;	// is true if compiler override in effect

	bool IsActive()
	{
		return s_overridden;
	}

	static __declspec(naked) void CompareBlockName(void)
	{
		// edx: token
		// ecx: CommandInfo longName or altName

		__asm {
			push edx	// we may be incrementing edx here, so preserve its original value
			push eax

			// first, toggle override off if it's already activated
			test [s_overridden], 1
			jz TestForUnderscore
			pushad
			push 0
			call ToggleOverride
			popad

	TestForUnderscore:
			// if block name preceded by '_', enable compiler override
			mov al, byte ptr [edx]
			cmp al, '_'
			setz al
			mov [s_overridden], al
			test al, al
			jz DoStricmp
			// skip the '_' character
			add edx, 1

	DoStricmp:
			// do the comparison
			pop eax
			push edx
			push eax
			call [stricmpAddr]
			add esp, 8
			pop edx
			
			// a match?
			test eax, eax
			jnz Done

			// a match. override requested?
			test [s_overridden], 1
			jz Done

			// toggle the override
			pushad
			push 1
			call ToggleOverride
			popad

		Done:
			// return the result of stricmp(). caller will clean stack
			retn
		}
	}

	static void __stdcall ProcessBeginOrEnd(ScriptBuffer* buf, UInt32 opcode)
	{
		ASSERT(opcode == 0x10 || opcode == 0x11);

		// make sure we've got enough room in the buffer
		if (buf->dataOffset + 4 >= 0x4000) {
			g_ErrOut.Show("Error: Max script length exceeded. Please edit and recompile.");
		}
		else {
			const char* cmdName = "@PushExecutionContext";
			SInt32 offset = 0;

			if (opcode == 0x11) {
				// 'end'. Need to rearrange so we pop context before the 'end' statement
				cmdName = "@PopExecutionContext";
				offset = -4;
				
				// move 'end' state forward by 4 bytes (i.e. at current offset)
				*((UInt32*)(buf->scriptData+buf->dataOffset)) = 0x00000011;
			}

			CommandInfo* cmdInfo = g_scriptCommands.GetByName(cmdName);
			ASSERT(cmdInfo != NULL);

			// write a call to our cmd
			*((UInt16*)(buf->scriptData + buf->dataOffset + offset)) = cmdInfo->opcode;
			buf->dataOffset += sizeof(UInt16);

			// write length of params
			*((UInt16*)(buf->scriptData + buf->dataOffset + offset)) = 0;
			buf->dataOffset += sizeof(UInt16);
		}
	}


	static __declspec(naked) void OnCompileSuccessHook(void)
	{
		// esi: ScriptLineBuffer
		// edi: ScriptBuffer
		// eax: volatile
		__asm {
			// is override in effect?
			test [s_overridden], 1
			jz Done

			// have we just parsed a begin or end statement?
			movzx eax, word ptr [esi+0x410]		// cmd opcode for this line
			cmp eax, 0x10						// 0x10 == 'begin'
			jz Begin
			cmp eax, 0x11						// 0x11 == 'end'
			jz End
			jmp Done							// not a block statement

		Begin:
			// commands following start of block should have access to script context
			mov	[s_currentMode], kOverride_Command
			jmp Process

		End:
			// expect a new block or end of script, no execution context available
			mov [s_currentMode], kOverride_BlockType

		Process:
			// got a begin or end statement, handle it
			pushad
			push eax		// opcode
			push edi		// ScriptBuffer
			call ProcessBeginOrEnd
			popad

		Done:
			retn
		}
	}

	static __declspec(naked) void StoreBlockLenHook(void)
	{
		__asm {
			// overwritten code
			mov ecx, [eax]
			sub edx, ecx

			// override in effect?
			test [s_overridden], 1
			jz Done

			// add 4 bytes to block len to account for 'push context' cmd
			add edx, 4

	Done:
			// (overwritten) store block len
			mov [eax], edx
			jmp [storeBlockLenRetnAddr]
		}
	}
			

	void InitHooks()
	{
		// overwrite calls to compare block name when parsing 'begin'
		WriteRelCall(stricmpPatchAddr_1, (UInt32)&CompareBlockName);
		WriteRelCall(stricmpPatchAddr_2, (UInt32)&CompareBlockName);

		// overwrite a call to a null sub just before returning true from CompileScriptLine()
		WriteRelCall(nullCallAddr, (UInt32)&OnCompileSuccessHook);

		// hook code that records the jump offset for the current block
		WriteRelJump(storeBlockLenHookAddr, (UInt32)&StoreBlockLenHook);
	}

	bool Cmd_Plugin_Default_Parse(UInt32 numParams, ParamInfo* params, ScriptLineBuffer* lineBuf, ScriptBuffer* scriptBuf)
	{
		bool bOverride = IsActive();
		if (bOverride) {
			ToggleOverride(false);
		}

		bool result = Cmd_Default_Parse(numParams, params, lineBuf, scriptBuf);

		if (bOverride) {
			ToggleOverride(true);
		}

		return result;
	}
}

bool __stdcall HandleBeginCompile(ScriptBuffer* buf)
{
	// empty out the loop stack
	while (s_loopStartOffsets.size())
		s_loopStartOffsets.pop();

	// Preprocess the script:
	//  - check for usage of array variables in Set statements (disallowed)
	//  - check loop structure integrity
	//  - check for use of ResetAllVariables on scripts containing string/array vars

	bool bResult = PrecompileScript(buf);
	if (bResult) {
		PluginManager::Dispatch_Message(0, NVSEMessagingInterface::kMessage_Precompile, buf, sizeof(buf), NULL);
	}

	return bResult;
}

static __declspec(naked) void CompileScriptHook(void)
{
	static bool precompileResult;

	__asm	
	{
		mov		eax,	[esp+4]					// grab the second arg (ScriptBuffer*)
		pushad
		push	eax
		call	HandleBeginCompile				// Precompile
		mov		[precompileResult],	al			// save result
		popad
		call	[kBeginScriptCompileCallAddr]	// let the compiler take over
		test	al, al
		jz		EndHook							// return false if CompileScript() returned false
		mov		al,	[precompileResult]			// else return result of Precompile
	EndHook:
		// there's a small possibility that the compiler override is still in effect here (e.g. scripter forgot an 'end')
		// so make sure there's no chance it remains in effect, otherwise potentially could affect result script compilation
		pushad
		push 0
		call CompilerOverride::ToggleOverride
		popad
		jmp		[kBeginScriptCompileRetnAddr]	// bye
	}
}

// replace special characters ("%q" -> '"', "%r" -> '\n')
UInt32 __stdcall CopyStringArg(char* dest, const char* src, UInt32 len, ScriptLineBuffer* lineBuf)
{
	if (!src || !len || !dest)
		return len;

	std::string str(src);
	UInt32 pos = 0;

	while ((pos = str.find('%', pos)) != -1 && pos < str.length() - 1)
	{
		char toInsert = 0;
		switch (str[pos + 1])
		{
		case '%':
			pos += 2;
			continue;
		case 'r':
		case 'R':
			toInsert = '\n';
			break;
		case 'q':
		case 'Q':
			toInsert = '"';
			break;
		default:
			pos += 1;
			continue;
		}
		
		str.insert(pos, 1, toInsert);	// insert char at current pos
		str.erase(pos + 1, 2);			// erase format specifier
		pos += 1;
	}

	// copy the string to script data
	memcpy(dest, str.c_str(), str.length());

	// write length of string
	lineBuf->dataOffset -= 2;
	lineBuf->Write16(str.length());

	return str.length();
}

// major code differences between CS versions here so hooks differ significantly
static __declspec(naked) void __cdecl CopyStringArgHook(void)
{
// overwrite call to memcpy()

// On entry:
//	eax: dest buffer
//	edx: src string
//	edi: string len
//  esi: ScriptLineBuffer* (must be preserved)
// edi must be updated to reflect length of modified string (added to dataOffset on return)

__asm
{
	push	esi				

	push	esi
	push	edi
	push	edx
	push	eax
	call	CopyStringArg

	mov		edi, eax
	pop		esi

	retn
}
}

void Hook_Compiler_Init()
{
	// hook beginning of compilation process
	WriteRelJump(kBeginScriptCompilePatchAddr, (UInt32)&CompileScriptHook);

	// hook copying of string argument to compiled data
	// lets us modify the string before its copied
	WriteRelCall(kCopyStringArgHookAddr, (UInt32)&CopyStringArgHook);

	// hook code in the vanilla expression parser's subroutine to fix the buffer overflow
	WriteRelJump(kExpressionParserBufferOverflowHookAddr_1, (UInt32)&ExpressionParserBufferOverflowHook_1);
	WriteRelJump(kExpressionParserBufferOverflowHookAddr_2, (UInt32)&ExpressionParserBufferOverflowHook_2);

	CompilerOverride::InitHooks();
}

#else			// run-time

#include "common/ICriticalSection.h"

namespace ExtractArgsOverride {
	static std::vector<ExecutingScriptContext*>	s_stack;
	static ICriticalSection s_critSection;

	ExecutingScriptContext::ExecutingScriptContext(TESObjectREFR* thisObj, TESObjectREFR* container, UInt16 opcode)
	{
		callingRef = thisObj;
		containerRef = container;
		cmdOpcode = opcode;
		threadID = GetCurrentThreadId();
	}

	ExecutingScriptContext* PushContext(TESObjectREFR* thisObj, TESObjectREFR* containerObj, UInt8* scriptData, UInt32* offsetPtr)
	{
		UInt16* opcodePtr = (UInt16*)(scriptData + *offsetPtr - 4);
		ExecutingScriptContext* context = new ExecutingScriptContext(thisObj, containerObj, *opcodePtr);

		s_critSection.Enter();
		s_stack.push_back(context);
		s_critSection.Leave();

		return context;
	}

	bool PopContext()
	{
		DWORD curThreadID = GetCurrentThreadId();
		bool popped = false;

		s_critSection.Enter();
 		for (std::vector<ExecutingScriptContext*>::reverse_iterator iter = s_stack.rbegin(); iter != s_stack.rend(); ++iter) {
			ExecutingScriptContext* context = *iter;
			if (context->threadID == curThreadID) {
				delete context;
				s_stack.erase((++iter).base());
				popped = true;
				break;
			}
		}
		s_critSection.Leave();

		return popped;
	}

	ExecutingScriptContext* GetCurrentContext()
	{
		ExecutingScriptContext* context = NULL;
		DWORD curThreadID = GetCurrentThreadId();

		s_critSection.Enter();
		for (SInt32 i = s_stack.size() - 1; i >= 0; i--) {
			if (s_stack[i]->threadID == curThreadID) {
				context = s_stack[i];
				break;
			}
		}
		s_critSection.Leave();

		return context;
	}

	static bool __stdcall DoExtractExtendedArgs(CompilerOverride::Mode mode, UInt32 _ebp, va_list varArgs)
	{
		static const UInt8 stackOffset = 0x00;
		
		// grab the args to ExtractArgs()
		_ebp += stackOffset;
		ParamInfo* paramInfo = *(ParamInfo**)(_ebp+0x08);
		UInt8* scriptData = *(UInt8**)(_ebp+0x0C);
		UInt32* opcodeOffsetPtr = *(UInt32**)(_ebp+0x10);
		Script* scriptObj = *(Script**)(_ebp+0x1C);
		ScriptEventList* eventList = *(ScriptEventList**)(_ebp+0x20);
		
		// extract
		return ExtractArgs(paramInfo, scriptData, scriptObj, eventList, opcodeOffsetPtr, varArgs, true, mode);
	}

	bool ExtractArgs(ParamInfo* paramInfo, UInt8* scriptData, Script* scriptObj, ScriptEventList* eventList, UInt32* opcodeOffsetPtr,
		va_list varArgs, bool bConvertTESForms, UInt16 numArgs)
	{
		// get thisObj and containerObj from currently executing script context, if available
		TESObjectREFR* thisObj = NULL;
		TESObjectREFR* containingObj = NULL;
		if (numArgs == CompilerOverride::kOverride_Command) {
			// context should be available
			ExecutingScriptContext* context = GetCurrentContext();
			if (!context) {
				g_ErrOut.Show("ERROR: Could not get execution context in ExtractArgsOverride::ExtractArgs");
				return false;
			}
			else {
				thisObj = context->callingRef;
				containingObj = context->containerRef;
			}
		}

		// extract
		ExpressionEvaluator eval(paramInfo, scriptData, thisObj, containingObj, scriptObj, eventList, NULL, opcodeOffsetPtr);
		if (eval.ExtractDefaultArgs(varArgs, bConvertTESForms)) {
			return true;
		}
		else {
			DEBUG_PRINT("ExtractArgsOverride::ExtractArgs returns false");
			return false;
		}
	}

	bool ExtractFormattedString(ParamInfo* paramInfo, UInt8* scriptData, Script* scriptObj, ScriptEventList* eventList,
		UInt32* opcodeOffsetPtr, va_list varArgs, UInt32 fmtStringPos, char* fmtStringOut, UInt32 maxParams)
	{
		ExecutingScriptContext* context = GetCurrentContext();
		if (!context) {
			g_ErrOut.Show("ERROR: Could not get execution context in ExtractFormattedString()");
			return false;
		}
		
		ExpressionEvaluator eval(paramInfo, scriptData, context->callingRef, context->containerRef, scriptObj, eventList,
			NULL, opcodeOffsetPtr);
		return eval.ExtractFormatStringArgs(varArgs, fmtStringPos, fmtStringOut, maxParams);
	}

/*
	Oblivion (or Fallout3)
.text:004FAE90 work:                                   ; CODE XREF: ExtractArgs+8j
.text:004FAE90                 mov     ecx, [esp+18h+opcodeOffsetPtr]
.text:004FAE94                 mov     eax, [ecx]
.text:004FAE96                 mov     edx, [esp+18h+arg1]
.text:004FAE9A
.text:004FAE9A kExtractArgsReadNumArgsPatchAddr:
.text:004FAE9A                 movSx   edx, word ptr [eax+edx]
.text:004FAE9E                 push    ebx
.text:004FAE9F                 push    ebp
.text:004FAEA0                 push    esi
.text:004FAEA1                 add     eax, 2
.text:004FAEA4                 test    dx, dx
.text:004FAEA7                 push    edi
.text:004FAEA8                 lea     esi, [esp+28h+arg_1C]
.text:004FAEAC                 mov     [esp+28h+var_C], edx
.text:004FAEB0                 mov     [ecx], eax
.text:004FAEB2                 mov     [esp+28h+var_18], 0
.text:004FAEBA
.text:004FAEBA kExtractArgsNoArgsPatchAddr:
.text:004FAEBA                 jle     kExtractArgsNumArgsHookAddr

	FalloutNV
.text:005ACCC6 lea     eax, [ebp+arg_1C]               ; Load Effective Address
.text:005ACCC9 mov     [ebp+var_C], eax
.text:005ACCCC mov     ecx, [ebp+opcodeOffsetPtr]
.text:005ACCCF mov     edx, [ecx]
.text:005ACCD1 mov     eax, [ebp+scriptData]
.text:005ACCD4 kExtractArgsReadNumArgsPatchAddr:
.text:005ACCD4 mov     cx, [eax+edx]																jmp     kExtractArgsNumArgsHookAddr
.text:005ACCD8 mov     [ebp+var_4], cx
.text:005ACCDC kExtractArgsReadNumArgsRetnAddr:
.text:005ACCDC mov     edx, [ebp+opcodeOffsetPtr]
.text:005ACCDF mov     eax, [edx]
.text:005ACCE1 add     eax, 2                          ; Add
.text:005ACCE4 mov     ecx, [ebp+opcodeOffsetPtr]
.text:005ACCE7 mov     [ecx], eax
.text:005ACCE9 xor     edx, edx                        ; Logical Exclusive OR
.text:005ACCEB mov     [ebp+var_8], dx
.text:005ACCEF jmp     short loc_5ACCFD                ; Jump
.text:005ACCF1 ; ---------------------------------------------------------------------------
.text:005ACCF1
.text:005ACCF1 loop:                                   ; CODE XREF: ExtractArgs:loc_5AE0ECj
.text:005ACCF1 mov     ax, [ebp+var_8]
.text:005ACCF5 add     ax, 1                           ; Add
.text:005ACCF9 mov     [ebp+var_8], ax
.text:005ACCFD
.text:005ACCFD loc_5ACCFD:                             ; CODE XREF: ExtractArgs+3Fj
.text:005ACCFD movsx   ecx, [ebp+var_8]                ; Move with Sign-Extend
.text:005ACD01 movsx   edx, [ebp+var_4]                ; Move with Sign-Extend
.text:005ACD05 cmp     ecx, edx                        ; Compare Two Operands
.text:005ACD07 jge     kExtractArgsEndProcAddr         ; current argNum >= argCount, we are done

*/

	static __declspec(naked) void ExtractExtendedArgsHook(void)
	{
		static UInt32	_ebp;
		static UInt8	bResult;

		__asm {
			// restore missing ops
			mov     cx, [eax+edx]
			mov     [ebp-4], cx
			mov     edx, [ebp+0x10]
			mov     eax, [edx]
			add     eax, 2
			mov     [edx], eax				// original code used ecx, but we need to preserve it, so we reuse edx

			//saves context
			mov		[_ebp],	ebp
			pushad
			test	cx,cx
			jl		ourcode					// < 0 indicates compiled with Cmd_Expression_Parse rather than Cmd_Default_Parse
			popad
			jmp		kExtractArgsReadNumArgsRetnAddr
ourcode:
			lea		eax,	[ebp+0x24]		// va_list
			push	eax
			mov		eax,	[_ebp]			// caller context
			push	eax
			movsx	eax,	cx				// num args
			push	eax
			call	DoExtractExtendedArgs
			mov		[bResult], al

			popad
			movzx	eax, [bResult]
			jmp		[kExtractArgsEndProcAddr]
		}
	}

	// these hooks don't get invoked unless they are required (no run-time penalty if compiler override is not used)
	static void Init_Hooks()
	{
		//Not for F:NV	// change movzx to movsx to read # of args as a signed value
		//Not for F:NV	// < 0 indicates compiled with Cmd_Expression_Parse rather than Cmd_Default_Parse
		//Not for F:NV	SafeWrite16(kExtractArgsReadNumArgsPatchAddr, 0xBF0F);

		//Not for F:NV	// if numArgs == 0, return true; else execute our hook
		WriteRelJump(kExtractArgsReadNumArgsPatchAddr, (UInt32)&ExtractExtendedArgsHook);			// numArgs < 0, execute hook
		//Not for F:NV	WriteRelJump(kExtractArgsNumArgsHookAddr+6, kExtractArgsEndProcAddr);		// numArgs == 0, return true

		//Not for F:NV	// if numArgs <= 0, jump to our hook test instead of end proc
		//Not for F:NV	WriteRelJle(kExtractArgsNoArgsPatchAddr, kExtractArgsNumArgsHookAddr);
	}
}	// namespace

#endif			// run-time