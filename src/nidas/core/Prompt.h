/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2008-07-23 16:19:14 -0600 (Wed, 23 Jul 2008) $
    
    $LastChangedRevision: 4224 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/Prompt.h $
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
    Prompt(): _promptRate(0.0) {}

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
