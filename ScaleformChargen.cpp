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
#include "sfse/PluginAPI.h"

#include <filesystem>
#include <ctime>
#include <chrono>
#include <map>

#include "PresetInterface.h"
#include "ChargenInterface.h"
#include  "StringUtils.h"

extern PresetInterface g_presetInterface;
extern ChargenInterface g_chargenInterface;
extern SFSETaskInterface* g_taskInterface;

inline bool operator==(const BSFixedStringWCS& lhs, const BSFixedStringWCS& rhs)
{
	return lhs.pData == rhs.pData;
}

class SFEEScaleform_GetDirectoryListing : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount <= 0 || !args->pArgs[0].IsString())
			return;

		auto& movieRoot = args->pMovie->pASMovieRoot;

		const fs::path dir{ args->pArgs[0].GetString() };
		std::unordered_set<std::istring> extFilter;
		if (args->ArgCount >= 2 && args->pArgs[1].IsString())
		{
			std::string filters = args->pArgs[1].GetString();
			auto filterList = split(filters, ',');
			for (auto& filter : filterList)
			{
				extFilter.emplace(filter.c_str());
			}
		}

		movieRoot->CreateArray(args->pRetVal);
		if (!args->pRetVal->IsArray())
			return;

		if (!fs::exists(dir) || !fs::is_directory(dir))
			return;

		for (auto const& dir_entry : fs::directory_iterator{ dir })
		{
			auto extension = dir_entry.path().extension().string();
			if (fs::is_regular_file(dir_entry))
			{
				if (!extFilter.empty() && !extFilter.contains(extension.c_str()))
				{
					continue;
				}
			}

			Value fileInfo;
			movieRoot->CreateObject(&fileInfo);
			if (!fileInfo.IsObject())
				continue;

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
	auto ui = UI::GetSingleton();
	for (auto menu : ui->openMenus)
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

class SFEEScaleform_SaveNPCPreset : public Scaleform::GFx::FunctionHandler
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
				g_presetInterface.SaveNPC(filePath.string().c_str(), npc);
			}
		}
	}
};

#if _DEBUG
#include "sfse/NiObject.h"

bool VisitObjects(NiAVObject* parent, std::function<bool(NiAVObject*)> functor)
{
	if (functor(parent))
		return true;

	auto node = parent->IsNode();
	if (node) {
		for (u32 i = 0; i < node->m_kChildren.m_usSize; i++) {
			auto object = node->m_kChildren.m_pBase[i];
			if (object) {
				if (VisitObjects(object, functor))
					return true;
			}
		}
	}

	return false;
}

#define _AMD64_
#include <debugapi.h>
#include <cstdarg>
#include "sfse/NiRTTI.h"
#include "sfse/NiExtraData.h"

static u32 indentLevel = 0;
int print_log(const char* format, ...)
{
	static char s_printf_buf[1024];
	if(indentLevel)
		sprintf_s(s_printf_buf, "\t");
	for (u32 i = 1; i < indentLevel; ++i)
		strcat_s(s_printf_buf, "\t");
	va_list args;
	va_start(args, format);
	_vsnprintf_s(static_cast<char*>(s_printf_buf) + indentLevel, sizeof(s_printf_buf) - indentLevel, sizeof(s_printf_buf) - indentLevel, format, args);
	va_end(args);
	strcat_s(s_printf_buf, "\n");
	OutputDebugStringA(s_printf_buf);
	return 0;
}

void DumpNodeChildren(NiAVObject* node)
{
	print_log("{%s} {%s} {%p}", node->GetRTTI()->name, node->m_kName.c_str(), (void*)node);
	
	{
		BSAutoReadLock locker(node->extraLock);
		if (auto pExtra = node->pExtra) {
			for (auto& extra : *pExtra)
			{
				indentLevel++;
				print_log("{%s} {%s} {%p}", extra->GetRTTI()->name, extra->m_kName.c_str(), (void*)node);
				indentLevel--;
			}
		}
	}
	
	NiNode* niNode = node->IsNode();
	if (niNode && niNode->m_kChildren.m_usSize > 0)
	{
		indentLevel++;
		for (int i = 0; i < niNode->m_kChildren.m_usSize; i++)
		{
			NiAVObject* object = niNode->m_kChildren.m_pBase[i];
			if (object) {
				NiNode* childNode = object->IsNode();
				BSGeometry* geometry = object->IsGeometry();
				if (geometry) {
					print_log("{%s} {%s} {%p} - Geometry", object->GetRTTI()->name, object->m_kName.c_str(), (void*)object);
				}
				else if (childNode) {
					DumpNodeChildren(childNode);
				}
				else {
					print_log("{%s} {%s} {%p}", object->GetRTTI()->name, object->m_kName.c_str(), (void*)object);
				}
			}
		}
		indentLevel--;
	}
}
#endif

