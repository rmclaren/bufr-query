// (C) Copyright 2020 NOAA/NWS/NCEP/EMC

#include "bufr/encoders/Description.h"

#include "eckit/exception/Exceptions.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/filesystem/PathName.h"

#include "bufr/QueryParser.h"

namespace
{
    namespace ConfKeys
    {
        const char* OutputPathTemplate = "outputPathTemplate";
        const char* Dimensions = "dimensions";
        const char* Variables = "variables";
        const char* Globals = "globals";

        namespace Dimension
        {
            const char* Name = "name";
            const char* Path = "path";
            const char* Paths = "paths";
            const char* Source = "source";
        }  // Dimension

        namespace Variable
        {
            const char* Name = "name";
            const char* Source = "source";
            const char* LongName = "longName";
            const char* Units = "units";
            const char* Range = "range";
            const char* Coords = "coordinates";
            const char* Chunks = "chunks";
            const char* CompressionLevel = "compressionLevel";
        }  // namespace Variable

        namespace Global
        {
            const char* Name = "name";
            const char* Value = "value";
            const char* Type = "type";
            const char* StringType = "string";
            const char* FloatType = "float";
            const char* FloatVectorType = "floatVector";
            const char* IntType = "int";
            const char* IntVectorType = "intVector";
        }  // namespace Global
    }  // namespace ConfKeys
}  // namespace

namespace bufr {
namespace encoders {
    Description::Description(const std::string &yamlFile) : outputPathTemplate_("")
    {
        auto conf = eckit::YAMLConfiguration(eckit::PathName(yamlFile));
        init(conf.getSubConfiguration("encoder"));
    }

    Description::Description(const eckit::Configuration &conf) : outputPathTemplate_("")
    {
        init(conf);
    }

    void Description::init(const eckit::Configuration &conf)
    {
        if (conf.has(ConfKeys::OutputPathTemplate))
        {
            outputPathTemplate_ = conf.getString(ConfKeys::OutputPathTemplate);
        }

        if (conf.has(ConfKeys::Dimensions))
        {
            auto dimConfs = conf.getSubConfigurations(ConfKeys::Dimensions);
            if (dimConfs.size() == 0)
            {
                std::stringstream errStr;
                errStr << "netcdf dimensions must contain a list of dimensions!";
                throw eckit::BadParameter(errStr.str());
            }

            for (const auto &dimConf: dimConfs)
            {
                DimensionDescription dim;
                dim.name = dimConf.getString(ConfKeys::Dimension::Name);

                if (dimConf.has(ConfKeys::Dimension::Paths))
                {
                    for (const auto &path: dimConf.getStringVector(ConfKeys::Dimension::Paths))
                    {
                        dim.paths.push_back(QueryParser::parse(path)[0]);
                    }
                } else if (dimConf.has(ConfKeys::Dimension::Path))
                {
                    dim.paths =
                        QueryParser::parse(dimConf.getString(ConfKeys::Dimension::Path));
                } else
                {
                    throw eckit::BadParameter(
                        R"(netcdf dimensions section must have either "path" or "paths".)");
                }

                if (dimConf.has(ConfKeys::Dimension::Source))
                {
                    dim.source = {dimConf.getString(ConfKeys::Dimension::Source)};
                }

                addDimension(dim);
            }
        }

        auto varConfs = conf.getSubConfigurations(ConfKeys::Variables);
        if (varConfs.size() == 0)
        {
            std::stringstream errStr;
            errStr << "netcdf variables must contain a list of variables!";
            throw eckit::BadParameter(errStr.str());
        }

        for (const auto &varConf: varConfs)
        {
            VariableDescription variable;
            variable.name = varConf.getString(ConfKeys::Variable::Name);
            variable.source = varConf.getString(ConfKeys::Variable::Source);
            variable.longName = varConf.getString(ConfKeys::Variable::LongName);

            variable.units = "";
            if (varConf.has(ConfKeys::Variable::Units))
            {
                variable.units = varConf.getString(ConfKeys::Variable::Units);
            }

            if (varConf.has(ConfKeys::Variable::Coords))
            {
                variable.coordinates =
                    std::make_shared<std::string>(varConf.getString(ConfKeys::Variable::Coords));
            } else
            {
                variable.coordinates = nullptr;
            }

            variable.range = nullptr;
            if (varConf.has(ConfKeys::Variable::Range))
            {
                auto range = std::make_shared<Range>();
                range->start = std::stoi(varConf.getStringVector(ConfKeys::Variable::Range)[0]);
                range->end = std::stoi(varConf.getStringVector(ConfKeys::Variable::Range)[1]);
                variable.range = range;
            }

            variable.chunks = {};
            if (varConf.has(ConfKeys::Variable::Chunks))
            {
                auto chunks = std::vector<size_t>();

                for (const auto &chunkStr: varConf.getStringVector(ConfKeys::Variable::Chunks))
                {
                    chunks.push_back(std::stoi(chunkStr));
                }

                variable.chunks = chunks;
            }

            variable.compressionLevel = 6;
            if (varConf.has(ConfKeys::Variable::CompressionLevel))
            {
                int compressionLevel = varConf.getInt(ConfKeys::Variable::CompressionLevel);
                if (compressionLevel < 0 || compressionLevel > 9)
                {
                    throw eckit::BadParameter("GZip compression level must be a number 0-9");
                }

                variable.compressionLevel = varConf.getInt(ConfKeys::Variable::CompressionLevel);
            }

            addVariable(variable);
        }

        if (conf.has(ConfKeys::Globals))
        {
            auto globalConfs = conf.getSubConfigurations(ConfKeys::Globals);
            for (const auto &globalConf: globalConfs)
            {
                if (globalConf.getString(ConfKeys::Global::Type) == \
              ConfKeys::Global::StringType)
                {
                    auto global = std::make_shared<GlobalDescription<std::string>>();
                    global->name = globalConf.getString(ConfKeys::Global::Name);
                    global->value = globalConf.getString(ConfKeys::Global::Value);
                    addGlobal(global);
                } else if (globalConf.getString(ConfKeys::Global::Type) == \
                   ConfKeys::Global::FloatType)
                {
                    auto global = std::make_shared<GlobalDescription<float>>();
                    global->name = globalConf.getString(ConfKeys::Global::Name);
                    global->name = globalConf.getFloat(ConfKeys::Global::Name);
                    addGlobal(global);
                } else if (globalConf.getString(ConfKeys::Global::Type) == \
                   ConfKeys::Global::FloatVectorType)
                {
                    auto global = std::make_shared<GlobalDescription<std::vector<float>>>();
                    global->name = globalConf.getString(ConfKeys::Global::Name);
                    global->value = globalConf.getFloatVector(ConfKeys::Global::Value);
                    addGlobal(global);
                } else if (globalConf.getString(ConfKeys::Global::Type) == \
                   ConfKeys::Global::IntType)
                {
                    auto global = std::make_shared<GlobalDescription<int>>();
                    global->name = globalConf.getString(ConfKeys::Global::Name);
                    global->value = globalConf.getInt(ConfKeys::Global::Value);
                    addGlobal(global);
                } else if (globalConf.getString(ConfKeys::Global::Type) == \
                   ConfKeys::Global::IntVectorType)
                {
                    auto global = std::make_shared<GlobalDescription<std::vector<int>>>();
                    global->name = globalConf.getString(ConfKeys::Global::Name);
                    global->value = globalConf.getIntVector(ConfKeys::Global::Value);
                    addGlobal(global);
                } else
                {
                    throw eckit::BadParameter("Unsupported global attribute type");
                }
            }
        }
    }

