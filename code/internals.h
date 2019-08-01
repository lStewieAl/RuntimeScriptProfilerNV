#pragma once
#include "nvse/nvse/SafeWrite.h"
#include "nvse/nvse/GameScript.h"
#include "Configuration.h"

void PatchScriptRunnerPerformanceCounter(void);
void PatchExecuteScriptsMainLoop(void);