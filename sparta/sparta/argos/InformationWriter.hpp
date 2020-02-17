// <InformationWriter.hpp> -*- C++ -*-

/**
 * \file InformationWriter.hpp
 * \brief Define the Pipeline Collection Information outputter.
 */
#ifndef __INFORMATION_WRITER_H__
#define __INFORMATION_WRITER_H__

#include <fstream>
#include "sparta/utils/SpartaException.hpp"



namespace sparta{
    /**
     * \class InformationWriter
     * \brief A class that allows the simulation developer to
     * write data to an information file stored near pipeline collection
     * output files about the simulation.
     *
     * Human readible data can be written to file via the << operator or
     * public writeLine() methods.
     */
    class InformationWriter
    {
    public:

        /**
         * \brief construct a InformationWriter stream.
         * \param file The name of the file + path to use as the output stream.
         */
        InformationWriter(const std::string& file) :
            file_(file, std::ios::out)
        {
            if(!file_.is_open()){
                throw sparta::SpartaException("Failed to open InformationWriter file for file: " + file);
            }

            // Throw on write failure
            file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);
        }
        virtual ~InformationWriter()
        {
            file_.close();
        }

        /**
         * \brief Allow appending to the file via the << operator.
         * \param object the data to write to the file
         */
        template<typename T>
        InformationWriter& operator << (const T& object)
        {
            file_ << object;
            return *this;
        }
        /**
         * \brief Allow the user to write a string line to the file.
         * \param str the string to be written as a line in the file.
         */
        template<typename T>
        void writeLine(const T& str)
        {
            file_ << str << std::endl;
        }
        /**
         * \brief Allow the user to write some stuff to the file.
         * This will not break at the end of the line.
         */
        template<typename T>
        void write(const T& str)
        {
            file_ << str;
        }

    private:
        std::ofstream file_; /*!< an outstream to write to file with */

    };

}

#endif
