#include "Hooks_Animation.h"
#include "SafeWrite.h"
#include <cstdarg>
#include "Utilities.h"
#include "GameForms.h"

#if RUNTIME

#if RUNTIME_VERSION == RUNTIME_VERSION_1_4_0_525
#define kHookGetGlobalModelPath 0x0104A1B8
static const UInt32 kOriginalGetGlobalModelPath = 0x00601C30;
#elif RUNTIME_VERSION == RUNTIME_VERSION_1_4_0_525ng
#define kHookGetGlobalModelPath 0x0104A1B8
static const UInt32 kOriginalGetGlobalModelPath = 0x00601BF0;
#else
#error
#endif

static bool __stdcall doAnimationHook(TESModel* model)
{
	return (model && strrchr(model->nifPath.CStr(), '\\'));
}

static __declspec(naked) char* AnimationHook(void)
{
	_asm {
		pushad
		push ecx
		call doAnimationHook
		test eax,eax
		jz doCallOriginalGetGlobalModelPath
		popad
		mov eax, [ecx + 4]
		retn

doCallOriginalGetGlobalModelPath:
		popad
		jmp kOriginalGetGlobalModelPath
	}
}

void Hook_Animation_Init(void)
{
	UInt32	enableAnimationHook = 0;

	if(GetNVSEConfigOption_UInt32("Animation", "EnableAnimationHook", &enableAnimationHook) && enableAnimationHook)
	{
		SafeWrite32(kHookGetGlobalModelPath, (UInt32)AnimationHook);
	}
}

#endif
