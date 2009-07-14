/***********************************************************************
 * lidar_vel.c
 *
 * Mike Dixon  RAP NCAR Boulder CO USA
 *
 * May 2009
 *
 ************************************************************************/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define MAX_BUFFER 512

static int debug = 0;
static int verbose = 0;
static char *filepath = NULL;
static double nyq_freq_mhz = 100;
static double noise_mag = 1.0e6;

static int parse_args(int argc, char **argv);

static void usage(FILE *out);

static int process_file(char *path);

static void compute_freq(int nsamples,
                         double *spec,
                         double *mean_freq,
                         double *spec_width);

int main(int argc, char **argv)

{

  if (parse_args(argc, argv)) {
    fprintf(stderr, "ERROR - lidar_vel\n");
    fprintf(stderr, "  parsing command line\n");
    usage(stderr);
    return -1;
  }

  if (filepath == NULL) {
    usage(stderr);
    fprintf(stderr, "ERROR - lidar_vel\n");
    fprintf(stderr, "  You must set file path\n");
    return -1;
  }

  if (process_file(filepath)) {
    fprintf(stderr, "ERROR - lidar_vel\n");
    fprintf(stderr, "  Processing file: %s\n", filepath);
    return -1;
  }

  return 0;

}

/* parse command line */

static int parse_args(int argc, char **argv)

{

  int iret = 0;
  int i;
  
  for (i =  1; i < argc; i++) {

    if (!strcmp(argv[i], "--") ||
	!strcmp(argv[i], "-h") ||
	!strcmp(argv[i], "-help") ||
	!strcmp(argv[i], "-man")) {
      
      usage(stderr);
      exit (0);
      
    } else if (!strcmp(argv[i], "-debug")) {
      
      debug = 1;
      
    } else if (!strcmp(argv[i], "-verbose")) {
      
      debug = 1;
      verbose = 1;
      
    } else if (!strcmp(argv[i], "-f")) {
      
      if (i < argc - 1) {
        filepath = argv[++i];
      } else {
	iret = -1;
      }
	
    } else if (!strcmp(argv[i], "-nyq")) {
      
      if (i < argc - 1) {
        nyq_freq_mhz = atof(argv[++i]);
      } else {
	iret = -1;
      }
	
    } else if (!strcmp(argv[i], "-noise")) {
      
      if (i < argc - 1) {
        noise_mag = atof(argv[++i]);
      } else {
	iret = -1;
      }
	
    } // if
    
  } // i

  if (iret) {
    usage(stderr);
  }
  
  return iret;
    
}

/* print usage */

static void usage(FILE *out)

{

  fprintf(out, "Usage: lidar_vel [options as below]\n");
  fprintf(out, "  [-debug] : set debugging on\n");
  fprintf(out, "  [-verbose] : set verbose debugging on\n");
  fprintf(out, "  [-f ?] : set spectra file path\n");
  fprintf(out, "  [-nyq ?] : specify nyquist frequency (MHz)\n");
  fprintf(out, "  [-noise ?] : set noise magnitude\n");

}

/* process the file */

static int process_file(char *path)

{

  int ii;
  double spec[MAX_BUFFER];
  double peak[MAX_BUFFER];
  FILE *in;

  if (debug) {
    fprintf(stderr, "Processing file: %s\n", path);
    fprintf(stderr, "Nyquist freq (MHz): %g\n", nyq_freq_mhz);
    fprintf(stderr, "Noise magnitude: %g\n", noise_mag);
  }

  for (;;) {
    sleep(1);
    int nData  = 0;
    int n_spec = 0;
    int n_peak = 0;

    if ((in = fopen(path, "r")) == NULL) {
      fprintf(stderr, "failed to open\n");
      continue;
    }
    
    // bail out if file is not complete
    fseek(in, 0, SEEK_END);
    int fsize = ftell(in);
    rewind(in);
    if (fsize != 4104) {
//    fprintf(stderr, "file is wrong size (%d)\n", fsize);
      fclose(in);
      continue;
    }
    while (!feof(in)) {
      unsigned int ispec;
      if (fread(&ispec, sizeof(ispec), 1, in) != 1) {
        break;
      }
      if (nData < MAX_BUFFER)
        spec[n_spec++] = ispec;
     else
        peak[n_peak++] = ispec;

      nData++;
    }
    fclose(in);
  
    if (verbose) {
      fprintf(stderr, "==================== spectrum ===================\n");
      for (ii = 0; ii < n_spec; ii++) {
        fprintf(stderr, "%10d %10.2g\n", ii, spec[ii]);
      }
      fprintf(stderr, "=================================================\n");
    }
  
    double mean_freq, spec_width;
    compute_freq(n_spec, spec, &mean_freq, &spec_width);
//  if (mean_freq > 1)
    {
      fprintf(stderr, "spec: mean_freq  (mHz): %10.3f  ", mean_freq);
      fprintf(stderr, "spec: spec_width (mHz): %10.3f\n", spec_width);
    }
    compute_freq(n_peak, peak, &mean_freq, &spec_width);
//  if (mean_freq > 1)
    {
      fprintf(stderr, "peak: mean_freq  (mHz): %10.3f  ", mean_freq);
      fprintf(stderr, "peak: spec_width (mHz): %10.3f\n", spec_width);
      fprintf(stderr, "\n");
    }
  }
  return 0;
}

