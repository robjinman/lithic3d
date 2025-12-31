#include "height_map_gen.hpp"
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
    ("input-file,f", po::value<std::string>()->required(), "Path to an ACE2 file")
    ("max-elevation,m", po::value<float>()->required(), "Maximum elevation in metres")
    ("output-file,o", po::value<std::string>()->required(), "Path of PNG file to write to");

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

    fs::path inputFilePath = vm.at("input-file").as<std::string>();
    float maxElevation = vm.at("max-elevation").as<float>();
    fs::path outputFilePath = vm.at("output-file").as<std::string>();

    genHeightMap(inputFilePath, outputFilePath, maxElevation);
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
