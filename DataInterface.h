#pragma once

#include "IPluginInterface.h"

#include <optional>
#include <vector>
#include <string>

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
                    float x, y, z;
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

class DataInterface : public IDataInterface
{
public:
    virtual std::uint32_t GetVersion() const override { return kCurrentPluginVersion; }

    bool LoadBoneSliders(const char* filePath, std::vector<Regions::Region>& regions, ErrorVisitor* visitor = nullptr);
    void LoadSliderMods(ErrorVisitor* visitor = nullptr);

    void InjectBoneSliders(const TESFile* modName, TESRace* target, bool isFemale, const std::vector<Regions::Region>& regions);
};