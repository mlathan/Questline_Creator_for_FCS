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
    Items items;
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

static std::vector<std::string> ItemVector;
static std::vector<std::string> QuestCategoryVector;
static std::vector<std::string> RecentlyOpenedVector;

static std::string FilePathOpened;


struct NodeIdLess
{
    bool operator()(const ed::NodeId& lhs, const ed::NodeId& rhs) const
    {
        return lhs.AsPointer() < rhs.AsPointer();
    }
};

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
    s_Nodes.emplace_back(GetNextId(), "Quest Character", ImColor(255, 128, 128));
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Quest", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Name", PinType::String);

    s_Nodes.back().Name = "Character";
    s_Nodes.back().Typ = "Character";

    BuildNode(&s_Nodes.back());

    return &s_Nodes.back();
}

static Node* SpawnQuestNode()
{
    s_Nodes.emplace_back(GetNextId(), "Quest", ImColor(20, 20, 20)); //Quest
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Quest", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Quest", PinType::Flow);
    s_Nodes.back().Inputs.emplace_back(GetNextId(), "Required Quest", PinType::Bool);
    s_Nodes.back().Outputs.emplace_back(GetNextId(), "Quest to add after completion", PinType::Bool);

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
    return "Easy Quest - Questline Creator";
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
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.3f, 0.3f, 0.3f, 0.80f);



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
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.40f, 0.4f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
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
    }

    nlohmann::ordered_json json;

    try
    {
        json = nlohmann::ordered_json::parse(f);
    }
    catch (const std::string& ex){}


    if (!json.is_null())
    {
        nlohmann::ordered_json items = nlohmann::ordered_json::object();

        items = json["Items"];

        for (auto& it : items)
        {
            ItemVector.push_back(it);
        }

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
    }

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

void ShowStyleEditor(bool* show = nullptr)
{
    if (!ImGui::Begin("Style", show))
    {
        ImGui::End();
        return;
    }

    auto paneWidth = ImGui::GetContentRegionAvailWidth();

    auto& editorStyle = ed::GetStyle();
    ImGui::BeginHorizontal("Style buttons", ImVec2(paneWidth, 0), 1.0f);
    ImGui::TextUnformatted("Values");
    ImGui::Spring();
    if (ImGui::Button("Reset to defaults"))
        editorStyle = ed::Style();
    ImGui::EndHorizontal();
    ImGui::Spacing();
    ImGui::DragFloat4("Node Padding", &editorStyle.NodePadding.x, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Node Rounding", &editorStyle.NodeRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Node Border Width", &editorStyle.NodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Hovered Node Border Width", &editorStyle.HoveredNodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Selected Node Border Width", &editorStyle.SelectedNodeBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Pin Rounding", &editorStyle.PinRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Pin Border Width", &editorStyle.PinBorderWidth, 0.1f, 0.0f, 15.0f);
    ImGui::DragFloat("Link Strength", &editorStyle.LinkStrength, 1.0f, 0.0f, 500.0f);
    //ImVec2  SourceDirection;
    //ImVec2  TargetDirection;
    ImGui::DragFloat("Scroll Duration", &editorStyle.ScrollDuration, 0.001f, 0.0f, 2.0f);
    ImGui::DragFloat("Flow Marker Distance", &editorStyle.FlowMarkerDistance, 1.0f, 1.0f, 200.0f);
    ImGui::DragFloat("Flow Speed", &editorStyle.FlowSpeed, 1.0f, 1.0f, 2000.0f);
    ImGui::DragFloat("Flow Duration", &editorStyle.FlowDuration, 0.001f, 0.0f, 5.0f);
    //ImVec2  PivotAlignment;
    //ImVec2  PivotSize;
    //ImVec2  PivotScale;
    //float   PinCorners;
    //float   PinRadius;
    //float   PinArrowSize;
    //float   PinArrowWidth;
    ImGui::DragFloat("Group Rounding", &editorStyle.GroupRounding, 0.1f, 0.0f, 40.0f);
    ImGui::DragFloat("Group Border Width", &editorStyle.GroupBorderWidth, 0.1f, 0.0f, 15.0f);

    ImGui::Separator();

    static ImGuiColorEditFlags edit_mode = ImGuiColorEditFlags_RGB;
    ImGui::BeginHorizontal("Color Mode", ImVec2(paneWidth, 0), 1.0f);
    ImGui::TextUnformatted("Filter Colors");
    ImGui::Spring();
    ImGui::RadioButton("RGB", &edit_mode, ImGuiColorEditFlags_RGB);
    ImGui::Spring(0);
    ImGui::RadioButton("HSV", &edit_mode, ImGuiColorEditFlags_HSV);
    ImGui::Spring(0);
    ImGui::RadioButton("HEX", &edit_mode, ImGuiColorEditFlags_HEX);
    ImGui::EndHorizontal();

    static ImGuiTextFilter filter;
    filter.Draw("", paneWidth);

    ImGui::Spacing();

    ImGui::PushItemWidth(-160);
    for (int i = 0; i < ed::StyleColor_Count; ++i)
    {
        auto name = ed::GetStyleColorName((ed::StyleColor)i);
        if (!filter.PassFilter(name))
            continue;

        ImGui::ColorEdit4(name, &editorStyle.Colors[i].x, edit_mode);
    }
    ImGui::PopItemWidth();

    ImGui::End();
}

