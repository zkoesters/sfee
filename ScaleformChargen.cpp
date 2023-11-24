#include "ScaleformChargen.h"

#include "sfse/GameMenu.h"
#include "sfse/GameReferences.h"
#include "sfse/GameChargen.h"
#include "sfse/GameSettings.h"

#include "sfse/ScaleformTypes.h"
#include "sfse/ScaleformFunctions.h"
#include "sfse/ScaleformMovie.h"
#include "sfse/ScaleformValue.h"
#include "sfse/ScaleformTranslator.h"
#include "sfse/ScaleformManager.h"

#include "sfse/GameUI.h"

#include <filesystem>
#include <ctime>
#include <chrono>
#include <map>

#include "PresetInterface.h"

extern PresetInterface g_presetInterface;

class SFEEScaleform_GetDirectoryListing : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		auto movieRoot = args->pMovie->pASMovieRoot;

		const fs::path dir{ args->pArgs[0].GetString() };
		std::string extFilter;
		if (args->ArgCount >= 2 && args->pArgs[1].IsString())
		{
			extFilter = args->pArgs[1].GetString();
		}

		movieRoot->CreateArray(args->pRetVal);

		if (!fs::exists(dir) || !fs::is_directory(dir))
			return;

		for (auto const& dir_entry : fs::directory_iterator{ dir })
		{
			auto extension = dir_entry.path().extension().string();
			if (fs::is_regular_file(dir_entry))
			{
				if (!extFilter.empty() && _stricmp(extFilter.c_str(), extension.c_str()) != 0)
				{
					continue;
				}
			}

			Value fileInfo;
			movieRoot->CreateObject(&fileInfo);

			Value filePath;
			movieRoot->CreateString(&filePath, dir_entry.path().string().c_str());
			fileInfo.SetMember("path", filePath);

			Value fileName;
			movieRoot->CreateString(&fileName, dir_entry.path().filename().string().c_str());
			fileInfo.SetMember("name", fileName);

			Value fileSize(fs::is_regular_file(dir_entry) ? static_cast<u32>(fs::file_size(dir_entry)) : 0);
			fileInfo.SetMember("size", fileSize);

			Value ext;
			movieRoot->CreateString(&ext, extension.c_str());
			fileInfo.SetMember("ext", ext);

			const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(dir_entry.last_write_time());
			const auto time = std::chrono::system_clock::to_time_t(systemTime);
			
			struct tm ftm; ftm = { 0 };
			localtime_s(&ftm, &time);

			Value date;
			Value params[7];
			params[0].SetNumber(ftm.tm_year + 1900);
			params[1].SetNumber(ftm.tm_mon);
			params[2].SetNumber(ftm.tm_mday);
			params[3].SetNumber(ftm.tm_hour);
			params[4].SetNumber(ftm.tm_min);
			params[5].SetNumber(ftm.tm_sec);
			params[6].SetNumber(0);
			movieRoot->CreateObject(&date, "Date", params, 7);
			fileInfo.SetMember("lastModified", date);

			fileInfo.SetMember("directory", dir_entry.is_directory());
			args->pRetVal->PushBack(fileInfo);
		}
	}
};

class PresetDirectoryVisitor : public IPresetInterface::StringVisitor
{
public:
	PresetDirectoryVisitor(const Scaleform::GFx::FunctionHandler::Params* _args) : args(_args) { }
	virtual void String(const char* path) override
	{
		args->pMovie->pASMovieRoot->CreateString(args->pRetVal, path);
	}
private:
	const Scaleform::GFx::FunctionHandler::Params* args;
};

class SFEEScaleform_GetDocumentsDirectory : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		PresetDirectoryVisitor visitor{ args };
		g_presetInterface.GetDirectory(IPresetInterface::Directory::DOCUMENTS, visitor);
	}
};

class SFEEScaleform_GetExecutableDirectory : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		PresetDirectoryVisitor visitor{ args };
		g_presetInterface.GetDirectory(IPresetInterface::Directory::DATA, visitor);
	}
};

IMenu* FindOpenMenu(const BSFixedString& menuName)
{
	for (auto menu : UI::GetSingleton()->openMenus)
	{
		if (menu->MenuName == menuName)
		{
			return menu;
		}
	}

	return nullptr;
}

class SFEEScaleform_SavePreset : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		fs::path filePath(args->pArgs[0].GetString());
		fs::path dirOnly(filePath);
		dirOnly.remove_filename();
		fs::create_directories(dirOnly);
		if (fs::exists(dirOnly))
		{
			auto menu = static_cast<ChargenMenu*>(FindOpenMenu("ChargenMenu"));
			if (menu)
			{
				TESNPC* npc = static_cast<TESNPC*>(menu->pPaperDoll->menuActor->data.objectReference);
				g_presetInterface.SavePreset(filePath.string().c_str(), npc);
			}
		}
	}
};

class SFEEScaleform_LoadPreset : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		fs::path filePath(args->pArgs[0].GetString());
		if (fs::exists(fs::path(filePath).remove_filename()))
		{
			auto menu = static_cast<ChargenMenu*>(FindOpenMenu("ChargenMenu"));
			if (menu)
			{
				Actor* actor = menu->pPaperDoll->menuActor;
				TESNPC* npc = static_cast<TESNPC*>(menu->pPaperDoll->menuActor->data.objectReference);
				
				if (g_presetInterface.LoadPreset(filePath.string().c_str(), npc))
				{
					npc->AddChange(0x1000000);
					npc->AddChange(0x2);
					npc->AddChange(0x80);
					npc->AddChange(0x100); // WalkStyle?
					npc->AddChange(0x800); // HeadParts
					npc->AddChange(0x4000);
					menu->unk5E5 = 1; // Changing preset?
					menu->unk5E3 = 0;
					actor->UpdateAppearance(false, 0x28, false);
					menu->cameraPosition = ChargenMenu::BODY_CAMERA_POSITION;
					menu->unk5E0 = 1;
					TESNPCData::ChargenDataModel::GetSingleton()->Update(menu->npc, &menu->unk2D0);
				}
			}
		}
	}
};

