//
// Copyright (c) 2017-2019 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <regex>
#include <EASTL/sort.h>

#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/Archive.h>
#include <Urho3D/IO/ArchiveSerialization.h>
#include <Urho3D/Resource/JSONArchive.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/ResourceEvents.h>
#include <Urho3D/SystemUI/SystemUI.h>
#include <Tabs/ConsoleTab.h>
#include <Tabs/ResourceTab.h>
#include <Toolbox/SystemUI/Widgets.h>

#include "Editor.h"
#include "EditorEvents.h"
#include "Project.h"
#include "Plugins/ModulePlugin.h"
#include "Plugins/ScriptBundlePlugin.h"
#include "Tabs/Scene/SceneTab.h"


namespace Urho3D
{

Project::Project(Context* context)
    : Object(context)
    , pipeline_(new Pipeline(context))
#if URHO3D_PLUGINS
    , plugins_(new PluginManager(context))
#endif
{
    SubscribeToEvent(E_EDITORRESOURCESAVED, [this](StringHash, VariantMap&) { SaveProject(); });
    SubscribeToEvent(E_RESOURCERENAMED, [this](StringHash, VariantMap& args) {
        using namespace ResourceRenamed;
        if (args[P_FROM].GetString() == defaultScene_)
            defaultScene_ = args[P_TO].GetString();
    });
    SubscribeToEvent(E_RESOURCEBROWSERDELETE, [this](StringHash, VariantMap& args) {
        using namespace ResourceBrowserDelete;
        if (args[P_NAME].GetString() == defaultScene_)
            defaultScene_ = EMPTY_STRING;
    });

    SubscribeToEvent(E_ENDFRAME, [this](StringHash, VariantMap&) {
        // Save project every minute.
        // TODO: Make save time configurable.
        if (saveProjectTimer_.GetMSec(false) >= 60000)
        {
            SaveProject();
            saveProjectTimer_.Reset();
        }
    });
    context_->RegisterSubsystem(pipeline_);
    context_->RegisterSubsystem(plugins_);
}

Project::~Project()
{
    context_->RemoveSubsystem(pipeline_->GetType());
    context_->RemoveSubsystem(plugins_->GetType());

    if (context_->GetSystemUI())
        ui::GetIO().IniFilename = nullptr;

    if (auto* cache = context_->GetCache())
    {
        cache->RemoveResourceDir(GetCachePath());
        cache->RemoveResourceDir(GetResourcePath());

        for (const auto& path : cachedEngineResourcePaths_)
            context_->GetCache()->AddResourceDir(path);
        cache->SetAutoReloadResources(false);
    }

    if (auto* editor = GetSubsystem<Editor>())
        editor->UpdateWindowTitle();
}

bool Project::LoadProject(const ea::string& projectPath)
{
    if (!projectFileDir_.empty())
        // Project is already loaded.
        return false;

    if (projectPath.empty())
        return false;

    projectFileDir_ = AddTrailingSlash(projectPath);

    if (!context_->GetFileSystem()->Exists(GetCachePath()))
        context_->GetFileSystem()->CreateDirsRecursive(GetCachePath());

    if (!context_->GetFileSystem()->Exists(GetResourcePath()))
    {
        // Initialize new project
        context_->GetFileSystem()->CreateDirsRecursive(GetResourcePath());

        for (const auto& path : context_->GetCache()->GetResourceDirs())
        {
            if (path.ends_with("/EditorData/") || path.contains("/Autoload/"))
                continue;

            StringVector names;

            URHO3D_LOGINFOF("Importing resources from '%s'", path.c_str());

            // Copy default resources to the project.
            context_->GetFileSystem()->ScanDir(names, path, "*", SCAN_FILES, false);
            for (const auto& name : names)
                context_->GetFileSystem()->Copy(path + name, GetResourcePath() + name);

            context_->GetFileSystem()->ScanDir(names, path, "*", SCAN_DIRS, false);
            for (const auto& name : names)
            {
                if (name == "." || name == "..")
                    continue;
                context_->GetFileSystem()->CopyDir(path + name, GetResourcePath() + name);
            }
        }
    }

    // Unregister engine dirs
    auto enginePrefixPath = GetSubsystem<Editor>()->GetCoreResourcePrefixPath();
    auto pathsCopy = context_->GetCache()->GetResourceDirs();
    cachedEngineResourcePaths_.clear();
    for (const auto& path : pathsCopy)
    {
        if (path.starts_with(enginePrefixPath) && !path.ends_with("/EditorData/"))
        {
            cachedEngineResourcePaths_.emplace_back(path);
            context_->GetCache()->RemoveResourceDir(path);
        }
    }

    if (context_->GetSystemUI())
    {
        uiConfigPath_ = projectFileDir_ + ".ui.ini";
        isNewProject_ = !context_->GetFileSystem()->FileExists(uiConfigPath_);
        ui::GetIO().IniFilename = uiConfigPath_.c_str();
    }

#if URHO3D_HASH_DEBUG
    // StringHashNames.json
    {
        ea::string filePath(projectFileDir_ + "StringHashNames.json");
        if (context_->GetFileSystem()->Exists(filePath))
        {
            JSONFile file(context_);
            if (!file.LoadFile(filePath))
                return false;

            for (const auto& value : file.GetRoot().GetArray())
            {
                // Seed global string hash to name map.
                StringHash hash(value.GetString());
                (void) (hash);
            }
        }
    }
#endif

    // Register asset dirs
    context_->GetCache()->AddResourceDir(GetCachePath(), 0);
    context_->GetCache()->AddResourceDir(GetResourcePath(), 1);
    context_->GetCache()->SetAutoReloadResources(true);

#if URHO3D_PLUGINS
    if (!context_->GetEngine()->IsHeadless())
    {
        // Normal execution cleans up old versions of plugins.
        StringVector files;
        context_->GetFileSystem()->ScanDir(files, context_->GetFileSystem()->GetProgramDir(), "", SCAN_FILES, false);
        for (const ea::string& fileName : files)
        {
            if (std::regex_match(fileName.c_str(), std::regex("^.*[0-9]+\\.(dll|dylib|so)$")))
                context_->GetFileSystem()->Delete(context_->GetFileSystem()->GetProgramDir() + fileName);
        }
    }
#endif

    // Project.json
    ea::string filePath(projectFileDir_ + "Project.json");
    JSONFile file(context_);
    if (context_->GetFileSystem()->Exists(filePath))
    {
        if (!file.LoadFile(filePath))
            return false;
    }
    // Loading is performed even on empty file. Give a chance for serialization function to do default setup in case of missing data.
    JSONInputArchive archive(&file);
    return Serialize(archive);
}

bool Project::SaveProject()
{
    if (context_->GetEngine()->IsHeadless())
    {
        URHO3D_LOGERROR("Headless instance is supposed to use project as read-only.");
        return false;
    }

    // Saving project data of tabs may trigger saving resources, which in turn triggers saving editor project. Avoid
    // that loop.
    UnsubscribeFromEvent(E_EDITORRESOURCESAVED);

    if (projectFileDir_.empty())
    {
        URHO3D_LOGERROR("Unable to save project. Project path is empty.");
        return false;
    }

    ui::SaveIniSettingsToDisk(uiConfigPath_.c_str());

#if URHO3D_HASH_DEBUG
    // StringHashNames.json
    {
        auto hashNames = StringHash::GetGlobalStringHashRegister()->GetInternalMap().values();
        ea::quick_sort(hashNames.begin(), hashNames.end());
        JSONFile file(context_);
        JSONArray names;
        for (const auto& string : hashNames)
            names.push_back(string);
        file.GetRoot() = names;

        ea::string filePath(projectFileDir_ + "StringHashNames.json");
        if (!file.SaveFile(filePath))
        {
            projectFileDir_.clear();
            URHO3D_LOGERRORF("Saving StringHash names to '%s' failed", filePath.c_str());
            return false;
        }
    }
#endif

    // Project.json
    JSONFile file(context_);
    JSONOutputArchive archive(&file);
    if (!Serialize(archive))
        return false;

    ea::string filePath(projectFileDir_ + "Project.json");
    if (!file.SaveFile(filePath))
    {
        projectFileDir_.clear();
        URHO3D_LOGERROR("Saving project to '{}' failed", filePath);
        return false;
    }

    return true;
}

bool Project::Serialize(Archive& archive)
{
    const int version = 1;
    if (!archive.IsInput() && context_->GetEngine()->IsHeadless())
    {
        URHO3D_LOGERROR("Headless instance is supposed to use project as read-only.");
        return false;
    }

    // Saving project data of tabs may trigger saving resources, which in turn triggers saving editor project. Avoid that loop.
    UnsubscribeFromEvent(E_EDITORRESOURCESAVED);

    if (auto projectBlock = archive.OpenUnorderedBlock("project"))
    {
        int archiveVersion = version;
        SerializeValue(archive, "version", archiveVersion);
        SerializeValue(archive, "defaultScene", defaultScene_);

        if (!pipeline_->Serialize(archive))
            return false;

        if (!plugins_->Serialize(archive))
            return false;

        using namespace EditorProjectSerialize;
        SendEvent(E_EDITORPROJECTSERIALIZE, P_ARCHIVE, (void*)&archive);
    }

    SubscribeToEvent(E_EDITORRESOURCESAVED, [this](StringHash, VariantMap&) { SaveProject(); });

    return true;
}

ea::string Project::GetCachePath() const
{
    if (projectFileDir_.empty())
        return EMPTY_STRING;
    return projectFileDir_ + "Cache/";
}

ea::string Project::GetResourcePath() const
{
    if (projectFileDir_.empty())
        return EMPTY_STRING;
    return projectFileDir_ + "Resources/";
}

}
