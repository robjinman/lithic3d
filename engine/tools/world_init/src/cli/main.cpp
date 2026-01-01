#include "world_init.hpp"
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace
{

po::variables_map parseProgramArgs(po::options_description& desc, int argc, const char** argv)
{
  desc.add_options()
    ("help,h", "Show help")
    ("height-map", po::value<std::string>(), "Height map (8 bits per pixel, greyscale PNG)")
    ("splat-map", po::value<std::string>(), "Splat map (8 bits per pixel, RGBA PNG)")
    ("grid-width", po::value<uint32_t>()->required(), "Number of cells horizontally")
    ("grid-height", po::value<uint32_t>()->required(), "Number of cells vertically")
    ("cell-width", po::value<float>()->required(), "Cell width in metres")
    ("cell-height", po::value<float>()->required(), "Cell height in metres")
    ("r-texture", po::value<std::string>()->required(), "Splat map's red channel texture")
    ("g-texture", po::value<std::string>()->required(), "Splat map's green channel texture")
    ("b-texture", po::value<std::string>()->required(), "Splat map's blue channel texture")
    ("a-texture", po::value<std::string>()->required(), "Splat map's alpha channel texture")
    ("min-elevation", po::value<float>()->required(), "Minimum elevation in metres")
    ("max-elevation", po::value<float>()->required(), "Maximum elevation in metres")
    ("output-dir,o", po::value<std::string>()->required(), "Output directory");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  return vm;
}

} // namespace

int main(int argc, const char** argv)
{
  try {
    po::options_description desc{"Initialise a world"};
    auto vm = parseProgramArgs(desc, argc, argv);

    if (vm.count("help") || argc == 1) {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }

    po::notify(vm);

    fs::path heightMap = vm.at("height-map").as<std::string>();
    fs::path splatMap = vm.at("splat-map").as<std::string>();
    auto gridWidth = vm.at("grid-width").as<uint32_t>();
    auto gridHeight = vm.at("grid-height").as<uint32_t>();
    auto cellWidth = vm.at("cell-width").as<float>();
    auto cellHeight = vm.at("cell-height").as<float>();
    fs::path rTexture = vm.at("r-texture").as<std::string>();
    fs::path gTexture = vm.at("g-texture").as<std::string>();
    fs::path bTexture = vm.at("b-texture").as<std::string>();
    fs::path aTexture = vm.at("a-texture").as<std::string>();
    auto minElevation = vm.at("min-elevation").as<float>();
    auto maxElevation = vm.at("max-elevation").as<float>();
    fs::path outputDir = vm.at("output-dir").as<std::string>();

    if (std::filesystem::exists(outputDir)) {
      std::cout << "Directory '" << outputDir.string() << "' exists. Overwrite? y/n" << std::endl;
      char c = 'n';
      std::cin >> c;
      if (c != 'y' && c != 'Y') {
        return EXIT_FAILURE;
      }
      std::filesystem::remove_all(outputDir);
    }

    std::filesystem::create_directories(outputDir);

    lithic3d::tools::createWorld(heightMap, splatMap, gridWidth, gridHeight, cellWidth, cellHeight,
      rTexture, gTexture, bTexture, aTexture, minElevation, maxElevation, outputDir);
  }
  catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "Unknown error" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
