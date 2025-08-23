#include "b_generic.hpp"

namespace
{

class BGeneric : public CBehaviour
{
  public:
    BGeneric(HashedString name, const std::set<HashedString>& subscriptions, const BehaviourFn& fn);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;

  private:
    HashedString m_name;
    std::set<HashedString> m_subscriptions;
    BehaviourFn m_fn;
};

BGeneric::BGeneric(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& fn)
  : m_name(name)
  , m_subscriptions(subscriptions)
  , m_fn(fn)
{}

HashedString BGeneric::name() const
{
  return m_name;
}

const std::set<HashedString>& BGeneric::subscriptions() const
{
  return m_subscriptions;
}

void BGeneric::processEvent(const GameEvent& event)
{
  m_fn(event);
}

}

CBehaviourPtr createGenericBehaviour(HashedString name, const std::set<HashedString>& subscriptions,
  const BehaviourFn& fn)
{
  return std::make_unique<BGeneric>(name, subscriptions, fn);
}
