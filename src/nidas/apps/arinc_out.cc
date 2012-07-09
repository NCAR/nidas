/* arinc_out.cc

   User space code that generates ARINC data.

   Original Author: John Wasinger

   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

#include <iostream>

#include <math.h>

#include <stdlib.h>
#include <stdio.h>
//#include <linux/delay.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <nidas/dynld/raf/DSMArincSensor.h>

// bitmask for the Sign Status Matrix
#define SSM 0x60000000

const double PI = 3.14159265358979;
int    FREQ   = 1;
double MAXDEG = 10.0;
int    STEP   = 1000;
int    GAP    = 0;
bool SWEEP    = false;

using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

class TestArinc : public DSMArincSensor
{
public:
    double processLabel(const int,sampleType*) { return 0.0; }
};

/* -------------------------------------------------------------------- */

int usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-f ... ] [-m ... ] [-s ... ] [-g ... ] [-S]\n\
  -f: frequency or transmission (default is " << FREQ << " Hz)\n\
  -m: maximum angle (default is " << MAXDEG << ")\n\
  -s: step size (N*2Pi) (default is " << STEP << ")\n\
  -g: time gap between ARINC words (default is " << GAP << " usec)\n\
  -S: sweep the whole ARINC word\n\
\n\
Iteractive commands:  (must be terminated with a carriage return!)\n\
  p: toggle active state of pitch\n\
  r: toggle active state of roll\n\
  P: level pitch to zero\n\
  R: level roll to zero\n\
  P[+-.0123456789]: set pitch to a specified angle\n\
  R[+-.0123456789]: set roll to a specified angle\n\
  q: quit\n\
\n";
    return 1;
}

/* -------------------------------------------------------------------- */

int parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt()  */
    int opt_char;              /* option character */

    while ((opt_char = getopt(argc, argv, "f:m:s:g:h?NS")) != -1) {

	switch (opt_char) {
	case 'f':
            FREQ = atoi(optarg);
	    break;
	case 'm':
            MAXDEG = atof(optarg);
	    break;
	case 's':
            STEP = atoi(optarg);
	    break;
	case 'g':
            GAP = atoi(optarg);
	    break;
	case 'S':
            SWEEP = true;
	    break;
	case 'h':
	case '?':
	    return usage(argv[0]);
	}
    }
    return 0;
}

/* -------------------------------------------------------------------- */

