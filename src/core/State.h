#pragma once

class Application;

class State {
public:
  virtual ~State() = default;

  virtual void Init(Application *app) = 0;
  virtual void HandleInput(Application *app) = 0;
  virtual void Update(Application *app, float dt) = 0;
  virtual void Render(Application *app) = 0;
  virtual void Cleanup() = 0;

  // Optional: for ImGui drawing
  virtual void RenderUI(Application *app) {}
};
