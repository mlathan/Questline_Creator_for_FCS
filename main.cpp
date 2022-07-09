#include "external/ImGuiNodes/application.h"
#include "utilities/builders.h"
#include "utilities/widgets.h"

#include "external/ImGuiNodes/imgui_node_editor.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "external/ImGui/imgui_internal.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>
#include <afxdlgs.h>

#define _AFXDLL

#include "QuestClass.h"
#include <iostream>
#include "json/single_include/nlohmann/json.hpp"
#include <fstream>
#include <span>
#include <cstddef>
#include <iomanip>

static inline ImRect ImGui_GetItemRect()
{
    return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

static inline ImRect ImRect_Expanded(const ImRect& rect, float x, float y)
{
    auto result = rect;
    result.Min.x -= x;
    result.Min.y -= y;
    result.Max.x += x;
    result.Max.y += y;
    return result;
}

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;

using namespace ax;

using ax::Widgets::IconType;

static ed::EditorContext* m_Editor = nullptr;

//extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vkey);
//extern "C" bool Debug_KeyPress(int vkey)
//{
//    static std::map<int, bool> state;
//    auto lastState = state[vkey];
//    state[vkey] = (GetAsyncKeyState(vkey) & 0x8000) != 0;
//    if (state[vkey] && !lastState)
//        return true;
//    else
//        return false;
//}

enum class PinType
{
    Flow,
    Bool,
    Int,
    Float,
    String,
    Object,
    Function,
    Delegate,
};

enum class PinKind
{
    Output,
    Input
};

enum class NodeType
{
    Blueprint,
    Simple,
    Tree,
    Comment,
    Houdini
};

struct Node;

struct Pin
{
    ed::PinId   ID;
    ::Node* Node;
    std::string Name;
    PinType     Type;
    PinKind     Kind;

    Pin(int id, const char* name, PinType type) :
        ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input)
    {
    }
};

struct Node
{
    ed::NodeId ID;
    std::string Name;
    std::vector<Pin> Inputs;
    std::vector<Pin> Outputs;
    ImColor Color;
    NodeType Type;
    ImVec2 Size;

    Quest quest;
    Rewards rewards;
    std::string Typ;
    std::string QuestId;


    std::string State;
    std::string SavedState;

    Node(int id, const char* name, ImColor color = ImColor(255, 255, 255)) :
        ID(id), Name(name), Color(color), Type(NodeType::Blueprint), Size(0, 0)
    {
    }
};

struct Link
{
    ed::LinkId ID;

    ed::PinId StartPinID;
    ed::PinId EndPinID;

    ImColor Color;

    Link(ed::LinkId id, ed::PinId startPinId, ed::PinId endPinId) :
        ID(id), StartPinID(startPinId), EndPinID(endPinId), Color(255, 255, 255)
    {
    }
};


static const int            s_PinIconSize = 24; //24
static std::vector<Node>    s_Nodes;
static std::vector<Link>    s_Links;
static ImTextureID          s_HeaderBackground = nullptr;
//static ImTextureID          s_SampleImage = nullptr;
static ImTextureID          s_SaveIcon = nullptr;
static ImTextureID          s_RestoreIcon = nullptr;

static QuestType::QuestList s_QuestList;

static std::vector<std::string> QuestCategoryVector;
static std::vector<std::string> RecentlyOpenedVector;
static std::vector<std::string> QuestConditionVector;

static std::string FilePathOpened;


struct NodeIdLess
{
    bool operator()(const ed::NodeId& lhs, const ed::NodeId& rhs) const
    {
        return lhs.AsPointer() < rhs.AsPointer();
    }
};

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::vector<std::string> GetRowsOfDataTable(std::string& path)
{
    std::vector<std::string> rowNames;

    if (path.size() > 0)
    {
        std::ifstream t(path, std::ios::binary);

        t.seekg(0, std::ios::end);
        size_t size = t.tellg();
        std::string buffer(size, ' ');
        std::string result;
        t.seekg(0);
        t.read(&buffer[0], size);

        for (int i = 0; i < size; ++i)
        {
            if (buffer[i] == 'G' && buffer[i + 1] == 'a' && buffer[i + 2] == 'm' && buffer[i + 3] == 'e')
            {
                std::string tmpStr;
                int count = i;
                while (buffer[count] != '\0')
                {
                    tmpStr.push_back(buffer[count]);
                    count++;
                }

                size_t found = tmpStr.find_last_of('/');

                tmpStr.erase(0, found + 1);

                size_t dot = tmpStr.find_last_of('.');

                if (dot != std::string::npos)
                {
                    try
                    {
                        found = tmpStr.find_first_of('.');

                        tmpStr.erase(0, found + 1);

                        found = tmpStr.find_first_of('.');

                        tmpStr.erase(0, found + 1);
                    }
                    catch (std::exception ex)
                    {
                        tmpStr = "";
                    }
                }

                while (tmpStr.find_last_of('.') != std::string::npos)
                {
                    found = tmpStr.find_last_of('.');

                    tmpStr.erase(found, tmpStr.size() - found + 1);
                }

                if (tmpStr.find("Name_") != std::string::npos || tmpStr.find("Description_") != std::string::npos)
                    tmpStr = "";

                if (tmpStr != "")
                    rowNames.push_back(tmpStr);

                i = count;
            }
        }

        std::sort(rowNames.begin(), rowNames.end());
        rowNames.erase(std::unique(rowNames.begin(), rowNames.end()), rowNames.end());

        t.close();
    }

    return rowNames;
}

static const float          s_TouchTime = 1.0f;
static std::map<ed::NodeId, float, NodeIdLess> s_NodeTouchTime;

static int s_NextId = 1;
static int GetNextId()
{
    return s_NextId++;
}

static ed::LinkId GetNextLinkId()
{
    return ed::LinkId(GetNextId());
}

static void TouchNode(ed::NodeId id)
{
    s_NodeTouchTime[id] = s_TouchTime;
}

static float GetTouchProgress(ed::NodeId id)
{
    auto it = s_NodeTouchTime.find(id);
    if (it != s_NodeTouchTime.end() && it->second > 0.0f)
        return (s_TouchTime - it->second) / s_TouchTime;
    else
        return 0.0f;
}

static void UpdateTouch()
{
    const auto deltaTime = ImGui::GetIO().DeltaTime;
    for (auto& entry : s_NodeTouchTime)
    {
        if (entry.second > 0.0f)
            entry.second -= deltaTime;
    }
}

static Node* FindNode(ed::NodeId id)
{
    for (auto& node : s_Nodes)
        if (node.ID == id)
            return &node;

    return nullptr;
}

static Link* FindLink(ed::LinkId id)
{
    for (auto& link : s_Links)
        if (link.ID == id)
            return &link;

    return nullptr;
}

static Pin* FindPin(ed::PinId id)
{
    if (!id)
        return nullptr;

    for (auto& node : s_Nodes)
    {
        for (auto& pin : node.Inputs)
            if (pin.ID == id)
                return &pin;

        for (auto& pin : node.Outputs)
            if (pin.ID == id)
                return &pin;
    }

    return nullptr;
}

static Node* FindNodeFromPin(ed::PinId id)
{
    if (!id)
        return nullptr;

    for (auto& node : s_Nodes)
    {
        for (auto& pin : node.Inputs)
            if (pin.ID == id)
                return &node;

        for (auto& pin : node.Outputs)
            if (pin.ID == id)
                return &node;
    }

    return nullptr;
}

static bool IsPinLinked(ed::PinId id)
{
    if (!id)
        return false;

    for (auto& link : s_Links)
        if (link.StartPinID == id || link.EndPinID == id)
            return true;

    return false;
}

static bool CanCreateLink(Pin* a, Pin* b)
{
    if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node)
        return false;

    return true;
}

#pragma region NodeConfig

static void BuildNode(Node* node)
{
    for (auto& input : node->Inputs)
    {
        input.Node = node;
        input.Kind = PinKind::Input;
    }

    for (auto& output : node->Outputs)
    {
        output.Node = node;
        output.Kind = PinKind::Output;
    }
}

