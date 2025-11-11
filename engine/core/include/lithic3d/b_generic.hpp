#pragma once

#include "sys_behaviour.hpp"
#include <functional>

namespace lithic3d
{

using BehaviourFn = std::function<void(const Event&)>;

DBehaviourPtr createBGeneric(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& function);

}
