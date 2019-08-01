#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "Configuration.h"
#include "internals.h"

HMODULE rustHandle;
IDebugLog  gLog("RuntimeScriptProfiler.log");

void HandleIniOptions() {
	char filename[MAX_PATH];
	GetModuleFileNameA(rustHandle, filename, MAX_PATH);
	strcpy((char*)(strrchr(filename, '\\') + 1), "RuntimeScriptProfiler.ini");
	g_FilterFormID = GetPrivateProfileIntA("Main", "iFilterFormID", 0, filename);
	g_FilterModID = GetPrivateProfileIntA("Main", "iFilterModID ", 0, filename);
	g_SkipVanillaScripts = GetPrivateProfileIntA("Main", "bSkipVanillaScripts", 0, filename);
}

extern "C" {

	BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
		if (dwReason == DLL_PROCESS_ATTACH)
			rustHandle = (HMODULE)hDllHandle;
		return TRUE;
	}

	bool NVSEPlugin_Query(const NVSEInterface *nvse, PluginInfo *info) {
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "RuntimeProfiler";
		info->version = 1;
		return !(nvse->nvseVersion < NVSE_VERSION_INTEGER || nvse->isEditor);
	}

	bool NVSEPlugin_Load(const NVSEInterface *nvse) {
		_MESSAGE("RuntimeScriptProfiler Initializing...");
		gLog.Indent();

		gLog.SetAutoFlush(false);
		
		gLog.Outdent();
		
		_MESSAGE("RuntimeScriptProfiler Initialized!\n\n");

		_MESSAGE("Script\t\t\t\t\t\t\t\t\t\tElapsed Time\n");
		HandleIniOptions();
		PatchScriptRunnerPerformanceCounter();
		PatchExecuteScriptsMainLoop();
		return true;
	}
};