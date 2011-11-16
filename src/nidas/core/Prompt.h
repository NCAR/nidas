// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$
    
    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_CORE_PROMPT_H 
#define NIDAS_CORE_PROMPT_H 

#include <iostream>
#include <string>

using namespace std;

namespace nidas { namespace core {

/**
 *  Class to contain prompt information - string and rate
 */

class Prompt {

public: 
    Prompt(): _promptString(),_promptRate(0.0) {}

    void setString(const std::string& val) { 
        _promptString = val; }

    const std::string& getString() const { 
        return _promptString; }

    void setRate(const float val) {
         _promptRate = val; }

    float getRate() const {
         return _promptRate; }

private:

    std::string  _promptString;
    float        _promptRate;

};

}} // namespace nidas namespace core

#endif
