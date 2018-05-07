//
// Copyright (c) 2018 Rokas Kupstys
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
#if URHO3D_PLUGINS
#   define CR_HOST
#endif
#include "Editor.h"
#include "EditorEvents.h"
#include "EditorIconCache.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/Scene/SceneSettings.h"
#include <Toolbox/IO/ContentUtilities.h>
#include <Toolbox/SystemUI/ResourceBrowser.h>
#include <Toolbox/Toolbox.h>
#include <IconFontCppHeaders/IconsFontAwesome.h>
#include <nfd.h>
#include "Assets/AssetConverter.h"
#include "Urho3D/Misc/FreeFunctions.h"

URHO3D_DEFINE_APPLICATION_MAIN(Editor);


namespace Urho3D
{

Editor::Editor(Context* context)
    : Application(context)
{
}

void Editor::Setup()
{
#ifdef _WIN32
    // Required until SDL supports hdpi on windows
    if (HMODULE hLibrary = LoadLibraryA("Shcore.dll"))
    {
        typedef HRESULT(WINAPI*SetProcessDpiAwarenessType)(size_t value);
        if (auto fn = GetProcAddress(hLibrary, "SetProcessDpiAwareness"))
            ((SetProcessDpiAwarenessType)fn)(2);    // PROCESS_PER_MONITOR_DPI_AWARE
        FreeLibrary(hLibrary);
    }
#endif

    //engineResourceAutoloadPaths_ = {"Autoload"};
    //engineResourcePrefixPaths_ = {GetFileSystem()->GetProgramDir(), GetParentPath(GetFileSystem()->GetProgramDir()), 
    //    GetParentPath(GetParentPath(GetFileSystem()->GetProgramDir())) };


    //engineResourcePaths_ = { "Data", "CoreData", "EditorData"};

    engineParameters_[EP_WINDOW_TITLE] = GetTypeName();
    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_FULL_SCREEN] = false;
    engineParameters_[EP_WINDOW_HEIGHT] = 1080;
    engineParameters_[EP_WINDOW_WIDTH] = 1920;
    engineParameters_[EP_LOG_LEVEL] = LOG_DEBUG;
    engineParameters_[EP_WINDOW_RESIZABLE] = true;
    //engineParameters_[EP_AUTOLOAD_PATHS] = String::Joined(engineResourceAutoloadPaths_, ";");
    engineParameters_[EP_RESOURCE_PREFIX_PATHS] =  "..\\..\\..\\Urho3d\\bin;..\\..\\..\\bin";
    engineParameters_[EP_RESOURCE_PATHS] = String::Joined({ "Data", "CoreData", "EditorData" }, ";");

    SetRandomSeed(Time::GetTimeSinceEpoch());
}

void Editor::Start()
{
    context_->RegisterFactory<EditorIconCache>();
    context_->RegisterSubsystem(new EditorIconCache(context_));
    GetInput()->SetMouseMode(MM_ABSOLUTE);
    GetInput()->SetMouseVisible(true);
    RegisterToolboxTypes(context_);
    context_->RegisterFactory<Editor>();
    context_->RegisterSubsystem(this);
    SceneSettings::RegisterObject(context_);

    GetSystemUI()->ApplyStyleDefault(true, 1.0f);
    GetSystemUI()->AddFont("Fonts/DejaVuSansMono.ttf");
    GetSystemUI()->AddFont("Fonts/fontawesome-webfont.ttf", {ICON_MIN_FA, ICON_MAX_FA, 0}, 0, true);
    ui::GetStyle().WindowRounding = 3;
    // Disable imgui saving ui settings on it's own. These should be serialized to project file.
    ui::GetIO().IniFilename = nullptr;

    GetCache()->SetAutoReloadResources(true);

    SubscribeToEvent(E_UPDATE, std::bind(&Editor::OnUpdate, this, _2));
    SubscribeToEvent(E_EDITORRESOURCESAVED, std::bind(&Editor::SaveProject, this, ""));

    // Creates console but makes sure it's UI is not rendered. Console rendering is done manually in editor.
    auto* console = engine_->CreateConsole();
    console->SetAutoVisibleOnError(false);
    GetFileSystem()->SetExecuteConsoleCommands(false);
    SubscribeToEvent(E_CONSOLECOMMAND, std::bind(&Editor::OnConsoleCommand, this, _2));
    console->RefreshInterpreters();

    assetConverter_ = new AssetConverter(context_);

    // Load default project on start
    LoadProject("Etc/DefaultEditorProject.xml");
    // Prevent overwriting example scene.
    DynamicCast<SceneTab>(tabs_.Front())->ClearCachedPaths();

    // Load any native plugins in editor directory.
    {
        StringVector files;
        GetFileSystem()->ScanDir(files, GetFileSystem()->GetProgramDir(), "", SCAN_FILES, false);


#if WIN32
        const char* start = "EditorPlugin";
        const char* end = ".dll";
#elif APPLE
        const char* start = "libEditorPlugin";
        const char* end = ".dylib";
#else
        const char* start = "libEditorPlugin";
        const char* end = ".so";
#endif

        for (const auto& path : files)
        {
            auto lastCharacter = path.Length() - strlen(end) - 1;
            if (path.StartsWith(start) && path.EndsWith(end) && !IsDigit(path[lastCharacter]))
            {
                LoadNativePlugin(GetFileSystem()->GetProgramDir() + path);
            }
                
        }
    }
}

