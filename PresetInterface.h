#pragma once

#include "IPluginInterface.h"

#include "sfse/GameObjects.h"
#include "sfse/NiTypes.h"

class TESRace;

#include "nlohmann/json_fwd.hpp"

struct PresetData
{
	u32 Version = 0;
	std::string Name;
	TESRace* Race = nullptr;
	u8 Gender = 0;
	NiPoint3 MorphWeight;
	std::vector<BGSHeadPart*> HeadPartsA;

	struct HeadPartData
	{
		u32 type = 0;
		u32 unk04 = 0;
		std::string group;
		std::string name;
		std::string texture;
		struct Color
		{
			u8 a, b, g, r;
		};
		Color color;
		u32 intensity = 0;
	};
	std::vector<HeadPartData> HeadPartDataA;
	std::unique_ptr<std::vector<float>> BodyMorphRegionValuesA;
	std::unique_ptr<std::unordered_map<u32, float>> AdditionalSliders;
	std::unique_ptr<std::unordered_map<u32, std::unordered_map<std::string, float>>> Morphs;
	std::unique_ptr<std::unordered_map<std::string, float>> ShapeBlendData;
	u32 SkinTone = 0;
	u8 Pronoun = 0;
	std::string Teeth;
	std::string JewelryColor;
	std::string EyeColor;
	std::string HairColor;
	std::string FacialHairColor;
	std::string EyebrowColor;
};

class PresetInterface : public IPresetInterface
{
public:
    virtual std::uint32_t GetVersion() const override;

	bool ParsePreset(const char* json, PresetData& data, IPresetInterface::ErrorVisitor* visitor = nullptr);
	bool ParseNPC(const char* json, PresetData& data, IPresetInterface::ErrorVisitor* visitor = nullptr);

	void NPCToJson(TESNPC* source, nlohmann::json& j);
	bool JsonToPreset(const nlohmann::json& j, PresetData& data, IPresetInterface::ErrorVisitor* visitor);

	bool SaveNPC(const char* filePath, TESNPC* npc, IPresetInterface::ErrorVisitor* visitor = nullptr);

	void ApplyPresetDataToNPC(const PresetData& data, TESNPC* npc);

	virtual bool QueryPresetDependencies(const char* filePath, IPresetInterface::DependencyVisitor* visitor = nullptr) override;
	virtual bool LoadPreset(const char* filePath, TESNPC* target, IPresetInterface::ErrorVisitor* visitor = nullptr) override;
	virtual bool SavePreset(const char* filePath, TESNPC* source, IPresetInterface::ErrorVisitor* visitor = nullptr) override;

	virtual void GetDirectory(const Directory& dir, IPresetInterface::StringVisitor& visitor) override;

	using dir_string = std::wstring;

	void SetLocalSuffix(const dir_string& str) { m_localSuffix = str; }
	const dir_string& GetLocalSuffix() const { return m_localSuffix; }

	void SetModSuffix(const dir_string& str) { m_modSuffix = str; }
	const dir_string& GetModSuffix() const { return m_modSuffix; }

private:
	dir_string m_localSuffix;
	dir_string m_modSuffix;
};