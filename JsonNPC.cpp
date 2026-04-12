#include "JsonNPC.h"
#include "PresetInterface.h"

#include "sfse/GameTypes.h"
#include "sfse/GameForms.h"
#include "sfse/GameData.h"
#include "sfse/GameObjects.h"
#include "sfse/NiTypes.h"

#include <unordered_set>
#include <map>

bool NPCPreset::FromPreset(const PresetData& data, IPresetInterface::ErrorVisitor* visitor)
{
    RaceFormID = data.Race ? data.Race->editorID.c_str() : "";
    Sex = data.Gender == 1 ? "Female" : "Male";
    MorphWeights.x = data.MorphWeight.x;
    MorphWeights.y = data.MorphWeight.y;
    MorphWeights.z = data.MorphWeight.z;

    if (data.BodyMorphRegionValuesA)
    {
        for (auto v : *data.BodyMorphRegionValuesA)
        {
            BodyMorphRegionValuesA.emplace_back(v);
        }
    }
    
    if (data.ShapeBlendData)
    {
        for (auto& item : *data.ShapeBlendData)
        {
            FacialMorphSliderDataA.push_back({ item.first.c_str(),item.second });
        }
    }

    std::unordered_set<BGSHeadPart*> uniqueExtraParts;

    auto findUniqueType = [&](BGSHeadPart::HeadPartType partType) -> std::string
    {
        if (partType == BGSHeadPart::HeadPartType::HeadPartMisc)
            return "";

        auto partByType = std::find_if(data.HeadPartsA.begin(), data.HeadPartsA.end(), [=](const BGSHeadPart* headPart) { return headPart->eType == partType; });
        if (partByType != data.HeadPartsA.end())
        {
            for (auto child : (*partByType)->extraParts)
            {
                uniqueExtraParts.emplace(child);
            }

            return (*partByType)->unk68.c_str();
        }
        return "";
    };

    // Collect all the unique parts
    for (uint32_t uniqueType = BGSHeadPart::HeadPartType::HeadPartMisc; uniqueType < BGSHeadPart::HeadPartType::HeadPartCount; ++uniqueType)
    {
        UniqueHeadPartsA.emplace_back(findUniqueType(static_cast<BGSHeadPart::HeadPartType>(uniqueType)));
    }

    // Collect the parts which are Extra but are not brought by a unique part
    for (auto part : data.HeadPartsA)
    {
        if (part->eType == BGSHeadPart::HeadPartType::HeadPartMisc && !uniqueExtraParts.contains(part))
        {
            MiscHeadPartsA.emplace_back(part->unk68.c_str());
        }
    }

    if (data.Morphs)
    {
        for (auto& item : *data.Morphs)
        {
            NPCPreset::FacialBoneRegionData data;
            data.RegionID = item.first;
            for (auto& slider : item.second)
            {
                data.SlidersA.push_back({ slider.first, 0, slider.second });
            }
            FacialBoneRegionDataA.emplace_back(data);
        }
    }

    std::unordered_map<uint32_t, TESRace::ChargenData::FaceMorphData*> regionMap;
    std::map<uint32_t, std::map<uint32_t, float>> regionValueMap;

    auto getRegionByMorph = [&](uint32_t morphID) -> TESRace::ChargenData::FaceMorphData*
    {
        for (auto& faceMorph : data.Race->chargenData[data.Gender]->faceMorphMap)
        {
            for (auto& postBlend : faceMorph.Value->PostBlendSliderA)
            {
                if (postBlend == morphID)
                {
                    return faceMorph.Value;
                }
            }
        }
        return nullptr;
    };

    if (data.AdditionalSliders)
    {
        // Perform a first pass to gather all the relevant regions
        for (auto& slider : *data.AdditionalSliders)
        {
            auto region = getRegionByMorph(slider.first);
            if (region != nullptr)
            {
                regionMap[slider.first] = region;

                // Zero all sliders in this Region
                for (auto& postBlend : region->PostBlendSliderA)
                {
                    regionValueMap[region->ID][postBlend] = 0.0f;
                }
            }
        }

        // Apply the values to the sliders overtop
        for (auto& slider : *data.AdditionalSliders)
        {
            auto map = regionMap.find(slider.first);
            if (map != regionMap.end())
            {
                regionValueMap[map->second->ID][slider.first] = slider.second;
            }
        }
    }

    for (auto& region : regionValueMap)
    {
        NPCPreset::FacialBoneRegionData data;
        data.RegionID = static_cast<int32_t>(region.first);
        for (auto& slider : region.second)
        {
            data.SlidersA.push_back({ "", static_cast<int32_t>(slider.first), slider.second});
        }
        FacialBoneRegionDataA.emplace_back(data);
    }

    auto& materialDatabase = (*g_materialDatabase);
    for (auto& partData : data.HeadPartDataA)
    {
        PostBlendFaceCustomizationLayer layer{
            partData.intensity / 128.0f,
            partData.group,
            {partData.name},
        };

        std::string colorName;
        auto material = materialDatabase.materialMaps[2].find(partData.group.c_str());
        if (material != materialDatabase.materialMaps[2].end())
        {
            for (auto it = material->Value->entryBegin; it != material->Value->entryEnd; ++it)
            {
                if (it->color.a == partData.color.a && it->color.r == partData.color.r && it->color.g == partData.color.g && it->color.b == partData.color.b)
                {
                    colorName = it->name.c_str();
                    break;
                }
            }
        }
        if (!colorName.empty())
        {
            layer.ModulationValue.Value = colorName;
        }
        else if(partData.color.a != 0 || partData.color.b != 0 || partData.color.g != 0 || partData.color.r != 0)
        {
            layer.ModulationValue.CustomColorValue = { partData.color.a, partData.color.b, partData.color.g, partData.color.r };
        }

        PostBlendFaceCustomization.LayersA.emplace_back(layer);
    }

    SkinTone = data.SkinTone;
    TeethCustomization = data.Teeth;
    JewelryColor = data.JewelryColor;
    EyeColor = data.EyeColor;
    HairColor = data.HairColor;
    FacialHairColor = data.FacialHairColor;
    BrowHairColor = data.EyebrowColor;
    return true;
}

