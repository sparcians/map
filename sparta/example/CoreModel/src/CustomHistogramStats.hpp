
#pragma once

#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/CycleHistogram.hpp"

/**
 * \file CustomHistogramStats.h
 * This file contains some example custom stats calculations that users may
 * refer to in their inline YAML report definitions by function name.
*/

//! Calculate three times the standard deviation of all counts
//  in regular and over/under flow bins.
double stdev_x3(const sparta::CycleHistogramTreeNode* h)
{
    return (h->getStandardDeviation()) * 3;
}

//! Calculate sum of bin probabilities of all bins having
//  count greater than twice the standard deviation.
double fraction_coverage_greaterThan2StdDev(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    const auto& bin_counts = h->getRegularBin();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > (2*std_dev)){
            total += bin_probs[i];
        }
    }
    if(uf > (2*std_dev)){
        total += uf_p;
    }
    if(of > (2*std_dev)){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean plus SD and mean plus twice SD.
double fraction_coverage_mean_p_StdDev_mean_p_2StdDev(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean + std_dev;
    const double right_interval = mean + (2 * std_dev);
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean and mean plus SD.
double fraction_coverage_mean_mean_p_StdDev(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean;
    const double right_interval = mean + std_dev;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean minus SD and mean.
double fraction_coverage_mean_m_StdDev_mean(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean - std_dev;
    const double right_interval = mean;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean minus twice SD and mean minus SD.
double fraction_coverage_mean_m_2StdDev_mean_m_StdDev(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean - (2 * std_dev);
    const double right_interval = mean - std_dev;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count lesser than twice SD.
double fraction_coverage_lesserThan2StdDev(const sparta::CycleHistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    const auto& bin_counts = h->getRegularBin();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] < (2*std_dev)){
            total += bin_probs[i];
        }
    }
    if(uf < (2*std_dev)){
        total += uf_p;
    }
    if(of < (2*std_dev)){
        total += of_p;
    }
    return total;
}

//! Calculate three times the standard deviation of all counts
//  in regular and over/under flow bins.
double stdev_x3_h(const sparta::HistogramTreeNode* h)
{
    return (h->getStandardDeviation()) * 3;
}

//! Calculate sum of bin probabilities of all bins having
//  count greater than twice the standard deviation.
double fraction_coverage_greaterThan2StdDev_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    const auto& bin_counts = h->getRegularBin();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > (2*std_dev)){
            total += bin_probs[i];
        }
    }
    if(uf > (2*std_dev)){
        total += uf_p;
    }
    if(of > (2*std_dev)){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean plus SD and mean plus twice SD.
double fraction_coverage_mean_p_StdDev_mean_p_2StdDev_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean + std_dev;
    const double right_interval = mean + (2 * std_dev);
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean and mean plus SD.
double fraction_coverage_mean_mean_p_StdDev_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean;
    const double right_interval = mean + std_dev;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean minus SD and mean.
double fraction_coverage_mean_m_StdDev_mean_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean - std_dev;
    const double right_interval = mean;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count between mean minus twice SD and mean minus SD.
double fraction_coverage_mean_m_2StdDev_mean_m_StdDev_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const double mean = h->getMeanBinCount();
    const double left_interval = mean - (2 * std_dev);
    const double right_interval = mean - std_dev;
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const auto& bin_counts = h->getRegularBin();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] > left_interval && bin_counts[i] <= right_interval){
            total += bin_probs[i];
        }
    }
    if(uf > left_interval && uf <= right_interval){
        total += uf_p;
    }
    if(of > left_interval && of <= right_interval){
        total += of_p;
    }
    return total;
}

//! Calculate sum of bin probabilities of all bins having
//  count lesser than twice SD.
double fraction_coverage_lesserThan2StdDev_h(const sparta::HistogramTreeNode* h)
{
    double total = 0.0;
    const double std_dev = h->getStandardDeviation();
    const auto& bin_probs = h->recomputeRegularBinProbabilities();
    const double uf = h->getUnderflowBin();
    const double uf_p = h->getUnderflowProbability();
    const double of = h->getOverflowBin();
    const double of_p = h->getOverflowProbability();
    const auto& bin_counts = h->getRegularBin();
    for(size_t i = 0; i < bin_probs.size(); ++i){
        if(bin_counts[i] < (2*std_dev)){
            total += bin_probs[i];
        }
    }
    if(uf < (2*std_dev)){
        total += uf_p;
    }
    if(of < (2*std_dev)){
        total += of_p;
    }
    return total;
}
