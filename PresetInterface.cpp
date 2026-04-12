#include "PresetInterface.h"
#include "ChargenInterface.h"
#include "FormUtils.h"
#include "FileUtils.h"
#include "StringUtils.h"
#include "JsonNPC.h"

#include "sfse/GameData.h"
#include "sfse/GameStreams.h"
#include "sfse/GameReferences.h"
#include "sfse/GameObjects.h"
#include "sfse/GameForms.h"

#include <fstream>

extern ChargenInterface g_chargenInterface;

std::uint32_t PresetInterface::GetVersion() const
{
    return IPresetInterface::kCurrentPluginVersion;
}

bool PresetInterface::QueryPresetDependencies(const char* filePath, IPresetInterface::DependencyVisitor* visitor)
{
    using json = nlohmann::json;

    auto dataHandler = TESDataHandler::GetSingleton();

    BSResourceNiBinaryStream stream(filePath);
    if (stream)
    {
        auto data = std::make_unique<char[]>(stream.GetSize() + 1);
        stream.DoRead(data.get(), stream.GetSize());

        try
        {
            auto root = json::parse(data.get(), nullptr, true, true);
            auto j = root.template get<json::object_t>();

            bool pass = true;

            if (j.contains("Dependencies"))
            {
                auto dependencies = j["Dependencies"].template get<std::unordered_set<std::string>>();

                std::unordered_set<std::istring> modSet;
                for (auto mod : dataHandler->CompiledFileCollection.FileA)
                {
                    modSet.insert(mod->filePath);
                }
                for (auto mod : dataHandler->CompiledFileCollection.SmallFileA)
                {
                    modSet.insert(mod->filePath);
                }

                for (auto& item : dependencies)
                {
                    bool active = modSet.contains(item.c_str());
                    if (visitor)
                    {
                        active ? visitor->PassDependency(item.c_str()) : visitor->FailDependency(item.c_str());
                    }
                    pass &= active;
                }
            }

            return pass;
        }
        catch (json::exception& except)
        {
            if (visitor)
                visitor->Error(except.what());
        }
    }

    return false;
}

bool PresetInterface::ParseNPC(const char* jsonData, PresetData& data, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;
    try
    {
        auto npcPreset = json::parse(jsonData, nullptr, true, true).template get<NPCPreset>();
        npcPreset.ToPreset(data, visitor);
        return true;
    }
    catch (json::exception& except)
    {
        if (visitor)
            visitor->Error(except.what());
    }

    return false;
}

bool PresetInterface::ParsePreset(const char* jsonData, PresetData& data, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    try
    {
        auto j = json::parse(jsonData, nullptr, true, true).template get<json::object_t>();
        JsonToPreset(j, data, visitor);
        return true;
    }
    catch (json::exception& except)
    {
        if (visitor)
            visitor->Error(except.what());
    }

    return false;
}

bool PresetInterface::LoadPreset(const char* filePath, TESNPC* target, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    if (target->formType != static_cast<u8>(FormType::kNPC_))
    {
        if (visitor)
            visitor->Error(std::format("Target NPC {:X} wrong type {}", target->formID, target->formType).c_str());
        return false;
    }

    BSResourceNiBinaryStream stream(filePath);
    if (stream)
    {
        auto data = std::make_unique<char[]>(stream.GetSize() + 1);
        stream.DoRead(data.get(), stream.GetSize());

        PresetData preset;
        bool parseResult = false;

        std::istring fileInsensitive(filePath);
        if (fileInsensitive.rfind(".npc") != std::string::npos)
        {
            parseResult = ParseNPC(data.get(), preset, visitor);
        }
        else
        {
            parseResult = ParsePreset(data.get(), preset, visitor);
        }

        if (parseResult)
        {
            ApplyPresetDataToNPC(preset, target);
            return true;
        }
    }

    return false;
}

void PresetInterface::GetDirectory(const IPresetInterface::Directory& dir, IPresetInterface::StringVisitor& visitor)
{
    switch (dir) {
    case Directory::DOCUMENTS:
    {
        visitor.String(FileUtils::GetDocumentsPath().c_str());
        break;
    }
    case Directory::DATA:
    {
        visitor.String(FileUtils::GetExecutablePath().c_str());
        break;
    }
    }
}

