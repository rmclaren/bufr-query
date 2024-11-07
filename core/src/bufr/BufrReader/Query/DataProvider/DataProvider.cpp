// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

#include "bufr/DataProvider.h"
#include "bufr_interface.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "eckit/exception/Exceptions.h"


namespace bufr {

    int timeToYYMMDDHH(std::time_t time) {
      // Convert time_t to tm structure (using GMT)
      std::tm* timeinfo = std::gmtime(&time);

      // Extract the components
      int year = (timeinfo->tm_year + 1900) % 100; // YY format
      int month = timeinfo->tm_mon + 1;            // Month is 0-11, so add 1
      int day = timeinfo->tm_mday;
      int hour = timeinfo->tm_hour;

      // Combine components into the required format
      int result = year * 1000000 + month * 10000 + day * 100 + hour;

      return result;
    }

    void DataProvider::run(const QuerySet& querySet,
                           const std::function<void()> processSubset,
                           const std::function<void()> processMsg,
                           const std::function<bool()> continueProcessing,
                           const RunParameters& params)
    {
        if (!isOpen_)
        {
            std::ostringstream errStr;
            errStr << "Tried to call DataProvider::run, but the file is not open!";
            throw eckit::BadParameter(errStr.str());
        }

        static int SubsetLen = 9;
        char subsetChars[SubsetLen];
        int iddate;

        int bufrLoc;
        int il, im;  // throw away

        size_t msgCnt = 0;
        bool foundBufrMsg = false;
        bool foundBufrSubset = false;

        int startIdDate = 0;
        int endIdDate = 0;

        if (params.startTime)
        {
            startIdDate = timeToYYMMDDHH(*params.startTime);
        }

        if (params.stopTime)
        {
            endIdDate = timeToYYMMDDHH(*params.stopTime);
        }

        while (ireadmg_f(FileUnit, subsetChars, &iddate, SubsetLen) == 0)
        {
            foundBufrMsg = true;

            if (startIdDate > 0 && iddate < startIdDate) continue;
            if (endIdDate > 0 && iddate > endIdDate) break;

            subset_ = std::string(subsetChars);
            subset_.erase(std::remove_if(subset_.begin(), subset_.end(), isspace), subset_.end());

            if (!querySet.includesSubset(subset_)) continue;

            msgCnt++;
            if (msgCnt <= params.offset)
            {
                processMsg();
                if (!continueProcessing()) break;
                continue;
            }

            while (ireadsb_f(FileUnit) == 0)
            {
                foundBufrSubset = true;
                status_f(FileUnit, &bufrLoc, &il, &im);
                updateData(bufrLoc);

                processSubset();
                if (!continueProcessing()) break;
            }

            processMsg();

            if (params.numMessages > 0 && msgCnt - params.offset >= params.numMessages) break;
            if (!continueProcessing()) break;
        }

        deleteData();
        rewind();

        if (!foundBufrMsg)
        {
            std::ostringstream errStr;
            errStr << "No BUFR messages were found! ";
            errStr << "Please make sure that " << filePath_ << " exists and is a valid BUFR file.";
            // throw eckit::BadValue(errStr.str());
            std::cout << "WARNING: " << errStr.str() << std::endl;
        }

        if (!foundBufrSubset)
        {
            std::ostringstream errStr;
            errStr << "No valid BUFR subsets were found from your queries! ";
            errStr << "Please make sure you are querying for valid subsets that exist in ";
            errStr << filePath_ << ". ";
            errStr << "Otherwise there might be a problem with the BUFR file (no subsets).";
            // throw eckit::BadValue(errStr.str());
            std::cout << "WARNING: " << errStr.str() << std::endl;
        }
    }

    size_t DataProvider::numMessages(const QuerySet& querySet, const RunParameters& params)
    {
      if (!isOpen_)
      {
        std::ostringstream errStr;
        errStr << "Tried to call DataProvider::numMessages, but the file is not open!";
        throw eckit::BadParameter(errStr.str());
      }

      int startIdDate = 0;
      int endIdDate = 0;

      if (params.startTime)
      {
        startIdDate = timeToYYMMDDHH(*params.startTime);
      }

      if (params.stopTime)
      {
        endIdDate = timeToYYMMDDHH(*params.stopTime);
      }

      static int SubsetLen = 9;
      char subsetChars[SubsetLen];
      int iddate;

      size_t numMsgs = 0;
      while (ireadmg_f(FileUnit, subsetChars, &iddate, SubsetLen) == 0)
      {
        if (startIdDate > 0 && iddate < startIdDate) continue;
        if (endIdDate > 0 && iddate > endIdDate) break;

        subset_ = std::string(subsetChars);
        subset_.erase(std::remove_if(subset_.begin(), subset_.end(), isspace), subset_.end());

        if (querySet.includesSubset(subset_))
        {
          numMsgs++;
        }
      }

      rewind();

      return numMsgs;
    }

    void DataProvider::updateData(int bufrLoc)
    {
        bufrLoc_ = bufrLoc;
        int size = 0;
        int *intPtr = nullptr;
        double *dataPtr = nullptr;

        get_inode_f(bufrLoc, &inode_);
        get_nval_f(bufrLoc, &nval_);

        updateTableData(subset_);

        get_val_f(bufrLoc, &dataPtr, &size);
        val_ = gsl::span<const double>(dataPtr, size);

        get_inv_f(bufrLoc, &intPtr, &size);
        inv_ = gsl::span<const int>(intPtr, size);
    }

    TypeInfo DataProvider::getTypeInfo(FortranIdx idx) const
    {
        static const unsigned int UNIT_STR_LEN = 24;
        static const unsigned int DESC_STR_LEN = 55;

        char unitCStr[UNIT_STR_LEN];
        char descCStr[DESC_STR_LEN];

        int retVal;
        TypeInfo info;

        nemdefs_f(FileUnit,
                  getTag(idx).c_str(),
                   unitCStr,
                   UNIT_STR_LEN,
                   descCStr,
                   DESC_STR_LEN,
                   &retVal);

        if (retVal == 0)
        {
            // trim the unit string
            auto unitStr = std::string(unitCStr);
            size_t end = unitStr.find_last_not_of(" \n\r\t\f\v");
            unitStr = (end == std::string::npos) ? "" : unitStr.substr(0, end + 1);
            info.unit = unitStr;

            // trim the unit string
            auto descStr = std::string(descCStr);
            end = descStr.find_last_not_of(" \n\r\t\f\v");
            descStr = (end == std::string::npos) ? "" : descStr.substr(0, end + 1);
            info.description = descStr;

            int descriptor;
            int table_idx;
            char table_type;

            nemtab_f(bufrLoc_,
                     getTag(idx).c_str(),
                     &descriptor,
                     &table_type,
                     &table_idx);

            nemtbb_f(bufrLoc_,
                     table_idx,
                     unitCStr,
                     UNIT_STR_LEN,
                     &info.scale,
                     &info.reference,
                     &info.bits);
        }

        return info;
    }

    std::string DataProvider::getLongStr(const std::string& longStrId) const
    {
        static int MaxLongStrLen = 120;
        char charPtr[MaxLongStrLen];

        readlc_f(FileUnit, longStrId.c_str(), charPtr, MaxLongStrLen);

        if (charPtr[0] == '\xff')
        {
            return "";
        }

        return std::string(charPtr, strlen(charPtr));
    }
}  // namespace bufr
