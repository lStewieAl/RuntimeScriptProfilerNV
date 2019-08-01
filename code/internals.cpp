#include "internals.h"
#include "nvse/nvse/GameOSDepend.h"

void __fastcall MainLoopExecuteScriptsHook(void* processManager);
void __stdcall DoScriptRunnerExecuteTimeElapsed(Script* ExecutingScript, float ElapsedTime);

_declspec(naked) void __fastcall ScriptRunnerExecuteTimeElapsedHook(void* scriptRunner, void* dummyEDX, Script* script)
{
	static const UInt32 retnAddr = 0x5E10ED;
	_asm {
		fstp [ebp - 0x760]
		mov eax, [ebp - 0x760]
		pushad
		push eax
		push script
		call DoScriptRunnerExecuteTimeElapsed

		popad
		push ecx
		fld [ebp - 0x760]
		jmp retnAddr
	}
}

void PatchScriptRunnerPerformanceCounter(void)
{
	// ScriptRunnerExecuteProlog QueryPerformanceCounter
	SafeWrite16(0x5E0D6D, 0x9090);

	// ScriptRunnerExecuteEpilog QueryPerformanceCounter
	SafeWrite16(0x5E10AE, 0x9090);

	WriteRelJump(0x5E10E0, UInt32(ScriptRunnerExecuteTimeElapsedHook));
}

void PatchExecuteScriptsMainLoop(void)
{
	WriteRelCall(0x86E9FA, UInt32(MainLoopExecuteScriptsHook));
}

static bool			s_MainLoopExecuting = false;
static bool			scriptWasExecutedThisFrame = false;
void __stdcall DoScriptRunnerExecuteTimeElapsed(Script* ExecutingScript, float ElapsedTime)
{
	if (ElapsedTime < 0.0000000000001) return;
	if (GetCurrentThreadId() != (*g_osGlobals)->mainThreadID)
		return;

	if (g_SkipVanillaScripts && (ExecutingScript->modIndex == 0))
		return;
	else if (g_FilterModID && (ExecutingScript->modIndex != g_FilterModID))
		return;
	else if (g_FilterFormID && ExecutingScript->refID != g_FilterFormID)
		return;

	// the script runner won't record object scripts etc. if only checking the main loop
//	else if (!s_MainLoopExecuting)
//		return;

	_MESSAGE("%08X\t\t\t\t\t\t\t\t\t%.10f ms", ExecutingScript->refID, ElapsedTime * 1000);
	scriptWasExecutedThisFrame = true;
}


LARGE_INTEGER BufferA = { 0 }, BufferB = { 0 };
LARGE_INTEGER FreqBuffer = { 0 };

void __stdcall StartFrameExecutionTimer()
{
	if (FreqBuffer.HighPart == 0)
		QueryPerformanceFrequency(&FreqBuffer);

	QueryPerformanceCounter(&BufferA);
	s_MainLoopExecuting = true;
}

void EndFrameExecutionTimerAndLog()
{
	QueryPerformanceCounter(&BufferB);
	long double ElapsedTime = ((BufferB.QuadPart - BufferA.QuadPart) * 1000.0 / FreqBuffer.QuadPart);

	if (scriptWasExecutedThisFrame)
	{
		gLog.SetAutoFlush(true);
		_MESSAGE("================================================================================== %.010f ms", ElapsedTime);
		gLog.SetAutoFlush(false);
	}

	s_MainLoopExecuting = false;
	scriptWasExecutedThisFrame = false;
}

void __fastcall MainLoopExecuteScriptsHook(void* processManager)
{
	StartFrameExecutionTimer();
	ThisStdCall(0x978550, processManager);
	EndFrameExecutionTimerAndLog();
}