/* compute the mean frequency, and spectrum width */

static void compute_freq(int nsamples,
                         double *magnitude,
                         double *mean_freq_mhz,
                         double *spec_width_mhz)

{
  
  int ii;
  int kCent = nsamples / 2;
  int kMax;
  int kOffset;
  int kStart, kEnd;
  int count;
  int nTest;

  double maxMag;
  double sumPower;
  double sumK;
  double sumK2;
  double meanK;
  double sdevK;

  double powerCentered[MAX_BUFFER];
  const double *mp, *pw;
  
  double mhz_per_sample = nyq_freq_mhz / nsamples;
  double noise_power = noise_mag * noise_mag;

  /* find max magnitude */
  
  maxMag = 0.0;
  kMax = 0;
  mp = magnitude;
  for (ii = 0; ii < nsamples; ii++, mp++) {
    if (*mp > maxMag) {
      kMax = ii;
      maxMag = *mp;
    }
  }
  if (kMax >= kCent) {
    kMax -= nsamples;
  }

  /* center power array on the max value */

  kOffset = kCent - kMax;
  for (ii = 0; ii < nsamples; ii++) {
    int jj = (ii + kOffset) % nsamples;
    double powr = magnitude[ii] * magnitude[ii];
    powerCentered[jj] = powr;
  }

  /*
   * if the signal is noisy, we use the entire spectrum.
   * Otherwise we only use the part above the noise;
   */

  kStart = 0;
  kEnd = nsamples - 1;

  /*
   * moving away from the peak, find the points in the spectrum
   * where the signal drops below the noise threshold for at
   * least 3 points
   */
  
  count = 0;
  nTest = 3;
  kStart = kCent - 1;
  pw = powerCentered + kStart;
  for (ii = kStart; ii >= 0; ii--, pw--) {
    if (*pw < noise_power) {
      count ++;
      if (count >= nTest) {
        break;
      }
    } else {
      count = 0;
    }
    kStart = ii;
  }
  
  count = 0;
  kEnd = kCent + 1;
  pw = powerCentered + kEnd;
  for (ii = kEnd; ii < nsamples; ii++, pw++) {
    if (*pw < noise_power) {
      count ++;
      if (count >= nTest) {
        break;
      }
    } else {
      count = 0;
    }
    kEnd = ii;
  }
  
  /*
   * compute mom1 and mom2, using those points above the noise floor
   */

  sumPower = 0.0;
  sumK = 0.0;
  sumK2 = 0.0;
  pw = powerCentered + kStart;
  for (ii = kStart; ii <= kEnd; ii++, pw++) {
    double kk = (double) ii;
    double power = *pw;
    sumPower += power;
    sumK += power * kk;
    sumK2 += power * kk * kk;
  }
  meanK = 0.0;
  sdevK = 0.0;
  if (sumPower > 0.0) {
    meanK = sumK / sumPower;
    double diff = (sumK2 / sumPower) - (meanK * meanK);
    if (diff > 0) {
      sdevK = sqrt(diff);
    }
  }

  *mean_freq_mhz = mhz_per_sample * (meanK - kOffset);
  *spec_width_mhz = mhz_per_sample * sdevK;

  if (debug) {
    fprintf(stderr, "=================================================\n");
    fprintf(stderr, "    kMax: %d\n", kMax);
    fprintf(stderr, "    kOffset: %d\n", kOffset);
    fprintf(stderr, "    kStart: %d\n", kStart);
    fprintf(stderr, "    kEnd: %d\n", kEnd);
    fprintf(stderr, "    meanK: %g\n", meanK);
    fprintf(stderr, "    sdevK: %g\n", sdevK);
    fprintf(stderr, "    mhz_per_sample: %g\n", mhz_per_sample);
    fprintf(stderr, "    noise_mag: %g\n", noise_mag);
    fprintf(stderr, "=================================================\n");
  }

}