void PresetInterface::ApplyPresetDataToNPC(const PresetData& data, TESNPC* npc)
{
    if (!data.Name.empty()) {
        npc->strFullName = data.Name.c_str();
    }
    npc->pFormRace = data.Race;

    u8 Gender = std::clamp<u8>(data.Gender, 0, 1);

    npc->actorData.iActorBaseFlags &= ~ACTOR_BASE_DATA::Flags::kFlags_Gender;
    if (Gender != 0) {
        npc->actorData.iActorBaseFlags |= ACTOR_BASE_DATA::Flags::kFlags_Gender;
    }

    npc->MorphWeight = data.MorphWeight;

    auto& raceHeadParts = data.Race->headParts[Gender];

    auto newHeadParts = data.HeadPartsA;

    auto addMissingPart = [&](BGSHeadPart::HeadPartType partType)
    {
        auto missingPart = std::find_if(data.HeadPartsA.begin(), data.HeadPartsA.end(), [=](const BGSHeadPart* headPart) { return headPart->eType == partType; });
        if (missingPart == data.HeadPartsA.end())
        {
            auto racePart = std::find_if(raceHeadParts.begin(), raceHeadParts.end(), [=](const BGSHeadPart* headPart) { return headPart->eType == partType; });
            if (racePart != raceHeadParts.end())
            {
                newHeadParts.push_back(*racePart);

                // Push any of the missing Extra parts if they arent here
                for (auto& misc : (*racePart)->extraParts)
                {
                    auto miscPart = std::find_if(data.HeadPartsA.begin(), data.HeadPartsA.end(), [=](const BGSHeadPart* headPart) { return headPart == misc; });
                    if (miscPart == data.HeadPartsA.end())
                    {
                        newHeadParts.push_back(misc);
                    }
                }
            }
        }
    };

    addMissingPart(BGSHeadPart::HeadPartType::HeadPartFace);
    addMissingPart(BGSHeadPart::HeadPartType::HeadPartEyebrows);
    addMissingPart(BGSHeadPart::HeadPartType::HeadPartHair);
    addMissingPart(BGSHeadPart::HeadPartType::HeadPartLeftEye);
    addMissingPart(BGSHeadPart::HeadPartType::HeadPartRightEye);
    addMissingPart(BGSHeadPart::HeadPartType::HeadPartTeeth);

    npc->HeadPartsA.clear();
    for (auto& part : newHeadParts)
    {
        npc->HeadPartsA.emplace_back(part);
    }

    npc->HeadPartDataA.clear();
    for (auto& data : data.HeadPartDataA)
    {
        TESNPC::HeadPartData item{
           data.type,
           data.unk04,
           data.group.c_str(),
           data.name.c_str(),
           data.texture.c_str(),
           TESNPC::HeadPartData::Color{
              data.color.a,
              data.color.b,
              data.color.g,
              data.color.r,
           },
           data.intensity
        };
        npc->HeadPartDataA.emplace_back(item);
    }

    if (data.BodyMorphRegionValuesA)
    {
        if (npc->unk3D0)
            npc->unk3D0->clear();
        else
            npc->unk3D0 = new BSTArray<float>();

        for (auto f : *data.BodyMorphRegionValuesA)
        {
            npc->unk3D0->emplace_back(f);
        }
    }
    else if (npc->unk3D0)
    {
        delete npc->unk3D0;
        npc->unk3D0 = nullptr;
    }

    if (data.AdditionalSliders)
    {
        if (npc->AdditionalSliders)
            npc->AdditionalSliders->clear();
        else
            npc->AdditionalSliders = new BSTHashMap2<u32, float>();

        for (auto& item : *data.AdditionalSliders)
        {
            npc->AdditionalSliders->insert_or_assign({ item.first, item.second });
        }
    }
    else if (npc->AdditionalSliders)
    {
        delete npc->AdditionalSliders;
        npc->AdditionalSliders = nullptr;
    }

    if (data.Morphs)
    {
        if (npc->unk3E0)
        {
            for (auto& item : *npc->unk3E0) {
                item.Value->clear();
                delete item.Value;
            }
            npc->unk3E0->clear();
        }
        else
            npc->unk3E0 = new BSTHashMap<u32, BSTHashMap<BSFixedStringCS, float>*>();

        for (auto& item : *data.Morphs)
        {
            if (!item.second.empty())
            {
                auto subMap = new BSTHashMap<BSFixedStringCS, float>();
                for (auto& subItem : item.second)
                {
                    subMap->insert_or_assign({ subItem.first.c_str(), subItem.second });
                }
                npc->unk3E0->insert_or_assign({ item.first, subMap });
            }
        }
    }
    else if (npc->unk3E0)
    {
        for (auto& item : *npc->unk3E0) {
            item.Value->clear();
            delete item.Value;
        }
        npc->unk3E0->clear();
        delete npc->unk3E0;
        npc->unk3E0 = nullptr;
    }
    
    if (data.ShapeBlendData)
    {
        if (npc->shapeBlendData)
            npc->shapeBlendData->clear();
        else
            npc->shapeBlendData = new BSTHashMap<BSFixedStringCS, float>();

        for (auto& item : *data.ShapeBlendData)
        {
            npc->shapeBlendData->insert_or_assign({ item.first.c_str(), item.second });
        }
    }
    else if(npc->shapeBlendData)
    {
        // Delete previous data
        npc->shapeBlendData->clear();
        delete npc->shapeBlendData;
        npc->shapeBlendData = nullptr;
    }

    npc->skinTone = data.SkinTone;
    npc->teeth = data.Teeth.c_str();
    npc->jewelryColor = data.JewelryColor.c_str();
    npc->eyeColor = data.EyeColor.c_str();
    npc->hairColor = data.HairColor.c_str();
    npc->facialHairColor = data.FacialHairColor.c_str();
    npc->eyebrowColor = data.EyebrowColor.c_str();
    npc->pronoun = data.Pronoun;
    npc->pFaceNPC = nullptr;
}

