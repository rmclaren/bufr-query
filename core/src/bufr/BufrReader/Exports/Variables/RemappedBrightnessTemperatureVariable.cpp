// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

#include "RemappedBrightnessTemperatureVariable.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../../../Log.h"
#include "../../../DataObjectBuilder.h"

#include "bufr/DataObject.h"
#include "DatetimeVariable.h"
#include "Transforms/atms/atms_spatial_average_interface.h"
#include "eckit/exception/Exceptions.h"

// Function to find missing numbers in each set
std::vector<int> findMissingNumbers(const std::vector<int>& fovn_set) {
    int nfov = 96;
    std::set<int> full_set;
    for (int i = 1; i <= nfov; ++i) {
        full_set.insert(i);
    }
    std::set<int> fovn_set_unique(fovn_set.begin(), fovn_set.end());
    std::vector<int> missing;

    for (size_t num : full_set) {
        if (fovn_set_unique.find(num) == fovn_set_unique.end()) {
            missing.push_back(num);
        }
    }

    return missing;
}

namespace
{
    namespace ConfKeys
    {
        const char* ScanLineNumber = "scanLineNumber";
        const char* FieldOfViewNumber = "fieldOfViewNumber";
        const char* SensorChannelNumber = "sensorChannelNumber";
        const char* BrightnessTemperature = "brightnessTemperature";
        const char* ObsTime = "obsTime";
    }  // namespace ConfKeys

    const std::vector<std::string> FieldNames = {ConfKeys::FieldOfViewNumber,
                                                 ConfKeys::ScanLineNumber,
                                                 ConfKeys::SensorChannelNumber,
                                                 ConfKeys::BrightnessTemperature,
                                                };
}  // namespace


namespace bufr {
    RemappedBrightnessTemperatureVariable::RemappedBrightnessTemperatureVariable(
                                                       const std::string& exportName,
                                                       const std::string& groupByField,
                                                       const eckit::LocalConfiguration &conf) :
      Variable(exportName, groupByField, conf),
      datetime_(exportName, groupByField, conf_.getSubConfiguration(ConfKeys::ObsTime))
    {
        initQueryMap();
    }

