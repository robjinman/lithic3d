#include "image_partitioner.hpp"
#include <boost/program_options.hpp>
#include <iostream>

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace
{

po::variables_map parseProgramArgs(po::options_description& desc, int argc, const char** argv)
{
  desc.add_options()
    ("help,h", "Show help")
    ("height-map", po::value<std::string>(), "Partition a height map")
    ("splat-map", po::value<std::string>(), "Partition a splat map")
    ("width", po::value<uint32_t>()->required(), "Number of cells horizontally")
    ("height", po::value<uint32_t>()->required(), "Number of cells vertically")
    ("output-dir,o", po::value<std::string>()->required(), "Output directory");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  return vm;
}

} // namespace

int main(int argc, const char** argv)
{
  try {
    po::options_description desc{"Partition height maps and splat maps"};
    auto vm = parseProgramArgs(desc, argc, argv);

    if (vm.count("help") || argc == 1) {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }

    po::notify(vm);

    fs::path inputFilePath;
    ImageType type = ImageType::HeightMap;
    if (vm.contains("height-map")) {
      inputFilePath = vm.at("height-map").as<std::string>();
      type = ImageType::HeightMap;
    }
    else if (vm.contains("splat-map")) {
      inputFilePath = vm.at("splat-map").as<std::string>();
      type = ImageType::SplatMap;
    }
    else {
      std::cerr << "Expected one of --height-map or --splat-map" << std::endl;
      return EXIT_FAILURE;
    }
    auto width = vm.at("width").as<uint32_t>();
    auto height = vm.at("height").as<uint32_t>();
    fs::path outputDir = vm.at("output-dir").as<std::string>();

    partitionImage(inputFilePath, type, width, height, outputDir);
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
