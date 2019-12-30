// <TimeManager> -*- C++ -*-

#ifndef __TIME_MANAGER_H__
#define __TIME_MANAGER_H__

#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <ctime>

namespace sparta
{

    /*!
     * \brief Singleton which manages wall-clock time for simulations in SPARTA
     * This is not a "timer" manager, but rather an information service and
     * could possible become a timing interval manager.
     */
    class TimeManager
    {
        timeval tv_start_;

    public:

        /*!
         * \brief Default constructor
         */
        TimeManager() :
          tv_start_{0,0}
        {
            gettimeofday(&tv_start_, NULL);
        }

        /*!
         * \brief Returns the TimeManager singleton
         * \note This method is valid until static destruction
         */
        static TimeManager& getTimeManager() {
            static TimeManager tm;
            return tm;
        }

        /*!
         * \brief Gets the number of seconds elapsed since the instantiation of
         * SPARTA static and global vars
         */
        double getSecondsElapsed() const {
            struct timeval t;
            gettimeofday(&t, NULL);
            return ((t.tv_sec - tv_start_.tv_sec)) + ((t.tv_usec - tv_start_.tv_usec) / 1000000.0);
        }

        /*!
         * \brief Gets the absolute second timestamp for this machine
         */
        double getAbsoluteSeconds() const {
            struct timeval t;
            gettimeofday(&t, NULL);
            return (t.tv_sec) + (t.tv_usec / 1000000.0);
        }

        /*!
         * \brief Returns a string representing local time for the simulator
         */
        std::string getLocalTime() const {
            std::time_t t = std::time(nullptr);
            char mbstr[100];
            if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&t))) {
                return mbstr;
            }
            return "";
        }

        /*!
         * \brief Returns a string representing local time in a format suitable
         * for trival string sorting
         */
        std::string getSortableLocalTime() const {
            char buffer[512];
            time_t rawtime;
            struct tm* timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(buffer, sizeof(buffer), (const char*)"%Y-%m-%d_%a_%H-%M-%S", timeinfo);
            return buffer;
        }

        /*!
         * \brief Returns a unix timestamp string
         */
        std::string getTimestamp() const {
            std::stringstream ss;
            ss << std::time(0);
            return ss.str();
        }
    };

} // namespace sparta

// __STRING_MANAGER_H__
#endif
