/* TapeHeader.cc
  Class definition for reading and manipulating the tape header.

   Original Author: Jerry V. Pelk 
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <TapeHeader.h>

/******************************************************************************
** Public Functions 
******************************************************************************/ 

TapeHeader::TapeHeader ()

// Constructor.  
{
  printf("TapeHeader::TapeHeader called...\n");
// Set the flight descriptor pointer, and select the first descriptor.
  fl = (Fl*)hdr;
  firstDesc();

// Zero the hdr, rate and rate count arrays.
  memset(hdr, 0, sizeof(hdr));
  memset((char*)sdi_rate, 0, sizeof(sdi_rate));
  memset((char*)sdi_rate_cnt, 0, sizeof(sdi_rate_cnt));
  sdi_rate_idx = 0;

}

/*****************************************************************************/ 
int TapeHeader::readFile (char *fname)

// Reads the tape header from a disk file into the header memory.
{
  FILE *fp;
  unsigned int len = 0;
 
  if ((fp = fopen(fname, "rb")) == NULL)
  {
    printf ("TapeHeader: file '%s' not found.\n", fname);
    exit( ERROR );
  } 
  len = fread(hdr, 1, sizeof(hdr), fp);

  printf("fread(hdr, 1, sizeof(hdr)) = %d\n", len);
  printf("ntohl(fl->thdrlen)         = %d\n", ntohl(fl->thdrlen));

  if (len != ntohl(fl->thdrlen)) {
    printf ("TapeHeader: error reading file '%s'.\n", fname);
    exit( ERROR );
  }

  return OK;
}

/*****************************************************************************/

int TapeHeader::writeFile (char *fname)

// Writes the tape header from memory to a disk file.
{
  FILE *fp;
  int len = 0;

  if ((fp = fopen(fname, "wb")) == NULL) {
    printf ("TapeHeader: cannot open file '%s'\n", fname);
    exit( ERROR );
  }
  len = fwrite(&hdr[len], (size_t)ntohl(fl->thdrlen), 1, fp); 
  fclose (fp);

  if (len < 1) {
    printf ("TapeHeader: error writing file '%s'\n", fname);
    exit( ERROR );
  }

  return OK;
}

/*****************************************************************************/
int TapeHeader::firstDesc ()

// Sets the desc pointer to the first header descriptor. 
{
  desc = hdr; 
  desc_cnt = 0;
  return true;
}

/*****************************************************************************/
int TapeHeader::firstDesc (char blkName[])

// Finds first descriptor with matching type.
{
  int stat;
   
  for (stat = firstDesc(); stat; stat = nextDesc()) {
    if (strcmp(item_type(), blkName) == 0)
      return(true);
  }

  return(false);
}

/*****************************************************************************/
int TapeHeader::nextDesc ()

// Sets the desc pointer to the next descriptor of the header.
// Returns FALSE if the end of the header is reached.
{
  if (desc_cnt++ < ntohl(((Fl *)hdr)->n_items)) {
    desc += item_len();
    return(true);
  }

  return(false);
}

/*****************************************************************************/
int TapeHeader::nextDesc (char blkName[])

// Sets the desc pointer to the next descriptor of the header.
// Returns FALSE if the end of the header is reached.
{
  while (nextDesc()) {
    if (strcmp(item_type(), blkName) == 0)
      return(true);
  }

  return(false);
}

/*****************************************************************************/
int TapeHeader::selectShDesc (char *sh_name)
 
// Finds the Sh descriptor with a matching name.
// Returns FALSE if the end of the header is reached.
{
  int stat;

  for (stat = firstDesc(); stat; stat = nextDesc()) {
    if (shType() && !strcmp (sh_name, name()))
      return true;
  }

  return false;
}

/*****************************************************************************/
int TapeHeader::getSDI(char varName[])

// Finds the descriptor of type SDI and matching name.
{
   int stat;

   for (stat = firstDesc(); stat; stat = nextDesc()) {
      if ((strcmp(item_type(),SDI_STR) == 0) &&
          (strcmp(name(),varName) == 0))
         return(true);
   }

   return(false);
}

/*****************************************************************************/
/*****************************************************************************/
int TapeHeader::getPMS2DType(char varName[])

// Finds the descriptor of type PMS2D and matching name.
{
   int stat;

   for (stat = firstDesc(); stat; stat = nextDesc()) {
      if ((strcmp(item_type(),PMS2D_STR) == 0) &&
          (strcmp(name(),varName) == 0))
         return(true);
   }

   return(false);
}

