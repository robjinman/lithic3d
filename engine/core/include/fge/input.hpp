#pragma once

#include <set>
#include <variant>

namespace fge
{

enum class KeyboardKey
{
  Num0 = '0', Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  A = 'A', B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  Space = 32,
  Escape = 256,
  Enter = 257,
  Backspace = 259,
  F1 = 290,
  F2 = 291,
  F3 = 292,
  F4 = 293,
  F5 = 294,
  F6 = 295,
  F7 = 296,
  F8 = 297,
  F9 = 298,
  F10 = 299,
  F11 = 300,
  F12 = 301,
  Right = 262,
  Left = 263,
  Down = 264,
  Up = 265,
  // ...
  Unknown
};

enum class GamepadButton
{
  A,
  B,
  X,
  Y,
  L1,
  L2,
  R1,
  R2,
  Left,
  Right,
  Up,
  Down,
  // ...
  Unknown
};

enum class MouseButton
{
  Left,
  Right
};

using UserInput = std::variant<KeyboardKey, GamepadButton, MouseButton>;

struct InputState
{
  std::set<KeyboardKey> keysPressed;
  //std::set<GamepadButton> gamepadButtonsPressed;
  std::set<MouseButton> mouseButtonsPressed;

  float mouseX = 0.f;
  float mouseY = 0.f;
};

} // namespace fge
