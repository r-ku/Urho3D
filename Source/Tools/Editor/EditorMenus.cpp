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

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <nativefiledialog/nfd.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/SystemUI/SystemUI.h>
#include "Plugins/ModulePlugin.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/PreviewTab.h"
#include "Editor.h"
#include "EditorEvents.h"

namespace Urho3D
{

void Editor::RenderMenuBar()
{
    if (ui::BeginMainMenuBar())
    {
        if (ui::BeginMenu("File"))
        {
            if (project_)
            {
                if (ui::MenuItem("Save Project"))
                {
                    for (auto& tab : tabs_)
                        tab->SaveResource();
                    project_->SaveProject();
                }
            }

            if (ui::MenuItem("Open/Create Project"))
                OpenOrCreateProject();

            JSONValue& recents = editorSettings_["recent-projects"];
            // Does not show very first item, which is current project
            if (recents.Size() == (project_.NotNull() ? 1 : 0))
            {
                ui::PushStyleColor(ImGuiCol_Text, ui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ui::MenuItem("Recent Projects");
                ui::PopStyleColor();
            }
            else if (ui::BeginMenu("Recent Projects"))
            {
                for (int i = project_.NotNull() ? 1 : 0; i < recents.Size(); i++)
                {
                    const ea::string& projectPath = recents[i].GetString();

                    if (ui::MenuItem(GetFileNameAndExtension(RemoveTrailingSlash(projectPath)).c_str()))
                        OpenProject(projectPath);

                    if (ui::IsItemHovered())
                        ui::SetTooltip("%s", projectPath.c_str());
                }
                ui::Separator();
                if (ui::MenuItem("Clear All"))
                    recents.Clear();
                ui::EndMenu();
            }

            ui::Separator();

            if (project_)
            {
                if (ui::MenuItem("Reset UI"))
                {
                    ea::string projectPath = project_->GetProjectPath();
                    CloseProject();
                    context_->GetFileSystem()->Delete(projectPath + ".ui.ini");
                    OpenProject(projectPath);
                }

                if (ui::MenuItem("Close Project"))
                {
                    CloseProject();
                }
            }

            if (ui::MenuItem("Exit"))
                engine_->Exit();

            ui::EndMenu();
        }
        if (project_)
        {
            if (ui::BeginMenu("View"))
            {
                for (auto& tab : tabs_)
                {
                    if (tab->IsUtility())
                    {
                        // Tabs that can not be closed permanently
                        auto open = tab->IsOpen();
                        if (ui::MenuItem(tab->GetUniqueTitle().c_str(), nullptr, &open))
                            tab->SetOpen(open);
                    }
                }
                ui::EndMenu();
            }

            if (ui::BeginMenu("Project"))
            {
                RenderProjectMenu();
                ui::EndMenu();
            }

#if URHO3D_PROFILING
            if (ui::BeginMenu("Tools"))
            {
                if (ui::MenuItem("Profiler"))
                {
                    context_->GetFileSystem()->SystemSpawn(context_->GetFileSystem()->GetProgramDir() + "Profiler"
#if _WIN32
                        ".exe"
#endif
                        , {});
                }
                ui::EndMenu();
            }
#endif
        }

        SendEvent(E_EDITORAPPLICATIONMENU);

        // Scene simulation buttons.
        if (project_)
        {
            // Copied from ToolbarButton()
            auto& g = *ui::GetCurrentContext();
            float dimension = g.FontBaseSize + g.Style.FramePadding.y * 2.0f;
            ui::SetCursorScreenPos({ui::GetIO().DisplaySize.x / 2 - dimension * 4 / 2, ui::GetCursorScreenPos().y});
            if (auto* previewTab = GetTab<PreviewTab>())
                previewTab->RenderButtons();
        }

        ui::EndMainMenuBar();
    }

    if (!flavorPendingRemoval_.Expired())
        ui::OpenPopup("Remove Flavor?");

    if (ui::BeginPopupModal("Remove Flavor?"))
    {
        ui::Text("You are about to remove '%s' flavor.", flavorPendingRemoval_->GetName().c_str());
        ui::TextUnformatted("All asset settings of this flavor will be removed permanently.");
        ui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " This action can not be undone! " ICON_FA_EXCLAMATION_TRIANGLE);
        ui::NewLine();

        if (ui::Button(ICON_FA_TRASH " Remove"))
        {
            project_->GetPipeline()->RemoveFlavor(flavorPendingRemoval_->GetName());
            flavorPendingRemoval_ = nullptr;
            ui::CloseCurrentPopup();
        }
        ui::SameLine();
        if (ui::Button(ICON_FA_TIMES " Cancel"))
        {
            flavorPendingRemoval_ = nullptr;
            ui::CloseCurrentPopup();
        }

        ui::EndPopup();
    }
}

void Editor::RenderProjectMenu()
{
    settingsOpen_ |= ui::MenuItem("Settings");

    if (ui::BeginMenu(ICON_FA_BOXES " Repackage files"))
    {
        Pipeline* pipeline = project_->GetPipeline();

        if (ui::MenuItem("All Flavors"))
        {
            for (Flavor* flavor : pipeline->GetFlavors())
                pipeline->CreatePaksAsync(flavor);
        }

        for (Flavor* flavor : pipeline->GetFlavors())
        {
            if (ui::MenuItem(flavor->GetName().c_str()))
                pipeline->CreatePaksAsync(flavor);
        }

        ui::EndMenu();
    }
    ui::SetHelpTooltip("(Re)Packages all resources from scratch. Existing packages will be removed!");
}

}
