#include "FileUtils.h"

#include <shlobj.h>
#include <winerror.h>
#include <filesystem>
#include <minwindef.h>

#include "sfse/GameData.h"

namespace FileUtils
{
    std::string GetExecutablePath()
    {
        char tempPath[MAX_PATH] = { 0 };
        GetModuleFileName(nullptr, tempPath, MAX_PATH);
        return std::filesystem::path(tempPath).remove_filename().string();
    }
    std::string GetDocumentsPath()
    {
        char tempPath[MAX_PATH] = { 0 };
        HRESULT err = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, tempPath);
        if (SUCCEEDED(err))
        {
            std::filesystem::path path(tempPath);
            path += "\\My Games\\Starfield\\";
            return path.string();
        }
        return "";
    }

    void forEachMod(std::function<void(const TESFile*)> functor)
    {
        auto dataHandler = TESDataHandler::GetSingleton();

        // Try normal plugin
        for (auto item : dataHandler->CompiledFileCollection.FileA)
        {
            functor(item);
        }

        // Try small file
        for (auto item : dataHandler->CompiledFileCollection.SmallFileA)
        {
            functor(item);
        }
    }
}