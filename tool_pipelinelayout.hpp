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

#ifndef _TOOL_PIPELINE_LAYOUT_
#define _TOOL_PIPELINE_LAYOUT_

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include "tool_framework.hpp"

typedef enum Vk__PipelineStageFlagBits {
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004,
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008,
    VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x00000010,
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800,
    VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000,
    VK_PIPELINE_STAGE_HOST_BIT = 0x00004000,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT = 0x00008000,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000,
} Vk__PipelineStageFlagBits;

// -------------------------------------------------------- DescriptorLayout & PipelineLayout -----------------------------------------------

struct DescriptorLayout {
    std::string name = "UNNAMED_LAYOUT";
    int typeIdx = 0;
    std::string data = "";
    std::string comment = "";
    uint32_t stageFlagBits = 0x00010000; // VK_PIPELINE_STAGE_ALL_COMMANDS_BIT

public:
    DescriptorLayout(std::string name_);
    void stageFlagBitsToBools(std::vector<bool>& out);
    void stageFlagBitsFromBools(std::vector<bool>& out);
};

struct DescriptorSet {
    std::vector<DescriptorLayout*> dlayouts;
};

struct PipelineLayout {
    std::string name = "UNNAMED_PIPELINE_LAYOUT";
    std::vector<DescriptorSet> descsets;

public:
    PipelineLayout() {}
    PipelineLayout(std::string name_);
};

// -------------------------------------------------------- PipelineLayoutTool -----------------------------------------------

class PipelineLayoutTool : public ToolFramework
{
    std::vector<PipelineLayout> m_layouts;
    std::vector< std::unique_ptr<DescriptorLayout> > m_dlayouts;
    std::vector<std::string> m_dsets;
    std::string m_filename = "default.vkpipeline.json";

    // ---------------------- Names list UI helper ----------------------

    int displayNamedList(const char* title, const char* listboxName, const char* objname, const char* abbrev,
                         std::vector<const char*> listItems, int& activeItem, int listSize,
                         char *newNameBuffer, char *renameBuffer, bool *listChanged = nullptr);

    // ---------------------- Editor actions ----------------------

    void addLayout(const char* name);
    void delLayout(int idx);
    void renameLayout(int idx, const char* name);
    void reorderLayout(int& idx, bool up);

    void addDescset(int layout, const char* name);
    void delDescset(int layout, int idx);
    void renameDescset(int layout, int idx, const char* name);
    void reorderDescset(int layout, int& idx, bool up);

    void reorderDescsetlayout(int layout, int set, int& idx, bool up);
    void delDescsetlayout(int layout, int set, int idx);
    void addDescsetlayout(int layout, int set, int bindingIdx);

    void addDesclayout(const char* name);
    void delDesclayout(int idx);
    void renameDesclayout(int idx, const char* name);
    void reorderDesclayout(int& idx, bool up);

    DescriptorLayout* findDescLayoutByName(const std::string name);
    int findDescLayoutByPtr(DescriptorLayout* dlayout);

    std::vector<DescriptorLayout*>& findDLV(int layout, const std::string name);
    void getDLGetSetList(int layout, DescriptorLayout* dl, std::vector<bool>& out);
    void getDLSetSetList(int layout, DescriptorLayout* dl, std::vector<bool>& in);

    void save(std::string fileName);
    void load(std::string fileName);

public:
    const char* getWindowTitle(void) override;
    void init(void) override;
    void render(int screenWidth, int screenHeight) override;
};

#endif // _TOOL_PIPELINE_LAYOUT_