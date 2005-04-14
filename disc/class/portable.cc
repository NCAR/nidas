/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 1703 $

    $LastChangedBy: wasinger $

    $HeadURL: http://orion/svn/hiaper/ads3/disc/class/portable.cc $
 ********************************************************************
*/

#include <portable.h>

#if !defined(VXWORKS) && (defined(__LITTLE_ENDIAN) || defined(_LITTLE_ENDIAN) || defined(LITTLE_ENDIAN))

float ntohf(float inF)
{
  union
    {
    float       f;
    char        s[4];
    } tmp;
 
  char  sc;
 
  tmp.f = inF;
 
  sc = tmp.s[0];
  tmp.s[0] = tmp.s[3];
  tmp.s[3] = sc;
 
  sc = tmp.s[1];
  tmp.s[1] = tmp.s[2];
  tmp.s[2] = sc;
 
  return(tmp.f);
 
}

#endif

