// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2011 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_DICTIONARY_H
#define NIDAS_CORE_DICTIONARY_H

#include <string>

namespace nidas { namespace core {

/**
 * Interface for a Dictionary class, which can return a string
 * value for a string token name.
 */
class Dictionary {
public:
    virtual ~Dictionary() {}
    virtual bool getTokenValue(const std::string& token,std::string& value) const = 0;

    /**
     * Utility function that scans a string for tokens like ${XXXX}, or
     * $XXX followed by any characters from ".$/", and replaces them
     * with what is returned by getTokenValue(token,value);
     */
    std::string expandString(const std::string& input) const;
};

}}	// namespace nidas namespace core

#endif