/*****************************************************************************/
int TapeHeader::getAsync(char varName[])

// Finds the descriptor of type ASYNC and matching name.
{
   int stat;

   for (stat = firstDesc(); stat; stat = nextDesc())
   {
      if ((strcmp(item_type(),ASYNC_STR) == 0) &&
          (strcmp(name(),varName) == 0))
         return(true);
   }

   return(false);
}

/*****************************************************************************/
void TapeHeader::enterDesc (char *new_desc, int len)

// Appends a descriptor to the header. Assumes descriptors are entered in
// the proper order.
{
  desc = &hdr[ntohl(fl->thdrlen)];	// set current desc pointer
  memcpy(desc, new_desc, len); 		// copy in the new desc

// If this is a flight descriptor, init its fields.
  if (flType()) {
    fl->thdrlen = htonl(len);
    fl->n_items = 0;
    fl->lrlen = 0;
    fl->lrppr = 0;
    return;
  }

// Update the flight descriptor fields.
  fl->thdrlen = htonl(ntohl(fl->thdrlen) + len);	// fl->thdrlen += len
  fl->n_items = htonl(ntohl(fl->n_items) + 1);		// fl->n_items++

  if (sdiType()) {
    if (rate() != RATE_5000)
      fl->lrlen = htonl(ntohl(fl->lrlen) + length() * rate());

    if (rate() != sdi_rate[sdi_rate_idx]) {	// increment sample rate ctr
      if (sdi_rate[sdi_rate_idx])
        sdi_rate_idx++;
      sdi_rate[sdi_rate_idx] = rate();
    }
    sdi_rate_cnt[sdi_rate_idx]++;
  }

  else if (pms1v2Type()) 
    fl->lrlen = htonl(ntohl(fl->lrlen) + length() + ps_length());

  else if (strcmp(desc, PMS2D_STR) && strcmp(desc, ASYNC_STR) &&
           strcmp (desc, MCR_STR) && strcmp(desc, MASP_STR))
    fl->lrlen = htonl(ntohl(fl->lrlen) + length());

  fl->lrppr = htonl(MX_PHYS / ntohl(fl->lrlen));

} 

/*****************************************************************************/ 
void TapeHeader::calcDescFields ()

// Calculates the start and offset fields of the descriptor items.
{
  int ridx = 0;				// current rate index
  int rcnt = 0;				// current rate cnt
  int start = 0;			// block start index
  int offset = 0;			// offset between sdi samples

  firstDesc();				// skip over the flight descriptor

  while (nextDesc()) {
    if (sdiType())
      {
      if (rate() != sdi_rate[ridx])
        {
        fprintf (stderr, 
          "TapeHeader::calcDescFields: sdi rate does not match rate arrays.\n");
        exit(1);
        }

      if (convert() == 0.0)
        {
        fprintf(stderr, "\nWARNING >>> discComm:TapeHeader: A/D cal of 0 0.\n");
//        exit(1);
        }

      offset = sdi_rate_cnt[ridx] * length();
      ((Sh*)desc)->offset = htonl(offset);
      ((Sh*)desc)->start = htonl(start);

      if (++rcnt >= sdi_rate_cnt[ridx])		// check for new rate block
        {
        if (rate() != RATE_5000)
          start += offset * (sdi_rate[ridx] - 1);
        ridx++;
        rcnt = 0;
        }
      }
    else if (shType()) {
      ((Sh*)desc)->start = htonl(start);
      ((Sh*)desc)->offset = 0;
    }
    else if (blkType()) 
      ((Blk*)desc)->start = htonl(start);

    else if (evtType()) 
      ((Evt*)desc)->start = htonl(start);

    else if (insType()) 
      ((Ins*)desc)->start = htonl(start);

    else if (irsType())
      ((Irs*)desc)->start = htonl(start);

    else if (pms1v2Type()) {
      ((Pms1v2*)desc)->start = htonl(start);
      if (ps_length()) {
        ((Pms1v2*)desc)->ps_start = htonl(start + length());
        start += ps_length();
      }
    }

    else if (pms1Type())
      ((Pms1*)desc)->start = htonl(start);

    else if (pms2hType())
      ((Pms2h*)desc)->start = htonl(start);

    else if (greyhType())
      ((Greyh*)desc)->start = htonl(start);

    else if (serialType())
      ((Serial*)desc)->start = htonl(start);

    else if (sppType())
      ((Pms1v3*)desc)->start = htonl(start);

    else if (uvhygType())
      ((Uvhyg*)desc)->start = htonl(start);

    else if (maspType())
       ((Masp*)desc)->start = htonl(start);

    else
    if (pms2Type() || asyType() || mcrType())
      continue;

    else {
      fprintf (stderr, 
        "TapeHeader::calcDescFields: unknown descriptor type -->%s.\n", desc);
      exit(0);
      }

    if (rate() != RATE_5000)
      start += length();
    }

  firstDesc();					// reset to the first desc
}

