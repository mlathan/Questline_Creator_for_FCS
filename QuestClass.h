#pragma once

#include <stdexcept>
#include <regex>
#include <string>
#include <vector>

class Rewards {
public:
    std::string dataTable;
    std::string rowName;
    int64_t quantity = 1;
};

class QuestCondition
{
public:
    std::string questCondition;
    int64_t levelCondition = 1;
    std::string questName;
};

class Quest {
public:
    std::string name;
    std::string quest_id;
    std::string quest_name;
    std::string description;
    std::string objective;
    std::vector<Rewards> rewards;
    std::string category = "Talk";
    int64_t collection_number = 1;
    QuestCondition condition;
};

namespace QuestType {
    typedef std::vector<Quest> QuestList;
}
