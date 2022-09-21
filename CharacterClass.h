#pragma once
#include <string>
#include <vector>

class Reponse {
public:
    Reponse() = default;
    virtual ~Reponse() = default;

private:
    std::string response;
    int64_t nextDialogueLine;
    std::string triggerEvent;

public:
    const std::string & getResponse() const { return response; }
    std::string & getMutableResponse() { return response; }
    void setResponse(const std::string & value) { this->response = value; }

    const int64_t & getNextDialogueLine() const { return nextDialogueLine; }
    int64_t & getMutableNextDialogueLine() { return nextDialogueLine; }
    void setNextDialogueLine(const int64_t & value) { this->nextDialogueLine = value; }

    const std::string & getTriggerEvent() const { return triggerEvent; }
    std::string & getMutableTriggerEvent() { return triggerEvent; }
    void setTriggerEvent(const std::string & value) { this->triggerEvent = value; }
};

class CoreData {
public:
    CoreData() = default;
    virtual ~CoreData() = default;

private:
    std::string name;
    std::string dialogueLine;
    int64_t nextDialogueLine;
    std::string triggerEvent;
    std::string soundToPlay;
    std::string animationToPlay;
    int64_t activateActorFalse0;
    int64_t activateCameraTransitionFalse0;
    std::vector<Reponse> reponses;
    bool storeDialogueLineReturnNumber;
    int64_t dialogueLineReturnNumber;
    int64_t dialogueLineQuestStarted;
    int64_t dialogueLineQuestCompleted;
    int64_t dialogueLineQuestConditionsFailed;

public:
    const std::string & getName() const { return name; }
    std::string & getMutableName() { return name; }
    void setName(const std::string & value) { this->name = value; }

    const std::string & getDialogueLine() const { return dialogueLine; }
    std::string & getMutableDialogueLine() { return dialogueLine; }
    void setDialogueLine(const std::string & value) { this->dialogueLine = value; }

    const int64_t & getNextDialogueLine() const { return nextDialogueLine; }
    int64_t & getMutableNextDialogueLine() { return nextDialogueLine; }
    void setNextDialogueLine(const int64_t & value) { this->nextDialogueLine = value; }

    const std::string & getTriggerEvent() const { return triggerEvent; }
    std::string & getMutableTriggerEvent() { return triggerEvent; }
    void setTriggerEvent(const std::string & value) { this->triggerEvent = value; }

    const std::string & getSoundToPlay() const { return soundToPlay; }
    std::string & getMutableSoundToPlay() { return soundToPlay; }
    void setSoundToPlay(const std::string & value) { this->soundToPlay = value; }

    const std::string & getAnimationToPlay() const { return animationToPlay; }
    std::string & getMutableAnimationToPlay() { return animationToPlay; }
    void setAnimationToPlay(const std::string & value) { this->animationToPlay = value; }

    const int64_t & getActivateActorFalse0() const { return activateActorFalse0; }
    int64_t & getMutableActivateActorFalse0() { return activateActorFalse0; }
    void setActivateActorFalse0(const int64_t & value) { this->activateActorFalse0 = value; }

    const int64_t & getActivateCameraTransitionFalse0() const { return activateCameraTransitionFalse0; }
    int64_t & getMutableActivateCameraTransitionFalse0() { return activateCameraTransitionFalse0; }
    void setActivateCameraTransitionFalse0(const int64_t & value) { this->activateCameraTransitionFalse0 = value; }

    const std::vector<Reponse> & getReponses() const { return reponses; }
    std::vector<Reponse> & getMutableReponses() { return reponses; }
    void setReponses(const std::vector<Reponse> & value) { this->reponses = value; }

    const bool & getStoreDialogueLineReturnNumber() const { return storeDialogueLineReturnNumber; }
    bool & getMutableStoreDialogueLineReturnNumber() { return storeDialogueLineReturnNumber; }
    void setStoreDialogueLineReturnNumber(const bool & value) { this->storeDialogueLineReturnNumber = value; }

    const int64_t & getDialogueLineReturnNumber() const { return dialogueLineReturnNumber; }
    int64_t & getMutableDialogueLineReturnNumber() { return dialogueLineReturnNumber; }
    void setDialogueLineReturnNumber(const int64_t & value) { this->dialogueLineReturnNumber = value; }

    const int64_t & getDialogueLineQuestStarted() const { return dialogueLineQuestStarted; }
    int64_t & getMutableDialogueLineQuestStarted() { return dialogueLineQuestStarted; }
    void setDialogueLineQuestStarted(const int64_t & value) { this->dialogueLineQuestStarted = value; }

    const int64_t & getDialogueLineQuestCompleted() const { return dialogueLineQuestCompleted; }
    int64_t & getMutableDialogueLineQuestCompleted() { return dialogueLineQuestCompleted; }
    void setDialogueLineQuestCompleted(const int64_t & value) { this->dialogueLineQuestCompleted = value; }

    const int64_t & getDialogueLineQuestConditionsFailed() const { return dialogueLineQuestConditionsFailed; }
    int64_t & getMutableDialogueLineQuestConditionsFailed() { return dialogueLineQuestConditionsFailed; }
    void setDialogueLineQuestConditionsFailed(const int64_t & value) { this->dialogueLineQuestConditionsFailed = value; }
};

namespace CharacterClass {
    typedef std::vector<CoreData> CharClass;
}
