#pragma once

#include <memory>
#include <functional>

namespace lithic3d
{

template<typename R, typename... Args>
class Functor
{
  public:
    Functor() = default;

    template<typename T>
    Functor(T&& fn)
      : m_ptr(std::make_unique<CallableImpl<T>>(std::move(fn)))
    {}

    R operator()(Args... args)
    {
      if (m_ptr == nullptr) {
        throw std::bad_function_call{};
      }
      return m_ptr->invoke(args...);
    }

  private:
    struct Callable
    {
      virtual R invoke(Args... args) = 0;
      virtual ~Callable() = default;
    };

    template<typename T>
    struct CallableImpl : Callable {
      T m_fn;

      CallableImpl(T&& fn) : m_fn(std::move(fn)) {}

      R invoke(Args... args) override
      {
        return m_fn(args...);
      }
    };

    std::unique_ptr<Callable> m_ptr;
};

} // namespace lithic3d
