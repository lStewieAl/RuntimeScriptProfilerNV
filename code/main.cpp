#include "nvse/nvse/PluginAPI.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/SafeWrite.h"
#include "internals.h"
#include "Configuration.h"

HMODULE rustHandle;
IDebugLog  gLog("RuntimeScriptProfiler.log");

int  g_FilterFormID;
int  g_FilterModID;
int  g_SkipVanillaScripts;
bool isModEnabled;
int  g_iToggleExecutionHotkey;
bool bShowToggleMessage;

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