class SFEEScaleform_DeleteFile : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		fs::path filePath(args->pArgs[0].GetString());
		if (fs::is_regular_file(filePath))
		{
			args->pRetVal->SetBoolean(fs::remove(filePath));
		}
	}
};

class SFEEScaleform_GetPresetDependencies : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		class PresetDependencyVisitor : public IPresetInterface::DependencyVisitor
		{
		public:
			PresetDependencyVisitor(ASMovieRootBase* movie, Value* result) : pMovieRoot(movie), pResult(result)
			{
				pMovieRoot->CreateObject(result);
				pMovieRoot->CreateArray(&passArray);
				pMovieRoot->CreateArray(&failArray);
				pMovieRoot->CreateArray(&parseErrors);
				result->SetMember("passes", passArray);
				result->SetMember("failures", failArray);
				result->SetMember("errors", parseErrors);
			}

			virtual void PassDependency(const char* file)
			{
				Value str;
				pMovieRoot->CreateString(&str, file);
				passArray.PushBack(str);
			}
			virtual void FailDependency(const char* file)
			{
				Value str;
				pMovieRoot->CreateString(&str, file);
				failArray.PushBack(str);
			}
			virtual void Error(const char* error)
			{
				Value str;
				pMovieRoot->CreateString(&str, error);
				parseErrors.PushBack(str);
			}

		private:
			Value* pResult;
			ASMovieRootBase* pMovieRoot;
			Value passArray;
			Value failArray;
			Value parseErrors;
		};

		PresetDependencyVisitor visitor(args->pMovie->pASMovieRoot, args->pRetVal);

		fs::path filePath(args->pArgs[0].GetString());
		if (fs::exists(fs::path(filePath).remove_filename()))
		{
			g_presetInterface.QueryPresetDependencies(filePath.string().c_str(), &visitor);
		}
	}
};

typedef std::map <const std::type_info*, Scaleform::GFx::FunctionHandler*>	FunctionHandlerCache;
FunctionHandlerCache g_functionHandlerCache;

template <typename T>
void CreateFunction(Scaleform::GFx::Value* dst, Scaleform::GFx::ASMovieRootBase* movie)
{
	// either allocate the object or retrieve an existing instance from the cache
	Scaleform::GFx::FunctionHandler* fn = nullptr;

	// check the cache
	auto iter = g_functionHandlerCache.find(&typeid(T));
	if (iter != g_functionHandlerCache.end())
		fn = iter->second;

	if (!fn)
	{
		// not found, allocate a new one
		fn = new T;

		// add it to the cache
		// cache now owns the object as far as refcounting goes
		g_functionHandlerCache[&typeid(T)] = fn;
	}

	// create the function object
	movie->CreateFunction(dst, fn);
}

template<class T>
void RegisterFunction(Scaleform::GFx::Value& root, Scaleform::GFx::ASMovieRootBase* movie, const char* name)
{
	Scaleform::GFx::Value fn;
	CreateFunction<T>(&fn, movie);
	root.SetMember(name, fn);
}

void InstallChargenCallbacks(IMenu* menu)
{
	using namespace Scaleform::GFx;

	auto movieRoot = menu->pUIMovie->pASMovieRoot;
	auto rootPath = menu->GetRootPath();

	Value root;
	movieRoot->GetVariable(&root, rootPath);

	if (root.IsObject())
	{
		RegisterFunction<SFEEScaleform_GetDirectoryListing>(root, movieRoot, "GetDirectoryListing");
		RegisterFunction<SFEEScaleform_GetDocumentsDirectory>(root, movieRoot, "GetDocumentsDirectory");
		RegisterFunction<SFEEScaleform_GetExecutableDirectory>(root, movieRoot, "GetExecutableDirectory");
		RegisterFunction<SFEEScaleform_SavePreset>(root, movieRoot, "SavePreset");
		RegisterFunction<SFEEScaleform_LoadPreset>(root, movieRoot, "LoadPreset");
		RegisterFunction<SFEEScaleform_DeleteFile>(root, movieRoot, "DeleteFile");
		RegisterFunction<SFEEScaleform_GetPresetDependencies>(root, movieRoot, "GetPresetDependencies");
		root.SetMember("ModDirectorySuffix", g_presetInterface.GetModSuffix().c_str()); // Move to setting
		root.SetMember("LocalDirectorySuffix", g_presetInterface.GetLocalSuffix().c_str()); // Move to setting
		root.Invoke("onCustomFunctionsRegistered");
	}
}

extern std::unordered_map<std::string, std::unordered_map<std::wstring, std::wstring>> g_translations;

void AddTranslations(BSScaleformManager* manager)
{
	auto translator = static_cast<BSScaleformTranslator::ScaleformImpl*>(manager->pLoader->GetStateAddRef(Scaleform::GFx::State::State_Translator));
	if (translator)
	{
		// Apply English translations
		for (auto& item : g_translations["en"])
		{
			translator->translationMap->insert_or_assign({ item.first.c_str(), item.second.c_str() });
		}

		// Apply language specific ontop
		auto language = (*SettingT<INISettingCollection>::pCollection)->GetSetting("sLanguage:General");
		if (language)
		{
			for (auto& item : g_translations[language->data.s])
			{
				translator->translationMap->insert_or_assign({ item.first.c_str(), item.second.c_str() });
			}
		}
		translator->Release();
	}
}