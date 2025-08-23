#pragma once

#include "sys_behaviour.hpp"

using BehaviourFn = std::function<void(const GameEvent&)>;

CBehaviourPtr createGenericBehaviour(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& function);
