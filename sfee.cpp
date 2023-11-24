#include "sfse/PluginAPI.h"
#include "sfse_common/sfse_version.h"
#include "sfse_common/SafeWrite.h"


#include "sfse/GameMenu.h"
#include "sfse/GameData.h"
#include "sfse/ScaleformManager.h"
#include "sfse/GameStreams.h"
#include "sfse/GameSettings.h"

#include "ScaleformChargen.h"

#include "PluginInterface.h"
#include "PresetInterface.h"
#include "DataInterface.h"
#include "FileUtils.h"

#include "SimpleIni.h"

PluginHandle g_pluginHandle = kPluginHandle_Invalid;

SFSEMessagingInterface* g_messagingInterface = nullptr;
SFSEMenuInterface* g_menuInterface = nullptr;

InterfaceMap g_interfaceMap;
PresetInterface g_presetInterface;
DataInterface g_dataInterface;

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

#ifdef _DEBUG // TODO: Read load order from save file to determine Load-Order adjustment
		g_dataInterface.LoadSliderMods();
#endif
		break;
	}
	}
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
	{ RUNTIME_VERSION_1_8_86, 0 },

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

		g_messagingInterface->RegisterListener(g_pluginHandle, "SFSE", SFSEMessageHandler);
		g_menuInterface->RegisterMenuMovieCreated(OnMenuMovieCreated);

		if (g_menuInterface->interfaceVersion >= SFSEMenuInterface::kInterfaceVersion)
		{
			g_menuInterface->RegisterScaleformManagerCreated(OnScaleformManagerCreated);
		}

		g_interfaceMap.AddInterface("Preset", &g_presetInterface);
#ifdef _DEBUG // TODO: Read load order from save file to determine Load-Order adjustment
		g_interfaceMap.AddInterface("Data", &g_dataInterface);
#endif

		CSimpleIniW ini;
		SI_Error rc = ini.LoadFile(std::string(FileUtils::GetExecutablePath() + "/Data/SFSE/Plugins/sfee.ini").c_str());
		if (rc == SI_OK)
		{
			bool normalize = ini.GetBoolValue(L"Patches", L"bNormalizeBlendShapes", true);
			if (!normalize) {
				RelocAddr<uintptr_t> targetAddress(0x00231F0FC + 0x158); // 1.8.86
				safeWrite8(targetAddress.getUIntPtr(), 0xEB); // Write unconditional jmp instead of jbe
			}

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

		return true;
	}
	return false;
}

};
