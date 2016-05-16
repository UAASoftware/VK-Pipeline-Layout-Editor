/*
 Copyright (c) 2016 UAA Software
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "tool_framework.hpp"
#include "tool_pipelinelayout.hpp"
#include <algorithm>
#include <string>
#include <fstream>
#include <json/json.h>

using namespace std;

static vector<string> descLayoutTypes = {
    "VK_DESCRIPTOR_TYPE_SAMPLER",
    "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
    "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE",
    "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE",
    "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
    "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
    "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER",
    "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",
    "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
    "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",
    "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT"
};
static vector<string> stageBits = {
        "VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT",
        "VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT",
        "VK_PIPELINE_STAGE_VERTEX_INPUT_BIT",
        "VK_PIPELINE_STAGE_VERTEX_SHADER_BIT",
        "VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT",
        "VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT",
        "VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT",
        "VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT",
        "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT",
        "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT",
        "VK_PIPELINE_STAGE_TRANSFER_BIT",
        "VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT",
        "VK_PIPELINE_STAGE_HOST_BIT",
        "VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT",
        "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT"
};

// -------------------------------------------------------- Helpers -----------------------------------------------

template <typename T>
static vector<const char*> CStrList(vector<T> & list)
{
    vector<const char*> list_;
    for (auto& item: list) list_.push_back(item.name.c_str());
    return list_;
}
static vector<const char*> CStrList(vector<std::string>& list)
{
    vector<const char*> list_;
    for (auto& item: list) list_.push_back(item.c_str());
    return list_;
}
static vector<const char*> CStrList(std::vector< std::unique_ptr<DescriptorLayout> >& list)
{
    vector<const char*> list_;
    for (auto& item: list) list_.push_back(item->name.c_str());
    return list_;
}

static int DisplayConfirmWindow(const char* text)
{
    if (ImGui::BeginPopupModal(text, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.953f, 0.208f, 0.42f, 1.0f), "%s Are you sure?", text);
        ImGui::Text("This cannot be undone.");
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return -1;
        }
        ImGui::EndPopup();
    }
    return 0;
}

static bool displayAboutWindow = false;
static void DisplayAboutWindow(void)
{
    if (ImGui::BeginPopupModal("About VK Pipeline Layout Editor", &displayAboutWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.067f, 0.765f, 0.941f, 1.0f), "Vulkan Pipeline Layout Editor" 
                           "                                                                                              ");
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "(c) Copyright 2016 UAA Software");
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "Programming by Xi Ma Chen");
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextWrapped(
            "Static pipeline layout editor tool for the Vulkan API. Loads & saves from a simple json file format.\n"
            "Quick guide:\n"
        );
        {
            ImGui::Bullet(); 
            ImGui::TextWrapped(
                "A descriptor binding layout describes a single binding for a single shader stage. "
                "For example, a binding layout might represent texture sampler, a uniform buffer object, and so forth."
            );
            ImGui::Bullet(); ImGui::TextWrapped("Descriptor binding layouts are represented as a global list, shared between all sets & pipelines.");
            ImGui::Bullet(); ImGui::TextWrapped(
                "A descriptor set layout describes a collection of descriptor bindings. It may be a good idea to group these by update frequency. "
                "For example, one may have separate sets for per-frame, per-scene, per-camera, per-material and per-object shader data."
            );
            ImGui::Bullet(); ImGui::TextWrapped("A pipeline layout describes a collection of descriptor set layouts.");
        }
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0))) {
            displayAboutWindow = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void DisplayCombo(const char* title, int *current, vector<const char*>& items, vector<char>& buffer)
{
    size_t totalsz = 0;
    for (auto str : items) totalsz += (strlen(str) + 1);
    buffer.resize(totalsz + 1);
    size_t off = 0;
    for (auto str : items) {
        memcpy(&buffer[off], str, (strlen(str) + 1));
        off += (strlen(str) + 1);
    }
    buffer[off] = '\0';
    ImGui::Combo(title, current, buffer.data());
}

// src: http://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
inline bool EndsWith(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// -------------------------------------------------------- DescriptorLayout & PipelineLayout -----------------------------------------------

DescriptorLayout::DescriptorLayout(std::string name_)
    : name(name_)
{}

void DescriptorLayout::stageFlagBitsToBools(std::vector<bool>& out)
{
    out[0] = !!(stageFlagBits & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    out[1] = !!(stageFlagBits & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    out[2] = !!(stageFlagBits & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
    out[3] = !!(stageFlagBits & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    out[4] = !!(stageFlagBits & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
    out[5] = !!(stageFlagBits & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
    out[6] = !!(stageFlagBits & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
    out[7] = !!(stageFlagBits & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    out[8] = !!(stageFlagBits & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    out[9] = !!(stageFlagBits & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    out[10] = !!(stageFlagBits & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    out[11] = !!(stageFlagBits & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    out[12] = !!(stageFlagBits & VK_PIPELINE_STAGE_TRANSFER_BIT);
    out[13] = !!(stageFlagBits & VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    out[14] = !!(stageFlagBits & VK_PIPELINE_STAGE_HOST_BIT);
    out[15] = !!(stageFlagBits & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    out[16] = !!(stageFlagBits & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void DescriptorLayout::stageFlagBitsFromBools(std::vector<bool>& out)
{
    stageFlagBits = 0;
    if (out[0]) stageFlagBits |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (out[1]) stageFlagBits |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    if (out[2]) stageFlagBits |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    if (out[3]) stageFlagBits |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (out[4]) stageFlagBits |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    if (out[5]) stageFlagBits |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    if (out[6]) stageFlagBits |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    if (out[7]) stageFlagBits |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (out[8]) stageFlagBits |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    if (out[9]) stageFlagBits |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    if (out[10]) stageFlagBits |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (out[11]) stageFlagBits |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (out[12]) stageFlagBits |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (out[13]) stageFlagBits |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    if (out[14]) stageFlagBits |= VK_PIPELINE_STAGE_HOST_BIT;
    if (out[15]) stageFlagBits |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    if (out[16]) stageFlagBits |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
}

PipelineLayout::PipelineLayout(std::string name_)
    : name(name_)
{}

// -------------------------------------------------------- PipelineLayoutTool -----------------------------------------------

const char* PipelineLayoutTool::getWindowTitle()
{
    return "VK Pipeline Layout Editor";
}

void PipelineLayoutTool::init(void)
{
    m_layouts.push_back(PipelineLayout("LAYOUT_DEFAULT"));

    m_dsets.push_back(("PSET_PER_FRAME"));
    m_dsets.push_back(("PSET_PER_SCENE"));
    m_dsets.push_back(("PSET_PER_CAMERA"));
    m_dsets.push_back(("PSET_PER_MATERIAL"));
    m_dsets.push_back(("PSET_PER_OBJECT"));

    auto& descset = m_layouts[m_layouts.size() - 1].descsets;
    descset.push_back(DescriptorSet());
    descset.push_back(DescriptorSet());
    descset.push_back(DescriptorSet());
    descset.push_back(DescriptorSet());
    descset.push_back(DescriptorSet());

    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_FRAME_GLOBAL_INFO"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_CAMERA_INFO"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_WORLD_MATRIX"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_SCENE_PARAMS_GENERIC"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_MATERIAL_PARAMS_GENERIC"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("UBO_OBJECT_PARAMS_GENERIC"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SSBO_LIGHT_GRID_DATA"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SSBO_BATCHED_DRAWCALL_DATA"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_ARRAY"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_DIFFUSE_MAP"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_NORMAL_MAP"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_SHININESS_MAP"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_METALLIC_MAP"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_IRRADIANCE"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_RADIANCE"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_POSTROCESS0"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_POSTROCESS1"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_POSTROCESS2"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_POSTROCESS3"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_SHADOW0"));
    m_dlayouts.push_back(make_unique<DescriptorLayout>("SAMPLER_SHADOW1"));

    findDLV(0, "PSET_PER_FRAME").push_back(findDescLayoutByName("UBO_FRAME_GLOBAL_INFO"));
    findDLV(0, "PSET_PER_CAMERA").push_back(findDescLayoutByName("UBO_CAMERA_INFO"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("UBO_WORLD_MATRIX"));
    findDLV(0, "PSET_PER_SCENE").push_back(findDescLayoutByName("UBO_SCENE_PARAMS_GENERIC"));
    findDLV(0, "PSET_PER_MATERIAL").push_back(findDescLayoutByName("UBO_MATERIAL_PARAMS_GENERIC"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("UBO_OBJECT_PARAMS_GENERIC"));
    findDLV(0, "PSET_PER_CAMERA").push_back(findDescLayoutByName("SSBO_LIGHT_GRID_DATA"));
    findDLV(0, "PSET_PER_MATERIAL").push_back(findDescLayoutByName("SSBO_BATCHED_DRAWCALL_DATA"));
    findDLV(0, "PSET_PER_MATERIAL").push_back(findDescLayoutByName("SAMPLER_ARRAY"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_DIFFUSE_MAP"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_NORMAL_MAP"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_SHININESS_MAP"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_METALLIC_MAP"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_IRRADIANCE"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_RADIANCE"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_POSTROCESS0"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_POSTROCESS1"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_POSTROCESS2"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_POSTROCESS3"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_SHADOW0"));
    findDLV(0, "PSET_PER_OBJECT").push_back(findDescLayoutByName("SAMPLER_SHADOW1"));
}

void PipelineLayoutTool::render(int screenWidth, int screenHeight)
{
    ImGui::SetNextWindowPos(ImVec2(4, 4));
    ImGui::SetNextWindowSize(ImVec2(screenWidth - 8, screenHeight - 8));


    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::Begin("Main window", nullptr, windowFlags);
    {
        static int activeLayoutItem = 0;
        static int activeDescsetItem = 0;
        static int activeDesclayoutItem = 0;
        static int activeDescBindingItem = 0;

        // --------------------------- Menu bar ---------------------------------

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                if (ImGui::MenuItem("New", NULL, nullptr)) {
                }
                if (ImGui::MenuItem("Open..", NULL, nullptr)) {
                    string p;
                    if (openDialog(p, "vkpipeline.json")) {
                        this->load(p);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", NULL, nullptr)) {
                    this->save(m_filename.c_str());
                }
                if (ImGui::MenuItem("Save As..", NULL, nullptr)) {
                    string p = m_filename;
                    if (saveDialog(p, "vkpipeline.json")) {
                        this->save(p);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", NULL, nullptr)) {
                    exit(0);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About", NULL, nullptr)) {
                    displayAboutWindow = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        if (displayAboutWindow) {
            ImGui::OpenPopup("About VK Pipeline Layout Editor");
            DisplayAboutWindow();
        }

        // --------------------------- First column : info and pipeline layouts ---------------------------------

        ImGui::BeginChild("Column1", ImVec2(240, 0), true);
        {
            {
                ImGui::TextColored(ImVec4(0.067f, 0.765f, 0.941f, 1.0f), "Vulkan Pipeline Layout Editor");
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "(c) Copyright 2016 UAA Software");
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::TextWrapped("%s", m_filename.c_str());
                ImGui::Spacing(); ImGui::Spacing();
            }
            ImGui::Separator();
            {
                static char newPipelineName[256] = "new_pipeline_name_here";
                static char renamePipeline[256];
                auto action = this->displayNamedList("Pipeline Layout", "Pipeline", "pipeline", "PL",
                    CStrList(m_layouts), activeLayoutItem, 15, newPipelineName, renamePipeline);
                switch (action) {
                    case 1: this->reorderLayout(activeLayoutItem, true); break;
                    case 2: this->reorderLayout(activeLayoutItem, false); break;
                    case 3: this->delLayout(activeLayoutItem--); break;
                    case 4: this->renameLayout(activeLayoutItem, renamePipeline); break;
                    case 5: this->addLayout(newPipelineName); break;
                    case 0: break;
                    default: assert(!"unknown action.");
                }
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();

        // --------------------------- Second column : sets and desc binding layouts ---------------------------------

        ImGui::BeginChild("Column2", ImVec2(300, 0), true);
        {
            {
                static char newDescsetName[256] = "new_descriptor_set_name";
                static char renameDescset[256];
                auto action = this->displayNamedList("Global Descriptor Set Layout", "DescSet", "set", "DSET",
                    CStrList(m_dsets), activeDescsetItem, 8, newDescsetName, renameDescset);
                switch (action) {
                    case 1: this->reorderDescset(activeLayoutItem, activeDescsetItem, true); break;
                    case 2: this->reorderDescset(activeLayoutItem, activeDescsetItem, false); break;
                    case 3: this->delDescset(activeLayoutItem, activeDescsetItem--); break;
                    case 4: this->renameDescset(activeLayoutItem, activeDescsetItem, renameDescset); break;
                    case 5: this->addDescset(activeLayoutItem, newDescsetName); break;
                    case 0: break;
                    default: assert(!"unknown action.");
                }
            }

            ImGui::Separator();
            {
                bool bindingLayoutChanged = false;
                auto& descset = (activeLayoutItem < m_layouts.size()) ?
                    m_layouts[activeLayoutItem].descsets : std::vector<DescriptorSet>();
                std::vector<std::string> desclayout;
                if (activeDescsetItem < descset.size()) {
                    for (auto& dlayout : descset[activeDescsetItem].dlayouts) {
                        assert(dlayout);
                        desclayout.push_back(dlayout->name);
                    }
                }

                auto action = this->displayNamedList("Descriptor Binding Layout", "DescLayout", "descriptor layout", "DL",
                    CStrList(desclayout), activeDesclayoutItem, 12, nullptr, nullptr, &bindingLayoutChanged);
                switch (action) {
                    case 1: this->reorderDescsetlayout(activeLayoutItem, activeDescsetItem, activeDesclayoutItem, true); break;
                    case 2: this->reorderDescsetlayout(activeLayoutItem, activeDescsetItem, activeDesclayoutItem, false); break;
                    case 3: this->delDescsetlayout(activeLayoutItem, activeDescsetItem, activeDesclayoutItem--); break;
                    default: break;
                }
                if (bindingLayoutChanged) {
                    if (activeDescsetItem < descset.size() && activeDesclayoutItem < descset[activeDescsetItem].dlayouts.size()) {
                        auto* dlayout = descset[activeDescsetItem].dlayouts[activeDesclayoutItem];
                        activeDescBindingItem = findDescLayoutByPtr(dlayout);
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();

        // --------------------------- Third column : global binding list ---------------------------------

        ImGui::BeginChild("Column3", ImVec2(370, 0), true);
        {
            {
                static char newDescbindingsName[256] = "new_desc_binding_name";
                static char renameDescbinding[256];
                
                auto& desclayout = m_dlayouts;

                auto action = this->displayNamedList("Global Descriptor Binding List", "BList", "binding", "DB",
                    CStrList(desclayout), activeDescBindingItem, 17, newDescbindingsName, renameDescbinding);
                switch (action) {
                    case 1: this->reorderDesclayout(activeDescBindingItem, true); break;
                    case 2: this->reorderDesclayout(activeDescBindingItem, false); break;
                    case 3: this->delDesclayout(activeDescBindingItem--); break;
                    case 4: this->renameDesclayout(activeDescBindingItem, newDescbindingsName); break;
                    case 5: this->addDesclayout(newDescbindingsName); break;
                    default: break;
                }

                ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                auto p = ImGui::Button(
                    "\n"
                    "                                                  \n"
                    "                                                  \n"
                    "      <-       ADD THIS BINDING TO CURRENT SET    \n"
                    "                                                  \n"
                    "                                                  \n"
                    "\n",
                    ImVec2(0, 0));
                if (p) {
                    this->addDescsetlayout(activeLayoutItem, activeDescsetItem, activeDescBindingItem);
                }
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();

        // --------------------------- Fourth column : binding editor ---------------------------------

        ImGui::BeginChild("Column4", ImVec2(0, 0), true);
        {
            static vector<char> layoutTypesBuffer;
            static vector<bool> dsetsBuffer;
            static vector<bool> stageBitsBuffer;

            DescriptorLayout defaultLayout("");
            auto& dlayout = activeDescBindingItem < m_dlayouts.size() ? *m_dlayouts[activeDescBindingItem].get() : defaultLayout;
            ImGui::TextColored(ImVec4(0.067f, 0.765f, 0.941f, 1.0f), "Descriptor Binding Edit:");

            if (activeDescBindingItem < m_dlayouts.size()) {

                ImGui::Spacing(); ImGui::Spacing();
                ImGui::Text("Name: %s", dlayout.name.c_str());
                DisplayCombo("Binding Type", &dlayout.typeIdx, CStrList(descLayoutTypes), layoutTypesBuffer);

                ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                if (ImGui::CollapsingHeader("Belonged Sets & Stages", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing(); ImGui::Spacing();
                    if (ImGui::TreeNodeEx("Descriptor set layouts that this binding belongs to:", ImGuiTreeNodeFlags_DefaultOpen)) {
                        dsetsBuffer.resize(m_dsets.size());
                        this->getDLGetSetList(activeLayoutItem, &dlayout, dsetsBuffer);
                        for (int s = 0; s < m_dsets.size(); s++) {
                            bool sel = dsetsBuffer[s];
                            ImGui::Selectable(m_dsets[s].c_str(), &sel);
                            dsetsBuffer[s] = sel;
                        }
                        this->getDLSetSetList(activeLayoutItem, &dlayout, dsetsBuffer);
                        ImGui::TreePop();
                    }
                    ImGui::Spacing(); ImGui::Spacing();
                    if (ImGui::TreeNodeEx("Shader stages that this binding belongs to:", ImGuiTreeNodeFlags_DefaultOpen)) {
                        stageBitsBuffer.resize(stageBits.size());
                        dlayout.stageFlagBitsToBools(stageBitsBuffer);
                        for (int stage = 0; stage < stageBits.size(); stage++) {
                            bool sel = stageBitsBuffer[stage];
                            ImGui::Selectable(stageBits[stage].c_str(), &sel);
                            stageBitsBuffer[stage] = sel;
                        }
                        dlayout.stageFlagBitsFromBools(stageBitsBuffer);
                        ImGui::TreePop();
                    }
                    ImGui::Spacing(); ImGui::Spacing();
                }
                if (ImGui::CollapsingHeader("Custom Data & Comment", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Spacing(); ImGui::Spacing();
                    ImGui::Text("Custom binding data:");
                    static char data_[40960];
                    strcpy_s(data_, 40960, dlayout.data.c_str());
                    auto dataChanged = ImGui::InputTextMultiline("##source", data_, 40960,
                        ImVec2(-1.0f, ImGui::GetTextLineHeight() * 18),
                        ImGuiInputTextFlags_AllowTabInput);
                    if (dataChanged) {
                        dlayout.data = data_;
                    }

                    ImGui::Spacing(); ImGui::Spacing();
                    ImGui::Text("Binding comment:");
                    static char data2_[40960];
                    strcpy_s(data2_, 40960, dlayout.comment.c_str());
                    auto dataChanged2 = ImGui::InputTextMultiline("##source2", data2_, 40960,
                        ImVec2(-1.0f, ImGui::GetTextLineHeight() * 10),
                        ImGuiInputTextFlags_AllowTabInput);
                    if (dataChanged2) {
                        dlayout.comment = data2_;
                    }
                    ImGui::Spacing(); ImGui::Spacing();
                }
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

void PipelineLayoutTool::addLayout(const char* name)
{
    if (!strlen(name)) return;
    for (auto layout: m_layouts) {
        if (layout.name == name) {
            return;
        }
    }
    m_layouts.push_back(PipelineLayout(name));
    m_layouts[m_layouts.size() - 1].descsets.resize(m_dsets.size());
}

void PipelineLayoutTool::delLayout(int idx)
{
    if (idx < 0 || idx >= m_layouts.size()) return;
    m_layouts.erase(m_layouts.begin() + idx);
}

void PipelineLayoutTool::renameLayout(int idx, const char* name)
{
    if (!strlen(name)) return;
    if (idx < 0 || idx >= m_layouts.size()) return;
    m_layouts[idx].name = name;
}

void PipelineLayoutTool::reorderLayout(int& idx, bool up)
{
    if (idx < 0 || idx >= m_layouts.size()) return;
    int newidx = idx + (up ? -1 : 1);
    if (newidx < 0 || newidx >= m_layouts.size()) return;
    iter_swap(m_layouts.begin() + idx, m_layouts.begin() + newidx);
    idx = newidx;
}

void PipelineLayoutTool::addDescset(int layout, const char* name)
{
    if (!strlen(name)) return;
    for (auto descset: m_dsets) {
        if (descset == name) {
            return;
        }
    }
    m_dsets.push_back((name));
    for (auto& pl: m_layouts) {
        pl.descsets.resize(m_dsets.size());
    }
}

void PipelineLayoutTool::delDescset(int layout, int idx)
{
    if (idx < 0 || idx >= m_dsets.size()) return;
    m_dsets.erase(m_dsets.begin() + idx);
    for (auto& pl: m_layouts) {
        pl.descsets.resize(m_dsets.size());
    }
}

void PipelineLayoutTool::renameDescset(int layout, int idx, const char * name)
{
    if (!strlen(name)) return;
    if (idx < 0 || idx >= m_dsets.size()) return;
    m_dsets[idx] = name;
}

void PipelineLayoutTool::reorderDescset(int layout, int& idx, bool up)
{
    if (idx < 0 || idx >= m_dsets.size()) return;
    int newidx = idx + (up ? -1 : 1);
    if (newidx < 0 || newidx >= m_dsets.size()) return;
 
    iter_swap(m_dsets.begin() + idx, m_dsets.begin() + newidx);

    for (auto& pl: m_layouts) {
        pl.descsets.resize(m_dsets.size());
        iter_swap(pl.descsets.begin() + idx, pl.descsets.begin() + newidx);
    }

    idx = newidx;
}

void PipelineLayoutTool::reorderDescsetlayout(int layout, int set, int& idx, bool up)
{
    if (layout < 0 || layout >= m_layouts.size()) return;
    auto& descsets = m_layouts[layout].descsets;
    if (set < 0 || set >= descsets.size()) return;
    auto& dslayouts = descsets[set].dlayouts;
    if (idx < 0 || idx >= dslayouts.size()) return;
    int newidx = idx + (up ? -1 : 1);
    if (newidx < 0 || newidx >= dslayouts.size()) return;
    iter_swap(dslayouts.begin() + idx, dslayouts.begin() + newidx);
    idx = newidx;
}

void PipelineLayoutTool::delDescsetlayout(int layout, int set, int idx)
{
    if (layout < 0 || layout >= m_layouts.size()) return;
    auto& descsets = m_layouts[layout].descsets;
    if (set < 0 || set >= descsets.size()) return;
    auto& dslayouts = descsets[set].dlayouts;
    if (idx < 0 || idx >= dslayouts.size()) return;
    dslayouts.erase(dslayouts.begin() + idx);
}

void PipelineLayoutTool::addDescsetlayout(int layout, int set, int bindingIdx)
{
    if (layout < 0 || layout >= m_layouts.size()) return;
    auto& descsets = m_layouts[layout].descsets;
    if (set < 0 || set >= descsets.size()) return;
    auto& dslayouts = descsets[set].dlayouts;
    if (bindingIdx < 0 || bindingIdx > m_dlayouts.size()) return;
    for (auto& dsl : dslayouts) {
        if (dsl == m_dlayouts[bindingIdx].get()) {
            // No duplicates allowed.
            return;
        }
    }
    dslayouts.push_back(m_dlayouts[bindingIdx].get());
}

void PipelineLayoutTool::addDesclayout(const char* name)
{
    if (!strlen(name)) return;
    m_dlayouts.push_back(make_unique<DescriptorLayout>(name));
}

void PipelineLayoutTool::delDesclayout(int idx)
{
    if (idx < 0 || idx >= m_dlayouts.size()) return;

    for (auto& pipeline: m_layouts) {
        for (auto& dset: pipeline.descsets) {
            for (int i = 0; i < dset.dlayouts.size(); i++) {
                if (dset.dlayouts[i] == m_dlayouts[idx].get()) {
                    dset.dlayouts.erase(dset.dlayouts.begin() + i);
                    i--;
                }
            }
        }
    }

    m_dlayouts.erase(m_dlayouts.begin() + idx);
}

void PipelineLayoutTool::renameDesclayout(int idx, const char* name)
{
    if (!strlen(name)) return;
    if (idx < 0 || idx >= m_dlayouts.size()) return;
    m_dlayouts[idx]->name = name;
}

void PipelineLayoutTool::reorderDesclayout(int& idx, bool up)
{
    if (idx < 0 || idx >= m_dlayouts.size()) return;
    int newidx = idx + (up ? -1 : 1);
    if (newidx < 0 || newidx >= m_dlayouts.size()) return;
    iter_swap(m_dlayouts.begin() + idx, m_dlayouts.begin() + newidx);
    idx = newidx;
}

int PipelineLayoutTool::displayNamedList(const char* title, const char* listboxName, const char* objname, const char* abbrev,
                            vector<const char*> listItems, int& activeItem, int listSize,
                            char *newNameBuffer, char *renameBuffer, bool *listChanged)
{
    int retval = 0;
    string moveupStr = (string(abbrev) + " Move Up");
    string movedownStr = (string(abbrev) + " Move Down");

    string listboxStr = (string("##") + listboxName);
    string deleteStr = (string("DELETE THIS ") + objname);
    string addStr = (string("ADD NEW ") + objname);
    string renameStr = (string("RENAME THIS ") + objname);
    string addStrInput = (string("##Add ") + abbrev);
    string renameStrInput = (string("##Rename ") + abbrev);
    string defaultName = (string("new_") + objname + "_name");
    string deletePopupStr = (string("Delete ") + objname + " ?");

    std::transform(deleteStr.begin(), deleteStr.end(), deleteStr.begin(), ::toupper);
    std::transform(addStr.begin(), addStr.end(), addStr.begin(), ::toupper);
    std::transform(renameStr.begin(), renameStr.end(), renameStr.begin(), ::toupper);

    ImGui::TextColored(ImVec4(0.067f, 0.765f, 0.941f, 1.0f), "%s:", title);
    {
        bool newVal = ImGui::ListBox(listboxStr.c_str(), &activeItem, listItems.data(), listItems.size(), listSize);
        if (newVal && renameBuffer) {
            if (activeItem < listItems.size()) {
                strcpy_s(renameBuffer, 256, listItems[activeItem]);
            } else {
                renameBuffer[0] = '\0';
            }
        }
        if (listChanged) (*listChanged) = newVal;

        if (ImGui::Button(moveupStr.c_str(), ImVec2(0, 0))) {
            retval = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button(movedownStr.c_str(), ImVec2(0, 0))) {
            retval = 2;
        }
        if (ImGui::Button(deleteStr.c_str())) {
            ImGui::OpenPopup(deletePopupStr.c_str());
        }
        if (DisplayConfirmWindow(deletePopupStr.c_str()) == 1) {
            retval = 3;
        }

        if (renameBuffer) {
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
            ImGui::InputText(renameStrInput.c_str(), renameBuffer, 256);
            if (ImGui::Button(renameStr.c_str()) == 1) {
                retval = 4;
            }
        }

        if (newNameBuffer) {
            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
            ImGui::InputText(addStrInput.c_str(), newNameBuffer, 256);
            if (ImGui::Button(addStr.c_str()) == 1) {
                retval = 5;
            }
        }

        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
    }

    return retval;
}

DescriptorLayout* PipelineLayoutTool::findDescLayoutByName(const std::string name)
{
    for (auto& dlayout: m_dlayouts) {
        if (dlayout->name == name) return dlayout.get();
    }
    throw std::runtime_error("Could not find layout by name.");
    return 0;
}

int PipelineLayoutTool::findDescLayoutByPtr(DescriptorLayout* dlayout)
{
    for (int i = 0; i < m_dlayouts.size(); i++) {
        auto& dlayout_ = m_dlayouts[i];
        if (dlayout_.get() == dlayout) return i;
    }
    throw std::runtime_error("Could not find layout by ptr.");
    return 0;
}

std::vector<DescriptorLayout*>& PipelineLayoutTool::findDLV(int layout, const std::string name)
{
    if (layout < 0 || layout >= m_layouts.size()) {
        throw std::runtime_error("invalid layout.");
    }

    int idx = -1;
    for (int i = 0; i < m_dsets.size(); i++) {
        if (m_dsets[i] == name) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        throw std::runtime_error("invalid name.");
    }

    assert(m_layouts[layout].descsets.size() == m_dsets.size());
    return m_layouts[layout].descsets[idx].dlayouts;
}

void PipelineLayoutTool::getDLGetSetList(int layout, DescriptorLayout* dl, vector<bool>& out)
{
    assert(dl);
    if (layout < 0 || layout >= m_layouts.size()) {
        throw std::runtime_error("invalid layout.");
    }
    assert(m_layouts[layout].descsets.size() == m_dsets.size());
    assert(out.size() == m_dsets.size());
    for (int i = 0; i < m_layouts[layout].descsets.size(); i++) {
        bool found = false;
        for (int j = 0; j < m_layouts[layout].descsets[i].dlayouts.size(); j++) {
            if (m_layouts[layout].descsets[i].dlayouts[j] == dl) {
                found = true;
            }
        }
        out[i] = found;
    }
}

void PipelineLayoutTool::getDLSetSetList(int layout, DescriptorLayout* dl, vector<bool>& in)
{
    assert(dl);
    if (layout < 0 || layout >= m_layouts.size()) {
        throw std::runtime_error("invalid layout.");
    }
    assert(m_layouts[layout].descsets.size() == m_dsets.size());
    assert(in.size() == m_dsets.size());
    for (int i = 0; i < m_layouts[layout].descsets.size(); i++) {
        bool found = false;
        int foundIdx = -1;
        auto& dlayouts = m_layouts[layout].descsets[i].dlayouts;
        for (int j = 0; j < dlayouts.size(); j++) {
            if (dlayouts[j] == dl) {
                foundIdx = j;
                found = true;
            }
        }
        if (found && !in[i]) {
            // Removed.
            dlayouts.erase(dlayouts.begin() + foundIdx);
        }
        if (!found && in[i]) {
            // Added.
            dlayouts.push_back(dl);
        }
    }
}

void PipelineLayoutTool::save(std::string fileName)
{
    if (fileName.length() <= 0) return;
    if (!EndsWith(fileName, ".vkpipeline.json")) {
        if (EndsWith(fileName, ".vkpipeline")) {
            fileName += ".json";
        } else {
            fileName += ".vkpipeline.json";
        }
    }

    Json::Value value;
    value["num_layouts"] = m_layouts.size();  
    for (auto& playout: m_layouts) {
        Json::Value vplayout;
        vplayout["name"] = playout.name;
        auto setIdx = 0;
        for (auto& dset : playout.descsets) {
            Json::Value vdset;
            vdset["set_index"] = setIdx++;
            for (auto& dl : dset.dlayouts) {
                Json::Value vdl = this->findDescLayoutByPtr(dl);
                vdset["desc_layouts"].append(vdl);
            }
            vplayout["desc_sets"].append(vdset);
        }
        value["layouts"].append(vplayout);
    }
    value["num_sets"] = m_dsets.size();  
    for (auto& dset: m_dsets) {
        Json::Value vdset;
        vdset = dset;
        value["sets"].append(vdset);
    }
    value["num_bindings"] = m_dlayouts.size();  
    for (auto& binding: m_dlayouts) {
        Json::Value vbinding;
        vbinding["name"] = binding->name;
        vbinding["type"] = binding->typeIdx;
        vbinding["typeName"] = descLayoutTypes[binding->typeIdx];
        vbinding["data"] = binding->data;
        vbinding["comment"] = binding->comment;
        vbinding["stageFlagBits"] = binding->stageFlagBits;
        value["bindings"].append(vbinding);
    }

    ofstream ofs;
    ofs.open(fileName.c_str(), std::ofstream::out);
    Json::StyledStreamWriter writer;
    writer.write(ofs, value);
    ofs.close();

    m_filename = fileName;
}

void PipelineLayoutTool::load(std::string fileName)
{
    if (fileName.length() <= 0) return;
    if (!EndsWith(fileName, ".vkpipeline.json")) {
        if (EndsWith(fileName, ".vkpipeline")) {
            fileName += ".json";
        } else {
            fileName += ".vkpipeline.json";
        }
    }

    ifstream ifs;
    ifs.open(fileName.c_str(), std::ofstream::in);

    Json::Value value;
    Json::Features features;
    Json::Reader reader(features);
    reader.parse(ifs, value);
    ifs.close();

    m_dsets.resize(value["num_sets"].asInt());
    for (int i = 0; i < value["sets"].size(); i++) {
        m_dsets[i] = value["sets"][i].asString();
    }

    m_dlayouts.resize(value["num_bindings"].asInt());
    for (int i = 0; i < value["bindings"].size(); i++) {
        if (!m_dlayouts[i].get()) {
            m_dlayouts[i] = make_unique<DescriptorLayout>("UNKNOWN");
        }
        m_dlayouts[i]->name = value["bindings"][i]["name"].asString();
        m_dlayouts[i]->typeIdx = value["bindings"][i]["type"].asInt();
        m_dlayouts[i]->data = value["bindings"][i]["data"].asString();
        m_dlayouts[i]->comment = value["bindings"][i]["comment"].asString();
        m_dlayouts[i]->stageFlagBits = value["bindings"][i]["stageFlagBits"].asUInt();
    }

    m_layouts.resize(value["num_layouts"].asInt());
    for (int i = 0; i < value["layouts"].size(); i++) {
        auto& vplayout = value["layouts"][i];
        m_layouts[i].name = vplayout["name"].asString();
        m_layouts[i].descsets.resize(vplayout["desc_sets"].size());
        if (vplayout["desc_sets"].size() != value["num_sets"].asInt()) {
            throw std::runtime_error("Mismatch between desc_set and num_sets");
        }
        for (int j = 0; j < vplayout["desc_sets"].size(); j++) {
            int setIdx = vplayout["desc_sets"][j]["set_index"].asInt();
            if (setIdx < 0 || setIdx >= vplayout["desc_sets"].size()) {
                throw std::runtime_error("Invalid set index.");
            }
            m_layouts[i].descsets[setIdx].dlayouts.resize(vplayout["desc_sets"][j]["desc_layouts"].size());
            for (int k = 0; k < vplayout["desc_sets"][j]["desc_layouts"].size(); k++) {
                int dlIdx = vplayout["desc_sets"][j]["desc_layouts"][k].asInt();
                if (dlIdx < 0 || dlIdx >= m_dlayouts.size()) {
                    throw std::runtime_error("Invalid DL index.");
                }
                m_layouts[i].descsets[setIdx].dlayouts[k] = m_dlayouts[dlIdx].get();
            }
        }
    }

    m_filename = fileName;
}