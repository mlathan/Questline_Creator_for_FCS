#pragma once

#include <stdexcept>
#include <regex>
#include <string>
#include <vector>

class Objective {
public:
    std::string objective_id;
    std::string objective_description;
    std::vector<std::string> objective_tips;
    int64_t current_amount = 0;
    int64_t required_amount = 0;
    bool has_world_marker;
    bool objective_complete_another_quest;
    std::string quest_id = "None";
};

class Items {
public:
    std::string item;
    int64_t amount = 0;
};

class Rewards {
public:
    int64_t experience = 0;
    int64_t gold = 0;
    std::vector<Items> items;
};

class Quest {
public:
    std::string name;
    std::string quest_id;
    std::string quest_name;
    std::string category = "Main Quest";
    std::string area_name;
    int64_t recommended_level = 0;
    std::string description;
    std::vector<Objective> objectives;
    Rewards rewards;
    std::vector<std::string> required_quests;
    std::vector<std::string> quests_to_add_after_completion;
    bool can_quest_be_aborted;
};

namespace QuestType {
    typedef std::vector<Quest> QuestList;
}
