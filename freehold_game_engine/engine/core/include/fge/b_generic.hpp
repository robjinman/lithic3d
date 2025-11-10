#pragma once

#include "sys_behaviour.hpp"
#include <functional>

namespace fge
{

using BehaviourFn = std::function<void(const Event&)>;

DBehaviourPtr createBGeneric(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& function);

}
