#include "StateManager.h"
#include "Application.h"

StateManager::StateManager(Application *app) : m_App(app) {}

StateManager::~StateManager() {
  while (!m_States.empty()) {
    m_States.top()->Cleanup();
    m_States.pop();
  }
}

void StateManager::PushState(std::unique_ptr<State> state) {
  PendingChange change;
  change.type = ActionType::Push;
  change.state = std::move(state);
  m_PendingQueue.push_back(std::move(change));
}

void StateManager::PopState() {
  PendingChange change;
  change.type = ActionType::Pop;
  m_PendingQueue.push_back(std::move(change));
}

void StateManager::ChangeState(std::unique_ptr<State> state) {
  PendingChange change;
  change.type = ActionType::Change;
  change.state = std::move(state);
  m_PendingQueue.push_back(std::move(change));
}

void StateManager::ProcessStateChange() {
  for (auto &change : m_PendingQueue) {
    switch (change.type) {
    case ActionType::Push:
      change.state->Init(m_App);
      m_States.push(std::move(change.state));
      break;
    case ActionType::Pop:
      if (!m_States.empty()) {
        m_States.top()->Cleanup();
        m_States.pop();
      }
      break;
    case ActionType::Change:
      if (!m_States.empty()) {
        m_States.top()->Cleanup();
        m_States.pop();
      }
      change.state->Init(m_App);
      m_States.push(std::move(change.state));
      break;
    }
  }
  m_PendingQueue.clear();
}

void StateManager::Update(float dt) {
  if (!m_States.empty()) {
    m_States.top()->HandleInput(m_App);
    m_States.top()->Update(m_App, dt);
  }
}

void StateManager::Render() {
  if (!m_States.empty()) {
    m_States.top()->Render(m_App);
    m_States.top()->RenderUI(m_App);
  }
}
