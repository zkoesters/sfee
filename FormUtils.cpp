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

	// Try medium file
	for (auto item : dataHandler->CompiledFileCollection.MediumFileA)
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

uint32_t GetFormIdFromMod(const TESFile* fileInfo, const uint32_t formLower)
{
	uint32_t compileIndex = TESDataHandler::GetSingleton()->GetSubIndex(fileInfo);
	if (compileIndex == -1)
		return 0;

	uint32_t lowerId = formLower;
	if (fileInfo->IsLight())
	{
		lowerId &= 0xFFF;
		compileIndex &= 0xFFF;
		lowerId |= compileIndex << 12;
		compileIndex = TESFile::LightIndex;
	}
	else if (fileInfo->IsMedium())
	{
		lowerId &= 0xFFF;
		compileIndex &= 0xFFF;
		lowerId |= compileIndex << 12;
		compileIndex = TESFile::MediumIndex;
	}
	else
	{
		lowerId &= 0xFFFFFF;
	}
	return (compileIndex << 24) | lowerId;
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
	if (TESFile::IsLight(modIndex) || TESFile::IsMedium(modIndex))
	{
		formLower &= 0xFFF;
	}

	const TESFile* file = TESDataHandler::GetSingleton()->GetModByFormId(formId);
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
