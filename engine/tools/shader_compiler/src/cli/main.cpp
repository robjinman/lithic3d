#include "shader_compiler.hpp"
#include <lithic3d/shader_manifest.hpp>
#include <lithic3d/xml.hpp>
#include <lithic3d/utils.hpp>
#include <lithic3d/logger.hpp>
#include <iostream>
#include <set>

namespace fs = std::filesystem;
using namespace lithic3d;

// TODO: Use boost::program_options
int main(int argc, char** argv)
{
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " sources_dir manifest output_dir" << std::endl;
    return EXIT_FAILURE;
  }

  auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);

  const fs::path sourcesDir = argv[1];
  const fs::path manifestPath = argv[2];
  const fs::path outputDir = argv[3];

  std::filesystem::create_directories(outputDir);

  auto compiler = tools::createShaderCompiler(sourcesDir, outputDir);

  auto data = readBinaryFile(manifestPath);
  auto specs = parseShaderManifest(data, *logger);

  std::string message;

  for (auto& entry : fs::directory_iterator{outputDir}) {
    auto& path = entry.path();
    if (path.extension() == ".spv") {
      std::cout << "Deleting " << path << std::endl; // TODO: Use logger
      std::filesystem::remove(path);
    }
  }

  for (auto& spec : specs) {
    compiler->compileShaderProgram(spec);
  }

  return EXIT_SUCCESS;
}
