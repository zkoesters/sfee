#include "FormUtils.h"

#include "sfse/GameData.h"

#include <format>

namespace FormUtils
{
const TESFile* GetModByName(const char* name)
{
	auto dataHandler = TESDataHandler::GetSingleton();

	// Try normal plugin
	for (auto item : dataHandler->CompiledFileCollection.FileA)
	{
		if (_stricmp(item->filePath, name) == 0)
		{
			return item;
		}
	}

	// Try small file
	for (auto item : dataHandler->CompiledFileCollection.SmallFileA)
	{
		if (_stricmp(item->filePath, name) == 0)
		{
			return item;
		}
	}

	return nullptr;
}

const TESFile* GetModByFormId(const uint32_t formId)
{
	auto dataHandler = TESDataHandler::GetSingleton();

	uint8_t modIndex = formId >> 24;
	uint32_t modForm = formId & 0xFFFFFF;

	if (modIndex == 0xFE)
	{
		uint16_t lightIndex = (formId >> 12) & 0xFFF;
		if (lightIndex < dataHandler->CompiledFileCollection.SmallFileA.size())
		{
			return dataHandler->CompiledFileCollection.SmallFileA[lightIndex];
		}
	}
	else if(modIndex < dataHandler->CompiledFileCollection.FileA.size())
	{
		return dataHandler->CompiledFileCollection.FileA[modIndex];
	}
	return nullptr;
}

uint32_t GetFormIdFromMod(const TESFile* fileInfo, const uint32_t formLower)
{
	return fileInfo->cCompileIndex != 0xFE ? uint32_t(fileInfo->cCompileIndex) << 24 | (formLower & 0xFFFFFF) : 0xFE000000 | (uint32_t(fileInfo->sSmallFileCompileIndex) << 12) | (formLower & 0xFFF);
}

std::string to_identifier_raw(const TESFile* file, uint32_t formLower, std::unordered_set<const TESFile*>* dependencyOut)
{
	if (dependencyOut)
		dependencyOut->insert(file);

	return std::format("{}|{:X}", file->filePath, formLower);
}

std::string to_identifier_id(uint32_t formId, std::unordered_set<const TESFile*>* dependencyOut)
{
	uint32_t formLower = formId & 0xFFFFFF;
	uint8_t modIndex = formId >> 24;
	if (modIndex == 0xFE)
	{
		formLower &= 0xFFF;
	}

	const TESFile* file = GetModByFormId(formId);
	if (file) {
		return to_identifier_raw(file, formLower, dependencyOut);
	}

	return "";
}

std::string to_identifier(TESForm* form, std::unordered_set<const TESFile*>* dependencyOut)
{
	return to_identifier_id(form->formID, dependencyOut);
}

uint32_t from_identifier_id(const std::string& formIdentifier, std::unordered_set<const TESFile*>* dependencyOut)
{
	std::size_t pos = formIdentifier.find_first_of('|');
	std::string modName = formIdentifier.substr(0, pos);
	std::string modForm = formIdentifier.substr(pos + 1);

	uint32_t formId = 0;
	sscanf_s(modForm.c_str(), "%X", &formId);

	const TESFile* file = GetModByName(modName.c_str());
	if (!file) {
		return 0;
	}

	if (dependencyOut)
		dependencyOut->insert(file);

	return GetFormIdFromMod(file, formId);
}

TESForm* from_identifier(const std::string& formIdentifier, std::unordered_set<const TESFile*>* dependencyOut)
{
	auto id = from_identifier_id(formIdentifier, dependencyOut);
	if (!id)
		return nullptr;
	return TESForm::GetFormByNumericID(from_identifier_id(formIdentifier, dependencyOut));
}
}
