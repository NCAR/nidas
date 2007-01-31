/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    The macro DSM_VERSION should be defined with a compiler option:
    	-DDSM_VERSION=XXXX.
 ********************************************************************

*/

#ifndef DSM_VERSION_H
#define DSM_VERSION_H

/*
 * must have two levels of these stringify macros,
 * because "y(x) #x" does not pre-expand x if x is a macro.
 * These are the same as the STRINGX,XSTRING macros in <symcat.h>,
 * but we can't #include the standard header files here in driver code.
 */
#define DSM_STRINGX(x) #x
#define DSM_STRING(x) DSM_STRINGX(x)

#if !defined(DSM_VERSION)
#define DSM_VERSION unknown
#endif

#define DSM_VERSION_STRING DSM_STRING(DSM_VERSION)

#endif