    std::shared_ptr<DataObjectBase> RemappedBrightnessTemperatureVariable::exportData(
                                                       const BufrDataMap& map)
    {

        bool debug_print = false;
        checkKeys(map);

        // Read the variables from the map
        auto& radObj = map.at(getExportKey(ConfKeys::BrightnessTemperature));
        auto& sensorChanObj = map.at(getExportKey(ConfKeys::SensorChannelNumber));
        auto& fovnObj = map.at(getExportKey(ConfKeys::FieldOfViewNumber));
        auto& slnmObj = map.at(getExportKey(ConfKeys::ScanLineNumber));

        // Get dimensions
        if (radObj->getDims().size() != 2)
        {
           log::info()  << "Observation dimension should be 2 " << std::endl;
           log::error() << "Incorrect observation dimension : " << radObj->getDims().size()
                                                                       << std::endl;
        }
        int nobs = (radObj->getDims())[0];
        int nchn = (radObj->getDims())[1];

        // Declare and initialize scanline array
        // scanline has the same dimension as fovn
        std::vector<int> scanline(fovnObj->size(), DataObject<int>::missingValue());

        // Get observation time (obstime) variable
        auto datetimeObj = datetime_.exportData(map);
        std::vector<int64_t> obstime;
        std::vector<int64_t> obstime2;
        obstime = std::dynamic_pointer_cast<DataObject<int64_t>>(datetimeObj)->getRawData();
        obstime2 = std::dynamic_pointer_cast<DataObject<int64_t>>(datetimeObj)->getRawData();

        // Get field-of-view number
        std::vector<int> fovn(fovnObj->size(), DataObject<int>::missingValue());
        for (size_t idx = 0; idx < fovnObj->size(); idx++)
        {
           fovn[idx] = fovnObj->getAsInt(idx);
        }

	// Get scanline number 
        std::vector<int> slnm(fovnObj->size(), DataObject<int>::missingValue());
        for (size_t idx = 0; idx < slnmObj->size(); idx++)
        {
           slnm[idx] = slnmObj->getAsInt(idx);
        }

        // Combine fovn, slnm, and time into a list of tuples
	std::vector<std::tuple<int, int, int64_t>> combined;
        for (size_t i = 0; i < fovn.size(); ++i) {
            combined.emplace_back(fovn[i], slnm[i], obstime[i]);
        }

        if (debug_print) {
            log::debug() << "Checking combined data ..." << std::endl;
            for (const auto& item : combined) {
                // Access each element of the tuple using std::get
                log::debug() << "fovn: " << std::get<0>(item) 
                             << ", slnm: " << std::get<1>(item) 
                             << ", obstime: " << std::get<2>(item) 
                             << std::endl;
            }
        }

        // Group contiguous elements
	// Group fovn and obstime for each slnm
	std::vector<std::pair<int, std::vector<std::pair<int, int64_t>>>> sets;
	std::vector<std::pair<int, int64_t>> current_set;
        int current_slnm = slnm[0];

        for (const auto& [f, s, t] : combined) {
            if (s == current_slnm) {
                current_set.emplace_back(f, t);
            } else {
                sets.emplace_back(current_slnm, current_set);
                current_set = {{f, t}};
                current_slnm = s;
            }
        }

        // Append the last set
        sets.emplace_back(current_slnm, current_set);

        // Printing the contents of sets
        if (debug_print) {
	    log::debug() << "Checking group contiguous elements ..." << std::endl;
            for (const auto& [slnm, pairs] : sets) {
                log::debug() << "slnm: " << slnm << std::endl;
                for (const auto& [fovn, obstime] : pairs) {
	   	    log::debug() << "  fovn: " << fovn << ", obstime: " << obstime << std::endl;
                }
            }
        }

        size_t time_index = 0;
        // Check each set for missing numbers and modify times accordingly
	std::vector<std::tuple<int, int, std::vector<int>, std::vector<int>, int64_t, int64_t>> results;
	std::vector<std::tuple<int, int, std::vector<int>, std::vector<int>, int64_t, int64_t>> results_missing;
	std::vector<std::tuple<int, int, int64_t>> modified_combined;

        // Loop though sets (contains complete sets and incomplete sets)
	// For each set, do the following:
	// 1. identify missing fovs (missing)
	// 2. get the obstime for the first existing fov (first_time)
	// 3. for complete set: populate the first_time to all fovs in the scan
	// 4. for incomplete set: adjust the first_time and populate it to all fovs in the scan  
	// 5. load the adjusted obstime back to obstime2 (to be used in FFT calculation)  
        for (size_t i = 0; i < sets.size(); ++i) {
            int slnm_val = sets[i].first;
	    std::vector<std::pair<int, int64_t>> fovn_time_set = sets[i].second;

	    std::vector<int> fovn_set;
	    std::vector<int64_t> time_set;
            for (const auto& [f, t] : fovn_time_set) {
                fovn_set.push_back(f);
                time_set.push_back(t);
            }

            // 1. Identify missing fovs 
	    std::vector<int> missing = findMissingNumbers(fovn_set);

	    // 2. Get the obstime from the first existing fov 
            int64_t first_time = !time_set.empty() ? time_set[0] : 0;
            int64_t adjusted_first_time = first_time;
            int64_t scan_interval = 18; // milliseconds

            // 3. Complete set: replace all times with the first time element
            if (missing.empty()) {
                adjusted_first_time = first_time;
                for (const auto& [f, t] : fovn_time_set) {
                    modified_combined.emplace_back(f, slnm_val, first_time);
                }
            // 4. Corrected adjustment logic for incomplete sets
            } else {
                adjusted_first_time = first_time - (*min_element(fovn_set.begin(), fovn_set.end()) - 1) * scan_interval;
                for (const auto& [f, t] : fovn_time_set) {
                    modified_combined.emplace_back(f, slnm_val, adjusted_first_time);
                }
            }
            // 5. Load obstime2 with the adjusted_first_time for the indices corresponding to the current set
            for (size_t j = 0; j < fovn_time_set.size(); ++j) {
                obstime2[time_index++] = adjusted_first_time;
            }

            if (missing.empty()) {
                results.emplace_back(i + 1, slnm_val, fovn_set, missing, first_time, adjusted_first_time);
            } else {
                results_missing.emplace_back(i + 1, slnm_val, fovn_set, missing, first_time, adjusted_first_time);
            }
        }

        // Display results
        if (debug_print) { 
            log::debug() << "emily checking results ..." << std::endl;
            for (const auto& [set_number, slnm_val, fovn_set, missing, first_time, adjusted_first_time] : results) {
	        log::debug() << "Set " << set_number << ":\n";
	        log::debug() << "  SLNM: " << slnm_val << "\n";
	        log::debug() << "  FOVN Set: ";
                for (int num : fovn_set) log::debug() << num << " ";
	        log::debug() << "\n";
	        log::debug() << "  Missing Numbers: ";
                for (int num : missing) log::debug() << num << " ";
	        log::debug() << "\n";
	        log::debug() << "  First Time: " << first_time << "\n";
	        log::debug() << "  Adjusted First Time: " << adjusted_first_time << "\n\n";
            }
            log::debug() << "emily checking results missing ..." << std::endl;
            for (const auto& [set_number, slnm_val, fovn_set, missing, first_time, adjusted_first_time] : results_missing) {
	        log::debug() << "Set " << set_number << ":\n";
	        log::debug() << "  SLNM: " << slnm_val << "\n";
	        log::debug() << "  FOVN Set: ";
                for (int num : fovn_set) log::debug() << num << " ";
	        log::debug() << "\n";
	        log::debug() << "  Missing Numbers: ";
                for (int num : missing) log::debug() << num << " ";
	        log::debug() << "\n";
	        log::debug() << "  First Time: " << first_time << "\n";
	        log::debug() << "  Adjusted First Time: " << adjusted_first_time << "\n\n";
            }

            // Display modified combined array
	    log::debug() << "emily checking modified combined array:\n";
            for (const auto& [f, s, t] : modified_combined) {
	        log::debug()  << "(" << f << ", " << s << ", " << t << ")\n";
            }

            // Display obstime and obstime2 arrays side-by-side
	    log::debug() << "emily checking obstime vs obstime2:\n";
	    log::debug() << "\nTime and Time2 Arrays (side-by-side):\n";
	    log::debug() << "Index\tTime\tTime2\n";
            for (size_t i = 0; i < obstime2.size(); ++i) {
                log::debug() << i << "\t" << obstime[i] << "\t" << obstime2[i] << "\n";
            }
        }

        // Get sensor channel
        std::vector<int> channel(sensorChanObj->size(), DataObject<int>::missingValue());
        for (size_t idx = 0; idx < sensorChanObj->size(); idx++)
        {
           channel[idx] = sensorChanObj->getAsInt(idx);
        }

        // Get brightness temperature (observation)
        std::vector<float> btobs(radObj->size(), DataObject<float>::missingValue());
        for (size_t idx = 0; idx < radObj->size(); idx++)
        {
           btobs[idx] = radObj->getAsFloat(idx);
        }

        // Perform FFT image remapping
        // input only variables: nobs, nchn obstime, fovn, channel
        // input & output variables: btobs, scanline, error_status
        if (nobs > 0) {
            int error_status;
	    ATMS_Spatial_Average_f(nobs, nchn, &obstime2, &fovn, &slnm, &channel, &btobs,
                                               &scanline, &error_status);
        }

        // Export remapped observation (btobs)
        return DataObjectBuilder::make<float>(btobs,
                                              getExportName(),
                                              groupByField_,
                                              radObj->getDims(),
                                              radObj->getPath(),
                                              radObj->getDimPaths());
    }

