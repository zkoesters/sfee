#include "ChargenInterface.h"
#include "ChargenInterface.h"
#include "FileUtils.h"
#include "FormUtils.h"

#include "sfse/GameStreams.h"
#include "sfse/GameData.h"
#include "sfse/GameObjects.h"
#include "sfse/GameChargen.h"

#include <optional>

#define JSON_DIAGNOSTICS 1
#define NLOHMANN_JSON_TO_OPT(v1) if (nlohmann_json_t.v1 != std::nullopt) nlohmann_json_j[#v1] = nlohmann_json_t.v1.value();
#define NLOHMANN_JSON_FROM_OPT(v1) if (nlohmann_json_j.contains(#v1)) nlohmann_json_t.v1 = nlohmann_json_j[#v1].template get<decltype(nlohmann_json_t.v1)::value_type>(); else nlohmann_json_t.v1.reset();
#define NLOHMANN_JSON_FROM_DEFAULT(v1, v2) nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, v2);
#include <nlohmann/json.hpp>

#include "SimpleIni.h"

namespace Regions
{
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Region::SliderObject::BoneObject::Extent::Vec3, x, y, z)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Region::SliderObject::BoneObject::Extent, Position, Rotation, Scale)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Region::SliderObject::BoneObject, Bone, Minima, Maxima)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Region::SliderObject, Name, BonesA, ID, ZeroToOne)

    inline void to_json(nlohmann::json& nlohmann_json_j, const Region& nlohmann_json_t)
    {
        NLOHMANN_JSON_TO(SlidersA)
        NLOHMANN_JSON_TO(Name)
        NLOHMANN_JSON_TO_OPT(ID)
    }

    inline void from_json(const nlohmann::json& nlohmann_json_j, Region& nlohmann_json_t)
    {
        NLOHMANN_JSON_FROM(SlidersA)
        NLOHMANN_JSON_FROM(Name)
        NLOHMANN_JSON_FROM_OPT(ID)
    }
}

namespace MorphTargets
{
    inline void to_json(nlohmann::json& nlohmann_json_j, const Slider& nlohmann_json_t)
    {
        NLOHMANN_JSON_TO(Key)
        NLOHMANN_JSON_TO(Name)
        NLOHMANN_JSON_TO(Order)
    }

    inline void from_json(const nlohmann::json& nlohmann_json_j, Slider& nlohmann_json_t)
    {
        NLOHMANN_JSON_FROM(Key)
        NLOHMANN_JSON_FROM(Name)
        NLOHMANN_JSON_FROM_DEFAULT(Order, 0)
    }
}

void ChargenInterface::LoadSliderMods(ErrorVisitor* visitor)
{
    FileUtils::forEachMod([&](const TESFile* modFile)
    {
        auto modPath = std::format("Data/SFSE/Plugins/Chargen/Sliders/{}", modFile->filePath);
        auto iniPath = std::format("{}/config.ini", modPath);

        BSResourceNiBinaryStream iniStream(iniPath.c_str());
        if (iniStream)
        {
            auto iniData = std::make_unique<char[]>(iniStream.GetSize() + 1);
            iniStream.DoRead(iniData.get(), iniStream.GetSize());

            CSimpleIniA ini;
            SI_Error rc = ini.LoadData(iniData.get(), iniStream.GetSize());
            if (rc == SI_OK)
            {
                auto dataHandler = TESDataHandler::GetSingleton();
                auto raceForm = static_cast<TESRace*>(TESForm::GetFormByEditorID("HumanRace"));

                auto loadBoneFile = [&](const std::string& filePath, const Gender& gender)
                {
                    auto file = std::format("{}/{}", modPath, filePath);
                    if (raceForm && raceForm->formType == (u8)FormType::kRACE)
                    {
                        std::vector<Regions::Region> regions;
                        if (LoadBoneSliders(file.c_str(), regions, visitor))
                        {
                            InjectBoneSliders(modFile, raceForm, gender, regions);
                        }
                    }
                };

                auto loadMorphTargetFile = [&](const std::string& filePath, const Gender& gender)
                {
                    auto file = std::format("{}/{}", modPath, filePath);
                    std::vector<MorphTargets::Slider> sliders;
                    if (LoadMorphTargetSliders(file.c_str(), sliders, visitor))
                    {
                        InjectMorphTargetSliders(modFile, gender, sliders);
                    }
                };


                auto maleBoneFile = ini.GetValue("Bone", "Male");
                if (maleBoneFile)
                {
                    loadBoneFile(maleBoneFile, Gender::MALE);
                }
                auto femaleBoneFile = ini.GetValue("Bone", "Female");
                if (femaleBoneFile)
                {
                    loadBoneFile(femaleBoneFile, Gender::FEMALE);
                }

                auto maleMorphFile = ini.GetValue("MorphTargets", "Male");
                if (maleMorphFile)
                {
                    loadMorphTargetFile(maleMorphFile, Gender::MALE);
                }
                auto femaleMorphFile = ini.GetValue("MorphTargets", "Female");
                if (femaleMorphFile)
                {
                    loadMorphTargetFile(femaleMorphFile, Gender::FEMALE);
                }
            }
        }
    });
}

