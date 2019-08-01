#include "nvse/Commands_String.h"

#include "GameAPI.h"
#include "GameRTTI.h"
#include "StringVar.h"
#include "GameData.h"
#include "GameObjects.h"
#include "GameSettings.h"
#include "Utilities.h"

//////////////////////////
// Utility commands
//////////////////////////

// Commands that assign to a string_var should ALWAYS assign a value and return a string ID
// unless the lefthand variable cannot be extracted

bool Cmd_sv_Construct_Execute(COMMAND_ARGS)
{
	char buffer[kMaxMessageLength] = {0};

	//not checking the return value here 'cuz we need to assign to the string regardless
	ExtractFormatStringArgs(0, buffer, PASS_FMTSTR_ARGS, kCommandInfo_sv_Construct.numParams);
	AssignToStringVar(PASS_COMMAND_ARGS, buffer);

	return true;
}

bool Cmd_sv_Set_Execute(COMMAND_ARGS)
{
	char buffer[kMaxMessageLength] = { 0 };
	UInt32 stringID = 0;
	if (ExtractFormatStringArgs(0, buffer, PASS_FMTSTR_ARGS, kCommandInfo_sv_Set.numParams, &stringID))
	{
		StringVar* var = g_StringMap.Get(stringID);
		if (var)
		{
			var->Set(buffer);
		}
	}

	return true;
}

bool Cmd_sv_Destruct_Execute(COMMAND_ARGS)
{
    // as of v0017 beta 2, sv_Destruct has 2 different usages:
    //        'set var to sv_Destruct' -> destroys string contained in 'var'
    //        'sv_Destruct var1 [var2 ... var10]' -> destroys var1, var2, ... var10
    UInt16 numArgs = 0;
    UInt8* dataStart = (UInt8*)scriptData;
    if (*dataStart == 0x58 || *dataStart == 0x72) // !!! Only if inside a set statement !!!
    {
        *result = 0;            //store zero in destructed string_var
        double strID = 0;
        UInt8 modIndex = 0;

        if (ExtractSetStatementVar(scriptObj, eventList, scriptData, &strID, &modIndex, true))
            g_StringMap.Delete(strID);

        return true;
    }
	// alternate syntax: 'sv_Destruct var1 var2 .. var10
	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	if (!eval.ExtractArgs())
		return true;

	for (UInt32 i = 0; i < eval.NumArgs(); i++)
	{
		if (eval.Arg(i)->CanConvertTo(kTokenType_StringVar))
		{
			ScriptEventList::Var* var = eval.Arg(i)->GetVar();
			if (var)
			{
				g_StringMap.Delete(var->data);
				var->data = 0;
			}
		}
	}

	return true;
}

bool Cmd_sv_SubString_Execute(COMMAND_ARGS)
{
	UInt32 rhStrID = 0;
	UInt32 startPos = 0;
	UInt32 howMany = -1;
	std::string subStr;

	if (ExtractArgs(EXTRACT_ARGS, &rhStrID, &startPos, &howMany))
	{
		StringVar* rhVar = g_StringMap.Get(rhStrID);
		if (!rhVar)
			return true;

		if (howMany == -1)
			howMany = rhVar->GetLength() - startPos;

		subStr = rhVar->SubString(startPos, howMany);
	}

	AssignToStringVar(PASS_COMMAND_ARGS, subStr.c_str());
	return true;
}

bool Cmd_sv_Compare_Execute(COMMAND_ARGS)
{
	*result = -2;			//sentinel value if comparison fails
	UInt32 stringID = 0;
	char buffer[kMaxMessageLength] = { 0 };
	UInt32 bCaseSensitive = 0;

	if (!ExtractFormatStringArgs(0, buffer, PASS_FMTSTR_ARGS, kCommandInfo_sv_Compare.numParams, &stringID, &bCaseSensitive))
		return true;

	StringVar* lhs = g_StringMap.Get(stringID);
	if (!lhs)
		return true;

	*result = lhs->Compare(buffer, bCaseSensitive ? true : false);

	return true;
}

bool Cmd_sv_Length_Execute(COMMAND_ARGS)
{
	*result = -1;			// sentinel value if extraction fails
	UInt32 strID = 0;
	if (ExtractArgs(EXTRACT_ARGS, &strID))
	{
		StringVar* str = g_StringMap.Get(strID);
		if (str)
			*result = str->GetLength();
	}

	return true;
}

