#pragma once

#include "State.h"
#include <memory>
#include <stack>
#include <vector>

class Application;

class StateManager {
public:
  StateManager(Application *app);
  ~StateManager();

  void PushState(std::unique_ptr<State> state);
  void PopState();
  void ChangeState(std::unique_ptr<State> state);

  void Update(float dt);
  void Render();
  void ProcessStateChange();

private:
  Application *m_App;
  std::stack<std::unique_ptr<State>> m_States;

  enum class ActionType { Push, Pop, Change };

  struct PendingChange {
    ActionType type;
    std::unique_ptr<State> state;
  };

  std::vector<PendingChange> m_PendingQueue;
};
