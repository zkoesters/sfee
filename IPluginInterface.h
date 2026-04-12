#pragma once

#include <cstdint>

class TESNPC;

class IPluginInterface
{
public:
	IPluginInterface() { };
	virtual ~IPluginInterface() { };

	class ErrorVisitor
	{
	public:
		virtual void Error(const char* errorLine) = 0;
	};

	class StringVisitor
	{
	public:
		virtual void String(const char* data) = 0;
	};

	virtual std::uint32_t GetVersion() const = 0;
};

class IInterfaceMap
{
public:
	virtual IPluginInterface* QueryInterface(const char* name) = 0;
	virtual bool AddInterface(const char* name, IPluginInterface* pluginInterface) = 0;
	virtual IPluginInterface* RemoveInterface(const char* name) = 0;
};

struct InterfaceExchangeMessage
{
	enum
	{
		kMessage_ExchangeInterface = 0x9E3779B9
	};

	IInterfaceMap* interfaceMap = nullptr;
};

class IPresetInterface : public IPluginInterface
{
public:
	enum : std::uint32_t
	{
		kPluginVersion1 = 1,
		kFileVersion1 = 1,
		kFileVersion2,
		kCurrentPluginVersion = kPluginVersion1,
	};

	class DependencyVisitor : public ErrorVisitor
	{
	public:
		virtual void PassDependency(const char* file) = 0;
		virtual void FailDependency(const char* file) = 0;
	};

	enum class Directory
	{
		DATA,
		DOCUMENTS
	};

	// Returns true if All dependencies passed, false if any failed, or the file failed to parse, visitor provides details
	virtual bool QueryPresetDependencies(const char* filePath, DependencyVisitor* visitor) = 0;
	virtual bool LoadPreset(const char* filePath, TESNPC* target, ErrorVisitor* errorVisitor = nullptr) = 0;
	virtual bool SavePreset(const char* filePath, TESNPC* source, ErrorVisitor* errorVisitor = nullptr) = 0;

	virtual void GetDirectory(const Directory& dir, StringVisitor& visitor) = 0;
};

class IChargenInterface : public IPluginInterface
{
public:
	enum : std::uint32_t
	{
		kPluginVersion1 = 1,
		kFileVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};

	enum class Gender
	{
		BOTH = -1,
		MALE = 0,
		FEMALE
	};

	class MorphTargetSliderVisitor
	{
	public:
		virtual void Visit(const char* morphKey, const char* displayName, const char* identifier, const std::int64_t order) = 0;
	};

	// Adds a Morph Target slider to the Body section of the menu, overwrites a previous entry if it exists
	// Identifier should be your ESM/ESP/ESL name so that sliders can be identified by Chargen as dependencies to a Preset
	// Sorting is Order > Identifier > DisplayName ASC
	virtual void AddMorphTargetSlider(const char* morphKey, const char* displayName, const char* identifier, const Gender& gender, const std::int64_t order = 0) = 0;

	virtual void ForEachSlider(const Gender& gender, MorphTargetSliderVisitor& visitor) = 0;
};