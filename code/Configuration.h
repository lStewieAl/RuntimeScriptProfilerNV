#pragma once
extern HMODULE rustHandle;

extern int  g_FilterFormID;
extern int  g_FilterModID;
extern int  g_SkipVanillaScripts;
extern bool	isModEnabled;
extern int  g_iToggleExecutionHotkey;
extern bool bShowToggleMessage;

static void HandleIniOptions() {
	char iniPath[MAX_PATH];
	GetModuleFileNameA(rustHandle, iniPath, MAX_PATH);
	strcpy((char*)(strrchr(iniPath, '\\') + 1), "RuntimeScriptProfiler.ini");
	g_FilterFormID = GetPrivateProfileIntA("Main", "iFilterFormID", 0, iniPath);
	g_FilterModID = GetPrivateProfileIntA("Main", "iFilterModID ", 0, iniPath);
	g_SkipVanillaScripts = GetPrivateProfileIntA("Main", "bSkipVanillaScripts", 0, iniPath);
	g_iToggleExecutionHotkey = GetPrivateProfileIntA("Main", "iToggleExecutionHotkey", 0, iniPath);
	isModEnabled = GetPrivateProfileIntA("Main", "bStartGameEnabled", 1, iniPath);
	bShowToggleMessage = GetPrivateProfileIntA("Main", "bShowToggleMessage", 0, iniPath);
}
