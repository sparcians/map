// <HistogramFunctionManager.h> -*- C++ -*-


/**
 * \file HistogramFunctionManager.hpp
 *  This file contains a Singleton Function Manager
 *  which stores function names and function pointers
 *  as key-value pair in maps. There are two separate maps
 *  for histogram and cyclehistogram nodes. This file also
 *  contains the macro that users will need in order to
 *  register statistic calculation functions.
 *
 *  the following is an example usage.
 *  Step 1 - The user writes their free function.
 * \code{.cpp}
 *  double get_bin_count_greater_than_3_Stdev(
 *      const sparta::HistogramTreeNode* h)
 *  {
 *      double total = 0.0;
 *      const double std_dev = h->getStandardDeviation();
 *      const double uf = h->getUnderflowBinCount();
 *      const double of = h->getOverflowBinCount();
 *      const auto& bin_counts = h->getBinCount();
 *      for(const auto& bin : bin_counts){
 *          if(bin > (3 * std_dev)){
 *              total += bin;
 *          }
 *      }
 *      if(uf > (3 * std_dev)){
 *          total += uf_p;
 *      }
 *      if(of > (3 * std_dev)){
 *          total += of_p;
 *      }
 *      return total;
 *  }
 * \endcode
 *  Step 2 - The user registers this method with the registration macro.
 * \code{.cpp}
 *  REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, get_bin_count_greater_than_3_Stdev);
 * \endcode.
 *  Step 3 - User should know the exact fullpath of the histogram they are using in
 *  the device tree.
 *  Step 4 - If the fullpath of a certain histogram is top.core0.hist_node, then
 *  to get their free function as a statdef in reports, they should use the hist_def
 *  keyword and make it a prefix in the path string. For example,
 *  their yaml could look like this :
 *  content:
 *    top:
 *      hist_def.core0.hist_node.get_bin_count_greater_than_3_Stdev : my_stat
 *    top.core0:
 *      hist_def.hist_node.get_bin_count_less_than_mean : my_stat2
 *    top.core0.hist_node:
 *      hist_def.get_bin_count_greater_than_mean : my_stat3
 *  Using the hist_def keyword is essential as this tells the yaml parser to
 *  go and look in the Histogram Function Manager class for the string name.
*/

#ifndef __HIST_FCN_MGR_H__
#define __HIST_FCN_MGR_H__

#include <iostream>
#include <functional>
#include <map>
#include <string>

#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/utils/MetaStructs.hpp"

namespace sparta{

    // The signature of the methods that should be
    // registered with this macro is aliased here.
    template<typename T>
    using HistStatCalcFcn = double (*)(const T*);

    /*!
     * \brief Singleton Function Manager class.
     *  This class contains two maps and one constant string token.
     *  These maps contain a string as key and function pointers as value.
     *  The string key is the same as the name of the free functions as
     *  defined by the users and the keys are the function pointers to those
     *  methods. One map is dedicated for HistogramTreeNodes while the other
     *  is dedicated for CycleHistogramTreeNodes. This was necessary because
     *  these two classes do not share a common polymorphic base.
     */
    class FunctionManager{
    public:

        /*!
         * \brief Not copy-constructable
         */
        FunctionManager(const FunctionManager&) = delete;

        /*!
         * \brief Not move-constructable
         */
        FunctionManager(FunctionManager&&) = delete;

        /*!
         * \brief Not copy-assignable
         */
        FunctionManager& operator = (const FunctionManager&) = delete;

        /*!
         * \brief Not move-assignable
         */
        FunctionManager& operator = (FunctionManager*&) = delete;

        /*!
         * \brief This method returns the singleton instance of Function Manager.
         */
        static FunctionManager& get(){
            static FunctionManager function_manager;
            return function_manager;
        }

        /*!
         * \brief This method returns the constant string "hist_def" which
         *  should be prefixed in the stat pathnames in yaml files by users.
         */
        const std::string& getToken() const {
            return token_;
        }

        /*!
         * \brief This method adds an entry to one of the internal maps.
         *  This method takes as parameters, a string name of the free
         *  function as the user calls it and the function pointer to
         *  the same method.
         *  This method is enabled only when users are using a
         *  CycleHistogramTreeNode.
         */
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_same<MetaStruct::decay_t<T>,
                CycleHistogramTreeNode>::value, void>
        add(const std::string& name, HistStatCalcFcn<T> fcn){

            // Registering the same function twice should throw an exception.
            if(functions_cycle_.find(name) != functions_cycle_.end()){
                throw sparta::SpartaException(
                "This method " + name + " was already registered with the REGISTER_HISTOGRAM_STAT_CALC_FCN macro.");
            }
            functions_cycle_[name] = fcn;
        }