static Node* SpawnQuestCharacterNode() 
{
    s_Nodes.emplace_back(GetNextId(), "Quest Character", ImColor(60, 60, 60));
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Quest", PinType::Flow);

    s_Nodes.back().Name = "Character";
    s_Nodes.back().Typ = "Character";

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnQuestNode()
{
    s_Nodes.emplace_back(GetNextId(), "Quest", ImColor(20, 20, 20)); //Quest
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Quest Condition", PinType::Bool);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Quest", PinType::Bool);

    s_Nodes.back().Name = "Quest";
    s_Nodes.back().Typ = "Quest";

    Quest newQuest;

    s_Nodes.back().quest = newQuest;
    s_Nodes.back().quest.quest_name = "Test Quest";

    s_QuestList.push_back(newQuest);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

//default

static Node* SpawnInputActionNode()
{
    s_Nodes.emplace_back(GetNextId(), "InputAction Fire", ImColor(255, 128, 128));
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Delegate);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Pressed", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Released", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnBranchNode()
{
    s_Nodes.emplace_back(GetNextId(), "Branch");
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Condition", PinType::Bool);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "True", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "False", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnDoNNode()
{
    s_Nodes.emplace_back(GetNextId(), "Do N");
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Enter", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "N", PinType::Int);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Reset", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Exit", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Counter", PinType::Int);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnOutputActionNode()
{
    s_Nodes.emplace_back(GetNextId(), "OutputAction");
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Sample", PinType::Float);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Condition", PinType::Bool);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Event", PinType::Delegate);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnPrintStringNode()
{
    s_Nodes.emplace_back(GetNextId(), "Print String");
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "In String", PinType::String);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnMessageNode()
{
    s_Nodes.emplace_back(GetNextId(), "", ImColor(128, 195, 248));
    s_Nodes.back().Type = NodeType::Simple;
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Message", PinType::String);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnSetTimerNode()
{
    s_Nodes.emplace_back(GetNextId(), "Set Timer", ImColor(128, 195, 248));
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Object", PinType::Object);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Function Name", PinType::Function);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Time", PinType::Float);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Looping", PinType::Bool);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnLessNode()
{
    s_Nodes.emplace_back(GetNextId(), "<", ImColor(128, 195, 248));
    s_Nodes.back().Type = NodeType::Simple;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnWeirdNode()
{
    s_Nodes.emplace_back(GetNextId(), "o.O", ImColor(128, 195, 248));
    s_Nodes.back().Type = NodeType::Simple;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Float);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Float);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnTraceByChannelNode()
{
    s_Nodes.emplace_back(GetNextId(), "Single Line Trace by Channel", ImColor(255, 128, 64));
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Start", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "End", PinType::Int);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Trace Channel", PinType::Float);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Trace Complex", PinType::Bool);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Actors to Ignore", PinType::Int);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Draw Debug Type", PinType::Bool);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Ignore Self", PinType::Bool);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Out Hit", PinType::Float);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Return Value", PinType::Bool);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnTreeSequenceNode()
{
    s_Nodes.emplace_back(GetNextId(), "Sequence");
    s_Nodes.back().Type = NodeType::Tree;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnTreeTaskNode()
{
    s_Nodes.emplace_back(GetNextId(), "Move To");
    s_Nodes.back().Type = NodeType::Tree;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnTreeTask2Node()
{
    s_Nodes.emplace_back(GetNextId(), "Random Wait");
    s_Nodes.back().Type = NodeType::Tree;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnComment()
{
    s_Nodes.emplace_back(GetNextId(), "Test Comment");
    s_Nodes.back().Type = NodeType::Comment;
    s_Nodes.back().Size = ImVec2(300, 200);

    return &s_Nodes.back();
}

static Node* SpawnHoudiniTransformNode()
{
    s_Nodes.emplace_back(GetNextId(), "Transform");
    s_Nodes.back().Type = NodeType::Houdini;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnHoudiniGroupNode()
{
    s_Nodes.emplace_back(GetNextId(), "Group");
    s_Nodes.back().Type = NodeType::Houdini;
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "", PinType::Flow);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "", PinType::Flow);

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

void BuildNodes()
{
    for (auto& node : s_Nodes)
        BuildNode(&node);
}

#pragma endregion

const char* Application_GetName()
{
    return "FCS - Questline Creator";
}

char *convert(const std::string & s)
{
    char *pc = new char[s.size()+1];
    std::strcpy(pc, s.c_str());
    return pc; 
}

void SetupStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 0;

    style.Colors[ImGuiCol_Tab]                   = ImVec4(0.2f, 0.2f, 0.2f, 2.0f);
    style.Colors[ImGuiCol_TabActive]             = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);


    style.Colors[ImGuiCol_Text]                  = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.3f, 0.3f, 0.3f, 0.80f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.3f, 0.3f, 0.3f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.3f, 0.3f, 0.3f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

void Application_Initialize()
{
    SetupStyle();

    s_HeaderBackground = Application_LoadTexture("data/BlueprintBackground.png");
    s_SaveIcon = Application_LoadTexture("data/ic_save_white_24dp.png");
    s_RestoreIcon = Application_LoadTexture("data/ic_restore_white_24dp.png");

    ed::Config config;

    config.SettingsFile = "Blueprints.json";

    m_Editor = ed::CreateEditor(&config);
    ed::SetCurrentEditor(m_Editor);

    std::ifstream f("Config.json");

    if (f.fail())
    {
        std::ofstream o("Config.json");
        std::ifstream f("Config.json");

        o.close();
    }

    nlohmann::ordered_json json;

    try
    {
        json = nlohmann::ordered_json::parse(f);
    }
    catch (const std::string& ex){}


    if (!json.is_null())
    {
        nlohmann::ordered_json cate = nlohmann::ordered_json::object();

        cate = json["QuestCategory"];

        for (auto& cat : cate)
        {
            QuestCategoryVector.push_back(cat);
        }

        nlohmann::ordered_json rec = nlohmann::ordered_json::object();

        rec = json["Recently"];

        for (auto& re : rec)
        {
            RecentlyOpenedVector.push_back(re);
        }

        nlohmann::ordered_json cond = nlohmann::ordered_json::object();

        cond = json["QuestConditions"];

        for (auto& con : cond)
        {
            QuestConditionVector.push_back(con);
        }
    }

    f.close();

    //auto& io = ImGui::GetIO();
}

void Application_Finalize()
{
    auto releaseTexture = [](ImTextureID& id)
    {
        if (id)
        {
            Application_DestroyTexture(id);
            id = nullptr;
        }
    };

    releaseTexture(s_RestoreIcon);
    releaseTexture(s_SaveIcon);
    releaseTexture(s_HeaderBackground);

    if (m_Editor)
    {
        ed::DestroyEditor(m_Editor);
        m_Editor = nullptr;
    }
}

static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

ImColor GetIconColor(PinType type)
{
    switch (type)
    {
    default:
    case PinType::Flow:     return ImColor(255, 255, 255);
    case PinType::Bool:     return ImColor(220, 48, 48);
    case PinType::Int:      return ImColor(68, 201, 156);
    case PinType::Float:    return ImColor(147, 226, 74);
    case PinType::String:   return ImColor(124, 21, 153);
    case PinType::Object:   return ImColor(51, 150, 215);
    case PinType::Function: return ImColor(218, 0, 183);
    case PinType::Delegate: return ImColor(255, 48, 48);
    }
};

void DrawPinIcon(const Pin& pin, bool connected, int alpha)
{
    IconType iconType;
    ImColor  color = GetIconColor(pin.Type);
    color.Value.w = alpha / 255.0f;
    switch (pin.Type)
    {
    case PinType::Flow:     iconType = IconType::Flow;   break;
    case PinType::Bool:     iconType = IconType::Circle; break;
    case PinType::Int:      iconType = IconType::Circle; break;
    case PinType::Float:    iconType = IconType::Circle; break;
    case PinType::String:   iconType = IconType::Circle; break;
    case PinType::Object:   iconType = IconType::Circle; break;
    case PinType::Function: iconType = IconType::Circle; break;
    case PinType::Delegate: iconType = IconType::Square; break;
    default:
        return;
    }

    ax::Widgets::Icon(ImVec2(s_PinIconSize, s_PinIconSize), iconType, connected, color, ImColor(32, 32, 32, alpha));
};

void WriteToConfigJson()
{
    nlohmann::ordered_json json = nlohmann::json::object();

    nlohmann::ordered_json recentlyOpened = nlohmann::json::array();
    nlohmann::ordered_json categoryJson = nlohmann::json::array();
    nlohmann::ordered_json conditions = nlohmann::json::array();

    for (auto& op : RecentlyOpenedVector)
        recentlyOpened.push_back(op);

    for (auto& cats : QuestCategoryVector)
        categoryJson.push_back(cats);

    for (auto& cats : QuestConditionVector)
        conditions.push_back(cats);

    json["Recently"] = recentlyOpened;
    json["QuestCategory"] = categoryJson;
    json["QuestConditions"] = conditions;

    std::ofstream o("Config.json");
    o << std::setw(json.size()) << json << std::endl;

    o.close();
}

void ShowAboutWindow(bool* show = nullptr)
{
    ImGui::SetNextWindowSize(ImVec2(450, 350));
    if (!ImGui::Begin("AboutMe", show))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Questline Creator - Developer");
    ImGui::Separator();

    ImGui::Text("Name: ");
    ImGui::SameLine();
    ImGui::Text("Robert Sittig");

    //
    ImGui::Text("Github: ");
    ImGui::SameLine();
    ImGui::InputText("##github", "https://github.com/Sittlon", 30, ImGuiInputTextFlags_ReadOnly);

    ImGui::Text("E-mail: ");
    ImGui::SameLine();
    ImGui::InputText("##email", "robertsittig1996@web.de", 30, ImGuiInputTextFlags_ReadOnly);

    ImGui::End();
}

void ShowLeftPane(float paneWidth, ed::NodeId& Selnode)
{
    auto& io = ImGui::GetIO();

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_FittingPolicyScroll | 
        ImGuiTabBarFlags_FittingPolicyResizeDown;

    float mainWidth = paneWidth;

    std::vector<ed::NodeId> selectedNodes;
    std::vector<ed::LinkId> selectedLinks;

    ImGui::BeginChild("LeftPlane", ImVec2(mainWidth, io.DisplaySize.y), true);

    ImGui::BeginChild("Selection", ImVec2(mainWidth - 12.0f, io.DisplaySize.y / 2 - 150.0f), true);

    if (ImGui::BeginTabBar("LeftPlaneTab", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Selection"))
        {
            paneWidth = 350.0f;

            ImGui::BeginHorizontal("Style Editor", ImVec2(paneWidth + 25.0f, 0));
            ImGui::Spring(0.0f, 0.0f);
            if (ImGui::Button("Zoom to Content"))
                ed::NavigateToContent();
            ImGui::Spring(0.0f);
            if (ImGui::Button("Show Flow"))
            {
                for (auto& link : s_Links)
                    ed::Flow(link.ID);
            }
            ImGui::EndHorizontal();

            selectedNodes.resize(ed::GetSelectedObjectCount());
            selectedLinks.resize(ed::GetSelectedObjectCount());

            int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
            int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

            selectedNodes.resize(nodeCount);
            selectedLinks.resize(linkCount);

            int saveIconWidth = Application_GetTextureWidth(s_SaveIcon);
            int saveIconHeight = Application_GetTextureWidth(s_SaveIcon);
            int restoreIconWidth = Application_GetTextureWidth(s_RestoreIcon);
            int restoreIconHeight = Application_GetTextureWidth(s_RestoreIcon);

            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() + ImVec2(mainWidth, ImGui::GetTextLineHeight()),
                ImColor(60,60,60), ImGui::GetTextLineHeight() * 0.25f);
            ImGui::Spacing(); ImGui::SameLine();
            ImGui::TextUnformatted("Nodes");
            ImGui::Indent();
            for (auto& node : s_Nodes)
            {
                ImGui::PushID(node.ID.AsPointer());
                auto start = ImGui::GetCursorScreenPos();

                if (const auto progress = GetTouchProgress(node.ID))
                {
                    ImGui::GetWindowDrawList()->AddLine(
                        start + ImVec2(-8, 0),
                        start + ImVec2(-8, ImGui::GetTextLineHeight()),
                        IM_COL32(255, 0, 0, 255 - (int)(255 * progress)), 4.0f);
                }

                bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node.ID) != selectedNodes.end();
                if (ImGui::Selectable((node.Name + "##" + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer()))).c_str(), &isSelected))
                {
                    if (io.KeyCtrl)
                    {
                        if (isSelected)
                            ed::SelectNode(node.ID, true);
                        else
                            ed::DeselectNode(node.ID);
                    }
                    else
                        ed::SelectNode(node.ID, false);

                    ed::NavigateToSelection();
                }
                if (ImGui::IsItemHovered() && !node.State.empty())
                    ImGui::SetTooltip("State: %s", node.State.c_str());

                auto id = std::string("(") + std::to_string(reinterpret_cast<uintptr_t>(node.ID.AsPointer())) + ")";
                auto textSize = ImGui::CalcTextSize(id.c_str(), nullptr);
                auto iconPanelPos = start + ImVec2(
                    mainWidth - 30.0f - ImGui::GetStyle().FramePadding.x - ImGui::GetStyle().IndentSpacing - saveIconWidth - restoreIconWidth - ImGui::GetStyle().ItemInnerSpacing.x * 1,
                    (ImGui::GetTextLineHeight() - saveIconHeight) / 2);
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(iconPanelPos.x - textSize.x - ImGui::GetStyle().ItemInnerSpacing.x, start.y),
                    IM_COL32(255, 255, 255, 255), id.c_str(), nullptr);

                auto drawList = ImGui::GetWindowDrawList();
                ImGui::SetCursorScreenPos(iconPanelPos);
                ImGui::SetItemAllowOverlap();

                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::SetItemAllowOverlap();

                ImGui::SameLine(0, 0);
                ImGui::SetItemAllowOverlap();
                ImGui::Dummy(ImVec2(0, (float)restoreIconHeight));

                ImGui::PopID();
            }
            ImGui::Unindent();

            static int changeCount = 0;

            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() + ImVec2(mainWidth, ImGui::GetTextLineHeight()),
                ImColor(60,60,60), ImGui::GetTextLineHeight() * 0.25f);
            ImGui::Spacing(); ImGui::SameLine();
            ImGui::TextUnformatted("Selection");

            ImGui::BeginHorizontal("Selection Stats", ImVec2(mainWidth, 0));
            ImGui::Spring();
            ImGui::EndHorizontal();
            ImGui::Indent();
            for (int i = 0; i < nodeCount; ++i)
            {
                ed::NodeId nodeId = selectedNodes[i];
                Node* node = FindNode(nodeId);

                if(s_Nodes.size() > 0 && node)
                    ImGui::Text((char*)std::string("Node (" + node->Name + ")").c_str());
            } 
            for (int i = 0; i < linkCount; ++i)
            {
                ImGui::Text("Link (%p)", selectedLinks[i].AsPointer());
            }
                
            ImGui::Unindent();

            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
                for (auto& link : s_Links)
                    ed::Flow(link.ID);

            if (ed::HasSelectionChanged())
                ++changeCount;

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::EndChild();

    //Properties

    ed::NodeId contextNodeId = 0;
    int selectionCount = 0;

    for (auto& node : s_Nodes)
    {
        bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node.ID) != selectedNodes.end();
        if (isSelected)
        {
            contextNodeId = node.ID;
            selectionCount++;
        }
    }

    auto* node = FindNode(contextNodeId);
    bool isSel = false;
    if(s_Nodes.size() > 0 && node)
        isSel = std::find(selectedNodes.begin(), selectedNodes.end(), node->ID) != selectedNodes.end();

    ImGui::BeginChild("Properties", ImVec2(mainWidth - 12.0f, io.DisplaySize.y / 2 + 100.0f), true);

    if (ImGui::BeginTabBar("LeftPlaneProperties", tab_bar_flags)) 
    {
        if (selectionCount > 1)
        {
            isSel = false;
        }
        if (isSel) 
        {
            if (node->Typ == "Quest")
            {
                if (ImGui::BeginTabItem("Quest Properties"))
                {
                    try
                    {

                        char* questID = (char*)node->quest.quest_id.data();

                        ImGui::Text("Quest ID: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questID", questID, 801))
                        {
                            node->quest.quest_id = questID;
                            node->quest.name = questID;
                        }

                        char* questName = (char*)node->quest.quest_name.c_str();

                        ImGui::Text("Quest Name: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questName", questName, 801))
                        {
                            node->quest.quest_name = questName;
                        }

                        const char* current_Category = node->quest.category.data();
                        ImGui::Text("Category"); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::BeginCombo("##category", current_Category))
                        {
                            for (int n = 0; n < QuestCategoryVector.size(); n++)
                            {
                                bool is_selected = (current_Category == QuestCategoryVector.at(n));
                                if (ImGui::Selectable(QuestCategoryVector.at(n).c_str(), is_selected))
                                {
                                    current_Category = QuestCategoryVector.at(n).c_str();
                                    node->quest.category = QuestCategoryVector.at(n);
                                }
                                if (is_selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        int amount = node->quest.collection_number;

                        ImGui::Text("Collection Number: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputInt("##collectionNumber", &amount))
                        {
                            node->quest.collection_number = amount;
                        }

                        char* questDescription = (char*)node->quest.description.data();

                        ImGui::Text("Description: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questDescription", questDescription, 501))
                        {
                            node->quest.description = questDescription;
                        }

                        char* questObjective = (char*)node->quest.objective.data();

                        ImGui::Text("Objective: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questObjective", questObjective, 501, ImGuiInputTextFlags_None))
                        {
                            node->quest.objective = node->quest.objective.empty() ? "" : questObjective;
                        }

                    }
                    catch (std::exception ex)
                    {
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Quest Rewards"))
                {
                    if (ImGui::Button("Add Reward"))
                    {
                        Rewards* obj = new Rewards();
                        node->quest.rewards.push_back(*obj);
                        delete(obj);
                    }

                    if (ImGui::TreeNode("Reward"))
                    {
                        for (int k = 0; k < node->quest.rewards.size(); k++)
                        {
                            std::string name = std::string("X##" + std::to_string(k));
                            if (ImGui::Button(name.data()))
                            {
                                node->quest.rewards.erase(node->quest.rewards.begin() + k
                                    , node->quest.rewards.begin() + k + 1);
                            }
                            ImGui::SameLine();
                            if (ImGui::TreeNode((void*)(intptr_t)k, "Reward %d", k + 1))
                            {
                                char* item_current = (char*)node->quest.rewards.at(k).dataTable.data();

                                ImGui::Text("DataTable"); ImGui::Spacing(); ImGui::SameLine();
                                ImGui::InputText("##datatable", item_current, 300);
                                ImGui::SameLine();
                                if (ImGui::Button("..."))
                                {
                                    static char BASED_CODE szFilter[] = "Data Table (*.uasset)|*.uasset|";

                                    CFileDialog dlg(true, CString(".uasset"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilter);

                                    auto result = dlg.DoModal();
                                    if (result != IDOK) 
                                        return;
                                    else
                                    {
                                        std::string filename = dlg.GetPathName().GetString();
                                        node->quest.rewards.at(k).dataTable = filename;
                                    }
                                }

                                const char* rownames = node->quest.rewards.at(k).rowName.data();
                                std::vector<std::string> rows = GetRowsOfDataTable(node->quest.rewards.at(k).dataTable);
                                ImGui::Text("Row Name"); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::BeginCombo("##rownames", rownames))
                                {
                                    for (int n = 0; n < rows.size(); n++)
                                    {
                                        bool is_selected = (rownames == rows.at(n));
                                        if (ImGui::Selectable(rows.at(n).c_str(), is_selected)) {
                                            rownames = rows.at(n).c_str();
                                            node->quest.rewards.at(k).rowName = rows.at(n);
                                        }
                                        if (is_selected)
                                            ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }

                                int amount = node->quest.rewards.at(k).quantity;

                                ImGui::Text("Amount: "); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::InputInt("##amount", &amount))
                                {
                                    node->quest.rewards.at(k).quantity = amount;
                                }

                                ImGui::TreePop();
                            }
                        }

                        ImGui::TreePop();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Quest Conditions"))
                {
                    const char* current_Condition = node->quest.condition.questCondition.data();
                    ImGui::Text("Quest Condition"); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::BeginCombo("##questCondition", current_Condition))
                    {
                        for (int n = 0; n < QuestConditionVector.size(); n++)
                        {
                            bool is_selected = (current_Condition == QuestConditionVector.at(n));
                            if (ImGui::Selectable(QuestConditionVector.at(n).c_str(), is_selected)) {
                                current_Condition = QuestConditionVector.at(n).c_str();
                                node->quest.condition.questCondition = QuestConditionVector.at(n);
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (node->quest.condition.questCondition == "Level")
                    {
                        int amount = node->quest.condition.levelCondition;

                        ImGui::Text("Level: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputInt("##levelCondition", &amount))
                        {
                            node->quest.condition.levelCondition = amount;
                        }
                    }
                    if (node->quest.condition.questCondition == "Quest")
                    {
                        char* questNameCon = (char*)node->quest.condition.questName.c_str();

                        ImGui::Text("Quest Name: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questNameCondition", questNameCon, 51))
                        {
                            node->quest.condition.questName = questNameCon;
                        }
                    }
                    if (node->quest.condition.questCondition == "Level & Quest")
                    {
                        int amount = node->quest.condition.levelCondition;

                        ImGui::Text("Level: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputInt("##level", &amount))
                        {
                            node->quest.condition.levelCondition = amount;
                        }

                        char* questNameCon = (char*)node->quest.condition.questName.c_str();

                        ImGui::Text("Quest Name: "); ImGui::Spacing(); ImGui::SameLine();
                        if (ImGui::InputText("##questNameCondition", questNameCon, 51))
                        {
                            node->quest.condition.questName = questNameCon;
                        }
                    }


                    ImGui::EndTabItem();
                }
            }

            if (node->Typ == "Character")
            {
                if (ImGui::BeginTabItem("Character Properties"))
                {
                    char* charName = (char*)node->Name.data();

                    ImGui::Text("Character Name: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputTextMultiline("##charName", charName, 21))
                    {
                        node->Name = charName;
                    }

                    ImGui::EndTabItem();
                }
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    ImGui::EndChild();
}

void SetQuestComplete(Node& node)
{
    for (auto& output : node.Outputs)
    {
        if (IsPinLinked(output.ID))
        {
            for (auto& link : s_Links)
            {
                if (link.StartPinID == output.ID || link.EndPinID == output.ID)
                {
                    if (output.Name == "Quest")
                    {
                        auto endNode = FindNodeFromPin(link.EndPinID);

                        endNode->quest.condition.questName = node.quest.quest_id;
                    }
                }
            }
        }
    }
}

void SetCharacterLink(Node& node)
{
    for (auto& output : node.Outputs)
    {
        if (IsPinLinked(output.ID) )
        {
            for (auto& link : s_Links)
            {
                if (link.StartPinID == output.ID)
                {
                    auto endNode = FindNodeFromPin(link.EndPinID);

                    node.QuestId = endNode->quest.quest_id;
                }
            }
        }
    }
}

void SaveWork()
{
    nlohmann::ordered_json jsonObjects = nlohmann::json::array();

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            SetQuestComplete(node);
        }
        if (node.Typ == "Character")
        {
            SetCharacterLink(node);
        }
    }

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            nlohmann::ordered_json j = nlohmann::json::object();

            ImVec2 pos = ed::GetNodePosition(node.ID);

            j["PositionX"] = pos.x;
            j["PositionY"] = pos.y;
            j["Typ"] = "Quest";

            j["Name"] = node.quest.name;
            j["Quest ID"] = node.quest.quest_id;
            j["Quest Name"] = node.quest.quest_name;
            j["Quest Description"] = node.quest.description;
            j["Quest Objective"] = node.quest.objective;
            
            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& rew : node.quest.rewards)
            {
                nlohmann::ordered_json reward = nlohmann::json::object();
                nlohmann::ordered_json rewardObject = nlohmann::json::object();

                rewardObject["DataTable"] = rew.dataTable;
                rewardObject["RowName"] = rew.rowName;

                reward["Reward"] = rewardObject;
                reward["Quantity"] = rew.quantity;

                objectives.push_back(reward);
            }

            j["Quest Reward"] = objectives;

            j["Quest Type"] = node.quest.category;
            j["Quest Collection Number"] = node.quest.collection_number;

            nlohmann::ordered_json conditions = nlohmann::json::object();

            conditions["Quest Condition"] = node.quest.condition.questCondition;
            conditions["Level Condition (If Set)"] = node.quest.condition.levelCondition;
            conditions["Quest Name (IF Set)"] = node.quest.condition.questName;

            j["Quest Conditions"] = conditions;

            jsonObjects.push_back(j);
        }

        if (node.Typ == "Character")
        {
            nlohmann::ordered_json j = nlohmann::json::object();

            ImVec2 pos = ed::GetNodePosition(node.ID);

            j["PositionX"] = pos.x;
            j["PositionY"] = pos.y;
            j["Typ"] = "Character";

            j["Name"] = node.Name;
            j["QuestId"] = node.QuestId;

            jsonObjects.push_back(j);
        }
    }

    static char BASED_CODE szFilter[] = "Json Node Files (*.json)|*.json|";

    if (FilePathOpened != "")
    {
        std::string filename = FilePathOpened;
        std::ofstream o(filename);
        o << std::setw(jsonObjects.size()) << jsonObjects << std::endl;

        FilePathOpened = filename;

        o.close();
    }
    else
    {
        CFileDialog dlg(FALSE, CString(".json"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilter);

        auto result = dlg.DoModal();
        if (result != IDOK) 
            return;
        else
        {
            std::string filename = dlg.GetPathName().GetString();
            std::ofstream o(filename);
            o << std::setw(jsonObjects.size()) << jsonObjects << std::endl;

            FilePathOpened = filename;

            o.close();
        }
    }
}

void SaveWorkAs()
{
    nlohmann::ordered_json jsonObjects = nlohmann::json::array();

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            SetQuestComplete(node);
        }
        if (node.Typ == "Character")
        {
            SetCharacterLink(node);
        }
    }

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            nlohmann::ordered_json j = nlohmann::json::object();

            ImVec2 pos = ed::GetNodePosition(node.ID);

            j["PositionX"] = pos.x;
            j["PositionY"] = pos.y;
            j["Typ"] = "Quest";

            j["Name"] = node.quest.name;
            j["Quest ID"] = node.quest.quest_id;
            j["Quest Name"] = node.quest.quest_name;
            j["Quest Description"] = node.quest.description;
            j["Quest Objective"] = node.quest.objective;

            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& rew : node.quest.rewards)
            {
                nlohmann::ordered_json reward = nlohmann::json::object();
                nlohmann::ordered_json rewardObject = nlohmann::json::object();

                rewardObject["DataTable"] = rew.dataTable;
                rewardObject["RowName"] = rew.rowName;

                reward["Reward"] = rewardObject;
                reward["Quantity"] = rew.quantity;

                objectives.push_back(reward);
            }

            j["Quest Reward"] = objectives;

            j["Quest Type"] = node.quest.category;
            j["Quest Collection Number"] = node.quest.collection_number;

            nlohmann::ordered_json conditions = nlohmann::json::object();

            conditions["Quest Condition"] = node.quest.condition.questCondition;
            conditions["Level Condition (If Set)"] = node.quest.condition.levelCondition;
            conditions["Quest Name (IF Set)"] = node.quest.condition.questName;

            j["Quest Conditions"] = conditions;

            jsonObjects.push_back(j);
        }

        if (node.Typ == "Character")
        {
            nlohmann::ordered_json j = nlohmann::json::object();

            ImVec2 pos = ed::GetNodePosition(node.ID);

            j["PositionX"] = pos.x;
            j["PositionY"] = pos.y;
            j["Typ"] = "Character";

            j["Name"] = node.Name;
            j["QuestId"] = node.QuestId;

            jsonObjects.push_back(j);
        }
    }

    static char BASED_CODE szFilter[] = "Json Node Files (*.json)|*.json|";

    CFileDialog dlg(FALSE, CString(".json"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilter);

    auto result = dlg.DoModal();
    if (result != IDOK) 
        return;
    else
    {
        std::string filename = dlg.GetPathName().GetString();
        std::ofstream o(filename);
        o << std::setw(jsonObjects.size()) << jsonObjects << std::endl;

        FilePathOpened = filename;

        o.close();
    }
}

void ExportToJson()
{
    nlohmann::ordered_json jsonObjects = nlohmann::json::array();

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            SetQuestComplete(node);
        }
        if (node.Typ == "Character")
        {
            SetCharacterLink(node);
        }
    }

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            nlohmann::ordered_json j = nlohmann::json::object();

            j["Name"] = node.quest.name;
            j["Quest ID"] = node.quest.quest_id;
            j["Quest Name"] = node.quest.quest_name;
            j["Quest Description"] = node.quest.description;
            j["Quest Objective"] = node.quest.objective;

            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& rew : node.quest.rewards)
            {
                nlohmann::ordered_json reward = nlohmann::json::object();
                nlohmann::ordered_json rewardObject = nlohmann::json::object();

                std::string path = rew.dataTable;

                size_t pos = path.find("Content");
                path.erase(0, pos);
                replace(path, "Content", "Game");
                pos = path.find_last_of('.');
                path.erase(pos + 1);
                pos = path.find_last_of("\\");
                std::string tm = path.substr(pos + 1);
                tm.erase(tm.find('.'));
                path.append(tm);
                replaceAll(path, "\\", "/");
                path.insert(0, "DataTable'");
                path.append("'");

                rewardObject["DataTable"] = path;
                rewardObject["RowName"] = rew.rowName;

                reward["Reward"] = rewardObject;
                reward["Quantity"] = rew.quantity;

                objectives.push_back(reward);
            }

            j["Quest Reward"] = objectives;

            j["Quest Type"] = node.quest.category;
            j["Quest Collection Number"] = node.quest.collection_number;

            nlohmann::ordered_json conditions = nlohmann::json::object();

            conditions["Quest Condition"] = node.quest.condition.questCondition;
            conditions["Level Condition (If Set)"] = node.quest.condition.levelCondition;
            conditions["Quest Name (IF Set)"] = node.quest.condition.questName;

            j["Quest Conditions"] = conditions;

            jsonObjects.push_back(j);
        }
    }

    static char BASED_CODE szFilter[] = "Json Node Files (*.json)|*.json|";

    CFileDialog dlg(FALSE, CString(".json"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilter);

    auto result = dlg.DoModal();
    if (result != IDOK) 
        return;
    else
    {
        std::string filename = dlg.GetPathName().GetString();
        std::ofstream o(filename);
        o << std::setw(jsonObjects.size()) << jsonObjects << std::endl;

        o.close();
    }

    jsonObjects.~basic_json();
}

