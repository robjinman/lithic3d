#include <lithic3d/lithic3d.hpp>
#include <iostream>

namespace lithic3d
{

WindowDelegatePtr createWindowDelegate();
PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

namespace
{

class Application
{
  public:
    Application();

    void run();

    ~Application();

  private:

};

Application::Application()
{
}

void Application::run()
{
}

Application::~Application()
{
}

} // namespace
} // namespace lithic3d

int main()
{
  try {
    lithic3d::Application app;
    app.run();
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
