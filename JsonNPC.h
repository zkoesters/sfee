#pragma once

#include "PresetInterface.h"
#include "JsonUtils.h"

struct NPCPreset
{
    struct FacialMorphSliderData
    {
        std::string Name;
        float Value;
    };

    struct PostBlendFaceCustomizationLayer
    {
        float Intensity;
        std::string Name;

        struct Modulation
        {
            struct CustomColor
            {
                uint8_t Rough;
                uint8_t Blue;
                uint8_t Green;
                uint8_t Red;
            };
            std::string Value;
            std::optional<CustomColor> CustomColorValue;
        };
        struct ValueEntry
        {
            std::string Value;
        };
        ValueEntry Value;
        Modulation ModulationValue;
    };
    struct PostBlendFaceCustomizations
    {
        std::vector<PostBlendFaceCustomizationLayer> LayersA;
    };
    struct FacialBoneRegionData
    {
        int32_t RegionID;
        struct Slider
        {
            std::string GroupName;
            int32_t ID;
            float Value;
        };
        std::vector<Slider> SlidersA;
    };
    struct MorphWeight
    {
        float x;
        float y;
        float z;
    };

    std::vector<float> BodyMorphRegionValuesA;
    std::string BrowHairColor;
    std::string EyeColor;
    std::vector<FacialBoneRegionData> FacialBoneRegionDataA;
    std::string FacialHairColor;
    std::vector<FacialMorphSliderData> FacialMorphSliderDataA;
    std::string HairColor;
    std::string JewelryColor;
    std::vector<std::string> MiscHeadPartsA;
    MorphWeight MorphWeights;
    std::string NPCFormEditorID;
    PostBlendFaceCustomizations PostBlendFaceCustomization;
    std::string RaceFormID;
    std::string Sex;
    uint32_t SkinTone;
    std::string TeethCustomization;
    std::vector<std::string> UniqueHeadPartsA;

    bool FromPreset(const PresetData& data, IPresetInterface::ErrorVisitor* visitor);
    bool ToPreset(PresetData& data, IPresetInterface::ErrorVisitor* visitor);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::FacialMorphSliderData, Name, Value);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::PostBlendFaceCustomizationLayer::Modulation::CustomColor, Blue, Green, Red, Rough);
inline void to_json(nlohmann::json& nlohmann_json_j, const NPCPreset::PostBlendFaceCustomizationLayer::Modulation& nlohmann_json_t) {
    nlohmann_json_j["Value"] = nlohmann_json_t.Value; 
    if(nlohmann_json_t.CustomColorValue.has_value()) nlohmann_json_j["CustomColorValue"] = nlohmann_json_t.CustomColorValue;
} 
inline void from_json(const nlohmann::json& nlohmann_json_j, NPCPreset::PostBlendFaceCustomizationLayer::Modulation& nlohmann_json_t) {
    NPCPreset::PostBlendFaceCustomizationLayer::Modulation nlohmann_json_default_obj; 
    nlohmann_json_t.Value = nlohmann_json_j.value("Value", nlohmann_json_default_obj.Value); 
    nlohmann_json_t.CustomColorValue = nlohmann_json_j.value("CustomColorValue", nlohmann_json_default_obj.CustomColorValue);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::PostBlendFaceCustomizationLayer::ValueEntry, Value);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::PostBlendFaceCustomizationLayer, Intensity, Name, Value, ModulationValue);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::PostBlendFaceCustomizations, LayersA);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::FacialBoneRegionData::Slider, GroupName, ID, Value);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::FacialBoneRegionData, RegionID, SlidersA);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset::MorphWeight, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NPCPreset, 
    BodyMorphRegionValuesA,
    BrowHairColor,
    EyeColor,
    FacialBoneRegionDataA,
    FacialHairColor,
    FacialMorphSliderDataA,
    HairColor,
    JewelryColor,
    MiscHeadPartsA,
    MorphWeights,
    NPCFormEditorID,
    PostBlendFaceCustomization,
    RaceFormID,
    Sex,
    SkinTone,
    TeethCustomization,
    UniqueHeadPartsA
);