    void RemappedBrightnessTemperatureVariable::checkKeys(const BufrDataMap& map)
    {
        std::vector<std::string> requiredKeys;
        for (const auto& fieldName : FieldNames)
        {
            if (conf_.has(fieldName))
            {
                requiredKeys.push_back(getExportKey(fieldName));
            }
        }

        std::stringstream errStr;
        errStr << "Query ";

        bool isKeyMissing = false;
        for (const auto& key : requiredKeys)
        {
            if (map.find(key) == map.end())
            {
                isKeyMissing = true;
                errStr << key;
                break;
            }
        }

        errStr << " could not be found during export of datetime object.";

        if (isKeyMissing)
        {
            throw eckit::BadParameter(errStr.str());
        }
    }

    QueryList RemappedBrightnessTemperatureVariable::makeQueryList() const
    {
        auto queries = QueryList();

        for (const auto& fieldName : FieldNames)
        {
            if (conf_.has(fieldName))
            {
                QueryInfo info;
                info.name = getExportKey(fieldName);
                info.query = conf_.getString(fieldName);
                info.groupByField = groupByField_;
                queries.push_back(info);
            }
        }

        auto datetimequerys = datetime_.makeQueryList();
        queries.insert(queries.end(), datetimequerys.begin(), datetimequerys.end());

        return queries;
    }

    std::string RemappedBrightnessTemperatureVariable::getExportKey(const std::string& name) const
    {
        return getExportName() + "_" + name;
    }
}  // namespace bufr