/*****************************************************************************/
int TapeHeader::start()
// Returns the start field of the current descriptor. 0 if invalid.
{
  if (shType())
    return(ntohl(((Sh *)desc)->start));
 
  if (blkType())
    return(ntohl(((Blk *)desc)->start));
 
  if (evtType())
    return(ntohl(((Evt *)desc)->start));
 
  if (irsType())
    return(ntohl(((Irs *)desc)->start));

  if (insType())
    return(ntohl(((Ins *)desc)->start));
 
  if (pms1v2Type())
    return(ntohl(((Pms1v2 *)desc)->start));

  if (sppType())
    return(ntohl(((Pms1v3 *)desc)->start));

  if (pms1Type())
    return(ntohl(((Pms1 *)desc)->start));
 
  if (serialType())
    return(ntohl(((Serial *)desc)->start));
 
  if (uvhygType())
    return(ntohl(((Uvhyg *)desc)->start));
 
  if (pms2hType())
    return(ntohl(((Pms2h *)desc)->start));
 
  if (greyhType())
    return(ntohl(((Greyh *)desc)->start));

  if (maspType())
     return(ntohl(((Masp *)desc)->start));
  
  if (pms2Type() || asyType() || mcrType())
    return(0);
 
  fprintf(stderr, "TapeHeader::length: unknown descriptor type -->%s\n.", desc);

  return(0);

}

/*****************************************************************************/ 
int TapeHeader::length()
// Returns the length field of the current descriptor. 0 if invalid.
{ 
  if (shType())
    return(ntohl(((Sh*)desc)->length));

  if (blkType())
    return(ntohl(((Blk*)desc)->length));

  if (evtType())
    return(ntohl(((Evt*)desc)->length));

  if (irsType())
    return(ntohl(((Irs*)desc)->length));

  if (insType())
    return(ntohl(((Ins*)desc)->length));

  if (pms1v2Type())
    return(ntohl(((Pms1v2*)desc)->length));

  if (sppType())
    return(ntohl(((Pms1v3*)desc)->length));

  if (pms1Type())
    return(ntohl(((Pms1*)desc)->length));

  if (serialType())
    return(ntohl(((Serial*)desc)->length));

  if (uvhygType())
    return(ntohl(((Uvhyg*)desc)->length));

  if (pms2hType())
    return(ntohl(((Pms2h*)desc)->length));

  if (greyhType())
    return(ntohl(((Greyh*)desc)->length));

  if (pms2Type())
    return(ntohl(((Pms2*)desc)->lrlen));

  if (mcrType())
    return(ntohl(((Mcrh*)desc)->lrlen));

  if (maspType())
     return(ntohl(((Masp*)desc)->length));
  
  if (asyType())
    return(ntohl(((Asy*)desc)->lrlen));
 
  fprintf(stderr, "TapeHeader::length: unknown descriptor type -->%s\n.", desc);

  return(0);

}

/*****************************************************************************/ 
void TapeHeader::coefficients (float cof[])

// Copies the coefficients of the current descriptor into the passed in array.
// Returns TRUE if ok, FALSE otherwise.
{
  int j;

  for (j = 0; j < order(); j++)
    cof[j] = ntohf(((Sh*)desc)->cof[j]);

}

/*****************************************************************************/
void TapeHeader::spp_OPCthesholds (unsigned short chans[])

// Copies the coefficients of the current descriptor into the passed in array.
// Returns TRUE if ok, FALSE otherwise.
{
  int j;

  for (j = 0; j < spp_bins(); j++)
    chans[j] = ntohs(((Pms1v3 *)desc)->OPCthreshold[j]);

}