bool Cmd_sv_Erase_Execute(COMMAND_ARGS)
{
	*result = 0;
	UInt32 strID = 0;
	UInt32 startPos = 0;
	UInt32 howMany = -1;

	if (!ExtractArgs(EXTRACT_ARGS, &strID, &startPos, &howMany))
		return true;

	StringVar* strVar = g_StringMap.Get(strID);
	if (strVar)
	{
		if (howMany == -1)
			howMany = strVar->GetLength() - startPos;

		strVar->Erase(startPos, howMany);
	}

	return true;
}

enum {
	eMode_svFind,
	eMode_svCount,
	eMode_svReplace,
};

bool StringVar_Find_Execute(COMMAND_ARGS, UInt32 mode, CommandInfo* commandInfo)
{
	*result = -1;
	UInt32 strID = 0;
	UInt32 startPos = 0;
	UInt32 numChars = -1;
	UInt32 bCaseSensitive = 0;
	UInt32 numToReplace = -1;			//replace all by default
	char toFind[kMaxMessageLength] = { 0 };

	UInt32 intResult = -1;

	if (!ExtractFormatStringArgs(0, toFind, PASS_FMTSTR_ARGS, commandInfo->numParams, &strID, &startPos, &numChars, &bCaseSensitive, &numToReplace))
		return true;

	StringVar* strVar = g_StringMap.Get(strID);
	if (strVar)
	{
		if (numChars == -1)
			numChars = strVar->GetLength() - startPos;

		switch (mode)
		{
		case eMode_svFind:
			intResult = strVar->Find(toFind, startPos, numChars, bCaseSensitive ? true : false);
			break;
		case eMode_svCount:
			intResult = strVar->Count(toFind, startPos, numChars, bCaseSensitive ? true : false);
			break;
		case eMode_svReplace:
			{
				std::string str(toFind);
				UInt32 splitPoint = str.find(GetSeparatorChar(scriptObj));
				if (splitPoint != -1 && splitPoint < str.length())
				{
					toFind[splitPoint] = '\0';
					char* replaceWith = (splitPoint == str.length() - 1) ? "" : toFind + splitPoint + 1;
					intResult = strVar->Replace(toFind, replaceWith, startPos, numChars, bCaseSensitive ? true : false, numToReplace);
				}
				break;
			}
		}
	}

	if (intResult != -1)
		*result = intResult;

	return true;
}

bool Cmd_sv_Find_Execute(COMMAND_ARGS)
{
	StringVar_Find_Execute(PASS_COMMAND_ARGS, eMode_svFind, &kCommandInfo_sv_Find);
	return true;
}

bool Cmd_sv_Count_Execute(COMMAND_ARGS)
{
	StringVar_Find_Execute(PASS_COMMAND_ARGS, eMode_svCount, &kCommandInfo_sv_Count);
	return true;
}

bool Cmd_sv_Replace_Execute(COMMAND_ARGS)
{
	StringVar_Find_Execute(PASS_COMMAND_ARGS, eMode_svReplace, &kCommandInfo_sv_Replace);
	return true;
}

bool Cmd_sv_ToNumeric_Execute(COMMAND_ARGS)
{
	UInt32 strID = 0;
	UInt32 startPos = 0;
	*result = 0;

	if (!ExtractArgs(EXTRACT_ARGS, &strID, &startPos))
		return true;

	StringVar* strVar = g_StringMap.Get(strID);
	if (strVar)
	{
		const char* cStr = strVar->GetCString();
		*result = strtod(cStr + startPos, NULL);
	}

	return true;
}

bool Cmd_sv_Insert_Execute(COMMAND_ARGS)
{
	UInt32 strID = 0;
	UInt32 insertionPos = 0;
	char subString[kMaxMessageLength] = { 0 };
	*result = 0;

	if (!ExtractFormatStringArgs(0, subString, PASS_FMTSTR_ARGS, kCommandInfo_sv_Insert.numParams, &strID, &insertionPos))
		return true;

	StringVar* lhs = g_StringMap.Get(strID);
	if (lhs)
		lhs->Insert(subString, insertionPos);

	return true;
}

