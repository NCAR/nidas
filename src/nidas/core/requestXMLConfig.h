// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2012 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2011-11-16 15:03:17 -0700 (Wed, 16 Nov 2011) $

    $LastChangedRevision: 6326 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/requestXMLConfig.h $
 ********************************************************************
*/

#ifndef REQUESTXMLCONFIG_H
#define REQUESTXMLCONFIG_H

#include <xercesc/dom/DOM.hpp>
#include <nidas/util/Inet4SocketAddress.h>
#include <nidas/util/Exception.h>

#include <signal.h>

namespace n_u = nidas::util;

namespace nidas { namespace core {

class reqXMLconf {
public:
/**
 * Request the XML configuration via a McSocket request to
 * a given multicast socket address.
 */
static xercesc::DOMDocument* requestXMLConfig(
  const n_u::Inet4SocketAddress& mcastAddr, sigset_t* signalMask=(sigset_t*)0 )
 throw(n_u::Exception);
};

}}  // namespace nidas namespace core

#endif // REQUESTXMLCONFIG_H