void Editor::Stop()
{
    ui::ShutdownDock();
}

void Editor::SaveProject(String filePath)
{
    // Saving project data of tabs may trigger saving resources, which in turn triggers saving editor project. Avoid
    // that loop.
    UnsubscribeFromEvent(E_EDITORRESOURCESAVED);

    filePath = GetResourceAbsolutePath(filePath, projectFilePath_, "xml", "Save Project As");

    if (filePath.Empty())
        return;

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement root = xml->CreateRoot("project");
    root.SetAttribute("version", "0");

    auto window = root.CreateChild("window");
    window.SetAttribute("width", ToString("%d", GetGraphics()->GetWidth()));
    window.SetAttribute("height", ToString("%d", GetGraphics()->GetHeight()));
    window.SetAttribute("x", ToString("%d", GetGraphics()->GetWindowPosition().x_));
    window.SetAttribute("y", ToString("%d", GetGraphics()->GetWindowPosition().y_));

    auto resources = root.CreateChild("resources");
    for (const auto& dir : GetCache()->GetResourceDirs())
    {
        if (IsInternalResourcePath(dir))
            continue;

        // Saving relative paths allows moving projects easily.
        String relative;
        GetRelativePath(GetPath(filePath), dir, relative);
        resources.CreateChild("path").SetValue(relative);
    }

    auto scenes = root.CreateChild("tabs");
    for (auto& tab: tabs_)
    {
        XMLElement tabXml = scenes.CreateChild("tab");
        tab->SaveProject(tabXml);
    }

    ui::SaveDock(root.CreateChild("docks"));

    if (!xml->SaveFile(filePath))
    {
        projectFilePath_.Clear();
        URHO3D_LOGERRORF("Saving project to %s failed", filePath.CString());
    }

    SubscribeToEvent(E_EDITORRESOURCESAVED, std::bind(&Editor::SaveProject, this, ""));
}

void Editor::LoadProject(String filePath)
{
    if (filePath.Empty())
        return;

    if (!IsAbsolutePath(filePath))
        filePath = GetCache()->GetResourceFileName(filePath);

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->LoadFile(filePath))
        return;

    auto root = xml->GetRoot();
    if (root.NotNull())
    {
        Vector<String> cacheDirectories = GetCache()->GetResourceDirs();
        for (const auto& dir : cacheDirectories)
        {
            if (IsInternalResourcePath(dir))
                continue;

            assetConverter_->RemoveAssetDirectory(dir);
            GetCache()->RemoveResourceDir(dir);
        }

            idPool_.Clear();
        auto window = root.GetChild("window");
        if (window.NotNull())
        {
            GetGraphics()->SetMode(ToInt(window.GetAttribute("width")), ToInt(window.GetAttribute("height")));
            GetGraphics()->SetWindowPosition(ToInt(window.GetAttribute("x")), ToInt(window.GetAttribute("y")));
        }

        auto resources = root.GetChild("resources");
        for (auto path = resources.GetChild("path"); path.NotNull(); path = path.GetNext("path"))
        {
            String resourceDir = GetAbsolutePath(GetPath(filePath) + path.GetValue());
            if (GetFileSystem()->DirExists(resourceDir))
            {
                GetCache()->AddResourceDir(resourceDir);
                assetConverter_->AddAssetDirectory(resourceDir);
            }
            else
                URHO3D_LOGWARNINGF("Project tried to load missing resource path \"%s\"", resourceDir.CString());
        }

        auto tabs = root.GetChild("tabs");
        tabs_.Clear();
        if (tabs.NotNull())
        {
            auto tab = tabs.GetChild("tab");
            while (tab.NotNull())
            {
                if (tab.GetAttribute("type") == "scene")
                    CreateNewTab<SceneTab>(tab);
                else if (tab.GetAttribute("type") == "ui")
                    CreateNewTab<UITab>(tab);
                tab = tab.GetNext();
            }
        }

        ui::LoadDock(root.GetChild("docks"));
    }

    projectFilePath_ = filePath;
    assetConverter_->VerifyCacheAsync();
}