bool Cmd_sv_GetChar_Execute(COMMAND_ARGS)
{
	UInt32 strID = 0;
	UInt32 charPos = 0;
	*result = -1;			// error return value

	if (!ExtractArgs(EXTRACT_ARGS, &strID, &charPos))
		return true;

	StringVar* strVar = g_StringMap.Get(strID);
	if (strVar)
		*result = strVar->At(charPos);

	return true;
}

bool MatchCharType_Execute(COMMAND_ARGS, UInt32 mask)
{
	UInt32 charCode = 0;
	*result = 0;

	if (ExtractArgs(EXTRACT_ARGS, &charCode))
		if ((StringVar::GetCharType(charCode) & mask) == mask)
			*result = 1;

	return true;
}

bool Cmd_IsLetter_Execute(COMMAND_ARGS)
{
	return MatchCharType_Execute(PASS_COMMAND_ARGS, kCharType_Alphabetic);
}

bool Cmd_IsDigit_Execute(COMMAND_ARGS)
{
	return MatchCharType_Execute(PASS_COMMAND_ARGS, kCharType_Digit);
}

bool Cmd_IsPunctuation_Execute(COMMAND_ARGS)
{
	return MatchCharType_Execute(PASS_COMMAND_ARGS, kCharType_Punctuation);
}

bool Cmd_IsPrintable_Execute(COMMAND_ARGS)
{
	return MatchCharType_Execute(PASS_COMMAND_ARGS, kCharType_Printable);
}

bool Cmd_IsUpperCase_Execute(COMMAND_ARGS)
{
	return MatchCharType_Execute(PASS_COMMAND_ARGS, kCharType_Uppercase);
}

bool Cmd_ToUpper_Execute(COMMAND_ARGS)
{
	UInt32 character = 0;
	*result = 0;
	if (ExtractArgs(EXTRACT_ARGS, &character))
		*result = toupper(character);

	return true;
}

bool Cmd_ToLower_Execute(COMMAND_ARGS)
{
	UInt32 character = 0;
	*result = 0;
	if (ExtractArgs(EXTRACT_ARGS, &character))
		*result = tolower(character);

	return true;
}

bool Cmd_CharToAscii_Execute(COMMAND_ARGS)
{
	//converts a single char to ASCII
	*result = -1;
	char buffer[512];		//user shouldn't pass string of more than one char but someone will anyway

	if (ExtractArgs(EXTRACT_ARGS, &buffer))
		if (strlen(buffer) == 1)
			*result = *buffer;

	return true;
}

/////////////////////////////////////
// Getters/Setters
/////////////////////////////////////

bool Cmd_GetNthModName_Execute(COMMAND_ARGS)
{
	UInt32 modIdx = 0xFF;
	const char* modName = "";

	if (ExtractArgs(EXTRACT_ARGS, &modIdx))
		modName = DataHandler::Get()->GetNthModName(modIdx);

	AssignToStringVar(PASS_COMMAND_ARGS, modName);

	return true;
}

// This works for any TESFullName-derived form as well as references
bool Cmd_GetName_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	const char* name = "";

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		if (!form)
			if (thisObj)
				form = thisObj;

		if (form)
			name = GetFullName(form);
	}

	AssignToStringVar(PASS_COMMAND_ARGS, name);
	return true;
}

bool Cmd_GetStringGameSetting_Execute(COMMAND_ARGS)
{
	char settingName[0x100] = { 0 };
	const char* settingString = "";

	if (ExtractArgs(EXTRACT_ARGS, &settingName))
	{
		Setting* setting = NULL;
		GameSettingCollection* gmsts = GameSettingCollection::GetSingleton();
		if (gmsts && gmsts->GetGameSetting(settingName, &setting) && setting && setting->GetType() == Setting::kSetting_String)
			setting->Get(settingString);
	}

	AssignToStringVar(PASS_COMMAND_ARGS, settingString);

	return true;
}

bool Cmd_GetStringIniSetting_Execute(COMMAND_ARGS)
{
	char settingName[kMaxMessageLength] = { 0 };
	*result = -1;

	if (ExtractArgs(EXTRACT_ARGS, &settingName))
	{
		Setting* setting;
		if (GetIniSetting(settingName, &setting))
		{
			char val[kMaxMessageLength] = { 0 };
			if (const char * pVal = setting->Get())
			{
				strcpy_s(val, kMaxMessageLength, pVal);
				AssignToStringVar(PASS_COMMAND_ARGS, val);
				if (IsConsoleMode())
					Console_Print("GetStringIniSetting >> %s", val);
			}
		}
		else if (IsConsoleMode())
			Console_Print("GetStringIniSetting >> SETTING NOT FOUND");
	}

	return true;
}

