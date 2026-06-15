#include "sfse/PluginAPI.h"
#include "sfse_common/sfse_version.h"
#include "sfse_common/SafeWrite.h"
#include "sfse_common/BranchTrampoline.h"

#include "sfse/GameMenu.h"
#include "sfse/GameData.h"
#include "sfse/ScaleformManager.h"
#include "sfse/GameStreams.h"
#include "sfse/GameSettings.h"

#include "ScaleformChargen.h"

#include "PluginInterface.h"
#include "PresetInterface.h"
#include "ChargenInterface.h"
#include "FileUtils.h"

#include "SimpleIni.h"


#include "xbyak/xbyak.h"

PluginHandle g_pluginHandle = kPluginHandle_Invalid;

SFSEMessagingInterface* g_messagingInterface = nullptr;
SFSEMenuInterface* g_menuInterface = nullptr;
SFSETrampolineInterface* g_trampolineInterface = nullptr;
SFSETaskInterface* g_taskInterface = nullptr;

InterfaceMap g_interfaceMap;
PresetInterface g_presetInterface;
ChargenInterface g_chargenInterface;

namespace Patches
{
	bool bNormalizeBlendShapes = true;
}

std::unordered_map<std::string, std::unordered_map<std::wstring, std::wstring>> g_translations;

void OnMenuMovieCreated(IMenu* menu)
{
	if (menu->MenuName == "ChargenMenu")
	{
		InstallChargenCallbacks(menu);
	}
}

void OnScaleformManagerCreated(BSScaleformManager* manager)
{
	AddTranslations(manager);
}

void SFSEMessageHandler(SFSEMessagingInterface::Message* message)
{
	switch (message->type)
	{
	case SFSEMessagingInterface::kMessage_PostDataLoad:
	{
		// If we dont have the BSScaleformManager hook, add it after PostDataLoad instead
		if (g_menuInterface->interfaceVersion < SFSEMenuInterface::kInterfaceVersion)
		{
			AddTranslations(BSScaleformManager::GetSingleton());
		}

		g_chargenInterface.LoadSliderMods();
		break;
	}
	}
}

/*class NiRefObject
{
public:
	virtual ~NiRefObject();
	virtual void Release();
};

class BSMorphTargetData : public NiRefObject
{
public:
	virtual ~BSMorphTargetData();

	u32	unk08;
	u32	unk0C;
	u32	unk10;
	u32	unk14;
	u32	unk18;
	u16	unk1C;
	void* unk20; // StreamingResource Ptr
	BSTArray<BSFixedStringCS> Morphs; // 28
	u32	numAxis;
	u32	numVertices;
	void* unk40;
};
static_assert(sizeof(BSMorphTargetData) == 0x48);

typedef BSMorphTargetData* (*_BSMorphTargetData_ctor)(BSMorphTargetData* __this, void* unk1, int16_t unk2);
RelocAddr <_BSMorphTargetData_ctor> BSMorphTargetData_ctor(0x034D3988);
_BSMorphTargetData_ctor BSMorphTargetData_ctor_Original = nullptr;



BSMorphTargetData* BSMorphTargetData_ctor_Hook(BSMorphTargetData* __this, void* unk1, int16_t unk2)
{
	BSMorphTargetData* ret = BSMorphTargetData_ctor_Original(__this, unk1, unk2);

	return ret;
}*/

