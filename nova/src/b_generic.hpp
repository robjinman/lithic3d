#pragma once

#include "sys_behaviour.hpp"

using BehaviourFn = std::function<void(const Event&)>;

CBehaviourPtr createGenericBehaviour(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& function);
