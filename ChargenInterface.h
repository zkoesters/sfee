#pragma once

#include "IPluginInterface.h"

#include <optional>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_set>
#include <map>

class TESRace;
class TESFile;

namespace Regions
{
struct Region
{
    struct SliderObject
    {
        struct BoneObject
        {
            std::string Bone;
            struct Extent {
                struct Vec3 {
                    float x = 0.0f, y = 0.0f, z = 0.0f;
                };
                Vec3 Position;
                Vec3 Rotation;
                Vec3 Scale;
            };
            Extent Minima;
            Extent Maxima;
        };
        std::vector<BoneObject> BonesA;
        std::uint32_t ID;
        std::string Name;
        bool ZeroToOne;
    };
    std::vector<SliderObject> SlidersA;
    std::optional<std::uint32_t> ID;
    std::string Name;
};
}

namespace MorphTargets
{
struct Slider
{
    std::string Name;
    std::string Key;
    std::int64_t Order;
};
}

class ChargenInterface : public IChargenInterface
{
public:
    virtual std::uint32_t GetVersion() const override { return kCurrentPluginVersion; }

    void LoadSliderMods(ErrorVisitor* visitor = nullptr);

    bool LoadBoneSliders(const char* filePath, std::vector<Regions::Region>& regions, ErrorVisitor* visitor = nullptr);
    void InjectBoneSliders(const TESFile* modName, TESRace* target, const Gender& gender, const std::vector<Regions::Region>& regions);

    bool LoadMorphTargetSliders(const char* filePath, std::vector<MorphTargets::Slider>& sliders, ErrorVisitor* visitor);
    void InjectMorphTargetSliders(const TESFile* modName, const Gender& gender, const std::vector<MorphTargets::Slider>& sliders);

    void SortSliders(const Gender& gender);

    const TESFile* GetSliderDependency(const char* morphKey, const Gender& gender) const;

    // Inherited via IChargenInterface
    virtual void AddMorphTargetSlider(const char* morphKey, const char* displayName, const char* identifier, const Gender& gender, const std::int64_t order) override;
    virtual void ForEachSlider(const Gender& gender, MorphTargetSliderVisitor& visitor) override;

private:
    void AddSlider(const std::string& Key, const std::string& Name, const std::string& Identifier, const Gender& gender, const std::int64_t order);
    void SortGenderSliders(std::uint8_t genderIndex);

    mutable std::recursive_mutex m_sliderLock;
    std::unordered_set<std::string> m_morphSet[2];
    struct Slider
    {
        std::string displayName;
        std::string identifier;
        std::string key;
        std::int64_t order;
    };
    std::vector<Slider> m_sliders[2];
};