void CreateLinks()
{
    for (auto& node : s_Nodes)
    {
        if (node.quest.condition.questName != "")
        {
            for (auto& search : s_Nodes)
            {
                if (search.quest.quest_id == node.quest.condition.questName)
                {
                    s_Links.emplace_back(Link(GetNextLinkId(), search.Outputs[1].ID, node.Inputs[1].ID));
                    s_Links.back().Color = GetIconColor(search.Outputs[1].Type);
                    s_Links.emplace_back(Link(GetNextLinkId(), search.Outputs[0].ID, node.Inputs[0].ID));
                }
            }
        }

        if (node.Typ == "Character")
        {
            for (auto& search : s_Nodes)
            {
                if (search.quest.quest_id == node.QuestId)
                {
                    s_Links.emplace_back(Link(GetNextLinkId(), node.Outputs[0].ID, search.Inputs[0].ID));
                }
            }
        }
    }
}

void OpenFromJson(std::string path = "")
{
    if (s_Nodes.size() > 0)
    {
        int result = MessageBox(NULL, "There are already Nodes placed. They will be deleted. Do you want to continue?", NULL, MB_YESNO);
        if (result == 6)
        {
            s_Nodes.clear();
            s_Links.clear();

            FilePathOpened.clear();
        }
        else if(result == 7)
        {
            return;
        }
    }

    static char BASED_CODE szFilter[] = "Json Node Files (*.json)|*.json|";

    std::string pathName;

    if (path == "")
    {
        CFileDialog dlg(true, NULL, NULL, NULL, szFilter, NULL);

        auto result = dlg.DoModal();
        if (result != IDOK)
            return;
        else
            pathName = dlg.GetPathName().GetString();
    }
    else
    {
        pathName = path;
    }

    if(pathName != "")
    {
        try
        {
            std::ifstream f(pathName);
            nlohmann::ordered_json json;
            
            json = nlohmann::ordered_json::parse(f);

            for (auto& value : json)
            {
                if (value["PositionX"].is_null())
                    break;

                if (value["Typ"] == "Quest")
                {
                    Node* node = SpawnQuestNode();

                    ImVec2 pos = ImVec2(value["PositionX"], value["PositionY"]);
                    ed::SetNodePosition(node->ID, pos);

                    node->quest.name = value["Name"];
                    node->quest.quest_id = value["Quest ID"];
                    node->quest.quest_name = value["Quest Name"];
                    node->quest.description = value["Quest Description"];
                    node->quest.objective = value["Quest Objective"];

                    nlohmann::ordered_json obj = nlohmann::ordered_json::array();
                    obj = value["Quest Reward"];

                    for (auto& rews : obj)
                    {
                        Rewards rewards;

                        nlohmann::ordered_json rewa = nlohmann::ordered_json::object();
                        rewa = rews["Reward"];

                        rewards.dataTable = rewa["DataTable"];
                        rewards.rowName = rewa["RowName"];
                        rewards.quantity = rews["Quantity"];

                        node->quest.rewards.push_back(rewards);
                    }

                    node->quest.category = value["Quest Type"];
                    node->quest.collection_number = value["Quest Collection Number"];
                    
                    nlohmann::ordered_json condis = nlohmann::json::object();
                    condis = value["Quest Conditions"];

                    
                    node->quest.condition.questCondition = condis["Quest Condition"];
                    node->quest.condition.levelCondition = condis["Level Condition (If Set)"];
                    node->quest.condition.questName = condis["Quest Name (IF Set)"];
                }

                if (value["Typ"] == "Character")
                {
                    Node* node = SpawnQuestCharacterNode();

                    node->Name = value["Name"];
                    ImVec2 pos = ImVec2(value["PositionX"], value["PositionY"]);
                    ed::SetNodePosition(node->ID, pos);

                    node->QuestId = value["QuestId"];
                }
            }

            CreateLinks();

            if (std::find(RecentlyOpenedVector.begin(), RecentlyOpenedVector.end(), pathName) == RecentlyOpenedVector.end())
            {
                RecentlyOpenedVector.push_back(pathName);
                WriteToConfigJson();
            }

            FilePathOpened = pathName;
        }
        catch (...) {}
    }
}