int main(int argc, char** argv)
{
    int res = parseRunstring(argc,argv);
    if (res != 0) return res;

    std::cout << __FILE__ << " creating sensors...\n";

    TestArinc sensor_out_0;
    sensor_out_0.setDeviceName("/dev/arinc4");

    std::cout << __FILE__ << " opening sensors...\n";

    try {
        sensor_out_0.open(O_WRONLY);
    }
    catch (n_u::IOException& ioe) {
        std::cout << ioe.what() << std::endl;
        return 1;
    }

    std::cout << __FILE__ << " sensor_out_0.getWriteFd() = " << sensor_out_0.getWriteFd() << std::endl;

    int tem;
    char c;

    bool active = true;

    bool actPitch = false;
    bool actRoll  = false;

    int pitch = 0;
    int roll  = 0;

    double fltPitch = 0.0;
    double fltRoll  = 0.0;

    double valPitch = 0.0;
    double valRoll  = 0.0;

    int intPitch = 0;
    int intRoll  = 0;

    int arnPitch = 0;
    int arnRoll  = 0;

    tem = fcntl(0, F_GETFL, 0);
    fcntl (0, F_SETFL, (tem | O_NDELAY));

    struct timeval tvStart;
    struct timeval tvEnd;
    time_t printed = 0;
    int delta = 0;

    char numValue[20];
    int nVi = -1, setPitch = 0, setRoll = 0;

    int arnSweep;
    int sweep = 0;

    for (int i=0; i<20; i++) numValue[i] = 0;

    while (active) {

        ::gettimeofday(&tvStart, NULL);

        while (::read(0, &c, 1) > 0) {
            if ( ( nVi > -1) & ( nVi < 20) ) {
                numValue[nVi++] = c;
                continue;
            }
            switch (c) {
            case 'p':
                nVi = -1;
                setPitch = 0;
                actPitch = !actPitch;
                break;
            case 'r':
                nVi = -1;
                setRoll  = 0;
                actRoll  = !actRoll;
                break;
            case 'P':
                nVi = 0;
                setPitch = 2;
                break;
            case 'R':
                nVi = 0;
                setRoll  = 2;
                break;
            case 'q':
                active = false;
                break;
            default:
                break;
            }
        }
        if ( nVi > -1) {
            nVi--;
            for (int i=0; i<nVi; i++) {
                switch (numValue[i]) {
                case '+':
                case '-':
                case '.':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    break;
                default:
                    nVi = 0;
                    break;
                }
            }
            if (nVi > -1)
                numValue[nVi++] = 0;
        }
        if (actPitch) pitch++;
        if (actRoll)  roll++;

        if (pitch>=4*STEP) pitch = 0;
        if (roll>=4*STEP)   roll = 0;

//      printf("%d %d | %d %d\n", actPitch, actRoll, pitch, roll);

        fltPitch = sin(pitch * PI / (2.0 * STEP)) * MAXDEG;
        fltRoll  = sin(roll  * PI / (2.0 * STEP)) * MAXDEG;

//      printf("%d %d | %f %f\n", actPitch, actRoll, fltPitch, fltRoll);

        if (setPitch == 2) {
            valPitch = atof(numValue);
            for (int i=0; i<20; i++) numValue[i] = 0;
            setPitch = 1;
            nVi = -1;
        }
        if (setPitch == 1) {
            fltPitch = valPitch;
        }
        if (setRoll  == 2) {
            valRoll  = atof(numValue);
            for (int i=0; i<20; i++) numValue[i] = 0;
            setRoll  = 1;
            nVi = -1;
        }
        if (setRoll  == 1) {
            fltRoll  = valRoll ;
        }

        intPitch = int(fltPitch / 6.866455078125e-4);
        intRoll  = int(fltRoll  / 6.866455078125e-4);

//      printf("%d %d | %d %d\n", actPitch, actRoll, intPitch, intRoll);

        // ARINC 429 word format:
        //           1           f           f           f           f           c           0           0
        // 32|31 30|29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11|10  9| 8  7  6  5  4  3  2  1
        // --+-----+--------------------------------------------------------+-----+-----------------------
        // P | SSM |                          data                          | SDI |      8-bit label
        arnPitch = SSM | ((intPitch<<10) & 0x1ffffc00) | 0324;
        arnRoll  = SSM | ((intRoll <<10) & 0x1ffffc00) | 0325;
//      arnPitch = SSM | (intPitch<<10) | 0324;
//      arnRoll  = SSM | (intRoll <<10) | 0325;

        if (SWEEP) {
            arnSweep = (1<<sweep);
            if (++sweep == 32) sweep = 0;
        }
        // periodically print what is being sent
        if ((tvStart.tv_sec % 1 == 0) && (printed != tvStart.tv_sec)) {
            printed = tvStart.tv_sec;
            if (SWEEP) {
                printf("%11d.%-6d (%-6d) # %08x\n",
                       (int)tvStart.tv_sec, (int)tvStart.tv_usec, delta, arnSweep);
            } else {
                printf("%11d.%-6d (%-6d) # %d %d | %08x %08x | %08x %08x | %8d %8d | % 9.5f % 9.5f\n",
                       (int)tvStart.tv_sec, (int)tvStart.tv_usec, (int)delta,
                       actPitch, actRoll, arnPitch, arnRoll, intPitch, intRoll, intPitch, intRoll, fltPitch, fltRoll);
            }
        }
        if (SWEEP) {
            ::write(sensor_out_0.getWriteFd(), &arnSweep, sizeof(int));
        } else {
            ::write(sensor_out_0.getWriteFd(), &arnPitch, sizeof(int));
            usleep(GAP);  // space the bursted messages apart
            ::write(sensor_out_0.getWriteFd(), &arnRoll,  sizeof(int));
        }
        // delay for the remaining 1/Nth of a second...
        ::gettimeofday(&tvEnd, NULL);
        delta = (1000000*tvEnd.tv_sec + tvEnd.tv_usec) - (1000000*tvStart.tv_sec + tvStart.tv_usec);
        usleep((1000000 / FREQ) - delta);
    }
    fcntl(0, F_SETFL, tem);

    std::cout << __FILE__ << " closing sensors...\n";
    try {
        sensor_out_0.close();
    }
    catch (n_u::IOException& ioe) {
        std::cout << ioe.what() << std::endl;
        return 1;
    }
    std::cout << __FILE__ << " sensors closed.\n";
    return 0;
}