// setting name included in format string i.e. "sSomeSetting|newSettingValue"
bool Cmd_SetStringGameSettingEX_Execute(COMMAND_ARGS)
{
	char fmtString[kMaxMessageLength] = { 0 };
	*result = 0;

	if (ExtractFormatStringArgs(0, fmtString, PASS_FMTSTR_ARGS, kCommandInfo_SetStringGameSettingEX.numParams))
	{
		UInt32 pipePos = std::string(fmtString).find(GetSeparatorChar(scriptObj));
		if (pipePos != -1)
		{
			fmtString[pipePos] = 0;
			char* newValue = fmtString + pipePos + 1;

			Setting* setting = NULL;
			if (GetIniSetting(fmtString, &setting))
			{
				setting->Set(newValue);;
				*result = 1;
			}
		}
	}

	return true;
}

// setting name included in format string i.e. "sSomeSetting|newSettingValue"
bool Cmd_SetStringIniSetting_Execute(COMMAND_ARGS)
{
	char fmtString[kMaxMessageLength] = { 0 };
	*result = 0;

	if (ExtractFormatStringArgs(0, fmtString, PASS_FMTSTR_ARGS, kCommandInfo_SetStringGameSettingEX.numParams))
	{
		UInt32 pipePos = std::string(fmtString).find(GetSeparatorChar(scriptObj));
		if (pipePos != -1)
		{
			fmtString[pipePos] = 0;
			char* newValue = fmtString + pipePos + 1;

			Setting* setting = NULL;
			GameSettingCollection* gmsts = GameSettingCollection::GetSingleton();
			if (gmsts && gmsts->GetGameSetting(fmtString, &setting) && setting && setting->GetType() == Setting::kSetting_String)
			{
				setting->Set(newValue);;
				*result = 1;
			}
		}
	}

	return true;
}

bool Cmd_GetModelPath_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	const char* pathStr = "";
	*result = 0;

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		if (form)
			form = form->TryGetREFRParent();
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESModel* model = DYNAMIC_CAST(form, TESForm, TESModel);
		if (model)
			pathStr = model->nifPath.m_data;
	}

	AssignToStringVar(PASS_COMMAND_ARGS, pathStr);
	return true;
}

bool Cmd_SetModelPathEX_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	char newPath[kMaxMessageLength] = { 0 };

	if (ExtractFormatStringArgs(0, newPath, PASS_FMTSTR_ARGS, kCommandInfo_SetModelPathEX.numParams, &form))
	{
		if (form)
			form = form->TryGetREFRParent();
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESModel* model = DYNAMIC_CAST(form, TESForm, TESModel);
		if (model)
			model->SetPath(newPath);
	}

	return true;
}

enum {
	kPath_Icon,
	kPath_Texture
};

bool GetPath_Execute(COMMAND_ARGS, UInt32 whichPath)
{
	TESForm* form = NULL;
	const char* pathStr = "";

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		if (form)
			form = form->TryGetREFRParent();
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;
		switch (whichPath) {
			case kPath_Icon:
				{
					TESIcon* icon = DYNAMIC_CAST(form, TESForm, TESIcon);
					if (icon)
						pathStr = icon->ddsPath.m_data;
				}
				break;
			case kPath_Texture:
				{
					TESTexture* tex = DYNAMIC_CAST(form, TESForm, TESTexture);
					if (tex)
						pathStr = tex->ddsPath.m_data;
				}
				break;
		}
	}

	AssignToStringVarLong(PASS_COMMAND_ARGS, pathStr);
	return true;
}

bool Cmd_GetIconPath_Execute(COMMAND_ARGS)
{
	// not all objects with icons can be cast to TESIcon (e.g. skills, classes, TESObjectMISC)
	// so GetTexturePath is preferred over GetIconPath
	return GetPath_Execute(PASS_COMMAND_ARGS, kPath_Icon);
}

bool Cmd_GetTexturePath_Execute(COMMAND_ARGS)
{
	return GetPath_Execute(PASS_COMMAND_ARGS, kPath_Texture);
}