void Editor::OnUpdate(VariantMap& args)
{
#if URHO3D_PLUGINS
    for (auto& plugin : nativePlugins_)
    {
        if (plugin.context_.userdata)
        {
            bool reloading = cr_plugin_changed(plugin.context_);
            if (reloading)
                SendEvent(E_EDITORUSERCODERELOADSTART);

            if (cr_plugin_update(plugin.context_) != 0)
            {
                URHO3D_LOGERRORF("Processing plugin \"%s\" failed and it was unloaded.", GetFileNameAndExtension(plugin.path_).CString());
                cr_plugin_close(plugin.context_);
                plugin.context_.userdata = nullptr;
            }

            if (reloading)
            {
                SendEvent(E_EDITORUSERCODERELOADEND);
                if (plugin.context_.userdata != nullptr)
                    URHO3D_LOGINFOF("Loaded plugin \"%s\" version %d.", GetFileNameAndExtension(plugin.path_).CString(), plugin.context_.version);
            }
        }
    }
#endif
    ui::RootDock({0, 20}, ui::GetIO().DisplaySize - ImVec2(0, 20));

    RenderMenuBar();

    ui::SetNextDockPos(nullptr, ui::Slot_Left, ImGuiCond_FirstUseEver);
    if (ui::BeginDock("Hierarchy"))
    {
        if (!activeTab_.Expired())
            activeTab_->RenderNodeTree();
    }
    ui::EndDock();

    bool renderedWasActive = false;
    for (auto it = tabs_.Begin(); it != tabs_.End();)
    {
        auto& tab = *it;
        if (tab->RenderWindow())
        {
            if (tab->IsRendered())
            {
                // Only active window may override another active window
                if (renderedWasActive && tab->IsActive())
                    activeTab_ = tab;
                else if (!renderedWasActive)
                {
                    renderedWasActive = tab->IsActive();
                    activeTab_ = tab;
                }
            }

            ++it;
        }
        else
            it = tabs_.Erase(it);
    }


    if (!activeTab_.Expired())
    {
        activeTab_->OnActiveUpdate();
        ui::SetNextDockPos(activeTab_->GetUniqueTitle().CString(), ui::Slot_Right, ImGuiCond_FirstUseEver);
    }
    if (ui::BeginDock("Inspector"))
    {
        if (!activeTab_.Expired())
            activeTab_->RenderInspector();
    }
    ui::EndDock();

    if (ui::BeginDock("Console"))
        GetSubsystem<Console>()->RenderContent();
    ui::EndDock();

    String selected;
    if (tabs_.Size())
        ui::SetNextDockPos(tabs_.Back()->GetUniqueTitle().CString(), ui::Slot_Bottom, ImGuiCond_FirstUseEver);
    if (ResourceBrowserWindow(selected))
    {
        auto type = GetContentType(selected);
        if (type == CTYPE_SCENE)
            CreateNewTab<SceneTab>()->LoadResource(selected);
        else if (type == CTYPE_UILAYOUT)
            CreateNewTab<UITab>()->LoadResource(selected);
    }
}

void Editor::RenderMenuBar()
{
    bool save = false;
    if (ui::BeginMainMenuBar())
    {
        if (ui::BeginMenu("File"))
        {
            save = ui::MenuItem("Save Project");
            if (ui::MenuItem("Save Project As"))
            {
                save = true;
                projectFilePath_.Clear();
            }

            if (ui::MenuItem("Open Project"))
            {
                nfdchar_t* projectFilePath = nullptr;
                if (NFD_OpenDialog("xml", "", &projectFilePath) == NFD_OKAY)
                {
                    projectFilePath_ = projectFilePath;
                    NFD_FreePath(projectFilePath);
                    LoadProject(projectFilePath_);
                }
            }

            ui::Separator();

            if (ui::MenuItem("New Scene"))
                CreateNewTab<SceneTab>();

            if (ui::MenuItem("New UI Layout"))
                CreateNewTab<UITab>();

            ui::Separator();

            if (ui::MenuItem("Exit"))
                engine_->Exit();

            ui::EndMenu();
        }

        if (ui::BeginMenu("Settings"))
        {
            Vector<String> cacheDirectories = GetCache()->GetResourceDirs();
            for (const auto& dir : cacheDirectories)
            {
                if (IsInternalResourcePath(dir))
                    continue;

                if (ui::Button(ICON_FA_TRASH))
                {
                    assetConverter_->RemoveAssetDirectory(dir);
                    GetCache()->RemoveResourceDir(dir);
                }

                ui::SameLine();
                ui::TextUnformatted(dir.CString());
            }
            if (ui::Button(ICON_FA_FOLDER_OPEN " Add data directory"))
            {
                nfdchar_t* result = nullptr;
                if (NFD_PickFolder("", &result) == NFD_OKAY)
                {
                    GetCache()->AddResourceDir(result);
                    NFD_FreePath(result);
                }
            }
            ui::EndMenu();
        }

        if (!activeTab_.Expired())
        {
            SendEvent(E_EDITORTOOLBARBUTTONS);
        }

        ui::EndMainMenuBar();
    }

    if (save)
        SaveProject(projectFilePath_);
}

