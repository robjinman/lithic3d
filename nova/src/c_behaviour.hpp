#pragma once

#include <memory>
#include "component.hpp"

class Behaviour
{
  public:
    virtual void handleEvent(const Event& event) = 0;

    virtual ~Behaviour() {}
};

using BehaviourPtr = std::unique_ptr<Behaviour>;

class CBehaviour : public Component
{
  public:
    CBehaviour();

    void handleEvent(const Event& event);
    void addBehaviour(BehaviourPtr behaviour);

    void update();
};

using CBehaviourPtr = std::unique_ptr<CBehaviour>;

CBehaviourPtr createBehaviourComponent();