bool Cmd_SetIconPathEX_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	char newPath[kMaxMessageLength] = { 0 };

	if (ExtractFormatStringArgs(0, newPath, PASS_FMTSTR_ARGS, kCommandInfo_SetIconPathEX.numParams, &form))
	{
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESIcon* icon = DYNAMIC_CAST(form, TESForm, TESIcon);
		if (icon)
			icon->SetPath(newPath);
	}

	return true;
}

bool Cmd_SetTexturePath_Execute(COMMAND_ARGS)
{
	*result = 0;
	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	if (eval.ExtractArgs() && eval.NumArgs() > 0) {
		TESForm* form = NULL;
		if (eval.NumArgs() == 2) {
			form = eval.Arg(1)->GetTESForm();
		}
		else if (thisObj) {
			form = thisObj->baseForm;
		}

		TESTexture* tex = DYNAMIC_CAST(form, TESForm, TESTexture);
		if (tex) {
			const char* nuPath = eval.Arg(0)->GetString();
			if (nuPath) {
				tex->ddsPath.Set(nuPath);
				*result = 1;
			}
		}
	}

	return true;
}

static enum {
	eMode_Get,
	eMode_Set
};

bool BipedPathFunc_Execute(COMMAND_ARGS, UInt32 mode, bool bIcon)
{
	UInt32 whichPath = 0;
	TESForm* form = NULL;
	const char* pathStr = "";
	char newPath[kMaxMessageLength] = { 0 };

	bool bExtracted = false;
	if (mode == eMode_Set)
		bExtracted = ExtractFormatStringArgs(0, newPath, PASS_FMTSTR_ARGS, kCommandInfo_SetBipedModelPathEX.numParams, &whichPath, &form);
	else
		bExtracted = ExtractArgsEx(EXTRACT_ARGS_EX, &whichPath, &form);

	if (bExtracted)
	{
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESBipedModelForm* bipedModel = DYNAMIC_CAST(form, TESForm, TESBipedModelForm);
		if (bipedModel)
		{
			bool bFemale = (whichPath % 2) ? true : false;
			whichPath = bIcon ? bipedModel->ePath_Icon : whichPath / 2;

			if (mode == eMode_Set)
				bipedModel->SetPath(newPath, whichPath, bFemale);
			else
				pathStr = bipedModel->GetPath(whichPath, bFemale);
		}
	}

	if (mode == eMode_Get)
		AssignToStringVarLong(PASS_COMMAND_ARGS, pathStr);

	return true;
}

bool Cmd_GetBipedModelPath_Execute(COMMAND_ARGS)
{
	return BipedPathFunc_Execute(PASS_COMMAND_ARGS, eMode_Get, false);
}

bool Cmd_SetBipedModelPathEX_Execute(COMMAND_ARGS)
{
return BipedPathFunc_Execute(PASS_COMMAND_ARGS, eMode_Set, false);
}

bool Cmd_GetBipedIconPath_Execute(COMMAND_ARGS)
{
	return BipedPathFunc_Execute(PASS_COMMAND_ARGS, eMode_Get, true);
}

bool Cmd_SetBipedIconPathEX_Execute(COMMAND_ARGS)
{
	return BipedPathFunc_Execute(PASS_COMMAND_ARGS, eMode_Set, true);
}


bool Cmd_GetNthFactionRankName_Execute(COMMAND_ARGS)
{
	TESFaction* faction = NULL;
	UInt32 rank = 0;
	UInt32 gender = 0;
	const char* rankName = "";

	if (ExtractArgs(EXTRACT_ARGS, &faction, &rank, &gender))
		rankName = faction->GetNthRankName(rank, (gender ? true : false));

	AssignToStringVar(PASS_COMMAND_ARGS, rankName);
	return true;
}

bool Cmd_SetNthFactionRankNameEX_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	UInt32 rank = 0;
	UInt32 gender = 0;
	char newName[kMaxMessageLength] = { 0 };

	if (ExtractFormatStringArgs(0, newName, PASS_FMTSTR_ARGS, kCommandInfo_SetNthFactionRankNameEX.numParams, &form, &rank, &gender))
	{
		TESFaction* faction = DYNAMIC_CAST(form, TESForm, TESFaction);
		if (faction)
			faction->SetNthRankName(newName, rank, gender ? true : false);
	}

	return true;
}

#if 0