void WriteToConfigJson()
{
    nlohmann::ordered_json json = nlohmann::json::object();

    nlohmann::ordered_json itemsJson = nlohmann::json::array();
    nlohmann::ordered_json categoryJson = nlohmann::json::array();
    nlohmann::ordered_json recentlyOpened = nlohmann::json::array();

    for (auto& items : ItemVector)
        itemsJson.push_back(items);

    for (auto& cats : QuestCategoryVector)
        categoryJson.push_back(cats);

    for (auto& op : RecentlyOpenedVector)
        recentlyOpened.push_back(op);

    json["Items"] = itemsJson;
    json["QuestCategory"] = categoryJson;
    json["Recently"] = recentlyOpened;

    std::ofstream o("Config.json");
    o << std::setw(json.size()) << json << std::endl;
}

void ShowItemCatalog(bool* show = nullptr)
{
    static int listbox_item_current = 1;

    ImGui::SetNextWindowSize(ImVec2(350, 400));
    if (!ImGui::Begin("Item Catalog", show))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add Item"))
        ImGui::OpenPopup("Add Item");
    ImGui::SameLine();
    if (ImGui::Button("Remove Item"))
    {
        ItemVector.erase(ItemVector.begin() + listbox_item_current, ItemVector.begin() + listbox_item_current + 1);
        WriteToConfigJson();
    }

    if (ImGui::BeginPopupModal("Add Item", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {

        static char bufferItem[100] = "Item";
        ImGui::InputTextMultiline("##itemToAdd", bufferItem, 100);
        ImGui::Separator();

        if (ImGui::Button("Add", ImVec2(120, 0))) 
        {
            ItemVector.push_back(bufferItem);
            WriteToConfigJson();
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    std::vector<char*> ItemArray;
    std::transform(ItemVector.begin(), ItemVector.end(), std::back_inserter(ItemArray), convert);
    ImGui::ListBox("##ItemCatalog", &listbox_item_current, ItemArray.data(), ItemVector.size(), 5);

    ImGui::End();
}

void ShowCategoryCatalog(bool* show = nullptr)
{
    static int listbox_item_current = 1;

    ImGui::SetNextWindowSize(ImVec2(350, 400));
    if (!ImGui::Begin("Quest Category Catalog", show))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add Item"))
        ImGui::OpenPopup("Add Item");
    ImGui::SameLine();
    if (ImGui::Button("Remove Item"))
    {
        QuestCategoryVector.erase(QuestCategoryVector.begin() + listbox_item_current, QuestCategoryVector.begin() + listbox_item_current + 1);
        WriteToConfigJson();
    }

    if (ImGui::BeginPopupModal("Add Item", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {

        static char bufferItem[100] = "Item";
        ImGui::InputText("##itemToAdd", bufferItem, 100);
        ImGui::Separator();

        if (ImGui::Button("Add", ImVec2(120, 0))) 
        {
            QuestCategoryVector.push_back(bufferItem);
            WriteToConfigJson();
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    std::vector<char*> ItemArray;
    std::transform(QuestCategoryVector.begin(), QuestCategoryVector.end(), std::back_inserter(ItemArray), convert);
    ImGui::ListBox("##ItemCatalog", &listbox_item_current, ItemArray.data(), QuestCategoryVector.size(), 8);

    ImGui::End();
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

            static bool showStyleEditor = false;
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
            ImGui::Spring();
            if (ImGui::Button("Edit Style"))
                showStyleEditor = true;
            ImGui::EndHorizontal();

            if (showStyleEditor)
                ShowStyleEditor(&showStyleEditor);

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

    struct Funcs
    {
        static int MyResizeCallback(ImGuiInputTextCallbackData* data)
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                std::string str = (std::string)data->Buf;
                IM_ASSERT(str == data->Buf);
                str.resize(data->BufSize);
                data->Buf = (char*)str.c_str();

                //ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                //IM_ASSERT(my_str->begin() == data->Buf);
                //my_str->resize(data->BufSize);
                //data->Buf = my_str->begin();
            }
            return 0;
        }

        static bool MyInputTextMultiline(const char* label, char* my_str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0)
        {
            IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
            return ImGui::InputTextMultiline(label, my_str, (size_t)my_str, size, flags | ImGuiInputTextFlags_CallbackResize, Funcs::MyResizeCallback, (void*)my_str);
        }

        static bool MyInputText(const char* label, char* my_str, ImGuiInputTextFlags flags = 0)
        {
            IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
            return ImGui::InputText(label, my_str, (size_t)my_str, flags | ImGuiInputTextFlags_CallbackResize, Funcs::MyResizeCallback, (void*)my_str);
        }
    };

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
    bool isSel = std::find(selectedNodes.begin(), selectedNodes.end(), node->ID) != selectedNodes.end();

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
                    char* questID = (char*)node->quest.quest_id.data();

                    ImGui::Text("Quest ID: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputTextMultiline("##questID", questID, 50))
                    {
                        node->quest.quest_id = questID;
                        node->quest.name = questID;
                    }

                    char* questName = (char*)node->quest.quest_name.c_str();

                    ImGui::Text("Quest Name: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputTextMultiline("##questName", questName, 51))
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
                            if (ImGui::Selectable(QuestCategoryVector.at(n).c_str(), is_selected)) {
                                current_Category = QuestCategoryVector.at(n).c_str();
                                node->quest.category = QuestCategoryVector.at(n);
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    char* questArea = (char*)node->quest.area_name.data();

                    ImGui::Text("Area Name: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputTextMultiline("##questArea", questArea, 31))
                    {
                        node->quest.area_name = questArea;
                    }

                    int i0 = node->quest.recommended_level;
                    ImGui::Text("Recommended Level: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputInt("##questRecLevel", &i0))
                    {
                        node->quest.recommended_level = i0;
                    }

                    char* questDescription = (char*)node->quest.description.data();

                    ImGui::Text("Description: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputTextMultiline("##questDescription", questDescription, 5000,
                        ImVec2(-FLT_MIN, io.DisplaySize.y / 2 - 300.0f)))
                    {
                        node->quest.description = questDescription;
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Quest Objective"))
                {
                    if (ImGui::Button("Add Objective"))
                    {
                        Objective* obj = new Objective();
                        node->quest.objectives.push_back(*obj);
                        delete(obj);
                    }

                    if (ImGui::TreeNode("Objective Overview"))
                    {
                        for (int i = 0; i < node->quest.objectives.size(); i++)
                        {
                            if (ImGui::TreeNode((void*)(intptr_t)i, "Objective %d", i + 1))
                            {
                                char* objectiveId = (char*)node->quest.objectives.at(i).objective_id.data();

                                ImGui::Text("Objective id: "); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::InputTextMultiline("##objectiveId", objectiveId, 21))
                                {
                                    node->quest.objectives.at(i).objective_id = objectiveId;
                                }

                                char* objectiveDescription = (char*)node->quest.objectives.at(i).objective_description.data();

                                ImGui::Text("Objective Description: "); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::InputTextMultiline("##objectiveDescription", objectiveDescription, 301, 
                                    ImVec2(-FLT_MIN, io.DisplaySize.y - 900.0f)))
                                {
                                    node->quest.objectives.at(i).objective_description = objectiveDescription;
                                }

                                if (ImGui::Button("Add Objective Tip"))
                                {
                                    std::string* str = new std::string("Objective");
                                    node->quest.objectives.at(i).objective_tips.push_back(*str);
                                    delete(str);
                                }
                                if (ImGui::TreeNode("Objective Tips"))
                                {
                                    for (int j = 0; j < node->quest.objectives.at(i).objective_tips.size(); j++)
                                    {
                                        if (ImGui::TreeNode((void*)(intptr_t)j, "Objective Tip %d", j + 1))
                                        {
                                            char* objectiveTip = (char*)node->quest.objectives.at(i).objective_tips.at(j).data();
                                            if (ImGui::InputTextMultiline("##objectiveTip", objectiveTip, 51))
                                            {
                                                node->quest.objectives.at(i).objective_tips.at(j) = objectiveTip;
                                            }

                                            ImGui::TreePop();
                                        }
                                    }
                                    ImGui::TreePop();
                                }

                                int requiredAmount = node->quest.objectives.at(i).required_amount;

                                ImGui::Text("Required Amount: "); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::InputInt("##reqAmount", &requiredAmount))
                                {
                                    node->quest.objectives.at(i).required_amount = requiredAmount;
                                }

                                bool hasWorldMarker = node->quest.objectives.at(i).has_world_marker;

                                if (ImGui::Checkbox("Has World Marker: ", &hasWorldMarker))
                                {
                                    node->quest.objectives.at(i).has_world_marker = hasWorldMarker;
                                }

                                ImGui::TreePop();
                            }
                            ImGui::Separator();
                        }
                        ImGui::TreePop();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Quest Rewards"))
                {
                    int rewardExp = node->quest.rewards.experience;

                    ImGui::Text("Experience: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputInt("##rewardsExp", &rewardExp))
                    {
                        node->quest.rewards.experience = rewardExp;
                    }

                    int rewardGold = node->quest.rewards.gold;

                    ImGui::Text("Gold: "); ImGui::Spacing(); ImGui::SameLine();
                    if (ImGui::InputInt("##rewardsGold", &rewardGold))
                    {
                        node->quest.rewards.gold = rewardGold;
                    }

                    if (ImGui::Button("Add Item"))
                    {
                        Items* obj = new Items();
                        node->quest.rewards.items.push_back(*obj);
                        delete(obj);
                    }

                    if (ImGui::TreeNode("Items"))
                    {
                        for (int k = 0; k < node->quest.rewards.items.size(); k++)
                        {
                            if (ImGui::TreeNode((void*)(intptr_t)k, "Item %d", k + 1))
                            {
                                const char* item_current = node->quest.rewards.items.at(k).item.data();
                                ImGui::Text("Item"); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::BeginCombo("##item", item_current))
                                {
                                    for (int n = 0; n < ItemVector.size(); n++)
                                    {
                                        bool is_selected = (item_current == ItemVector.at(n));
                                        if (ImGui::Selectable(ItemVector.at(n).c_str(), is_selected)) {
                                            item_current = ItemVector.at(n).c_str();
                                            node->quest.rewards.items.at(k).item = ItemVector.at(n);
                                        }
                                        if (is_selected)
                                            ImGui::SetItemDefaultFocus();
                                    }
                                    ImGui::EndCombo();
                                }

                                int amount = node->quest.rewards.items.at(k).amount;

                                ImGui::Text("Amount: "); ImGui::Spacing(); ImGui::SameLine();
                                if (ImGui::InputInt("##amount", &amount))
                                {
                                    node->quest.rewards.items.at(k).amount = amount;
                                }

                                ImGui::TreePop();
                            }

                        }

                        ImGui::TreePop();
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

void SetRequiredQuests(Node& node)
{
    for (auto& input : node.Inputs)
    {
        if (IsPinLinked(input.ID))
        {
            for (auto& link : s_Links)
            {
                if (link.StartPinID == input.ID || link.EndPinID == input.ID)
                {
                    if (input.Name == "Required Quest")
                    {
                        auto startNode = FindNodeFromPin(link.StartPinID);

                        if (!std::count(startNode->quest.quests_to_add_after_completion.begin(),
                            startNode->quest.quests_to_add_after_completion.end(), node.quest.quest_id))
                        {
                            startNode->quest.quests_to_add_after_completion.push_back(node.quest.quest_id);

                        }

                    }
                }
            }
        }
    }
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
                    if (output.Name == "Quest to add after completion")
                    {
                        auto endNode = FindNodeFromPin(link.EndPinID);

                        if (!std::count(endNode->quest.required_quests.begin(),
                            endNode->quest.required_quests.end(), node.quest.quest_id))
                        {
                            endNode->quest.required_quests.push_back(node.quest.quest_id);

                        }
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
            SetRequiredQuests(node);
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
            j["QuestID"] = node.quest.quest_id;
            j["QuestName"] = node.quest.quest_name;
            j["Category"] = node.quest.category;
            j["AreaName"] = node.quest.area_name;
            j["RecommendedLevel"] = node.quest.recommended_level;
            j["Description"] = node.quest.description;

            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& obj : node.quest.objectives)
            {
                nlohmann::ordered_json jObj = nlohmann::json::object();

                jObj["Objective_ID"] = obj.objective_id;
                jObj["ObjectiveDescription"] = obj.objective_description;

                nlohmann::ordered_json objectiveTips = nlohmann::json::array();

                for (auto& objTips : obj.objective_tips)
                {
                    objectiveTips.push_back(objTips);
                }

                jObj["ObjectiveTips"] = objectiveTips;

                jObj["CurrentAmount"] = 0;
                jObj["RequiredAmount"] = obj.required_amount;
                jObj["HasWorldMarker?"] = obj.has_world_marker;
                jObj["ObjectiveCompleteAnotherQuest"] = obj.objective_complete_another_quest;
                jObj["QuestID"] = obj.quest_id;

                objectives.push_back(jObj);
            }

            j["Objectives"] = objectives;

            nlohmann::ordered_json rewards = nlohmann::json::object();

            rewards["Experience"] = node.quest.rewards.experience;
            rewards["Gold"] = node.quest.rewards.gold;

            nlohmann::ordered_json items = nlohmann::json::object();

            for (auto& item : node.quest.rewards.items)
            {
                items[item.item] = item.amount;
            }

            rewards["Items"] = items;

            j["Rewards"] = rewards;

            nlohmann::ordered_json requiredQuest = nlohmann::json::array();

            for (auto& reqQuest : node.quest.required_quests)
            {
                requiredQuest.push_back(reqQuest);
            }

            j["RequiredQuests"] = requiredQuest;

            nlohmann::ordered_json questToAdd = nlohmann::json::array();

            for (auto& reqQuest : node.quest.quests_to_add_after_completion)
            {
                questToAdd.push_back(reqQuest);
            }

            j["QuestsToAddAfterCompletion"] = questToAdd;

            j["CanQuestBeAborted?"] = node.quest.can_quest_be_aborted;

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
            SetRequiredQuests(node);
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
            j["QuestID"] = node.quest.quest_id;
            j["QuestName"] = node.quest.quest_name;
            j["Category"] = node.quest.category;
            j["AreaName"] = node.quest.area_name;
            j["RecommendedLevel"] = node.quest.recommended_level;
            j["Description"] = node.quest.description;

            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& obj : node.quest.objectives)
            {
                nlohmann::ordered_json jObj = nlohmann::json::object();

                jObj["Objective_ID"] = obj.objective_id;
                jObj["ObjectiveDescription"] = obj.objective_description;

                nlohmann::ordered_json objectiveTips = nlohmann::json::array();

                for (auto& objTips : obj.objective_tips)
                {
                    objectiveTips.push_back(objTips);
                }

                jObj["ObjectiveTips"] = objectiveTips;

                jObj["CurrentAmount"] = 0;
                jObj["RequiredAmount"] = obj.required_amount;
                jObj["HasWorldMarker?"] = obj.has_world_marker;
                jObj["ObjectiveCompleteAnotherQuest"] = obj.objective_complete_another_quest;
                jObj["QuestID"] = obj.quest_id;

                objectives.push_back(jObj);
            }

            j["Objectives"] = objectives;

            nlohmann::ordered_json rewards = nlohmann::json::object();

            rewards["Experience"] = node.quest.rewards.experience;
            rewards["Gold"] = node.quest.rewards.gold;

            nlohmann::ordered_json items = nlohmann::json::object();

            for (auto& item : node.quest.rewards.items)
            {
                items[item.item] = item.amount;
            }

            rewards["Items"] = items;

            j["Rewards"] = rewards;

            nlohmann::ordered_json requiredQuest = nlohmann::json::array();

            for (auto& reqQuest : node.quest.required_quests)
            {
                requiredQuest.push_back(reqQuest);
            }

            j["RequiredQuests"] = requiredQuest;

            nlohmann::ordered_json questToAdd = nlohmann::json::array();

            for (auto& reqQuest : node.quest.quests_to_add_after_completion)
            {
                questToAdd.push_back(reqQuest);
            }

            j["QuestsToAddAfterCompletion"] = questToAdd;

            j["CanQuestBeAborted?"] = node.quest.can_quest_be_aborted;

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
    }
}

void ExportToJson()
{
    nlohmann::ordered_json jsonObjects = nlohmann::json::array();

    for (auto& node : s_Nodes)
    {
        if (node.Typ == "Quest")
        {
            SetRequiredQuests(node);
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
            j["QuestID"] = node.quest.quest_id;
            j["QuestName"] = node.quest.quest_name;
            j["Category"] = node.quest.category;
            j["AreaName"] = node.quest.area_name;
            j["RecommendedLevel"] = node.quest.recommended_level;
            j["Description"] = node.quest.description;

            nlohmann::ordered_json objectives = nlohmann::json::array();

            for (auto& obj : node.quest.objectives)
            {
                nlohmann::ordered_json jObj = nlohmann::json::object();

                jObj["Objective_ID"] = obj.objective_id;
                jObj["ObjectiveDescription"] = obj.objective_description;
                
                nlohmann::ordered_json objectiveTips = nlohmann::json::array();

                for (auto& objTips : obj.objective_tips)
                {
                    objectiveTips.push_back(objTips);
                }

                jObj["ObjectiveTips"] = objectiveTips;

                jObj["CurrentAmount"] = 0;
                jObj["RequiredAmount"] = obj.required_amount;
                jObj["HasWorldMarker?"] = obj.has_world_marker;
                jObj["ObjectiveCompleteAnotherQuest"] = obj.objective_complete_another_quest;
                jObj["QuestID"] = obj.quest_id;

                objectives.push_back(jObj);
            }

            j["Objectives"] = objectives;

            nlohmann::ordered_json rewards = nlohmann::json::object();

            rewards["Experience"] = node.quest.rewards.experience;
            rewards["Gold"] = node.quest.rewards.gold;

            nlohmann::ordered_json items = nlohmann::json::object();

            for (auto& item : node.quest.rewards.items)
            {
                items[item.item] = item.amount;
            }

            rewards["Items"] = items;

            j["Rewards"] = rewards;

            nlohmann::ordered_json requiredQuest = nlohmann::json::array();

            for (auto& reqQuest : node.quest.required_quests)
            {
                requiredQuest.push_back(reqQuest);
            }

            j["RequiredQuests"] = requiredQuest;

            nlohmann::ordered_json questToAdd = nlohmann::json::array();

            for (auto& reqQuest : node.quest.quests_to_add_after_completion)
            {
                questToAdd.push_back(reqQuest);
            }

            j["QuestsToAddAfterCompletion"] = questToAdd;

            j["CanQuestBeAborted?"] = node.quest.can_quest_be_aborted;

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
    }
}

void CreateLinks()
{
    for (auto& node : s_Nodes)
    {
        if (node.quest.required_quests.size() > 0)
        {
            for (int t = 0; t < node.quest.required_quests.size(); ++t)
            {
                for (auto& search : s_Nodes)
                {
                    if (search.quest.quest_id == node.quest.required_quests.at(t))
                    {
                        s_Links.emplace_back(Link(GetNextLinkId(), search.Outputs[1].ID, node.Inputs[1].ID));
                        s_Links.back().Color = GetIconColor(search.Outputs[1].Type);
                        s_Links.emplace_back(Link(GetNextLinkId(), search.Outputs[0].ID, node.Inputs[0].ID));
                    }
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
                    node->quest.quest_id = value["QuestID"];
                    node->quest.quest_name = value["QuestName"];
                    node->quest.area_name = value["AreaName"];
                    node->quest.recommended_level = value["RecommendedLevel"];
                    node->quest.description = value["Description"];
                    nlohmann::ordered_json obj = nlohmann::ordered_json::array();
                    obj = value["Objectives"];
                    for (auto& objects : obj)
                    {
                        Objective objective;
                        objective.objective_id = objects["Objective_ID"];
                        objective.objective_description = objects["ObjectiveDescription"];
                        nlohmann::ordered_json objTips = nlohmann::ordered_json::array();
                        objTips = objects["ObjectiveTips"];
                        for (auto& tips : objTips)
                        {
                            objective.objective_tips.push_back(tips);
                        }
                        objective.current_amount = objects["CurrentAmount"];
                        objective.required_amount = objects["RequiredAmount"];
                        objective.has_world_marker = objects["HasWorldMarker?"];
                        objective.objective_complete_another_quest = objects["ObjectiveCompleteAnotherQuest"];
                        objective.quest_id = objects["QuestID"];
                        node->quest.objectives.push_back(objective);
                    }
                    Rewards reward;
                    nlohmann::ordered_json rewards;
                    rewards = value["Rewards"];

                    reward.experience = rewards["Experience"];
                    reward.gold = rewards["Gold"];

                    Items item;
                    nlohmann::ordered_json items;
                    items = rewards["Items"];
                    for (nlohmann::ordered_json::iterator it = items.begin(); it != items.end(); ++it)
                    {
                        item.item = it.key();
                        item.amount = it.value();
                        reward.items.push_back(item);
                    }

                    node->quest.rewards = reward;

                    nlohmann::ordered_json requiredQuest = nlohmann::json::array();

                    requiredQuest = value["RequiredQuests"];
                    for (auto& req : requiredQuest)
                    {
                        node->quest.required_quests.push_back(req);
                    }

                    nlohmann::ordered_json questToAdd = nlohmann::json::array();

                    questToAdd = value["QuestsToAddAfterCompletion"];
                    for (auto& q : questToAdd)
                    {
                        node->quest.quests_to_add_after_completion.push_back(q);
                    }

                    node->quest.can_quest_be_aborted = value["CanQuestBeAborted?"];
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

        FilePathOpened = "";
    }
    if (ImGui::MenuItem("Open", "Ctrl+O")) 
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
    if (ImGui::MenuItem("Save", "Ctrl+S")) 
    {
        SaveWork();
    }
    if (ImGui::MenuItem("Save as"))
    {
        SaveWorkAs();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Export as Json")) 
    {
        ExportToJson();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Quit", "Alt+F4")) 
    {
    }
}

void Application_Frame()
{
    UpdateTouch();

    static bool showStyleEditor = false;
    static bool showItemCatalog = false;
    static bool showCategoryCatalog = false;
    static bool showAboutWindow = false;

    auto& io = ImGui::GetIO();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            ShowMenuFile();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::MenuItem("Add Item", NULL, &showItemCatalog);
            ImGui::MenuItem("Add Quest Category", NULL, &showCategoryCatalog);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About"))
        {
            ImGui::MenuItem("Questline Creator - Developer", NULL, &showAboutWindow);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (showStyleEditor)
        ShowStyleEditor(&showStyleEditor);

    if (showItemCatalog)
        ShowItemCatalog(&showItemCatalog);

    if (showCategoryCatalog)
        ShowCategoryCatalog(&showCategoryCatalog);

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

        //for (auto& node : s_Nodes)
        //{
        //    if (node.Type != NodeType::Comment)
        //        continue;

        //    const float commentAlpha = 0.75f;

        //    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
        //    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
        //    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 64));
        //    ed::BeginNode(node.ID);
        //    ImGui::PushID(node.ID.AsPointer());
        //    ImGui::BeginVertical("content");
        //    ImGui::BeginHorizontal("horizontal");
        //    ImGui::Spring(1);
        //    ImGui::TextUnformatted(node.Name.c_str());
        //    ImGui::Spring(1);
        //    ImGui::EndHorizontal();
        //    ed::Group(node.Size);
        //    ImGui::EndVertical();
        //    ImGui::PopID();
        //    ed::EndNode();
        //    ed::PopStyleColor(2);
        //    ImGui::PopStyleVar();

        //    if (ed::BeginGroupHint(node.ID))
        //    {
        //        //auto alpha   = static_cast<int>(commentAlpha * ImGui::GetStyle().Alpha * 255);
        //        auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

        //        //ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha * ImGui::GetStyle().Alpha);

        //        auto min = ed::GetGroupMin();
        //        //auto max = ed::GetGroupMax();

        //        ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
        //        ImGui::BeginGroup();
        //        ImGui::TextUnformatted(node.Name.c_str());
        //        ImGui::EndGroup();

        //        auto drawList = ed::GetHintBackgroundDrawList();

        //        auto hintBounds = ImGui_GetItemRect();
        //        auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

        //        drawList->AddRectFilled(
        //            hintFrameBounds.GetTL(),
        //            hintFrameBounds.GetBR(),
        //            IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

        //        drawList->AddRect(
        //            hintFrameBounds.GetTL(),
        //            hintFrameBounds.GetBR(),
        //            IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);

        //        //ImGui::PopStyleVar();
        //    }
        //    ed::EndGroupHint();
        //}
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
        if (ImGui::MenuItem("String"))
            node = SpawnMessageNode();
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
# endif

    /*
        cubic_bezier_t c;
        c.p0 = pointf(100, 600);
        c.p1 = pointf(300, 1200);
        c.p2 = pointf(500, 100);
        c.p3 = pointf(900, 600);

        auto drawList = ImGui::GetWindowDrawList();
        auto offset_radius = 15.0f;
        auto acceptPoint = [drawList, offset_radius](const bezier_subdivide_result_t& r)
        {
            drawList->AddCircle(to_imvec(r.point), 4.0f, IM_COL32(255, 0, 255, 255));

            auto nt = r.tangent.normalized();
            nt = pointf(-nt.y, nt.x);

            drawList->AddLine(to_imvec(r.point), to_imvec(r.point + nt * offset_radius), IM_COL32(255, 0, 0, 255), 1.0f);
        };

        drawList->AddBezierCurve(to_imvec(c.p0), to_imvec(c.p1), to_imvec(c.p2), to_imvec(c.p3), IM_COL32(255, 255, 255, 255), 1.0f);
        cubic_bezier_subdivide(acceptPoint, c);
    */

    ed::End();

    //ImGui::ShowTestWindow();
    //ImGui::ShowMetricsWindow();
}