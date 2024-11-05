// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

#include "SensorScanAngleVariable.h"

#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "eckit/exception/Exceptions.h"

#include "bufr/DataObject.h"
#include "../../../DataObjectBuilder.h"
#include "../../../ObjectFactory.h"

namespace
{
    namespace ConfKeys
    {
        const char* FieldOfViewNumber = "fieldOfViewNumber";
        const char* ScanStart = "scanStart";
        const char* ScanStep = "scanStep";
        const char* ScanStepAdjust = "scanStepAdjust";
        const char* Sensor = "sensor";
    }  // namespace ConfKeys

    const std::vector<std::string> FieldNames = {ConfKeys::FieldOfViewNumber };
}  // namespace


namespace bufr {

  namespace details
  {
    class ScanAngleComputer
    {
    public:
      ScanAngleComputer(float start, float step, float stepAdj = 0) :
        start_(start),
        step_(step),
        stepAdj_(stepAdj) {}

      virtual ~ScanAngleComputer() = default;

      virtual float compute(const int fieldOfViewNumber) = 0;

    protected:
      float start_;
      float step_;
      float stepAdj_;
    };

    class StdScanAngleComputer : public ScanAngleComputer
    {
    public:
      StdScanAngleComputer(float start, float step, float stepAdj = 0) :
        ScanAngleComputer(start, step, stepAdj) {}

      float compute(const int fieldOfViewNumber) override
      {
        return start_ + static_cast<float>(fieldOfViewNumber) * step_;
      }
    };

    class IasiScanAngleComputer : public ScanAngleComputer
    {
    public:
      IasiScanAngleComputer(float start, float step, float stepAdj = 0) :
        ScanAngleComputer(start, step, stepAdj) {}

      float compute(const int fieldOfViewNumber) override
      {
        return start_ + static_cast<float>(step_ / 2) +
               std::floor(static_cast<float>(fieldOfViewNumber) / 4) * step_ - (stepAdj_ / 2)
               + static_cast<float>(fieldOfViewNumber % 2) * stepAdj_;
      }
    };
  }  // namespace details

    SensorScanAngleVariable::SensorScanAngleVariable(const std::string& exportName,
                                                     const std::string& groupByField,
                                                     const eckit::LocalConfiguration &conf) :
      Variable(exportName, groupByField, conf)
    {
        initQueryMap();
    }

    std::shared_ptr<DataObjectBase> SensorScanAngleVariable::exportData(const BufrDataMap& map)
    {
        typedef ObjectFactory<details::ScanAngleComputer,
          float /*start*/,
          float /*step*/,
          float /*step adjust*/> ScanAngleComputerFactory;

        ScanAngleComputerFactory scanAngleComputerFactory;
        scanAngleComputerFactory.registerObject<details::StdScanAngleComputer>("");
        scanAngleComputerFactory.registerObject<details::IasiScanAngleComputer>("iasi");

        checkKeys(map);

        // Get input parameters for sensor scan angle calculation
        float start;
        float step;
        if (conf_.has(ConfKeys::ScanStart) && conf_.has(ConfKeys::ScanStep))
        {
             start = conf_.getFloat(ConfKeys::ScanStart);
             step = conf_.getFloat(ConfKeys::ScanStep);
        }
        else
        {
            throw eckit::BadParameter("Missing required parameters: scan starting angle and step. "
                                      "Check your configuration.");
        }

        std::string sensor;
        if (conf_.has(ConfKeys::Sensor))
        {
          sensor = conf_.getString(ConfKeys::Sensor);
        }

        float stepAdj = 0;
        if (conf_.has(ConfKeys::ScanStepAdjust))
        {
          stepAdj = conf_.getFloat(ConfKeys::ScanStepAdjust);
        }

        // Read the variables from the map
        auto& fovnObj = map.at(getExportKey(ConfKeys::FieldOfViewNumber));

        // Declare and initialize scanline array
        std::vector<float> scanang(fovnObj->size(), DataObject<float>::missingValue());

        // Get field-of-view number
        auto scanAngleComputer = scanAngleComputerFactory.create(sensor, start, step, stepAdj);
        for (size_t idx = 0; idx < fovnObj->size(); idx++)
        {
           scanang[idx] = scanAngleComputer->compute(fovnObj->getAsInt(idx));
        }

        return DataObjectBuilder::make<float>(scanang,
                                              getExportName(),
                                              groupByField_,
                                              fovnObj->getDims(),
                                              fovnObj->getPath(),
                                              fovnObj->getDimPaths());
    }

    void SensorScanAngleVariable::checkKeys(const BufrDataMap& map)
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

        errStr << " could not be found during export of scanang object.";

        if (isKeyMissing)
        {
            throw eckit::BadParameter(errStr.str());
        }
    }

    QueryList SensorScanAngleVariable::makeQueryList() const
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
        return queries;
    }

    std::string SensorScanAngleVariable::getExportKey(const std::string& name) const
    {
        return getExportName() + "_" + name;
    }
}  // namespace bufr