class PresetDependencyVisitor : public IPresetInterface::DependencyVisitor
{
public:
	PresetDependencyVisitor(Scaleform::GFx::ASMovieRootBase* movie, Scaleform::GFx::Value* result) : pMovieRoot(movie), pResult(result)
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
		Scaleform::GFx::Value str;
		pMovieRoot->CreateString(&str, file);
		passArray.PushBack(str);
	}
	virtual void FailDependency(const char* file)
	{
		Scaleform::GFx::Value str;
		pMovieRoot->CreateString(&str, file);
		failArray.PushBack(str);
	}
	virtual void Error(const char* error)
	{
		Scaleform::GFx::Value str;
		pMovieRoot->CreateString(&str, error);
		parseErrors.PushBack(str);
	}

private:
	Scaleform::GFx::Value* pResult;
	Scaleform::GFx::ASMovieRootBase* pMovieRoot;
	Scaleform::GFx::Value passArray;
	Scaleform::GFx::Value failArray;
	Scaleform::GFx::Value parseErrors;
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
				MenuActor* actor = menu->pPaperDoll->menuActor;
				TESNPC* npc = static_cast<TESNPC*>(actor->data.objectReference);

#if 0
				{
					auto data = actor->loadedData.lock_read();
					if (auto& object = data->data3D) {
						DumpNodeChildren(object);
					}
				}
#endif
				PresetDependencyVisitor visitor(args->pMovie->pASMovieRoot, args->pRetVal);
				
				if (g_presetInterface.LoadPreset(filePath.string().c_str(), npc, &visitor))
				{
					npc->AddChange(0x1000000);
					npc->AddChange(0x2);
					npc->AddChange(0x80);
					npc->AddChange(0x100); // WalkStyle?
					npc->AddChange(0x800); // HeadParts
					npc->AddChange(0x4000);
					actor->UpdateAppearance(false, 0x28, false);
					menu->cameraPosition = ChargenMenu::BODY_CAMERA_POSITION;
					TESNPCData::ChargenDataModel::GetSingleton()->Update(*TESNPCData::g_actorCheckpoint);
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

		PresetDependencyVisitor visitor(args->pMovie->pASMovieRoot, args->pRetVal);

		fs::path filePath(args->pArgs[0].GetString());
		if (fs::exists(fs::path(filePath).remove_filename()))
		{
			g_presetInterface.QueryPresetDependencies(filePath.string().c_str(), &visitor);
		}
	}
};

class SFEEScaleform_GetExtendedSliders : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		class SliderVisitor : public IChargenInterface::MorphTargetSliderVisitor
		{
		public:
			SliderVisitor(ASMovieRootBase* movie, Value* result, TESNPC* npc) : pMovieRoot(movie), pResult(result), pNPC(npc)
			{
				pMovieRoot->CreateArray(result);
			}

			virtual void Visit(const char* morphKey, const char* displayName, const char* identifier, const std::int64_t order) override
			{
				Value slider;
				pMovieRoot->CreateObject(&slider);
				Value localizedName;
				pMovieRoot->CreateString(&localizedName, displayName);
				slider.SetMember("UILocalizedName", localizedName);
				Value eventName;
				pMovieRoot->CreateString(&eventName, morphKey);
				slider.SetMember("EventName", eventName);
				float value = 0.0f;
				if (pNPC->shapeBlendData)
				{
					auto it = pNPC->shapeBlendData->find(morphKey);
					if (it != pNPC->shapeBlendData->end())
					{
						value = it->Value;
					}
				}
				slider.SetMember("Value", value);
				pResult->PushBack(slider);
			}

		private:
			Value* pResult;
			TESNPC* pNPC;
			ASMovieRootBase* pMovieRoot;
		};

		auto menu = static_cast<ChargenMenu*>(FindOpenMenu("ChargenMenu"));
		if (menu)
		{
			Actor* actor = menu->pPaperDoll->menuActor;
			TESNPC* npc = static_cast<TESNPC*>(menu->pPaperDoll->menuActor->data.objectReference);
			SliderVisitor visitor(args->pMovie->pASMovieRoot, args->pRetVal, npc);
			g_chargenInterface.ForEachSlider(static_cast<IChargenInterface::Gender>(npc->actorData.GetSex()), visitor);
		}
	}
};

