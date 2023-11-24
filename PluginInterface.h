#pragma once

#include "IPluginInterface.h"

#include <unordered_map>

struct SKSESerializationInterface;

class InterfaceMap
	: public IInterfaceMap
	, public std::unordered_map<const char*, IPluginInterface*>
{
public:
	virtual IPluginInterface* QueryInterface(const char* name) override;
	virtual bool AddInterface(const char* name, IPluginInterface* pluginInterface) override;
	virtual IPluginInterface* RemoveInterface(const char* name) override;
};