bool RegisterHooks()
{
	/*if (g_trampolineInterface) {
		void* branch = g_trampolineInterface->AllocateFromBranchPool(g_pluginHandle, 128);
		if (!branch) {
			return false;
		}

		g_branchTrampoline.setBase(128, branch);

		void* local = g_trampolineInterface->AllocateFromLocalPool(g_pluginHandle, 128);
		if (!local) {
			return false;
		}

		g_localTrampoline.setBase(128, local);
	}
	else {
		if (!g_branchTrampoline.create(128)) {
			return false;
		}
		if (!g_localTrampoline.create(128, nullptr))
		{
			return false;
		}
	}*/

	if (!Patches::bNormalizeBlendShapes) {
		RelocAddr<uintptr_t> targetAddress(0x02BA466C); // 1.16.244
		safeWrite8(targetAddress.getUIntPtr(), 0xEB); // Write unconditional jmp instead of jbe
	}

	/*{
		struct BSMorphTargetData_ctor_Code : Xbyak::CodeGenerator {
			BSMorphTargetData_ctor_Code(void* buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label retnLabel;

				mov(ptr[rsp + 0x08], rcx);
				jmp(ptr[rip + retnLabel]);

				L(retnLabel);
				dq(BSMorphTargetData_ctor.getUIntPtr() + 5);
			}
		};

		void* codeBuf = g_localTrampoline.startAlloc();
		BSMorphTargetData_ctor_Code code(codeBuf);
		g_localTrampoline.endAlloc(code.getCurr());
		BSMorphTargetData_ctor_Original = (_BSMorphTargetData_ctor)codeBuf;
		g_branchTrampoline.write5Branch(BSMorphTargetData_ctor.getUIntPtr(), (uintptr_t)BSMorphTargetData_ctor_Hook);
	}*/

	return true;
}

extern "C" {
__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version =
{
	SFSEPluginVersionData::kVersion,
	
	1,
	"Starfield Engine Extender",
	"Expired6978",

	0,	// not address independent
	0,	// not structure independent
	{ RUNTIME_VERSION_1_16_244, 0 },

	0,	// works with any version of the script extender. you probably do not need to put anything here
	0, 0,	// set these reserved fields to 0
};

__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse)
{
	if (sfse)
	{
		g_pluginHandle = sfse->GetPluginHandle();
		g_messagingInterface = static_cast<SFSEMessagingInterface*>(sfse->QueryInterface(kInterface_Messaging));
		if (!g_messagingInterface)
		{
			return false;
		}

		g_menuInterface = static_cast<SFSEMenuInterface*>(sfse->QueryInterface(kInterface_Menu));
		if (!g_menuInterface)
		{
			return false;
		}

		g_trampolineInterface = static_cast<SFSETrampolineInterface*>(sfse->QueryInterface(kInterface_Trampoline));
		if (!g_trampolineInterface)
		{
			return false;
		}

		g_taskInterface = static_cast<SFSETaskInterface*>(sfse->QueryInterface(kInterface_Task));
		if (!g_taskInterface)
		{
			return false;
		}

		g_messagingInterface->RegisterListener(g_pluginHandle, "SFSE", SFSEMessageHandler);
		g_menuInterface->RegisterMenuMovieCreated(OnMenuMovieCreated);

		if (g_menuInterface->interfaceVersion >= SFSEMenuInterface::kInterfaceVersion)
		{
			g_menuInterface->RegisterScaleformManagerCreated(OnScaleformManagerCreated);
		}

		g_interfaceMap.AddInterface("Preset", &g_presetInterface);
		g_interfaceMap.AddInterface("Chargen", &g_chargenInterface);

		CSimpleIniW ini;
		SI_Error rc = ini.LoadFile(std::string(FileUtils::GetExecutablePath() + "/Data/SFSE/Plugins/sfee.ini").c_str());
		if (rc == SI_OK)
		{
			Patches::bNormalizeBlendShapes = ini.GetBoolValue(L"Patches", L"bNormalizeBlendShapes", Patches::bNormalizeBlendShapes);
			

			g_presetInterface.SetLocalSuffix(ini.GetValue(L"Presets", L"sLocalDirectorySuffix", L"SFSE\\Plugins\\Chargen\\Presets"));
			g_presetInterface.SetModSuffix(ini.GetValue(L"Presets", L"sModDirectorySuffix", L"SFSE\\Plugins\\Chargen\\Presets"));

			static std::string languages[] = {"en","de","es","fr","it","ja","pl","ptbr","zhhans"};
			static std::wstring wlanguages[] = {L"en",L"de",L"es",L"fr",L"it",L"ja",L"pl",L"ptbr",L"zhhans"};

			for (size_t i = 0; i < sizeof(languages) / sizeof(std::string); ++i)
			{
				auto translationKeys = ini.GetSection((std::wstring(L"Language_") + wlanguages[i]).c_str());
				if (translationKeys)
				{
					for (auto& item : *translationKeys)
					{
						g_translations[languages[i]][item.first.pItem] = item.second;
					}
				}
			}
		}

		return RegisterHooks();
	}
	return false;
}

};