class SFEEScaleform_SetExtendedSlider : public Scaleform::GFx::FunctionHandler
{
public:
	virtual void Call(const Scaleform::GFx::FunctionHandler::Params* args) override
	{
		using namespace Scaleform::GFx;
		namespace fs = std::filesystem;

		if (args->ArgCount < 2 || !args->pArgs[0].IsString() || !args->pArgs[1].IsNumber())
			return;

		auto key = args->pArgs[0].GetString();
		auto value = args->pArgs[1].GetNumber();
		
		auto menu = static_cast<ChargenMenu*>(FindOpenMenu("ChargenMenu"));
		if (menu)
		{
			Actor* actor = menu->pPaperDoll->menuActor;
			TESNPC* npc = static_cast<TESNPC*>(actor->data.objectReference);

			if (!npc->shapeBlendData) {
				npc->shapeBlendData = new BSTHashMap<BSFixedStringCS, float>();
			}

			if (value == 0.0f) {
				npc->shapeBlendData->erase(key);
			}
			else {
				npc->shapeBlendData->insert_or_assign(key, static_cast<float>(value));
			}
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
		RegisterFunction<SFEEScaleform_SaveNPCPreset>(root, movieRoot, "SaveNPCPreset");
		RegisterFunction<SFEEScaleform_LoadPreset>(root, movieRoot, "LoadPreset");
		RegisterFunction<SFEEScaleform_DeleteFile>(root, movieRoot, "DeleteFile");
		RegisterFunction<SFEEScaleform_GetPresetDependencies>(root, movieRoot, "GetPresetDependencies");
		RegisterFunction<SFEEScaleform_GetExtendedSliders>(root, movieRoot, "GetExtendedSliders");
		RegisterFunction<SFEEScaleform_SetExtendedSlider>(root, movieRoot, "SetExtendedSlider");
		Value modSuffix;
		movieRoot->CreateStringW(&modSuffix, g_presetInterface.GetModSuffix().c_str());
		root.SetMember("ModDirectorySuffix", modSuffix); // Move to setting
		Value localSuffix;
		movieRoot->CreateStringW(&localSuffix, g_presetInterface.GetLocalSuffix().c_str());
		root.SetMember("LocalDirectorySuffix", localSuffix); // Move to setting
		root.Invoke("onCustomFunctionsRegistered");
	}
}

extern std::unordered_map<std::string, std::unordered_map<std::wstring, std::wstring>> g_translations;

void AddTranslations(BSScaleformManager* manager)
{
	if (manager->pLoader)
	{
		auto translator = static_cast<BSScaleformTranslator::ScaleformImpl*>(manager->pLoader->GetStateAddRef(Scaleform::GFx::State::State_Translator));
		if (translator)
		{
			// Apply English translations
			for (auto& item : g_translations["en"])
			{
				BSFixedStringWCS key(item.first.c_str());
				BSFixedStringWCS value(item.second.c_str());
				translator->translationMap->insert_or_assign(key, value);
			}

			// Apply language specific ontop
			auto language = (*SettingT<INISettingCollection>::pCollection)->GetSetting("sLanguage:General");
			if (language)
			{
				for (auto& item : g_translations[language->data.s])
				{
					BSFixedStringWCS key(item.first.c_str());
					BSFixedStringWCS value(item.second.c_str());
					translator->translationMap->insert_or_assign(key, value);
				}
			}
			translator->Release();
		}
	}
}
