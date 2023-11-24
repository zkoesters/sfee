#pragma once

#include <string>
#include <functional>

class TESFile;

namespace FileUtils
{
    std::string GetExecutablePath();
    std::string GetDocumentsPath();

    void forEachMod(std::function<void(const TESFile*)> functor);
}