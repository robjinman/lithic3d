#pragma once

#include <vector>
#include <array>
#include <concepts>
#include <filesystem>

namespace fge
{

template<typename T>
bool inRange(T value, T min, T max)
{
  return value >= min && value <= max;
}

template<std::floating_point T>
T parseFloat(const std::string& s)
{
  return static_cast<T>(std::stod(s));
}

template<typename T>
std::ostream& operator<<(std::ostream& stream, const std::vector<T>& v)
{
  for (size_t i = 0; i < v.size(); ++i) {
    stream << v[i] << (i + 1 < v.size() ? ", " : "");
  }
  return stream;
}

template<typename T, size_t N>
std::ostream& operator<<(std::ostream& stream, const std::array<T, N>& v)
{
  for (size_t i = 0; i < v.size(); ++i) {
    stream << v[i] << (i + 1 < v.size() ? ", " : "");
  }
  return stream;
}

std::string getVersionString();
uint32_t getVersionMajor();
uint32_t getVersionMinor();
//std::string getBuildId();

int randomInt();

std::vector<char> readBinaryFile(const std::filesystem::path& path);
void writeBinaryFile(const std::filesystem::path& path, const char* data, size_t size);

} // namespace fge
