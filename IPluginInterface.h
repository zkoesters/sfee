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

class IDataInterface : public IPluginInterface
{
public:
	enum : std::uint32_t
	{
		kPluginVersion1 = 1,
		kFileVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};
};