void ChargenInterface::InjectBoneSliders(const TESFile* modFile, TESRace* target, const Gender& gender, const std::vector<Regions::Region>& regions)
{
    auto findGroupByName = [](TESRace::ChargenData* chargenData, const char* name) -> BSTScatterTableDefaultKVStorage<TESRace::FaceMorphID, TESRace::ChargenData::FaceMorphData*>*
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

    auto addBoneSlider = [&](std::uint8_t genderIndex)
    {
        auto& chargenData = target->chargenData[genderIndex];
        for (auto& region : regions)
        {
            if (region.ID != std::nullopt)
            {
                continue;
            }

            auto faceMorph = findGroupByName(chargenData, region.Name.c_str());
            if (!faceMorph)
            {
                continue;
            }

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
    };

    switch (gender)
    {
    case Gender::BOTH:
        addBoneSlider(0);
        addBoneSlider(1);
        break;
    case Gender::MALE:
        addBoneSlider(0);
        break;
    case Gender::FEMALE:
        addBoneSlider(1);
        break;
    }
}

void ChargenInterface::AddSlider(const std::string& Key, const std::string& Name, const std::string& Identifier, const Gender& gender, const std::int64_t order)
{
    std::lock_guard<std::recursive_mutex> locker(m_sliderLock);

    auto addSlider = [&](std::uint8_t genderIndex)
    {
        auto it = m_morphSet[genderIndex].find(Key);
        if (it == m_morphSet[genderIndex].end())
        {
            m_sliders[genderIndex].push_back({
                Identifier,
                Name,
                Key,
                order
                });
        }
        else
        {
            auto it = std::find_if(m_sliders[genderIndex].begin(), m_sliders[genderIndex].end(), [&](const Slider& slider) { return slider.key == Key; });
            if (it != m_sliders[genderIndex].end())
            {
                it->displayName = Name;
                it->identifier = Identifier;
                it->order = order;
            }
        }
    };

    switch (gender)
    {
    case Gender::BOTH:
        addSlider(0);
        addSlider(1);
        break;
    case Gender::MALE:
        addSlider(0);
        break;
    case Gender::FEMALE:
        addSlider(1);
        break;
    }
}

void ChargenInterface::SortGenderSliders(std::uint8_t genderIndex)
{
    std::sort(m_sliders[genderIndex].begin(), m_sliders[genderIndex].end(), [](const Slider& a, const Slider& b)
    {
        if (a.order == b.order) {
            if (a.identifier == b.identifier) {
                return a.displayName < b.displayName;
            }
            return a.identifier < b.identifier;
        }
        return a.order < b.order;
    });
}

void ChargenInterface::SortSliders(const Gender& gender)
{
    std::lock_guard<std::recursive_mutex> locker(m_sliderLock);
    switch (gender)
    {
    case Gender::BOTH:
        SortGenderSliders(0);
        SortGenderSliders(1);
        break;
    case Gender::MALE:
        SortGenderSliders(0);
        break;
    case Gender::FEMALE:
        SortGenderSliders(1);
        break;
    }
}

void ChargenInterface::AddMorphTargetSlider(const char* morphKey, const char* identifier, const char* displayName, const Gender& gender, const std::int64_t order)
{
    AddSlider(morphKey, displayName, identifier, gender, order);
    SortSliders(gender);
}

void ChargenInterface::ForEachSlider(const Gender& gender, MorphTargetSliderVisitor& visitor)
{
    std::lock_guard<std::recursive_mutex> locker(m_sliderLock);

    auto visitSliders = [&](std::uint8_t genderIndex, MorphTargetSliderVisitor& visitor)
    {
        for (auto& slider : m_sliders[genderIndex])
        {
            visitor.Visit(slider.key.c_str(), slider.displayName.c_str(), slider.identifier.c_str(), slider.order);
        }
    };

    switch (gender)
    {
    case Gender::BOTH:
        visitSliders(0, visitor);
        visitSliders(1, visitor);
        break;
    case Gender::MALE:
        visitSliders(0, visitor);
        break;
    case Gender::FEMALE:
        visitSliders(1, visitor);
        break;
    }
}

const TESFile* ChargenInterface::GetSliderDependency(const char* morphKey, const Gender& gender) const
{
    std::lock_guard<std::recursive_mutex> locker(m_sliderLock);

    auto getDependency = [&](std::uint8_t genderIndex) -> const TESFile*
    {
        auto it = std::find_if(m_sliders[genderIndex].begin(), m_sliders[genderIndex].end(), [&](const Slider& slider) { return slider.key == morphKey; });
        if (it != m_sliders[genderIndex].end())
        {
            return FormUtils::GetModByName(it->identifier.c_str());
        }
        return nullptr;
    };

    switch (gender)
    {
    case Gender::MALE:
        return getDependency(0);
    case Gender::FEMALE:
        return getDependency(1);
    default:
        return nullptr;
    }
}

bool ChargenInterface::LoadBoneSliders(const char* filePath, std::vector<Regions::Region>& regions, ErrorVisitor* visitor)
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

bool ChargenInterface::LoadMorphTargetSliders(const char* filePath, std::vector<MorphTargets::Slider>& sliders, ErrorVisitor* visitor)
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
            sliders = root.template get<std::vector<MorphTargets::Slider>>();
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

void ChargenInterface::InjectMorphTargetSliders(const TESFile* modName, const Gender& gender, const std::vector<MorphTargets::Slider>& sliders)
{
    for (auto& slider : sliders)
    {
        AddSlider(slider.Key, modName->filePath, slider.Name, gender, slider.Order);
    }
    SortSliders(gender);
}