bool Cmd_GetNthEffectItemScriptName_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	UInt32 whichEffect = 0;
	const char* effectName = "";

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form, &whichEffect))
	{
		EffectItemList* effectList = GetEffectList(form);
		if (effectList)
		{
			EffectItem* effectItem = effectList->ItemAt(whichEffect);
			if (effectItem && effectItem->scriptEffectInfo)
				effectName = effectItem->scriptEffectInfo->effectName.m_data;
		}
	}

	AssignToStringVar(PASS_COMMAND_ARGS, effectName);
	return true;
}

bool Cmd_SetNthEffectItemScriptNameEX_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	UInt32 whichEffect = 0;
	char newName[kMaxMessageLength] = { 0 };

	if (ExtractFormatStringArgs(0, newName, PASS_FMTSTR_ARGS, kCommandInfo_SetNthEffectItemScriptNameEX.numParams, &form, &whichEffect))
	{
		EffectItemList* effectList = GetEffectList(form);
		if (effectList)
		{
			EffectItem* effectItem = effectList->ItemAt(whichEffect);
			if (effectItem && effectItem->scriptEffectInfo)
				effectItem->scriptEffectInfo->effectName.Set(newName);
		}
	}

	return true;
}

#endif

bool Cmd_ActorValueToString_Execute(COMMAND_ARGS)
{
	UInt32 av = eActorVal_FalloutMax;
	std::string avStr = "";

	if (ExtractArgs(EXTRACT_ARGS, &av) && av < eActorVal_FalloutMax)
		avStr = GetActorValueString(av);

	AssignToStringVar(PASS_COMMAND_ARGS, avStr.c_str());
	return true;
}

bool Cmd_ActorValueToStringC_Execute(COMMAND_ARGS)
{
	UInt32 av = eActorVal_FalloutMax;
	std::string avStr = "";

	if (ExtractArgs(EXTRACT_ARGS, &av) && av < eActorVal_FalloutMax)
		avStr = GetActorValueString(av);

	AssignToStringVar(PASS_COMMAND_ARGS, avStr.c_str());
	return true;
}

bool Cmd_GetKeyName_Execute(COMMAND_ARGS)
{
	const char* keyname = "";
	UInt32 keycode = 0;
	if (ExtractArgs(EXTRACT_ARGS, &keycode))
		keyname = GetDXDescription(keycode);

	AssignToStringVar(PASS_COMMAND_ARGS, keyname);
	return true;
}

bool Cmd_AsciiToChar_Execute(COMMAND_ARGS)
{
	char charStr[2] = "\0";
	UInt32 asciiCode = 0;
	if (ExtractArgs(EXTRACT_ARGS, &asciiCode) && asciiCode < 256)
		charStr[0] = asciiCode;

	AssignToStringVar(PASS_COMMAND_ARGS, charStr);
	return true;
}

bool Cmd_GetFormIDString_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	ExtractArgsEx(EXTRACT_ARGS_EX, &form);
	if (!form)
		form = thisObj;

	UInt32 formID = form ? form->refID : 0;
	char str[0x20] = { 0 };
	sprintf_s(str, sizeof(str), "%08X", formID);
	AssignToStringVar(PASS_COMMAND_ARGS, str);
	return true;
}

bool Cmd_GetRawFormIDString_Execute(COMMAND_ARGS)
{
	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	char str[0x20] = { 0 };
	UInt32 formID = 0;

	if (eval.ExtractArgs() && eval.NumArgs() == 1) {
		ScriptToken* arg = eval.Arg(0);
		if (arg->CanConvertTo(kTokenType_ArrayElement)) {
			formID = arg->GetFormID();
		}
		else if (arg->Type() == kTokenType_RefVar) {
			ScriptEventList::Var* var = arg->GetVar();
			if (var) {
				formID = *((UInt32*)(&var->data));
			}
		}
	}

	sprintf_s(str, sizeof(str), "%08X", formID);
	AssignToStringVar(PASS_COMMAND_ARGS, str);
	DEBUG_PRINT(str);
	return true;
}

bool Cmd_NumToHex_Execute(COMMAND_ARGS)
{
	UInt32 num = 0;
	UInt32 width = 8;
	ExtractArgs(EXTRACT_ARGS, &num, &width);

	char fmtStr[0x20];
	width = width <= 8 ? width : 8;
	sprintf_s(fmtStr, sizeof(fmtStr), "%%0%dX", width);

	char hexStr[0x20];
	sprintf_s(hexStr, sizeof(hexStr), fmtStr, num);

	AssignToStringVar(PASS_COMMAND_ARGS, hexStr);
	return true;
}