void PresetInterface::NPCToJson(TESNPC* npc, nlohmann::json& j)
{
    using json = nlohmann::json;

    j["Version"] = kFileVersion2;
    j["Name"] = npc->strFullName.c_str();
    j["Race"] = FormUtils::to_identifier(npc->pFormRace);
    j["Gender"] = npc->actorData.GetSex() == 0 ? "Male" : "Female";
    j["Weight"] = {
        {"Thin", npc->MorphWeight.x},
        {"Muscular", npc->MorphWeight.y},
        {"Heavy", npc->MorphWeight.z}
    };

    std::unordered_set<const TESFile*> dependencies;

    auto headPartA = json::array();
    for (auto part : npc->HeadPartsA)
    {
        headPartA.push_back(FormUtils::to_identifier(part, &dependencies));
    }
    j["HeadParts"] = headPartA;

    if (npc->unk3D0)
    {
        auto unk408 = json::array();
        for (auto f : *npc->unk3D0)
        {
            unk408.push_back(f);
        }
        j["BodyMorphRegionValues"] = unk408;
    }
    if (npc->AdditionalSliders)
    {
        auto sliders = json::object();
        for (auto& item : *npc->AdditionalSliders)
        {
            if (item.Key & 0xFF000000)
            {
                auto identifier = FormUtils::to_identifier_id(item.Key, &dependencies);
                if (identifier.length())
                {
                    sliders[identifier] = item.Value;
                }
            }
            else {
                sliders[std::to_string(item.Key)] = item.Value;
            }
        }
        j["AdditionalSliders"] = sliders;
    }
    if (npc->unk3E0)
    {
        auto unk418 = json::object();
        for (auto& item : *npc->unk3E0)
        {
            if (item.Value)
            {
                auto unk418_j = json::object();
                for (auto item2 : *item.Value)
                {
                    unk418_j[item2.Key.c_str()] = item2.Value;
                }

                unk418[std::to_string(item.Key)] = unk418_j;
            }
        }
        j["Morphs"] = unk418;
    }

    auto headPartDataA = json::array();
    for (auto& part : npc->HeadPartDataA)
    {
        auto headPartData = json::object();
        headPartData["Type"] = part.type;
        headPartData["unk04"] = part.unk04;
        headPartData["Group"] = part.group;
        headPartData["Name"] = part.name;
        headPartData["Texture"] = part.texture;
        headPartData["Color"] = {
            { "r", part.color.r },
            { "g", part.color.g },
            { "b", part.color.b },
            { "a", part.color.a }
        };
        headPartData["Intensity"] = (float)part.intensity / 128.0f;
        headPartDataA.push_back(headPartData);
    }
    j["HeadPartData"] = headPartDataA;
    j["SkinTone"] = npc->skinTone;
    j["Teeth"] = npc->teeth;
    j["JewelryColor"] = npc->jewelryColor;
    j["EyeColor"] = npc->eyeColor;
    j["HairColor"] = npc->hairColor;
    j["FacialHairColor"] = npc->facialHairColor;
    j["EyebrowColor"] = npc->eyebrowColor;

    if (npc->shapeBlendData)
    {
        auto shapeBlendData = json::object();
        for (auto& item : *npc->shapeBlendData)
        {
            shapeBlendData[item.Key.c_str()] = item.Value;

            // If the slider is coming from a mod, write it as a dependency
            auto file = g_chargenInterface.GetSliderDependency(item.Key.c_str(), static_cast<IChargenInterface::Gender>(npc->actorData.GetSex()));
            if (file)
            {
                dependencies.insert(file);
            }
        }
        j["ShapeBlendData"] = shapeBlendData;
    }

    j["unk470"] = npc->unk438;
    j["Pronoun"] = npc->pronoun;

    if (dependencies.size() > 0)
    {
        auto deps = json::array();
        for (auto item : dependencies)
        {
            deps.push_back(item->filePath);
        }
        j["Dependencies"] = deps;
    }
}

