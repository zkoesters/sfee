#pragma once

#include <string>
#include <xstring>
#include <iostream>
#include <unordered_set>

class TESForm;
class TESFile;

namespace FormUtils
{
const TESFile* GetModByName(const char* name);
uint32_t GetFormIdFromMod(const TESFile* fileInfo, const uint32_t formLower);

std::string to_identifier_raw(const TESFile* file, uint32_t formLower, std::unordered_set<const TESFile*>* dependencyOut = nullptr);
std::string to_identifier_id(uint32_t formId, std::unordered_set<const TESFile*>* dependencyOut = nullptr);
std::string to_identifier(TESForm* form, std::unordered_set<const TESFile*>* dependencyOut = nullptr);

uint32_t from_identifier_id(const std::string& formIdentifier, std::unordered_set<const TESFile*>* dependencyOut = nullptr);
TESForm* from_identifier(const std::string& formIdentifier, std::unordered_set<const TESFile*>* dependencyOut = nullptr);
}
