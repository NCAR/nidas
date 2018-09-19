
#include "metek.h"
#include <cmath>

using std::isnan;

/*namespace metek holds some metek specific corrections*/

namespace nidas { namespace dynld { namespace isff { namespace metek {

  /*CorrectTemperature corrects the sonic temperature by using corrections from
  the raw MeTek values.  This is Risø-R-1658(en) Eqn 20.  '0.0024813895781637717' is
  a precomputed 1/403.0 value*/
  void CorrectTemperature(uvwt &x) {
    x.t += 0.0024813895781637717 * (
        0.75*(std::pow(x.u, 2.0) + std::pow(x.v, 2.0)) +
        0.5*std::pow(x.w, 2.0)
      );
    }

  /*Remove2DCorrections removes the 2D correction applied during sampling by
  the MeTek instrument as a first pass correction.  It remains to be known if
  for PERDIGAO this correction was applied or not. The passed values X is modied
  in place.*/
  void Remove2DCorrections(uvwt &x) {
    double alpha = -std::atan2(x.v, x.u),
           delta = 1.0 + 0.015*std::sin(3*alpha + M_PI/6), // Risø-R-1658(en) Eqn 9
           correctedSpeed = std::sqrt(std::pow(x.u, 2.0) + std::pow(x.v, 2.0)); //corrected horizontal speed
    x.u = x.u/delta; // Risø-R-1658(en) Eqn 6
    x.v = x.v/delta; //Risø-R-1658(en) Eqn 7
    x.w = x.w - 0.031*correctedSpeed / delta * (sin(3*alpha) - 1); //Risø-R-1658(en) Eqn 8
  }

  /*Apply3DCorrect applies any corrections to uvwt, first by removing the applied
  2D corrections, fixing the temperature, and then altering the 3D parameters to correct for measured wind flow per
  the procedue given in Risø-R-1659(EN).  The values in x are altered in place.*/
  void Apply3DCorrect(uvwt &x) {
    Remove2DCorrections(x); //x.u, x.v, and x.w are now all raw values
    CorrectTemperature(x); //do this before we apply final 3D corrections

    // rectangular -> polar
    double s_raw = std::sqrt(std::pow(x.u, 2.0) + std::pow(x.v, 2.0) + std::pow(x.w, 2.0)); //s_raw is the raw wind speed magnitude
    double alpha_raw = std::atan2(-x.v, -x.u); //wind direction
    double phi_raw = -std::atan2( x.w, std::sqrt(std::pow(x.u, 2.0) + std::pow(x.v, 2.0))); //tilt angle

    //calcualte the coeffiecnts to the fourier array:
    double fourierCoeffs[6] = {
      std::cos(3*alpha_raw),
      std::sin(3*alpha_raw),
      std::cos(6*alpha_raw),
      std::sin(6*alpha_raw),
      std::cos(9*alpha_raw),
      std::sin(9*alpha_raw)
    };

    //Applied corrections to the raw values
    double speed_0 = s_raw * CalcCorrection(fourierCoeffs, phi_raw, metek::mu_lut), //Eqn 11?
           alpha_0 = alpha_raw + Degrees2Rad*CalcCorrection(fourierCoeffs, phi_raw, metek::alpha_lut),  //Eqn 12
           phi_0 = phi_raw + Degrees2Rad*CalcCorrection(fourierCoeffs, phi_raw, metek::phi_lut); //Eqn 13

    //with corrections, convert polar -> rectangular
    x.u = -speed_0 * std::cos(alpha_0) * std::cos(phi_0); //Eqn 14
    x.v = -speed_0 * std::sin(alpha_0) * std::cos(phi_0); //Eqn 15
    x.w = -speed_0 * std::sin(phi_0); //Eqn 16

    //If u, v, or w are NAN, go ahead and NAN out all the parameters
    // including temperature which does depending quite heavily on u, v, & w
    if (isnan(x.u) || isnan(x.v) || isnan(x.w)) {
        x.u = NAN;
        x.v = NAN;
        x.w = NAN;
        x.t = NAN;
    }
  }
  
  /*Apply3DCorrect applies Metek corrections to a single sample.  It corrects teh values in place*/
  void Apply3DCorrect(float uvwtd[5]) {
    metek::uvwt samp;
    samp.u = uvwtd[0]; samp.v = uvwtd[1]; samp.w = uvwtd[2]; samp.t = uvwtd[3];
    Apply3DCorrect(samp);
    uvwtd[0] = samp.u; uvwtd[1] = samp.v; uvwtd[2] = samp.w; uvwtd[3] = samp.t;
  }
/*CalcCorrection returns a correction value utilizing tables given in Risø-R-1659(EN).
  fourierCoeffs is a double[6] array which is composed of the following values:
    double fourierCoeffs[6] = {
      std::cos(3*alpha),
      std::sin(3*alpha),
      std::cos(6*alpha),
      std::sin(6*alpha),
      std::cos(9*alpha),
      std::sin(9*alpha),
    };
  where alpha is the wind direction, in radians.  This fourier array can be used for each
  LUT, and should be precomputed by the called and passed here.  phi is the tilt angle of
  the wind, in degrees, that is used to linearly extrapolated based on table values given in
  Risø-R-1659(EN).  The returned value is a extrapolated value that is not corrected for units -
  caller must correct the values in the table directly (eg table values must be in degrees where
  corrections applied must be in radians, etc).

  This used to returns a NAN if the incoming phi is greater than 45 degrees or less than 
  -50 degrees.  As part of the PERDIGAO data processing, there are large periods where phi
  is much higher or lower than -45  and 50.  In those case, this nails the correction to the
  upper or lower limit.
  */
  double CalcCorrection(double fourierCoeffs[6], double phi, const double lut[20][7]) {
    const double maxTiltAngle = 45 * Degrees2Rad; // 45 Degrees
    const double minTiltAngle = -50 * Degrees2Rad; // -50 Degrees
    const double radsPerStep = 5 * Degrees2Rad; //5 degrees per step
    //if (phi > maxTiltAngle || phi < minTiltAngle) return NAN; //old behaviour
    if (phi > maxTiltAngle) phi = maxTiltAngle; //!! 'horrizontal' Wind doing up!
    if (phi < minTiltAngle) phi = minTiltAngle; //!! ' likewise, down!
    
    /*
    index:   N                        N+1
             |<------radsPerStep------>|
    partial: 0                        1.0
    */
        int index = (int)((phi - minTiltAngle)/radsPerStep);
        double partial = (phi - minTiltAngle)/radsPerStep - index;
    /*It is a fourier expansion coefficients:  normally there would be 4 pairs, but since sin(0) === 0,
    The sin(alpha) angle term is dropped from the 0th term.  Likewise, the cos(0) is always one, meaning
    that we just return the coefficient. With this, we only have 7 terms in the table. */
    return                      (1 - partial) * lut[index][0] + partial * lut[index+1][0] +
          fourierCoeffs[0] * ( (1 - partial) * lut[index][1] + partial * lut[index+1][1]) +
          fourierCoeffs[1] * ( (1 - partial) * lut[index][2] + partial * lut[index+1][2]) +
          fourierCoeffs[2] * ( (1 - partial) * lut[index][3] + partial * lut[index+1][3]) +
          fourierCoeffs[3] * ( (1 - partial) * lut[index][4] + partial * lut[index+1][4]) +
          fourierCoeffs[4] * ( (1 - partial) * lut[index][5] + partial * lut[index+1][5]) +
          fourierCoeffs[5] * ( (1 - partial) * lut[index][6] + partial * lut[index+1][6]);
  }

}}}}; //namespace nidas; dynld; isff; metek;

