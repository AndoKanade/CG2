#pragma once
#include "main.cpp"

class DebugCamera {
public:
  void Initialize();

  void Update();

private:
  Vector3 rotation_ = {0, 0, 0};

  Vector3 tranclation_{0, 0, -50};

  Matrix4x4 viewMatrix_ = MakeIdentity4x4();
  Matrix4x4 projectionMatrix_ = MakeIdentity4x4();
};
