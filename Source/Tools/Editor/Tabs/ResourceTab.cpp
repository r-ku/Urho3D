//
// Copyright (c) 2017-2020 the rbfx project.
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

#include <EASTL/sort.h>

#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/ResourceEvents.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Glow/LightmapUVGenerator.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/ModelView.h>

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Toolbox/IO/ContentUtilities.h>
#include <SDL/SDL_clipboard.h>

#include "EditorEvents.h"
#include "Inspector/MaterialInspector.h"
#include "Inspector/ModelInspector.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/UI/UITab.h"
#include "Tabs/InspectorTab.h"
#include "Pipeline/Importers/ModelImporter.h"
#include "Editor.h"
#include "ResourceTab.h"

namespace Urho3D
{

static ea::unordered_map<ContentType, ea::string> contentToTabType{
    {CTYPE_SCENE, "SceneTab"},
    {CTYPE_UILAYOUT, "UITab"},
};

ResourceTab::ResourceTab(Context* context)
    : Tab(context)
{
    SetID("29d1a5dc-6b8d-4a27-bfb2-a84417f33ee2");
    SetTitle("Resources");
    isUtility_ = true;

    SubscribeToEvent(E_INSPECTORLOCATERESOURCE, [&](StringHash, VariantMap& args) {
        auto resourceName = args[InspectorLocateResource::P_NAME].GetString();

        auto* project = GetSubsystem<Project>();
        auto* fs = context_->GetFileSystem();

        resourcePath_ = GetPath(resourceName);
        if (fs->FileExists(project->GetCachePath() + resourceName))
        {
            // File is in the cache. resourcePath_ should point to a directory of source resource. For example:
            // We have Resources/Models/cube.fbx which is a source model.
            // It is converted to Cache/Models/cube.fbx/Model.mdl
            // ResourceBrowserWidget() expects:
            // * resourcePath = Models/ (path same as if cube.fbx was selected)
            // * resourceSelection = cube.fbx/Model.mdl (selection also includes a directory which resides in cache)
            while (!fs->DirExists(project->GetResourcePath() + resourcePath_))
                resourcePath_ = GetParentPath(resourcePath_);
            resourceSelection_ = resourceName.substr(resourcePath_.length());
        }
        else
            resourceSelection_ = GetFileNameAndExtension(resourceName);
        flags_ |= RBF_SCROLL_TO_CURRENT;
        if (ui::GetIO().KeyCtrl)
            SelectCurrentItemInspector();
    });
    SubscribeToEvent(E_RESOURCERENAMED, [&](StringHash, VariantMap& args) {
        using namespace ResourceRenamed;
        const ea::string& from = args[P_FROM].GetString();
        const ea::string& to = args[P_TO].GetString();
        if (from == resourcePath_ + resourceSelection_)
        {
            resourcePath_ = GetParentPath(to);
            resourceSelection_ = GetFileNameAndExtension(RemoveTrailingSlash(to));
            if (to.ends_with("/"))
                resourceSelection_ = AddTrailingSlash(resourceSelection_);
        }
    });
    SubscribeToEvent(E_RESOURCEBROWSERDELETE, [&](StringHash, VariantMap& args) {
        using namespace ResourceBrowserDelete;
        auto* project = GetSubsystem<Project>();
        auto fileName = project->GetResourcePath() + args[P_NAME].GetString();
        if (context_->GetFileSystem()->FileExists(fileName))
            context_->GetFileSystem()->Delete(fileName);
        else if (context_->GetFileSystem()->DirExists(fileName))
            context_->GetFileSystem()->RemoveDir(fileName, true);
    });
}

bool ResourceTab::RenderWindowContent()
{
    auto* project = GetSubsystem<Project>();
    auto action = ResourceBrowserWidget(resourcePath_, resourceSelection_, flags_);
    if (action == RBR_ITEM_OPEN)
    {
        ea::string selected = resourcePath_ + resourceSelection_;
        auto it = contentToTabType.find(GetContentType(context_, selected));
        if (it != contentToTabType.end())
        {
            if (auto* tab = GetSubsystem<Editor>()->GetTab(it->second))
            {
                if (tab->IsUtility())
                {
                    // Tabs that can be opened once.
                    tab->LoadResource(selected);
                    tab->Activate();
                }
                else
                {
                    // Tabs that can be opened multiple times.
                    if ((tab = GetSubsystem<Editor>()->GetTabByResource(selected)))
                        tab->Activate();
                    else if ((tab = GetSubsystem<Editor>()->CreateTab(it->second)))
                    {
                        tab->LoadResource(selected);
                        tab->AutoPlace();
                        tab->Activate();
                    }
                }
            }
            else if ((tab = GetSubsystem<Editor>()->CreateTab(it->second)))
            {
                // Tabs that can be opened multiple times.
                tab->LoadResource(selected);
                tab->AutoPlace();
                tab->Activate();
            }
        }
        else
        {
            // Unknown resources are opened with associated application.
            ea::string resourcePath = GetSubsystem<Project>()->GetResourcePath() + selected;
            if (!context_->GetFileSystem()->Exists(resourcePath))
                resourcePath = GetSubsystem<Project>()->GetCachePath() + selected;

            if (context_->GetFileSystem()->Exists(resourcePath))
                context_->GetFileSystem()->SystemOpen(resourcePath);
        }
    }
    else if (action == RBR_ITEM_CONTEXT_MENU)
        ui::OpenPopup("Resource Context Menu");
    else if (action == RBR_ITEM_SELECTED)
        SelectCurrentItemInspector();

    flags_ = RBF_NONE;

    bool hasSelection = !resourceSelection_.empty();
    if (hasSelection && ui::IsWindowFocused())
    {
        if (ui::IsKeyReleased(SCANCODE_F2))
            flags_ |= RBF_RENAME_CURRENT;

        if (ui::IsKeyReleased(SCANCODE_DELETE))
            flags_ |= RBF_DELETE_CURRENT;
    }

    if (ui::BeginPopup("Resource Context Menu"))
    {
        if (ui::BeginMenu("Create"))
        {
            if (ui::MenuItem(ICON_FA_FOLDER " Folder"))
            {
                ea::string newFolderName("New Folder");
                ea::string path = GetNewResourcePath(resourcePath_ + newFolderName);
                if (context_->GetFileSystem()->CreateDir(path))
                {
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = newFolderName;
                }
                else
                    URHO3D_LOGERRORF("Failed creating folder '%s'.", path.c_str());
            }

            if (ui::MenuItem("Scene"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New Scene.xml");
                context_->GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<Scene> scene(new Scene(context_));
                scene->CreateComponent<Octree>();
                File file(context_, path, FILE_WRITE);
                if (file.IsOpen())
                {
                    scene->SaveXML(file);
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
            }

            if (ui::MenuItem("Material"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New Material.xml");
                context_->GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<Material> material(new Material(context_));
                File file(context_, path, FILE_WRITE);
                if (file.IsOpen())
                {
                    material->Save(file);
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
            }

            if (ui::MenuItem("UI Layout"))
            {
                auto path = GetNewResourcePath(resourcePath_ + "New UI Layout.xml");
                context_->GetFileSystem()->CreateDirsRecursive(GetPath(path));

                SharedPtr<UIElement> scene(new UIElement(context_));
                XMLFile layout(context_);
                auto root = layout.GetOrCreateRoot("element");
                if (scene->SaveXML(root) && layout.SaveFile(path))
                {
                    flags_ |= RBF_RENAME_CURRENT | RBF_SCROLL_TO_CURRENT;
                    resourceSelection_ = GetFileNameAndExtension(path);
                }
                else
                    URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
            }

            ui::EndMenu();
        }

        if (!hasSelection)
            ui::PushStyleColor(ImGuiCol_Text, ui::GetStyle().Colors[ImGuiCol_TextDisabled]);

        if (ui::MenuItem("Copy Path") && hasSelection)
            SDL_SetClipboardText((resourcePath_ + resourceSelection_).c_str());

        if (ui::MenuItem("Rename", "F2") && hasSelection)
            flags_ |= RBF_RENAME_CURRENT;

        if (ui::MenuItem("Delete", "Del") && hasSelection)
            flags_ |= RBF_DELETE_CURRENT;

        // TODO(glow): Move it into separate addon
        const bool modelSelected = resourceSelection_.ends_with(".mdl");
        if (modelSelected)
        {
            ui::Separator();

            if (ui::MenuItem("Generate Lightmap UV", nullptr, nullptr) && hasSelection)
            {
                auto model = context_->GetCache()->GetResource<Model>(resourcePath_ + resourceSelection_);
                if (model && !model->GetNativeFileName().empty())
                {
                    ModelView modelView(context_);
                    if (modelView.ImportModel(model))
                    {
                        if (GenerateLightmapUV(modelView, {}))
                        {
                            model->SendEvent(E_RELOADSTARTED);
                            modelView.ExportModel(model);
                            model->SendEvent(E_RELOADFINISHED);

                            model->SaveFile(model->GetNativeFileName());
                        }
                    }
                }
            }
        }

        if (!hasSelection)
            ui::PopStyleColor();

        using namespace EditorResourceContextMenu;
        ea::string selected = resourcePath_ + resourceSelection_;
        ContentType ctype = GetContentType(context_, selected);
        SendEvent(E_EDITORRESOURCECONTEXTMENU, P_CTYPE, ctype, P_RESOURCENAME, selected);

        ui::EndPopup();
    }

    return true;
}

ea::string ResourceTab::GetNewResourcePath(const ea::string& name)
{
    auto* project = GetSubsystem<Project>();
    if (!context_->GetFileSystem()->FileExists(project->GetResourcePath() + name))
        return project->GetResourcePath() + name;

    auto basePath = GetPath(name);
    auto baseName = GetFileName(name);
    auto ext = GetExtension(name, false);

    for (auto i = 1; i < M_MAX_INT; i++)
    {
        auto newName = project->GetResourcePath() + ToString("%s%s %d%s", basePath.c_str(), baseName.c_str(), i, ext.c_str());
        if (!context_->GetFileSystem()->FileExists(newName))
            return newName;
    }

    std::abort();
}

void ResourceTab::SelectCurrentItemInspector()
{
    ea::string selected = resourcePath_ + resourceSelection_;

    auto* editor = GetSubsystem<Editor>();
    auto* pipeline = GetSubsystem<Pipeline>();
    editor->ClearInspector();

    if (Asset* asset = pipeline->GetAsset(selected))
        asset->Inspect();

    using namespace EditorResourceSelected;
    SendEvent(E_EDITORRESOURCESELECTED, P_CTYPE, GetContentType(context_, selected), P_RESOURCENAME, selected);
}

}