        /*!
         * \brief This method adds an entry to one of the internal maps.
         *  This method takes as parameters, a string name of the free
         *  function as the user calls it and the function pointer to
         *  the same method.
         *  This method is enabled only when users are using a
         *  HistogramTreeNode.
         */
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_same<MetaStruct::decay_t<T>,
                HistogramTreeNode>::value, void>
        add(const std::string& name, HistStatCalcFcn<T> fcn){

            // Registering the same function twice should throw an exception.
            if(functions_normal_.find(name) != functions_normal_.end()){
                throw sparta::SpartaException(
                "This method " + name + "was already registered with the REGISTER_HISTOGRAM_STAT_CALC_FCN macro.");
            }
            functions_normal_[name] = fcn;
        }

        /*!
         * \brief This method finds the function pointer mapped to the name.
         *  This method takes as parameters, a string name of the free
         *  function as the user calls it.
         *  It looks through its internal map to find the correct function
         *  pointer and returns it.
         *  This method is enable only when users are using a
         *  CycleHistogramTreeNode.
         */
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_same<MetaStruct::decay_t<T>,
                CycleHistogramTreeNode>::value,
                    HistStatCalcFcn<T>&>
        find(const std::string& name){
            const auto& iter = functions_cycle_.find(name);

            // If no such function pointers were found, it should throw.
            if(iter == functions_cycle_.end()){
                throw sparta::SpartaException(
                "This method " + name + " was never registered with the REGISTER_HISTOGRAM_STAT_CALC_FCN macro.");
            }
            return iter->second;
        }

        /*!
         * \brief This method finds the function pointer mapped to the name.
         *  This method takes as parameters, a string name of the free
         *  function as the user calls it.
         *  It looks through its internal map to find the correct function
         *  pointer and returns it.
         *  This method is enable only when users are using a
         *  HistogramTreeNode.
         */
        template<typename T>
        MetaStruct::enable_if_t<
            std::is_same<MetaStruct::decay_t<T>,
                HistogramTreeNode>::value,
                    HistStatCalcFcn<T>&>
        find(const std::string& name){
            const auto& iter = functions_normal_.find(name);

            // If no such function pointers were found, it should throw.
            if(iter == functions_normal_.end()){
                throw sparta::SpartaException(
                "This method " + name + " was never registered with the REGISTER_HISTOGRAM_STAT_CALC_FCN macro.");
            }
            return iter->second;
        }
    private:

        // Singleton private default constructor.
        FunctionManager() = default;

        // Constant string token which the users will use as a prefix
        // while writing their statdefinitions in yaml files.
        const std::string token_ = "hist_def";

        // The map of method names to function pointers for CycleHistogramNodes.
        std::map<std::string, HistStatCalcFcn<CycleHistogramTreeNode>> functions_cycle_;

        // The map of method names to function pointers for CycleHistogramNodes.
        std::map<std::string, HistStatCalcFcn<HistogramTreeNode>> functions_normal_;
    };
}

/*!
 * \brief Function Registration Macro for Histogram/CycleHistogram.
 *  This macro is called by the users in their code when they are
 *  trying to register a free function for stat collection.
 *  This macro takes two parameters, the name of the method they
 *  are trying to register and the type of argument this method
 *  takes whether it is a HistogramTreeNode or a CycleHistogramTreeNode.
 *
 *  Example :
 *  Calculate three times the standard deviation of all counts
 *  in regular and over/under flow bins.
 *  double stdev_x3(const sparta::CycleHistogramTreeNode* h)
 *  {
 *      return (h->getStandardDeviation()) * 3;
 *  }
 *
 *  REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, stdev_x3);
 */
#define REGISTER_HISTOGRAM_STAT_CALC_FCN(histogram_type, fcn_name)                              \
{                                                                                               \
    static_assert(MetaStruct::matches_any<sparta::histogram_type,                                 \
                                          sparta::HistogramTreeNode,                              \
                                          sparta::CycleHistogramTreeNode>::value,                 \
        "Invalid class type used for histogram stat calculation registration.");                \
    std::string key = #fcn_name;                                                                \
    sparta::HistStatCalcFcn<sparta::histogram_type> callable = fcn_name;                            \
    sparta::FunctionManager::get().add<sparta::histogram_type>(key, callable);                      \
}
#endif