    void Description::addDimension(const DimensionDescription &dim)
    {
        dimensions_.push_back(dim);
    }

    void Description::addVariable(const VariableDescription &variable)
    {
        variables_.push_back(variable);
    }

    void Description::removeVariable(const std::string& name)
    {
        auto it = std::find_if(variables_.begin(), variables_.end(),
                               [&name](const VariableDescription &var) {
                                   return var.name == name;
                               });

        if (it != variables_.end())
        {
            variables_.erase(it);
        }
    }

    void Description::addDimension(const std::string& name,
                                   const std::vector<std::string>& paths,
                                   const std::string& source)
    {
        DimensionDescription dim;
        dim.name = name;

        std::vector<Query> pathQueries(paths.size());
        for (size_t path_idx=0; path_idx < paths.size(); path_idx++)
        {
          pathQueries[path_idx] = QueryParser::parse(paths[path_idx])[0];
        }

        dim.paths = pathQueries;

        dim.source = source;
        addDimension(dim);
    }

    void Description::removeDimension(const std::string& name)
    {
        auto it = std::find_if(dimensions_.begin(), dimensions_.end(),
                               [&name](const DimensionDescription &dim) {
                                   return dim.name == name;
                               });

        if (it != dimensions_.end())
        {
            dimensions_.erase(it);
        }
    }

    void Description::removeGlobal(const std::string& name)
    {
        auto it = std::find_if(globals_.begin(), globals_.end(),
                               [&name](const std::shared_ptr<GlobalDescriptionBase> &global) {
                                   return global->name == name;
                               });

        if (it != globals_.end())
        {
            globals_.erase(it);
        }
    }

    void Description::py_addVariable(const std::string& name,
                                     const std::string& source,
                                     const std::string& units,
                                     const std::string& longName,
                                     const std::string& coordinates,
                                     const std::vector<size_t>& chunks,
                                     const int compressionLevel)
    {
        VariableDescription variable;
        variable.name = name;
        variable.source = source;
        variable.units = units;

        if (!longName.empty())
        {
          variable.longName = longName;
        }

        if (!coordinates.empty())
        {
            variable.coordinates = std::make_shared<std::string>(coordinates);
        }

        variable.compressionLevel = compressionLevel;

        if (!chunks.empty())
        {
            variable.chunks = chunks;
        }

        addVariable(variable);
    }

    void Description::addGlobal(const std::shared_ptr<GlobalDescriptionBase> &global)
    {
        globals_.push_back(global);
    }

}  // namespace encoders
}  // namespace bufr