template<typename T>
T* Editor::CreateNewTab(XMLElement project)
{
    SharedPtr<T> tab;
    StringHash id;

    if (project.IsNull())
        id = idPool_.NewID();           // Make new ID only if scene is not being loaded from a project.

    if (tabs_.Empty())
        tab = new T(context_, id, "Hierarchy", ui::Slot_Right);
    else
        tab = new T(context_, id, tabs_.Back()->GetUniqueTitle(), ui::Slot_Tab);

    if (project.NotNull())
    {
        tab->LoadProject(project);
        if (!idPool_.TakeID(tab->GetID()))
        {
            URHO3D_LOGERRORF("Scene loading failed because unique id %s is already taken",
                tab->GetID().ToString().CString());
            return nullptr;
        }
    }

    // In order to render scene to a texture we must add a dummy node to scene rendered to a screen, which has material
    // pointing to scene texture. This object must also be visible to main camera.
    tabs_.Push(DynamicCast<Tab>(tab));
    return tab;
}

StringVector Editor::GetObjectCategories() const
{
    return context_->GetObjectCategories().Keys();
}

StringVector Editor::GetObjectsByCategory(const String& category)
{
    StringVector result;
    const auto& factories = context_->GetObjectFactories();
    auto it = context_->GetObjectCategories().Find(category);
    if (it != context_->GetObjectCategories().End())
    {
        for (const StringHash& type : it->second_)
        {
            auto jt = factories.Find(type);
            if (jt != factories.End())
                result.Push(jt->second_->GetTypeName());
        }
    }
    return result;
}

String Editor::GetResourceAbsolutePath(const String& resourceName, const String& defaultResult, const char* patterns,
    const String& dialogTitle)
{
    String resourcePath = resourceName.Empty() ? defaultResult : resourceName;
    String fullPath;
    if (!resourcePath.Empty())
        fullPath = GetCache()->GetResourceFileName(resourcePath);

    if (fullPath.Empty())
    {
        nfdchar_t* savePath = nullptr;
        if (NFD_SaveDialog(patterns, "", &savePath) == NFD_OKAY)
        {
            fullPath = savePath;
            NFD_FreePath(savePath);
        }
    }

    return fullPath;
}

void Editor::OnConsoleCommand(VariantMap& args)
{
    using namespace ConsoleCommand;
    String command = args[P_COMMAND].GetString();
    if (command == "revision")
        URHO3D_LOGINFOF("Engine revision: %s", GetRevision());
    else if (command == "cache.sync")
        assetConverter_->VerifyCacheAsync();
    else
        URHO3D_LOGWARNINGF("Unknown command \"%s\".", command.CString());
}

bool Editor::LoadNativePlugin(const String& path)
{
#if URHO3D_PLUGINS
    NativePlugin plugin;
    if (cr_plugin_load(plugin.context_, path.CString()))
    {
        plugin.path_ = path;
        plugin.context_.userdata = context_;
        nativePlugins_.Push(plugin);
        return true;
    }
    else
    {
        URHO3D_LOGWARNINGF("Failed loading plugin \"%s\".", GetFileNameAndExtension(path).CString());
    }
#endif

    return false;
}

bool Editor::IsInternalResourcePath(const String& fullPath) const
{
    for (const auto& prefix : engineResourcePrefixPaths_)
    {
        for (const auto& path : engineResourcePaths_)
        {
            if (fullPath == AddTrailingSlash(prefix + path))
                return true;
        }
    }

    for (const auto& prefix : engineResourcePrefixPaths_)
    {
        for (const auto& path : engineResourceAutoloadPaths_)
        {
            if (fullPath.StartsWith(AddTrailingSlash(prefix + path)))
                return true;
        }
    }

    return false;
}

}