bool Cmd_ToNumber_Execute(COMMAND_ARGS)
{
	// usage: ToNumber string bool:fromHex
	*result = 0;
	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	if (eval.ExtractArgs() && eval.NumArgs() > 0)
	{
		bool bHex = false;
		if (eval.Arg(1) && eval.Arg(1)->CanConvertTo(kTokenType_Number))
			bHex = eval.Arg(1)->GetNumber() ? true : false;

		*result = eval.Arg(0)->GetNumericRepresentation(bHex);
	}

	return true;
}

bool Cmd_sv_Split_Execute(COMMAND_ARGS)
{
	// args: string delims
	ArrayID arrID = g_ArrayMap.Create(kDataType_Numeric, true, scriptObj->GetModIndex());
	*result = arrID;

	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	if (eval.ExtractArgs() && eval.NumArgs() == 2 && eval.Arg(0)->CanConvertTo(kTokenType_String) && eval.Arg(1)->CanConvertTo(kTokenType_String))
	{
		Tokenizer tokens(eval.Arg(0)->GetString(), eval.Arg(1)->GetString());
		std::string token;

		double idx = 0.0;
		while (tokens.NextToken(token) != -1)
		{
			g_ArrayMap.SetElementString(arrID, idx, token);
			idx += 1.0;
		}
	}

	return true;
}

bool Cmd_GetFalloutDirectory_Execute(COMMAND_ARGS)
{
	std::string path = GetFalloutDirectory();
	const char* pathStr = path.c_str();
	AssignToStringVar(PASS_COMMAND_ARGS, pathStr);
	return true;
}

bool Cmd_sv_Percentify_Execute(COMMAND_ARGS)
{
	std::string converted = "";
	ExpressionEvaluator eval(PASS_COMMAND_ARGS);
	if (eval.ExtractArgs() && eval.NumArgs() == 1) {
		const char* src = eval.Arg(0)->GetString();
		if (src) {
			converted = src;
			UInt32 pos = 0;
			while (pos < converted.length() && (pos = converted.find('%', pos)) != -1) {
				converted.insert(pos, 1, '%');
				pos += 2;
			}
		}
	}

	AssignToStringVar(PASS_COMMAND_ARGS, converted.c_str());
	return true;
}

bool ChangeCase_Execute (COMMAND_ARGS, bool bUpper)
{
	std::string converted = "";
	ExpressionEvaluator eval (PASS_COMMAND_ARGS);
	if (eval.ExtractArgs() && eval.NumArgs() == 1)
	{
		const char* src = eval.Arg (0)->GetString();
		if (src)
		{
			converted = src;
			if (bUpper)
				MakeUpper (converted);
			else
				MakeLower (converted);
		}
	}

	AssignToStringVar(PASS_COMMAND_ARGS, converted.c_str ());
	return true;
}

bool Cmd_sv_ToUpper_Execute (COMMAND_ARGS)
{
	return ChangeCase_Execute (PASS_COMMAND_ARGS, true);
}

bool Cmd_sv_ToLower_Execute (COMMAND_ARGS)
{
	return ChangeCase_Execute (PASS_COMMAND_ARGS, false);
}

bool Cmd_GetScopeModelPath_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	TESModel* model = NULL;
	const char* pathStr = "";
	*result = 0;

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &form))
	{
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (weapon && weapon->HasScope())
			model = &(weapon->targetNIF);

		if (model)
			pathStr = model->nifPath.m_data;
	}

	AssignToStringVar(PASS_COMMAND_ARGS, pathStr);

	return true;
}

bool Cmd_SetScopeModelPath_Execute(COMMAND_ARGS)
{
	TESForm* form = NULL;
	TESModel* model = NULL;
	char pathStr[512];
	*result = 0;

	if (ExtractArgsEx(EXTRACT_ARGS_EX, &pathStr, &form))
	{
		if (!form)
			if (thisObj)
				form = thisObj->baseForm;

		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (weapon && weapon->HasScope())
			model = &(weapon->targetNIF);

		if (model)
			model->nifPath.Set(pathStr);
	}

	return true;
}

