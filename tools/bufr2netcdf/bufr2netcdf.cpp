// (C) Copyright 2020 NOAA/NWS/NCEP/EMC

#include <chrono>  // NOLINT
#include <string>
#include <iostream>
#include <ostream>

#include "eckit/log/Log.h"
#include "eckit/runtime/Main.h"
#include "eckit/mpi/Comm.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/filesystem/PathName.h"

#include "bufr/BufrParser.h"
#include "bufr/encoders/Description.h"
#include "bufr/encoders/netcdf/Encoder.h"

namespace bufr {
namespace mpi {
  class App : public eckit::Main
  {
  public:
    App() = delete;
    App(int argc, char **argv) : eckit::Main(argc, argv)
    {
      name_ = "bufr2netcdf";
    }
  };
}  // namespace mpi

//    typedef ObjectFactory<Parser, const eckit::LocalConfiguration&> ParseFactory;

  void logElapsedTime(const std::string& msg,
                      const std::chrono::steady_clock::time_point& startTime)
  {
    auto timeElapsed = std::chrono::steady_clock::now() - startTime;
    auto timeElapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>
        (timeElapsed);
    eckit::Log::info() << msg << " "
                       << "[" << timeElapsedDuration.count() / 1000.0 << "s]" << std::endl;
  }

  std::string makeFilename(const std::string& prototype, const SubCategory& categories)
  {
    if (categories.empty())
    {
        return prototype;
    }

    // take a string formatted like ex: abc/gdas.{{ category }}.nc and replace {{ category }}
    // with the category string. The category string is the concatenation of the subcategories
    // with an underscore.

    std::string filename = prototype;
    std::string category = "";
    for (const auto& subCat : categories)
    {
        category += subCat + "_";
    }
    category.pop_back();

    size_t startPos = filename.find("{{");
    size_t endPos = filename.find("}}") + 2;
    if (startPos != std::string::npos)
    {
        filename.replace(startPos, endPos-startPos, category);
    }
    return filename;
  }

  void parse(const std::string& obsFile,
             const std::string& mappingFile,
             const std::string& outputFile,
             const std::string& tablePath = "",
             std::size_t numMsgs = 0)
  {
    auto startTime = std::chrono::steady_clock::now();

    std::unique_ptr<eckit::YAMLConfiguration>
        yaml(new eckit::YAMLConfiguration(eckit::PathName(mappingFile)));

    if (yaml->has("encoder"))
    {
      auto data = BufrParser(obsFile,
                             yaml->getSubConfiguration("bufr"), tablePath).parse(numMsgs);

      auto backend = encoders::netcdf::Encoder::Backend(false, outputFile);

      auto encoderConf = yaml->getSubConfiguration("encoder");
      auto e = encoders::netcdf::Encoder(encoderConf);
      e.encode(data, backend);
    }
    else
    {
        eckit::BadParameter("No section named \"encoder\"");
    }

    logElapsedTime("Total Time", startTime);
  }

  void parse(const eckit::mpi::Comm& comm,
                       const std::string& obsFile,
                       const std::string& mappingFile,
                       const std::string& outputFile,
                       const std::string& tablePath = "",
                       bool separateFiles = false)
  {
    auto startTime = std::chrono::steady_clock::now();

    std::unique_ptr<eckit::YAMLConfiguration>
      yaml(new eckit::YAMLConfiguration(eckit::PathName(mappingFile)));

    if (!yaml->has("encoder"))
    {
      throw eckit::BadParameter("No section named \"encoder\"");
    }

    auto parser = BufrParser(obsFile, yaml->getSubConfiguration("bufr"), tablePath);
    auto data = parser.parse(comm);

    if (separateFiles)
    {
      auto backend = encoders::netcdf::Encoder::Backend(false,
                                                        outputFile + ".task_" +
                                                        std::to_string(comm.rank()));

      auto encoderConf = yaml->getSubConfiguration("encoder");
      encoders::netcdf::Encoder(encoderConf).encode(data, backend);
    }
    else
    {
      data->gather(comm);

      if (comm.rank() == 0)
      {
        auto backend = encoders::netcdf::Encoder::Backend(false, outputFile);

        auto encoderConf = yaml->getSubConfiguration("encoder");
        encoders::netcdf::Encoder(encoderConf).encode(data, backend);
      }
    }

    comm.barrier();
    if (comm.rank() == 0)
    {
      logElapsedTime("Total Time", startTime);
    }
  }
}  // namespace bufr


static void showHelp()
{
    std::cerr << "Usage: bufr2netcdf.x [-t TABLE_PATH] [-n NUM_MESSAGES] SRC_FILE MAPPING_FILE"
              << " OUT_FILE\n"
              << "Options:\n"
              << "  -h,  Show this help message\n"
              << "  --no-gather, Don't gather the data into 1 output file. Makes 1 file per task.\n"
              << "  -t TABLE_PATH,  Path to BUFR table files (use with WMO BUFR files)\n"
              << "  -n NUM_MESSAGES,  Number of BUFR messages to parse.\n"
              << "Example:\n"
              << "  bufr2netcdf.x input/mhs.bufr input/mhs_mapping.yaml output/mhs.nc\n"
              << std::endl;
}


int main(int argc, char **argv)
{
    if (argc < 4)
    {
        showHelp();
        return 0;
    }

    std::string obsFile;
    std::string mappingFile;
    std::string outputFile;
    std::string tablePath = "";
    std::size_t numMsgs = 0;

    enum class ReqArgType
    {
        ObsFile = 0,
        MappingFile = 1,
        OutputFile = 2
    };

    bool separateFiles = false;
    auto reqArgIdx = ReqArgType::ObsFile;
    std::size_t argIdx = 1;
    while (argIdx < static_cast<std::size_t> (argc))
    {
        if (strcmp(argv[argIdx], "-n") == 0)
        {
            if (static_cast<std::size_t> (argc) > argIdx + 1)
            {
                numMsgs = atoi(argv[argIdx + 1]);
            } else
            {
                showHelp();
                return 0;
            }

            argIdx += 2;
        } else if (strcmp(argv[argIdx], "-h") == 0)
        {
            showHelp();
            return 0;
        } else if (strcmp(argv[argIdx], "-t") == 0)
        {
            if (static_cast<std::size_t> (argc) > argIdx + 1)
            {
                tablePath = std::string(argv[argIdx + 1]);
            } else
            {
                showHelp();
                return 0;
            }

            argIdx += 2;
        } else if (strcmp(argv[argIdx], "--no-gather") == 0)
        {
          separateFiles = true;
          argIdx += 1;
        } else
        {
            switch (reqArgIdx)
            {
                case ReqArgType::ObsFile:
                    obsFile = std::string(argv[argIdx]);
                    break;
                case ReqArgType::MappingFile:
                    mappingFile = std::string(argv[argIdx]);
                    break;
                case ReqArgType::OutputFile:
                    outputFile = std::string(argv[argIdx]);
                    break;
                default:
                    break;
            }

            argIdx++;
            reqArgIdx = static_cast<ReqArgType> (static_cast<int> (reqArgIdx) + 1);
        }
    }

    auto app = bufr::mpi::App(argc, argv);
    if (eckit::mpi::comm("world").size() > 1)
    {
      bufr::parse(eckit::mpi::comm("world"),
                     obsFile,
                     mappingFile,
                     outputFile,
                     tablePath,
                     separateFiles);
    }
    else
    {
      bufr::parse(obsFile, mappingFile, outputFile, tablePath, numMsgs);
    }

    return 0;
}  // namespace bufr