bool PresetInterface::JsonToPreset(const nlohmann::json& j, PresetData& data, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    data.Version = j["Version"].template get<int>();
    if (j.contains("Name")) data.Name = j["Name"].template get<std::string>();

    auto raceName = j["Race"].template get<std::string>();
    TESForm* form = FormUtils::from_identifier(raceName);
    if (!form) {
        if (visitor)
            visitor->Error(std::format("Invalid race {} specified", raceName.c_str()).c_str());
        return false;
    }
    else if (form->formType != static_cast<u8>(FormType::kRACE))
    {
        if (visitor)
            visitor->Error(std::format("Form resolved by {} is not a race form", raceName.c_str()).c_str());
        return false;
    }

    data.Race = static_cast<TESRace*>(form);
    data.Gender = 0;
    auto genderName = j["Gender"].template get<std::string>();
    if (_stricmp(genderName.c_str(), "Female") == 0) {
        data.Gender = 1;
    }

    data.MorphWeight.x = j["Weight"]["Thin"].template get<float>();
    data.MorphWeight.y = j["Weight"]["Muscular"].template get<float>();
    data.MorphWeight.z = j["Weight"]["Heavy"].template get<float>();

    auto headParts = j["HeadParts"].template get<std::vector<std::string>>();
    for (auto& part : headParts)
    {
        auto headPart = FormUtils::from_identifier(part);
        if (!headPart || headPart->formType != static_cast<u8>(FormType::kHDPT))
            continue;

        data.HeadPartsA.push_back(static_cast<BGSHeadPart*>(headPart));
    }

    auto headPartData = j["HeadPartData"].template get<std::vector<json::object_t>>();
    for (auto& part : headPartData)
    {
        PresetData::HeadPartData::Color v1{
             static_cast<u8>(part["Color"]["r"].template get<u32>()),
             static_cast<u8>(part["Color"]["g"].template get<u32>()),
             static_cast<u8>(part["Color"]["b"].template get<u32>()),
             static_cast<u8>(part["Color"]["a"].template get<u32>()),
        };
        PresetData::HeadPartData::Color v2{
             static_cast<u8>(part["Color"]["a"].template get<u32>()),
             static_cast<u8>(part["Color"]["b"].template get<u32>()),
             static_cast<u8>(part["Color"]["g"].template get<u32>()),
             static_cast<u8>(part["Color"]["r"].template get<u32>()),
        };

        PresetData::HeadPartData item{
           part["Type"].template get<u32>(),
           part["unk04"].template get<u32>(),
           part["Group"].template get<std::string>(),
           part["Name"].template get<std::string>(),
           part["Texture"].template get<std::string>(),
           data.Version == kFileVersion1 ? v1 : v2,
           static_cast<u32>(part["Intensity"].template get<float>() * 128)
        };
        data.HeadPartDataA.push_back(item);
    }

    if (j.contains("unk408"))
    {
        auto unk408 = j["unk408"].template get<std::vector<float>>();
        data.BodyMorphRegionValuesA = std::make_unique<std::vector<float>>();
        for (auto f : unk408)
        {
            data.BodyMorphRegionValuesA->emplace_back(f);
        }
    }
    if (j.contains("BodyMorphRegionValues"))
    {
        auto bodyMorphRegionValues = j["BodyMorphRegionValues"].template get<std::vector<float>>();
        data.BodyMorphRegionValuesA = std::make_unique<std::vector<float>>();
        for (auto f : bodyMorphRegionValues)
        {
            data.BodyMorphRegionValuesA->emplace_back(f);
        }
    }

    if (j.contains("AdditionalSliders"))
    {
        auto sliders = j["AdditionalSliders"].template get<std::unordered_map<std::string, float>>();
        std::unordered_map<u32, float> newSliders;
        for (auto& item : sliders)
        {
            char* endPtr;
            auto key = std::strtoul(item.first.c_str(), &endPtr, 0);
            if (endPtr == item.first.c_str() || *endPtr != '\0')
            {
                key = FormUtils::from_identifier_id(item.first);
                if (key != 0) {
                    newSliders[key] = item.second;
                }
            }
            else
            {
                newSliders[key] = item.second;
            }
        }

        data.AdditionalSliders = std::make_unique<std::unordered_map<u32, float>>();
        for (auto& item : newSliders)
        {
            data.AdditionalSliders->insert_or_assign(item.first, item.second);
        }
    }

    if (j.contains("Morphs"))
    {
        auto morphs = j["Morphs"].template get<json::object_t>();

        data.Morphs = std::make_unique<std::unordered_map<u32, std::unordered_map<std::string, float>>>();
        for (auto& item : morphs)
        {
            auto key = std::strtoul(item.first.c_str(), nullptr, 0);
            (*data.Morphs)[key] = item.second.template get<std::unordered_map<std::string, float>>();
        }
    }

    if (j.contains("ShapeBlendData"))
    {
        data.ShapeBlendData = std::make_unique<std::unordered_map<std::string, float>>();
        *data.ShapeBlendData = j["ShapeBlendData"].template get<std::unordered_map<std::string, float>>();
    }

    data.SkinTone = j["SkinTone"].template get<u32>();
    data.Teeth = j["Teeth"].template get<std::string>();
    data.JewelryColor = j["JewelryColor"].template get<std::string>();
    data.EyeColor = j["EyeColor"].template get<std::string>();
    data.HairColor = j["HairColor"].template get<std::string>();
    data.FacialHairColor = j["FacialHairColor"].template get<std::string>();
    data.EyebrowColor = j["EyebrowColor"].template get<std::string>();
    data.Pronoun = static_cast<u8>(j["Pronoun"].template get<u32>());
    return true;
}

bool PresetInterface::SavePreset(const char* filePath, TESNPC* npc, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    if (npc->formType != static_cast<u8>(FormType::kNPC_))
    {
        if(visitor)
            visitor->Error(std::format("Source NPC {:X} wrong type {}", npc->formID, npc->formType).c_str());
        return false;
    }

    json j;
    NPCToJson(npc, j);

    std::ofstream fileOut(filePath);
    fileOut << std::setw(4) << j << std::endl;
    return true;
}

bool PresetInterface::SaveNPC(const char* filePath, TESNPC* npc, IPresetInterface::ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    PresetData npcPreset;
    json npcJson;
    NPCToJson(npc, npcJson);
    JsonToPreset(npcJson, npcPreset, visitor);

    NPCPreset preset;
    preset.FromPreset(npcPreset, visitor);

    json j;
    to_json(j, preset);

    std::ofstream fileOut(filePath);
    fileOut << std::setw(3) << j << std::endl;
    return true;
}