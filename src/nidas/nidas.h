/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

/*!
  \mainpage NIDAS

  \section intro_sec Introduction
  
NIDAS is the NCAR In-Situ Data Acquisition Software, used for data
acquisition, archiving, and processing on NSF NCAR aircraft platforms and
surface observation networks.  This is the API documentation for the C++
libraries and executables.  More information on installing, building, and
developing NIDAS is in the top README.md, or at the NIDAS repository website:
https://github.com/NCAR/nidas.

  \section namespaces_sec Namespaces

There are three main namespaces in NIDAS: nidas::util, nidas::core,
nidas::util.

nidas::util provides general, lower-level utility interfaces.

nidas::core contains, naturally, the core of NIDAS.  The important base class
nidas::core::DSMSensor defines the interface which all NIDAS sensors
implement.  Other important interfaces define data samples
(nidas::core::Sample), the interfaces by which samples can be received
(nidas::core::SampleClient) and distributed (nidas::core::SampleSource).
Classes for reading and writing file and network IO devices implement
nidas::core::IOStream.  Classes which manage sensors and metadata
(nidas::core::Project) and archiving (nidas::core::SampleArchiver) can be
configured from the XML configuration file, facilitated by implementing the
nidas::core::DOMable interface.

nidas::dynld contains classes which implement more specific kinds of sensors
and outputs which are used by different platforms at runtime, so they can be
loaded dynamically as needed.

  \namespace nidas
    \brief Root namespace for the NCAR In-Situ Data Acquisition Software.
 */