void ShowMenuFile()
{
    if (ImGui::MenuItem("New")) 
    {
        s_Nodes.clear();
        s_Links.clear();

        FilePathOpened.clear();
    }
    if (ImGui::MenuItem("Open")) 
    {
        OpenFromJson();
    }
    if (ImGui::BeginMenu("Open Recent"))
    {
        for (auto& item : RecentlyOpenedVector)
        {
            if (ImGui::MenuItem(item.c_str()))
            {
                OpenFromJson(item);
            }
        }

        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save")) 
    {
        SaveWork();
    }
    if (ImGui::MenuItem("Save as"))
    {
        SaveWorkAs();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Export as Json for UE Datatable")) 
    {
        ExportToJson();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Quit")) 
    {
        int quit = MessageBox(NULL, "Do you want to quit?", NULL, MB_YESNO);
        if(quit == 6)
            PostQuitMessage(0);
    }
}

void Application_Frame()
{
    UpdateTouch();

    static bool showAboutWindow = false;

    auto& io = ImGui::GetIO();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            ShowMenuFile();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About"))
        {
            ImGui::MenuItem("Questline Creator - Developer", NULL, &showAboutWindow);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (showAboutWindow)
        ShowAboutWindow(&showAboutWindow);

    ed::SetCurrentEditor(m_Editor);

# if 0
    {
        for (auto x = -io.DisplaySize.y; x < io.DisplaySize.x; x += 10.0f)
        {
            ImGui::GetWindowDrawList()->AddLine(ImVec2(x, 0), ImVec2(x + io.DisplaySize.y, io.DisplaySize.y),
                IM_COL32(255, 255, 0, 255));
        }
    }
# endif

    static ed::NodeId contextNodeId = 0;
    static ed::LinkId contextLinkId = 0;
    static ed::PinId  contextPinId = 0;
    static bool createNewNode = false;
    static Pin* newNodeLinkPin = nullptr;
    static Pin* newLinkPin = nullptr;

    static float leftPaneWidth = 400.0f;
    static float rightPaneWidth = 800.0f;
    Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

    ShowLeftPane(leftPaneWidth, contextNodeId);

    ImGui::SameLine(0.0f, 12.0f);

    ed::Begin("Node editor");
    {
        auto cursorTopLeft = ImGui::GetCursorScreenPos();

        util::BlueprintNodeBuilder builder(s_HeaderBackground, Application_GetTextureWidth(s_HeaderBackground), Application_GetTextureHeight(s_HeaderBackground));

        for (auto& node : s_Nodes)
        {
            if (node.Type != NodeType::Blueprint && node.Type != NodeType::Simple)
                continue;

            const auto isSimple = node.Type == NodeType::Simple;

            bool hasOutputDelegates = false;
            for (auto& output : node.Outputs)
                if (output.Type == PinType::Delegate)
                    hasOutputDelegates = true;

            builder.Begin(node.ID);
            if (!isSimple)
            {
                builder.Header(node.Color);
                ImGui::BeginVertical("##NodeHori", ImVec2(-100, 58));
                if (node.Name == "Quest")
                {
                    std::string q1("QuestID: (");
                    std::string qId(node.quest.quest_id.c_str());
                    std::string q2(")");
                    ImGui::Spring(0);
                    ImGui::TextUnformatted(node.Name.c_str()); ImGui::SameLine(); ImGui::TextUnformatted((char*)std::string(q1 + qId + q2).c_str());
                    ImGui::Spring(1);
                    ImGui::TextColored(ImVec4(1.0f,1.0f,1.0f,1.0f), node.quest.quest_name.c_str());
                    ImGui::Dummy(ImVec2(422, 0));
                }
                else
                {
                    ImGui::Spring(0);
                    ImGui::TextUnformatted(node.Name.c_str());
                    ImGui::Spring(1);
                    ImGui::Dummy(ImVec2(170, 0));
                }
                
                ImGui::EndVertical();

                if (hasOutputDelegates)
                {
                    ImGui::BeginVertical("delegates");
                    ImGui::Spring(1, 0);
                    for (auto& output : node.Outputs)
                    {
                        if (output.Type != PinType::Delegate)
                            continue;

                        auto alpha = ImGui::GetStyle().Alpha;
                        if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
                            alpha = alpha * (48.0f / 255.0f);

                        ed::BeginPin(output.ID, ed::PinKind::Output);
                        ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                        ed::PinPivotSize(ImVec2(0, 0));
                        ImGui::BeginHorizontal(output.ID.AsPointer());
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                        if (!output.Name.empty())
                        {
                            ImGui::TextUnformatted(output.Name.c_str());
                            ImGui::Spring(0);
                        }
                        DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
                        ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
                        ImGui::EndHorizontal();
                        ImGui::PopStyleVar();
                        ed::EndPin();

                        //DrawItemRect(ImColor(255, 0, 0));
                    }
                    ImGui::Spring(1, 0);
                    ImGui::EndVertical();
                    ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
                }
                else
                    ImGui::Spring(0);
                builder.EndHeader();
            }

            for (auto& input : node.Inputs)
            {
                auto alpha = ImGui::GetStyle().Alpha;
                if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
                    alpha = alpha * (48.0f / 255.0f);

                builder.Input(input.ID);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
                ImGui::Spring(0);
                if (!input.Name.empty())
                {
                    ImGui::TextUnformatted(input.Name.c_str());
                    ImGui::Spring(0);
                }
                if (input.Type == PinType::Bool)
                {
                    ImGui::Spring(0);
                }
                ImGui::PopStyleVar();
                builder.EndInput();
            }

            if (isSimple)
            {
                builder.Middle();

                ImGui::Spring(1, 0);
                ImGui::TextUnformatted(node.Name.c_str());
                ImGui::Spring(1, 0);
            }

            for (auto& output : node.Outputs)
            {
                if (!isSimple && output.Type == PinType::Delegate)
                    continue;

                auto alpha = ImGui::GetStyle().Alpha;
                if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
                    alpha = alpha * (48.0f / 255.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                builder.Output(output.ID);
                if (output.Type == PinType::String)
                {
                    static char buffer[128] = "Edit Me\nMultiline!";
                    static bool wasActive = false;

                    ImGui::PushItemWidth(100.0f);
                    ImGui::InputText("##edit", buffer, 127);
                    ImGui::PopItemWidth();
                    if (ImGui::IsItemActive() && !wasActive)
                    {
                        ed::EnableShortcuts(false);
                        wasActive = true;
                    }
                    else if (!ImGui::IsItemActive() && wasActive)
                    {
                        ed::EnableShortcuts(true);
                        wasActive = false;
                    }
                    ImGui::Spring(0);
                }
                if (!output.Name.empty())
                {
                    ImGui::Spring(0);
                    ImGui::TextUnformatted(output.Name.c_str());
                }
                ImGui::Spring(0);
                DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
                ImGui::PopStyleVar();
                builder.EndOutput();
            }

            builder.End();
        }

#pragma region unusedNodeStuff
        //for (auto& node : s_Nodes)
        //{
        //    if (node.Type != NodeType::Tree)
        //        continue;

        //    const float rounding = 5.0f;
        //    const float padding = 12.0f;

        //    const auto pinBackground = ed::GetStyle().Colors[ed::StyleColor_NodeBg];

        //    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(128, 128, 128, 200));
        //    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(32, 32, 32, 200));
        //    ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(60, 180, 255, 150));
        //    ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(60, 180, 255, 150));

        //    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
        //    ed::PushStyleVar(ed::StyleVar_NodeRounding, rounding);
        //    ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(0.0f, 1.0f));
        //    ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
        //    ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
        //    ed::PushStyleVar(ed::StyleVar_PinBorderWidth, 1.0f);
        //    ed::PushStyleVar(ed::StyleVar_PinRadius, 5.0f);
        //    ed::BeginNode(node.ID);

        //    ImGui::BeginVertical(node.ID.AsPointer());
        //    ImGui::BeginHorizontal("inputs");
        //    ImGui::Spring(0, padding * 2);

        //    ImRect inputsRect;
        //    int inputAlpha = 200;
        //    if (!node.Inputs.empty())
        //    {
        //        auto& pin = node.Inputs[0];
        //        ImGui::Dummy(ImVec2(0, padding));
        //        ImGui::Spring(1, 0);
        //        inputsRect = ImGui_GetItemRect();

        //        ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
        //        ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
        //        ed::PushStyleVar(ed::StyleVar_PinCorners, 12);
        //        ed::BeginPin(pin.ID, ed::PinKind::Input);
        //        ed::PinPivotRect(inputsRect.GetTL(), inputsRect.GetBR());
        //        ed::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
        //        ed::EndPin();
        //        ed::PopStyleVar(3);

        //        if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
        //            inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        //    }
        //    else
        //        ImGui::Dummy(ImVec2(0, padding));

        //    ImGui::Spring(0, padding * 2);
        //    ImGui::EndHorizontal();

        //    ImGui::BeginHorizontal("content_frame");
        //    ImGui::Spring(1, padding);

        //    ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
        //    ImGui::Dummy(ImVec2(160, 0));
        //    ImGui::Spring(1);
        //    ImGui::TextUnformatted(node.Name.c_str());
        //    ImGui::Spring(1);
        //    ImGui::EndVertical();
        //    auto contentRect = ImGui_GetItemRect();

        //    ImGui::Spring(1, padding);
        //    ImGui::EndHorizontal();

        //    ImGui::BeginHorizontal("outputs");
        //    ImGui::Spring(0, padding * 2);

        //    ImRect outputsRect;
        //    int outputAlpha = 200;
        //    if (!node.Outputs.empty())
        //    {
        //        auto& pin = node.Outputs[0];
        //        ImGui::Dummy(ImVec2(0, padding));
        //        ImGui::Spring(1, 0);
        //        outputsRect = ImGui_GetItemRect();

        //        ed::PushStyleVar(ed::StyleVar_PinCorners, 3);
        //        ed::BeginPin(pin.ID, ed::PinKind::Output);
        //        ed::PinPivotRect(outputsRect.GetTL(), outputsRect.GetBR());
        //        ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
        //        ed::EndPin();
        //        ed::PopStyleVar();

        //        if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
        //            outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        //    }
        //    else
        //        ImGui::Dummy(ImVec2(0, padding));

        //    ImGui::Spring(0, padding * 2);
        //    ImGui::EndHorizontal();

        //    ImGui::EndVertical();

        //    ed::EndNode();
        //    ed::PopStyleVar(7);
        //    ed::PopStyleColor(4);

        //    auto drawList = ed::GetNodeBackgroundDrawList(node.ID);

        //    drawList->AddRectFilled(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
        //        IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
        //    //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        //    drawList->AddRect(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
        //        IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
        //    //ImGui::PopStyleVar();
        //    drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
        //        IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
        //    //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        //    drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
        //        IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
        //    //ImGui::PopStyleVar();
        //    drawList->AddRectFilled(contentRect.GetTL(), contentRect.GetBR(), IM_COL32(24, 64, 128, 200), 0.0f);
        //    //ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
        //    drawList->AddRect(
        //        contentRect.GetTL(),
        //        contentRect.GetBR(),
        //        IM_COL32(48, 128, 255, 100), 0.0f);
        //    //ImGui::PopStyleVar();
        //}

        //for (auto& node : s_Nodes)
        //{
        //    if (node.Type != NodeType::Houdini)
        //        continue;

        //    const float rounding = 10.0f;
        //    const float padding = 12.0f;


        //    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(229, 229, 229, 200));
        //    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(125, 125, 125, 200));
        //    ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(229, 229, 229, 60));
        //    ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(125, 125, 125, 60));

        //    const auto pinBackground = ed::GetStyle().Colors[ed::StyleColor_NodeBg];

        //    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
        //    ed::PushStyleVar(ed::StyleVar_NodeRounding, rounding);
        //    ed::PushStyleVar(ed::StyleVar_SourceDirection, ImVec2(0.0f, 1.0f));
        //    ed::PushStyleVar(ed::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
        //    ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
        //    ed::PushStyleVar(ed::StyleVar_PinBorderWidth, 1.0f);
        //    ed::PushStyleVar(ed::StyleVar_PinRadius, 6.0f);
        //    ed::BeginNode(node.ID);

        //    ImGui::BeginVertical(node.ID.AsPointer());
        //    if (!node.Inputs.empty())
        //    {
        //        ImGui::BeginHorizontal("inputs");
        //        ImGui::Spring(1, 0);

        //        ImRect inputsRect;
        //        int inputAlpha = 200;
        //        for (auto& pin : node.Inputs)
        //        {
        //            ImGui::Dummy(ImVec2(padding, padding));
        //            inputsRect = ImGui_GetItemRect();
        //            ImGui::Spring(1, 0);
        //            inputsRect.Min.y -= padding;
        //            inputsRect.Max.y -= padding;

        //            //ed::PushStyleVar(ed::StyleVar_PinArrowSize, 10.0f);
        //            //ed::PushStyleVar(ed::StyleVar_PinArrowWidth, 10.0f);
        //            ed::PushStyleVar(ed::StyleVar_PinCorners, 15);
        //            ed::BeginPin(pin.ID, ed::PinKind::Input);
        //            ed::PinPivotRect(inputsRect.GetCenter(), inputsRect.GetCenter());
        //            ed::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
        //            ed::EndPin();
        //            //ed::PopStyleVar(3);
        //            ed::PopStyleVar(1);

        //            auto drawList = ImGui::GetWindowDrawList();
        //            drawList->AddRectFilled(inputsRect.GetTL(), inputsRect.GetBR(),
        //                IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 15);
        //            drawList->AddRect(inputsRect.GetTL(), inputsRect.GetBR(),
        //                IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 15);

        //            if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
        //                inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        //        }

        //        //ImGui::Spring(1, 0);
        //        ImGui::EndHorizontal();
        //    }

        //    ImGui::BeginHorizontal("content_frame");
        //    ImGui::Spring(1, padding);

        //    ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
        //    ImGui::Dummy(ImVec2(160, 0));
        //    ImGui::Spring(1);
        //    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        //    ImGui::TextUnformatted(node.Name.c_str());
        //    ImGui::PopStyleColor();
        //    ImGui::Spring(1);
        //    ImGui::EndVertical();
        //    auto contentRect = ImGui_GetItemRect();

        //    ImGui::Spring(1, padding);
        //    ImGui::EndHorizontal();

        //    if (!node.Outputs.empty())
        //    {
        //        ImGui::BeginHorizontal("outputs");
        //        ImGui::Spring(1, 0);

        //        ImRect outputsRect;
        //        int outputAlpha = 200;
        //        for (auto& pin : node.Outputs)
        //        {
        //            ImGui::Dummy(ImVec2(padding, padding));
        //            outputsRect = ImGui_GetItemRect();
        //            ImGui::Spring(1, 0);
        //            outputsRect.Min.y += padding;
        //            outputsRect.Max.y += padding;

        //            ed::PushStyleVar(ed::StyleVar_PinCorners, 3);
        //            ed::BeginPin(pin.ID, ed::PinKind::Output);
        //            ed::PinPivotRect(outputsRect.GetCenter(), outputsRect.GetCenter());
        //            ed::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
        //            ed::EndPin();
        //            ed::PopStyleVar();

        //            auto drawList = ImGui::GetWindowDrawList();
        //            drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR(),
        //                IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 15);
        //            drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR(),
        //                IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 15);


        //            if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
        //                outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
        //        }

        //        ImGui::EndHorizontal();
        //    }

        //    ImGui::EndVertical();

        //    ed::EndNode();
        //    ed::PopStyleVar(7);
        //    ed::PopStyleColor(4);

        //    auto drawList = ed::GetNodeBackgroundDrawList(node.ID);
        //}

        for (auto& node : s_Nodes)
        {
            if (node.Type != NodeType::Comment)
                continue;

            const float commentAlpha = 0.75f;

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
            ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 64));
            ed::BeginNode(node.ID);
            ImGui::PushID(node.ID.AsPointer());
            ImGui::BeginVertical("content");
            ImGui::BeginHorizontal("horizontal");
            ImGui::Spring(1);
            ImGui::TextUnformatted(node.Name.c_str());
            ImGui::Spring(1);
            ImGui::EndHorizontal();
            ed::Group(node.Size);
            ImGui::EndVertical();
            ImGui::PopID();
            ed::EndNode();
            ed::PopStyleColor(2);
            ImGui::PopStyleVar();

            if (ed::BeginGroupHint(node.ID))
            {
                //auto alpha   = static_cast<int>(commentAlpha * ImGui::GetStyle().Alpha * 255);
                auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

                //ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha * ImGui::GetStyle().Alpha);

                auto min = ed::GetGroupMin();
                //auto max = ed::GetGroupMax();

                ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
                ImGui::BeginGroup();
                ImGui::TextUnformatted(node.Name.c_str());
                ImGui::EndGroup();

                auto drawList = ed::GetHintBackgroundDrawList();

                auto hintBounds = ImGui_GetItemRect();
                auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

                drawList->AddRectFilled(
                    hintFrameBounds.GetTL(),
                    hintFrameBounds.GetBR(),
                    IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

                drawList->AddRect(
                    hintFrameBounds.GetTL(),
                    hintFrameBounds.GetBR(),
                    IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);

                //ImGui::PopStyleVar();
            }
            ed::EndGroupHint();
        }
#pragma endregion

        for (auto& link : s_Links)
            ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

        if (!createNewNode)
        {
            if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
            {
                auto showLabel = [](const char* label, ImColor color)
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
                    auto size = ImGui::CalcTextSize(label);

                    auto padding = ImGui::GetStyle().FramePadding;
                    auto spacing = ImGui::GetStyle().ItemSpacing;

                    ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

                    auto rectMin = ImGui::GetCursorScreenPos() - padding;
                    auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

                    auto drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
                    ImGui::TextUnformatted(label);
                };

                ed::PinId startPinId = 0, endPinId = 0;
                if (ed::QueryNewLink(&startPinId, &endPinId))
                {
                    auto startPin = FindPin(startPinId);
                    auto endPin = FindPin(endPinId);

                    newLinkPin = startPin ? startPin : endPin;

                    if (startPin->Kind == PinKind::Input)
                    {
                        std::swap(startPin, endPin);
                        std::swap(startPinId, endPinId);
                    }

                    if (startPin && endPin)
                    {
                        if (endPin == startPin)
                        {
                            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                        }
                        else if (endPin->Kind == startPin->Kind)
                        {
                            showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
                            ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                        }
                        //else if (endPin->Node == startPin->Node)
                        //{
                        //    showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
                        //    ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
                        //}
                        else if (endPin->Type != startPin->Type)
                        {
                            showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
                            ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                        }
                        else
                        {
                            showLabel("+ Create Link", ImColor(32, 45, 32, 180));
                            if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
                            {
                                s_Links.emplace_back(Link(GetNextId(), startPinId, endPinId));
                                s_Links.back().Color = GetIconColor(startPin->Type);
                            }
                        }
                    }
                }

                ed::PinId pinId = 0;
                if (ed::QueryNewNode(&pinId))
                {
                    newLinkPin = FindPin(pinId);
                    if (newLinkPin)
                        showLabel("+ Create Node", ImColor(32, 45, 32, 180));

                    if (ed::AcceptNewItem())
                    {
                        createNewNode = true;
                        newNodeLinkPin = FindPin(pinId);
                        newLinkPin = nullptr;
                        ed::Suspend();
                        ImGui::OpenPopup("Create New Node");
                        ed::Resume();
                    }
                }
            }
            else
                newLinkPin = nullptr;

            ed::EndCreate();

            if (ed::BeginDelete())
            {
                ed::LinkId linkId = 0;
                while (ed::QueryDeletedLink(&linkId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        auto id = std::find_if(s_Links.begin(), s_Links.end(), [linkId](auto& link) { return link.ID == linkId; });
                        if (id != s_Links.end())
                            s_Links.erase(id);
                    }
                }

                ed::NodeId nodeId = 0;
                while (ed::QueryDeletedNode(&nodeId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        auto id = std::find_if(s_Nodes.begin(), s_Nodes.end(), [nodeId](auto& node) { return node.ID == nodeId; });
                        if (id != s_Nodes.end())
                            s_Nodes.erase(id);
                    }
                }
            }
            ed::EndDelete();
        }

        ImGui::SetCursorScreenPos(cursorTopLeft);
    }

# if 1 Context Menu
    auto openPopupPosition = ImGui::GetMousePos();
    ed::Suspend();
    if (ed::ShowNodeContextMenu(&contextNodeId))
        ImGui::OpenPopup("Node Context Menu");
    else if (ed::ShowPinContextMenu(&contextPinId))
        ImGui::OpenPopup("Pin Context Menu");
    else if (ed::ShowLinkContextMenu(&contextLinkId))
        ImGui::OpenPopup("Link Context Menu");
    else if (ed::ShowBackgroundContextMenu())
    {
        ImGui::OpenPopup("Create New Node");
        newNodeLinkPin = nullptr;
    }
    ed::Resume();

    ed::Suspend();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    auto node = FindNode(contextNodeId);

    if (ImGui::BeginPopup("Node Context Menu"))
    {
        ImGui::TextUnformatted("Node Properties");
        ImGui::Separator();
        if (node)
        {
            ImGui::Text("ID: %p", node->ID.AsPointer());
        }
        else
            ImGui::Text("Unknown node: %p", contextNodeId.AsPointer());
        ImGui::Separator();
        if (ImGui::MenuItem("Delete"))
            ed::DeleteNode(contextNodeId);
        ImGui::EndPopup();
    }
    

    if (ImGui::BeginPopup("Pin Context Menu"))
    {
        auto pin = FindPin(contextPinId);

        ImGui::TextUnformatted("Pin Context Menu");
        ImGui::Separator();
        if (pin)
        {
            ImGui::Text("ID: %p", pin->ID.AsPointer());
            if (pin->Node)
                ImGui::Text("Node: %p", pin->Node->ID.AsPointer());
            else
                ImGui::Text("Node: %s", "<none>");
        }
        else
            ImGui::Text("Unknown pin: %p", contextPinId.AsPointer());

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Link Context Menu"))
    {
        auto link = FindLink(contextLinkId);

        ImGui::TextUnformatted("Link Context Menu");
        ImGui::Separator();
        if (link)
        {
            ImGui::Text("ID: %p", link->ID.AsPointer());
            ImGui::Text("From: %p", link->StartPinID.AsPointer());
            ImGui::Text("To: %p", link->EndPinID.AsPointer());
        }
        else
            ImGui::Text("Unknown link: %p", contextLinkId.AsPointer());
        ImGui::Separator();
        if (ImGui::MenuItem("Delete"))
            ed::DeleteLink(contextLinkId);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Create New Node"))
    {
        auto newNodePostion = openPopupPosition;
        //ImGui::SetCursorScreenPos(ImGui::GetMousePosOnOpeningCurrentPopup());

        //auto drawList = ImGui::GetWindowDrawList();
        //drawList->AddCircleFilled(ImGui::GetMousePosOnOpeningCurrentPopup(), 10.0f, 0xFFFF00FF);

#pragma region MenuConfig

        Node* node = nullptr;
        if (ImGui::MenuItem("Quest Character"))
            node = SpawnQuestCharacterNode();
        ImGui::Separator();
        if (ImGui::MenuItem("Quest"))
            node = SpawnQuestNode();
        ImGui::Separator();
        if (ImGui::MenuItem("Comment"))
            node = SpawnComment();


#pragma endregion

        if (node)
        {
            BuildNodes();

            createNewNode = false;

            ed::SetNodePosition(node->ID, newNodePostion);

            if (auto startPin = newNodeLinkPin)
            {
                auto& pins = startPin->Kind == PinKind::Input ? node->Outputs : node->Inputs;

                for (auto& pin : pins)
                {
                    if (CanCreateLink(startPin, &pin))
                    {
                        auto endPin = &pin;
                        if (startPin->Kind == PinKind::Input)
                            std::swap(startPin, endPin);

                        s_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
                        s_Links.back().Color = GetIconColor(startPin->Type);

                        break;
                    }
                }
            }
        }

        ImGui::EndPopup();
    }
    else
        createNewNode = false;
    
    ImGui::PopStyleVar();
    ed::Resume();

    if (FilePathOpened.size() > 0)
        SaveWork();
# endif
    ed::End();
}