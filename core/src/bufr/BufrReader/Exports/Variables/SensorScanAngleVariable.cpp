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
      ScanAngleComputer() = default;
      virtual ~ScanAngleComputer() = default;

      virtual float compute(const int fieldOfViewNumber) = 0;
    };

    class StdScanAngleComputer : public ScanAngleComputer
    {
     public:
      StdScanAngleComputer(float start, float step) :
        start_(start),
        step_(step)
        {}

      float compute(const int fieldOfViewNumber) final
      {
        return start_ + static_cast<float>(fieldOfViewNumber) * step_;
      }

     private:
      float start_;
      float step_;
    };

    class AmsuaScanAngleComputer : public StdScanAngleComputer
    {
     public:
      AmsuaScanAngleComputer() : StdScanAngleComputer(-48.33, 3.333) {}
    };

    class AtmsScanAngleComputer : public StdScanAngleComputer
    {
     public:
      AtmsScanAngleComputer() : StdScanAngleComputer(-52.725, 1.110) {}
    };

    class IasiScanAngleComputer : public ScanAngleComputer
    {
     public:
      IasiScanAngleComputer() : ScanAngleComputer() {};

      float compute(const int fieldOfViewNumber) final
      {
        static const float Start = 48.33;
        static const float Step = 3.334;
        static const float PixelOffset = 1.25;

        return Start + static_cast<float>(Step / 2) +
               std::floor(static_cast<float>(fieldOfViewNumber) / 4) * Step - (PixelOffset / 2)
               + static_cast<float>(fieldOfViewNumber % 2) * PixelOffset;
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
        typedef ObjectFactory<details::ScanAngleComputer> ScanAngleComputerFactory;

        ScanAngleComputerFactory scanAngleComputerFactory;
        scanAngleComputerFactory.registerObject<details::AtmsScanAngleComputer>("atms");
        scanAngleComputerFactory.registerObject<details::AmsuaScanAngleComputer>("amsua");
        scanAngleComputerFactory.registerObject<details::IasiScanAngleComputer>("iasi");

        checkKeys(map);

        std::string sensor;
        if (conf_.has(ConfKeys::Sensor))
        {
          sensor = conf_.getString(ConfKeys::Sensor);
        }
        else
        {
          throw eckit::BadParameter("Missing required parameters: sensor in sensorScanAngle."
                                    "Check your configuration.");
        }

        // Read the variables from the map
        auto& fovnObj = map.at(getExportKey(ConfKeys::FieldOfViewNumber));

        // Declare and initialize scanline array
        std::vector<float> scanang(fovnObj->size(), DataObject<float>::missingValue());

        // Get field-of-view number
        auto scanAngleComputer = scanAngleComputerFactory.create(sensor);
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
