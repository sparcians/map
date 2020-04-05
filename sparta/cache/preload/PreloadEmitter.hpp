// <PreloadEmitter.h> -*- C++ -*-


/**
 * \file PreloadEmitter.h
 *
 * \brief Provide a class for dumping formatted
 *        preload packet information.
 */
#pragma once

#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/yaml.h>

namespace sparta {
namespace cache {

    /**
     * \class PreloadEmitter
     * \brief A class for creating hierarchical a preload
     *        packet information that can be serialized
     *        to yaml.
     *
     * Today this is a raw wrapper around a YAML::Emiter.
     *
     * Usage of this class is almost identical to the Emitter
     * described by yaml-cpp. https://code.google.com/p/yaml-cpp/wiki/HowToEmitYAML
     *
     * We just wrap it in PreloadEmitter if for what ever reason
     * we want to change the output format to not be yaml anymore.
     */
    class PreloadEmitter
    {
    public:
        PreloadEmitter()
        {
            yaml_emitter_.SetIndent(4);
            // yaml_emitter_.SetMapFormat(YAML::Flow);
        }
        /*****************************************************
         * \name EmitterTypeDefs
         * @{
         *****************************************************/
        enum EmitterItemTypes {
            BeginSeq = YAML::BeginSeq,
            EndSeq = YAML::EndSeq,
            BeginMap = YAML::BeginMap,
            EndMap = YAML::EndMap,
            Key = YAML::Key,
            Value = YAML::Value
        };

        /*****************************************************
         * @}
         *****************************************************/

        /**
         * \brief call into the yaml emitter. This is the same as the << operator.
         */
        template<typename T>
        void emit(const T& item)
        {
            yaml_emitter_ << item;
        }
        void preset(const EmitterItemTypes& t)
        {
            yaml_emitter_ << YAML::EMITTER_MANIP(t);
        }

        /**
         * \brief make sure the current stream is valid data.
         *        i.e. no unclosed sequences etc.
         * Raises an exception if it is not.
         */
        void assertValid(const std::string& location="")
        {
            if (!yaml_emitter_.good())
            {
                std::cerr << "Incomplete YAML... " << std::endl;
                std::cerr << yaml_emitter_.c_str() << std::endl;
                sparta::SpartaException ex("PreloadEmitter has an incomplete set of data.");
                ex << "You are likely missing an EndSeq or have a misplaced Key";
                if (location != "")
                {
                    ex << " originating from: " << location;
                }
                throw ex;
            }
        }
        /**
         * \brief output the data in yaml format,
         * Must be a valid amount of data. I.e.
         * no unclosed sequences or maps etc.
         */
        void print(std::ostream& stream)
        {
            assertValid();
            stream << yaml_emitter_.c_str();
        }
    private:
        //! A yaml-cpp emitter.
        YAML::Emitter yaml_emitter_;
    };

    inline PreloadEmitter& operator<<(PreloadEmitter& emitter, const PreloadEmitter::EmitterItemTypes& t)
    {
        emitter.preset(t);
        return emitter;
    }
    template<typename T>
    inline PreloadEmitter& operator<<(PreloadEmitter& emitter, const T& item)
    {
        emitter.emit(item);
        return emitter;
    }

    inline std::ostream& operator<< (std::ostream& out, PreloadEmitter& emitter)
    {
        emitter.print(out);
        return out;
    }
}// namespace cache
}// namespace sparta