bool NPCPreset::ToPreset(PresetData& data, IPresetInterface::ErrorVisitor* visitor)
{
    BSFixedString raceName = RaceFormID.c_str();
    auto raceForm = TESForm::GetFormByEditorID(raceName.c_str());
    if (!raceForm) {
        if (visitor)
            visitor->Error(std::format("Invalid race {} specified", raceName.c_str()).c_str());
        return false;
    }
    else if (raceForm->formType != static_cast<u8>(FormType::kRACE))
    {
        if (visitor)
            visitor->Error(std::format("Form resolved by {} is not a race form", raceName.c_str()).c_str());
        return false;
    }

    data.Race = static_cast<TESRace*>(raceForm);
    data.Gender = 0;
    auto genderName = Sex;
    if (_stricmp(genderName.c_str(), "Female") == 0) {
        data.Gender = 1;
    }

    data.MorphWeight.x = MorphWeights.x;
    data.MorphWeight.y = MorphWeights.y;
    data.MorphWeight.z = MorphWeights.z;

    data.BodyMorphRegionValuesA = std::make_unique<std::vector<float>>();
    for (auto f : BodyMorphRegionValuesA)
    {
        data.BodyMorphRegionValuesA->emplace_back(f);
    }

    data.ShapeBlendData = std::make_unique<std::unordered_map<std::string, float>>();
    for (auto& item : FacialMorphSliderDataA)
    {
        data.ShapeBlendData->emplace(item.Name.c_str(), item.Value);
    }

    for (auto& part : UniqueHeadPartsA)
    {
        if (part.empty())
            continue;

        auto headPart = TESForm::GetFormByEditorID(part.c_str());
        if (!headPart || headPart->formType != static_cast<u8>(FormType::kHDPT))
            continue;

        data.HeadPartsA.push_back(static_cast<BGSHeadPart*>(headPart));
    }

    for (auto& part : MiscHeadPartsA)
    {
        if (part.empty())
            continue;

        auto headPart = TESForm::GetFormByEditorID(part.c_str());
        if (!headPart || headPart->formType != static_cast<u8>(FormType::kHDPT))
            continue;

        data.HeadPartsA.push_back(static_cast<BGSHeadPart*>(headPart));
    }

    auto& materialDatabase = (*g_materialDatabase);
    auto findMaterialEntry = [&](const uint32_t type, const char* name, const char* value) -> BGSAVMData::Entry*
    {
        auto material = materialDatabase.materialMaps[type].find(name);
        if (material != materialDatabase.materialMaps[type].end())
        {
            for (auto it = material->Value->entryBegin; it != material->Value->entryEnd; ++it)
            {
                if (it->name == BSFixedString(value))
                {
                    return it;
                    break;
                }
            }
        }
        return nullptr;
    };

    for (auto& layer : PostBlendFaceCustomization.LayersA)
    {
        bool isComplex = false;
        std::string texture; // Lookup texture and color at runtime
        BGSAVMData::Entry* materialEntry = nullptr;

        if (!layer.Value.Value.empty())
        {
            auto material = materialDatabase.materialMaps[BGSAVMData::Type::COMPLEX].find(layer.Name.c_str());
            if (material != materialDatabase.materialMaps[BGSAVMData::Type::COMPLEX].end())
            {
                for (auto it = material->Value->entryBegin; it != material->Value->entryEnd; ++it)
                {
                    auto found = findMaterialEntry(BGSAVMData::Type::SIMPLE, it->name.c_str(), layer.Value.Value.c_str());
                    if (found)
                    {
                        materialEntry = found;
                        isComplex = true;
                        break;
                    }
                }
            }

            if(!materialEntry)
                materialEntry = findMaterialEntry(BGSAVMData::Type::SIMPLE, layer.Name.c_str(), layer.Value.Value.c_str());
            if (materialEntry) {
                texture = materialEntry->textureOrAVM.c_str();
            }
        }

        u8 r = 0, g = 0, b = 0, a = 0;
        if (!layer.ModulationValue.Value.empty())
        {
            // Find the modulation map
            auto material = findMaterialEntry(BGSAVMData::Type::MODULATION, layer.Name.c_str(), layer.ModulationValue.Value.c_str());
            if (material)
            {
                r = material->color.r;
                g = material->color.g;
                b = material->color.b;
                a = material->color.a;
            }
        }

        if (layer.ModulationValue.CustomColorValue.has_value())
        {
            r = layer.ModulationValue.CustomColorValue.value().Red;
            g = layer.ModulationValue.CustomColorValue.value().Green;
            b = layer.ModulationValue.CustomColorValue.value().Blue;
            a = layer.ModulationValue.CustomColorValue.value().Rough;
        }

        PresetData::HeadPartData item{
            isComplex ? 2 : 1,
            0,
            layer.Name,
            layer.Value.Value,
            texture,
            PresetData::HeadPartData::Color{
                a,
                b,
                g,
                r,
            },
            static_cast<u32>(layer.Intensity * 128.0f)
        };
        data.HeadPartDataA.push_back(item);
    }


    data.Morphs = std::make_unique<std::unordered_map<u32, std::unordered_map<std::string, float>>>();
    data.AdditionalSliders = std::make_unique<std::unordered_map<u32, float>>();

    for (auto& region : FacialBoneRegionDataA)
    {
        for (auto& slider : region.SlidersA)
        {
            if (slider.ID == 0 && !slider.GroupName.empty())
            {
                (*data.Morphs)[region.RegionID].insert_or_assign(slider.GroupName, slider.Value);
            }
            else
            {
                data.AdditionalSliders->insert_or_assign(slider.ID, slider.Value);
            }
        }
    }

    data.SkinTone = SkinTone;
    data.Teeth = TeethCustomization;
    data.JewelryColor = JewelryColor;
    data.EyeColor = EyeColor;
    data.HairColor = HairColor;
    data.FacialHairColor = FacialHairColor;
    data.EyebrowColor = BrowHairColor;
    return false;
}