/*****************************************************************************/ 
char* TapeHeader::name ()
// Returns the name field of the current descriptor. 0 if invalid.
{
  if (shType())
    return ((Sh*)desc)->name;

  if (pms1v2Type())
    return ((Pms1v2*)desc)->name;

  if (sppType())
    return ((Pms1v3*)desc)->name;

  if (pms1Type())
    return ((Pms1*)desc)->name;

  if (pms2Type())
    return ((Pms2*)desc)->name;

  if (pms2hType())
    return ((Pms2h*)desc)->name;

  if (greyhType())
    return ((Greyh*)desc)->name;

  if (mcrType())
    return ((Mcrh*)desc)->name;

  if (asyType())
    return ((Asy*)desc)->name;

  return (char*)0;                     // type doesn't have a name field

}

/*****************************************************************************/
int TapeHeader::rate()

// Returns the rate field of the current descriptor. 
// Returns 1 if no rate field for this descriptor.
{
  if (shType())
    return(ntohl(((Sh *)desc)->rate));

  if (sppType())
    return(ntohl(((Pms1v3 *)desc)->rate));

  if (pms1v2Type())
    return(ntohl(((Pms1v2 *)desc)->rate));

  if (pms1Type())
    return(ntohl(((Pms1 *)desc)->rate));

  if (insType())
    return(ntohl(((Ins *)desc)->rate));

  return(1);                     // all other types have a rate of 1

}

/*****************************************************************************/
int TapeHeader::sampleLength()
// Returns the length of one sample of the current descriptor. 0 if invalid. 
{  
  if (pms1v2Type())
    return(ntohl(((Pms1v2 *)desc)->length) / ntohl(((Pms1v2 *)desc)->rate));
  if (pms1Type())
    return(ntohl(((Pms1 *)desc)->length) / ntohl(((Pms1 *)desc)->rate));
  
  return(length());		// same as blk length for all others

} 

/*****************************************************************************/
char* TapeHeader::dsm_locn() 

// Returns the dsm location string of the current header.
{ 
  if (shType())
    return ((Sh*)desc)->dsm_locn;

  if (blkType())
    return ((Blk*)desc)->dsm_locn;

  if (evtType())
    return ((Evt*)desc)->dsm_locn;

  if (irsType())
    return ((Irs*)desc)->dsm_locn;
  
  if (insType())
    return ((Ins*)desc)->dsm_locn;

  if (pms1v2Type())
    return ((Pms1v2*)desc)->dsm_locn;

  if (sppType())
    return ((Pms1v3*)desc)->dsm_locn;

  if (pms1Type())
    return ((Pms1*)desc)->dsm_locn;

  if (serialType())
    return ((Serial*)desc)->dsm_locn;

  if (uvhygType())
    return ((Uvhyg*)desc)->dsm_locn;

  if (greyhType())
    return ((Greyh*)desc)->dsm_locn;

  if (pms2hType())
    return ((Pms2h*)desc)->dsm_locn;

  if (pms2Type())
    return ((Pms2*)desc)->dsm_locn;

  if (mcrType())
    return ((Mcrh*)desc)->dsm_locn;

  if (maspType())
     return((Masp*)desc)->dsm_locn;

  if (asyType())
    return ((Asy*)desc)->dsm_locn;

  fprintf (stderr, 
    "TapeHeader::dsm_locn: unknown descriptor type -->%s\n.", desc);

  return((char*)0);				// no dsm_locn field

}

/*****************************************************************************/
char* TapeHeader::locn ()
 
// Returns the instrument location field of the current descriptor.
{
  if (blkType())
    return ((Blk*)desc)->locn;

  if (evtType())
    return ((Evt*)desc)->locn;

  if (irsType())
    return ((Irs*)desc)->locn;

  if (pms1Type())
    return ((Pms1*)desc)->locn;

  if (pms1v2Type())
    return ((Pms1v2*)desc)->locn;

  if (sppType())
    return ((Pms1v3*)desc)->locn;

  if (greyhType())
    return ((Greyh*)desc)->locn;

  if (pms2hType())
    return ((Pms2h*)desc)->locn;

  if (pms2Type())
    return ((Pms2*)desc)->locn;

  if (mcrType())
    return ((Mcrh*)desc)->locn;

  if (maspType())
     return((Masp*)desc)->locn;
  
  if (uvhygType())
    return ((Uvhyg*)desc)->locn;

  return 0;
}
