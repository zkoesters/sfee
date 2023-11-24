#include "DataInterface.h"
#include "FileUtils.h"
#include "FormUtils.h"

#include "sfse/GameStreams.h"
#include "sfse/GameData.h"
#include "sfse/GameObjects.h"
#include "sfse/GameChargen.h"

#include <optional>

#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp>

#include "SimpleIni.h"

namespace Regions
{
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Region::SliderObject::BoneObject::Extent::Vec3, x, y, z)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Region::SliderObject::BoneObject::Extent, Position, Rotation, Scale)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Region::SliderObject::BoneObject, Bone, Minima, Maxima)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Region::SliderObject, Name, BonesA, ID, ZeroToOne)

    inline void to_json(nlohmann::json& j, const Region& opt)
    {
        j["SlidersA"] = opt.SlidersA;
        if (opt.ID != std::nullopt) j["ID"] = opt.ID.value();
        j["Name"] = opt.Name;
    }

    inline void from_json(const nlohmann::json& j, Region& opt)
    {
        opt.SlidersA = j["SlidersA"].template get<decltype(opt.SlidersA)>();
        if (j.contains("ID"))
            opt.ID = j["ID"].template get<decltype(opt.ID)::value_type>();
        else
            opt.ID.reset();
        opt.Name = j["Name"].template get<decltype(opt.Name)>();
    }
}

void DataInterface::LoadSliderMods(ErrorVisitor* visitor)
{
    FileUtils::forEachMod([&](const TESFile* modFile)
    {
        auto modPath = std::format("Data/SFSE/Plugins/Chargen/Sliders/{}", modFile->filePath);
        auto iniPath = std::format("{}/bone_sliders.ini", modPath);

        BSResourceNiBinaryStream iniStream(iniPath.c_str());
        if (iniStream)
        {
            auto iniData = std::make_unique<char[]>(iniStream.GetSize() + 1);
            iniStream.DoRead(iniData.get(), iniStream.GetSize());

            CSimpleIniA ini;
            SI_Error rc = ini.LoadData(iniData.get(), iniStream.GetSize());
            if (rc == SI_OK)
            {
                auto loadFileFromSection = [&](bool isFemale, auto section)
                {
                    if (section)
                    {
                        for (auto& keys : *section)
                        {
                            auto race = keys.first.pItem;
                            auto file = std::format("{}/{}", modPath, keys.second);
                            if (keys.second != nullptr && keys.second[0] != 0)
                            {
                                auto dataHandler = TESDataHandler::GetSingleton();
                                auto raceForm = static_cast<TESRace*>(TESForm::GetFormByEditorID(race));
                                if (raceForm && raceForm->formType == (u8)FormType::kRACE)
                                {
                                    std::vector<Regions::Region> regions;
                                    if (LoadBoneSliders(file.c_str(), regions, visitor))
                                    {
                                        InjectBoneSliders(modFile, raceForm, isFemale, regions);
                                    }
                                }
                            }
                        }
                    }
                };

                auto maleSection = ini.GetSection("Male");
                loadFileFromSection(false, maleSection);
                
                auto femaleSection = ini.GetSection("Female");
                loadFileFromSection(true, femaleSection);
            }
        }
    });
}

void DataInterface::InjectBoneSliders(const TESFile* modFile, TESRace* target, bool isFemale, const std::vector<Regions::Region>& regions)
{
    auto& chargenData = target->chargenData[isFemale ? 1 : 0];
    auto findGroupByName = [&](const char* name) -> BSTScatterTableDefaultKVStorage<TESRace::FaceMorphID, TESRace::ChargenData::FaceMorphData*>*
    {
        for (auto& faceMorph : chargenData->faceMorphMap)
        {
            if (faceMorph.Value)
            {
                if (faceMorph.Value->Name == BSFixedStringCS(name))
                {
                    return &faceMorph;
                }
            }
        }

        return nullptr;
    };

    for (auto& region : regions)
    {
        if (region.ID == std::nullopt)
        {
            auto faceMorph = findGroupByName(region.Name.c_str());
            if (faceMorph)
            {
                for (auto& slider : region.SlidersA)
                {
                    auto internalSlider = new BGSCharacterMorph::FacialBoneSculptSlider();
                    for (auto& bone : slider.BonesA)
                    {
                        internalSlider->BoneExtentMap.insert_or_assign({
                            bone.Bone.c_str(), new BGSCharacterMorph::FacialBoneSlider::SliderExtents(
                                { { bone.Minima.Position.x,bone.Minima.Position.y,bone.Minima.Position.z }, { bone.Minima.Rotation.x,bone.Minima.Rotation.y,bone.Minima.Rotation.z }, { bone.Minima.Scale.x, bone.Minima.Scale.y, bone.Minima.Scale.z } },
                                { { bone.Maxima.Position.x,bone.Maxima.Position.y,bone.Maxima.Position.z }, { bone.Maxima.Rotation.x,bone.Maxima.Rotation.y,bone.Maxima.Rotation.z }, { bone.Maxima.Scale.x, bone.Maxima.Scale.y, bone.Maxima.Scale.z } }
                            )
                        });
                    }

                    uint32_t sliderId = FormUtils::GetFormIdFromMod(modFile, slider.ID);
                    internalSlider->ID = sliderId;
                    internalSlider->unk40 = slider.Name.c_str();
                    internalSlider->unk48 = slider.Name.c_str();
                    internalSlider->zeroToOne = slider.ZeroToOne;
                    faceMorph->Value->PostBlendSliderA.emplace_back(sliderId);
                    chargenData->facialSliderMap.insert_or_assign({ sliderId, internalSlider });
                    chargenData->sliderCount++;
                }
            }
        }
    }
}

bool DataInterface::LoadBoneSliders(const char* filePath, std::vector<Regions::Region>& regions, ErrorVisitor* visitor)
{
    using json = nlohmann::json;

    BSResourceNiBinaryStream stream(filePath);
    if (stream)
    {
        auto data = std::make_unique<char[]>(stream.GetSize() + 1);
        stream.DoRead(data.get(), stream.GetSize());

        try
        {
            auto root = json::parse(data.get(), nullptr, true, true);
            auto j = root.template get<json::object_t>();

            regions = j["Regions"].template get<std::vector<Regions::Region>>();
        }
        catch (json::exception& except)
        {
            if (visitor)
                visitor->Error(std::format("{} : {}", filePath, except.what()).c_str());
        }

        return true;
    }

    return false;
}