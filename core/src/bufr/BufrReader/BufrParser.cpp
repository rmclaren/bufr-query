// (C) Copyright 2020-2024 NOAA/NWS/NCEP/EMC

#include "bufr/BufrParser.h"

#include <chrono>  // NOLINT
#include <iostream>
#include <ostream>

#include <unistd.h>

#include "bufr/DataContainer.h"
#include "bufr/DataObject.h"
#include "bufr/QuerySet.h"
#include "bufr/ResultSet.h"
#include "bufr/Export.h"
#include "bufr/Split.h"
#include "eckit/exception/Exceptions.h"
#include "../Log.h"

namespace bufr {

    BufrParser::BufrParser(const std::string& obsfile,
                       const BufrDescription& description,
                       const std::string& tablepath) :
      description_(description),
      file_(File(obsfile, tablepath))
    {
      // print message
      log::info() << "BufrParser: Parsing file " << obsfile << std::endl;
    }

    BufrParser::BufrParser(const std::string& obsfile,
                           const eckit::LocalConfiguration &conf,
                           const std::string& tablepath) :
      description_(BufrDescription(conf)),
      file_(File(obsfile, tablepath))
    {
      // print message
      log::info() << "BufrParser: Parsing file " << obsfile << std::endl;
    }

    BufrParser::BufrParser(const std::string& obsfile,
                           const std::string& mappingPath,
                           const std::string& tablepath) :
      description_(BufrDescription(mappingPath)),
      file_(File(obsfile, tablepath))
    {
      log::info() << "BufrParser: Parsing file " << obsfile << std::endl;
    }

    BufrParser::~BufrParser()
    {
        file_.close();
    }

    std::shared_ptr<DataContainer> BufrParser::parse(const size_t maxMsgsToParse)
    {
        auto startTime = std::chrono::steady_clock::now();

        auto querySet = QuerySet(description_.getExport().getSubsets());

        for (const auto &var : description_.getExport().getVariables())
        {
            for (const auto &queryPair : var->getQueryList())
            {
                querySet.add(queryPair.name, queryPair.query);
            }
        }

        log::info() << "Executing Queries" << std::endl;
        const auto resultSet = file_.execute(querySet, maxMsgsToParse);

        log::info() << "Building Bufr Data" << std::endl;
        auto srcData = BufrDataMap();
        for (const auto& var : description_.getExport().getVariables())
        {
            for (const auto& queryInfo : var->getQueryList())
            {
                srcData[queryInfo.name] = resultSet.get(
                    queryInfo.name, queryInfo.groupByField, queryInfo.type);
            }
        }

        log::info()  << "Exporting Data" << std::endl;
        auto exportedData = exportData(srcData);

        auto timeElapsed = std::chrono::steady_clock::now() - startTime;
        auto timeElapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>
                (timeElapsed);
        log::info()  << "Parser Finished "
                           << "[" << timeElapsedDuration.count() / 1000.0 << "s]"
                           << std::endl;

        return exportedData;
    }

    std::shared_ptr<DataContainer> BufrParser::parse(const eckit::mpi::Comm& comm)
    {
      // Make the QuerySet
      auto querySet = QuerySet(description_.getExport().getSubsets());
      for (const auto &var : description_.getExport().getVariables())
      {
        for (const auto &queryPair : var->getQueryList())
        {
          querySet.add(queryPair.name, queryPair.query);
        }
      }

      auto msgsInFile = file_.size(querySet);

      // Distribute the messages to the tasks
      auto msgsToParse = std::floor(msgsInFile / comm.size());
      size_t startOffset = comm.rank() * msgsToParse;

      // Messages may not split evenly among tasks, so distribute the remaining messages
      if (auto remainder = msgsInFile - comm.size() * msgsToParse)
      {
        if (comm.rank() < remainder)
        {
          msgsToParse++;
          startOffset += comm.rank();
        }
        else
        {
          startOffset += remainder;
        }
      }

      auto startTime = std::chrono::steady_clock::now();

      log::info() << "MPI task: " << comm.rank() << " Executing Queries for message ";
      log::info() << startOffset << " to " << startOffset + msgsToParse - 1 << std::endl;

      const auto resultSet = file_.execute(querySet, startOffset, msgsToParse);

      log::info() << "MPI task: " << comm.rank() << " Building Bufr Data" << std::endl;
      auto srcData = BufrDataMap();
      for (const auto& var : description_.getExport().getVariables())
      {
        for (const auto& queryInfo : var->getQueryList())
        {
          srcData[queryInfo.name] = resultSet.get(
            queryInfo.name, queryInfo.groupByField, queryInfo.type);
        }
      }

      log::info() << "MPI task: " << comm.rank() << " Exporting Data" << std::endl;
      auto exportedData = exportData(srcData);

      auto timeElapsed = std::chrono::steady_clock::now() - startTime;
      auto timeElapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>
        (timeElapsed);
      log::info()  << "MPI task: " << comm.rank()  << " Parser Finished "
                   << "[" << timeElapsedDuration.count() / 1000.0 << "s]"
                   << std::endl;

      return exportedData;
    }

    std::shared_ptr<DataContainer> BufrParser::exportData(const BufrDataMap &srcData) {
        auto exportDescription = description_.getExport();

        auto filters = exportDescription.getFilters();
        auto splits = exportDescription.getSplits();
        auto vars = exportDescription.getVariables();

        // Filter
        BufrDataMap dataCopy = srcData;  // make mutable copy
        for (const auto &filter : filters)
        {
            filter->apply(dataCopy);
        }

        // Split
        CategoryMap catMap;
        for (const auto &split : splits)
        {
            std::ostringstream catName;
            catName << "splits/" << split->getName();
            catMap.insert({catName.str(), split->subCategories(dataCopy)});
        }

        BufrParser::CatDataMap splitDataMaps;
        splitDataMaps.insert({std::vector<std::string>(), dataCopy});
        for (const auto &split : splits)
        {
            splitDataMaps = splitData(splitDataMaps, *split);
        }

        // Export
        auto exportData = std::make_shared<DataContainer>(catMap);
        for (const auto &dataPair : splitDataMaps)
        {
            for (const auto &var : vars)
            {
                log::debug() << "Exporting variable = " << var->getExportName() << std::endl;

                exportData->add(var->getExportName(),
                                var->exportData(dataPair.second),
                                dataPair.first);
            }
        }

        return exportData;
    }

    BufrParser::CatDataMap BufrParser::splitData(BufrParser::CatDataMap &splitMaps, Split &split)
    {
        CatDataMap splitDataMap;

        for (const auto &splitMapPair : splitMaps)
        {
            auto newData = split.split(splitMapPair.second);

            for (const auto &newDataPair : newData)
            {
                auto catVect = splitMapPair.first;
                catVect.push_back(newDataPair.first);
                splitDataMap.insert({catVect, newDataPair.second});
            }
        }

        return splitDataMap;
    }

    void BufrParser::reset()
    {
        file_.rewind();
    }

    void BufrParser::printMap(const BufrParser::CatDataMap &map)
    {
        for (const auto &mp : map)
        {
            std::cout << " keys: ";
            for (const auto &s : mp.first)
            {
                std::cout << s;
            }

            std::cout << " subkeys: ";
            for (const auto &m2p : mp.second)
            {
                std::cout << m2p.first << " " << m2p.second->getDims()[0] << " ";
            }

            std::cout << std::endl;
        }
    }
}  // namespace bufr
