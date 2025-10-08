#pragma once

#include "sys_behaviour.hpp"
#include <functional>

using BehaviourFn = std::function<void(const Event&)>;

BehaviourDataPtr createBGeneric(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& function);
