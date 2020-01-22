

#include "WindTilter.h"

#include <nidas/core/PhysConstants.h>

using namespace nidas::dynld::isff;
using namespace std;


WindTilter::WindTilter(): _lean(0.0),_leanaz(0.0),_identity(true),
	UP_IS_SONIC_W(false)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
	    _mat[i][j] = (i == j ? 1.0 : 0.0);
}

void WindTilter::rotate(float* up, float* vp, float* wp) const
{

    if (_identity) return;

    float vin[3] = {*up,*vp,*wp};
    double out[3];

    for (int i = 0; i < 3; i++) {
	out[i] = 0.0;
	for (int j = 0; j < 3; j++)
	    out[i] += _mat[i][j] * vin[j];
    }
    *up = (float) out[0];
    *vp = (float) out[1];
    *wp = (float) out[2];
}

void WindTilter::computeMatrix()
{
    double sinlean,coslean,sinaz,cosaz;
    double mag;

    _identity = fabs(_lean) < 1.e-5;

    sinlean = ::sin(_lean);
    coslean = ::cos(_lean);
    sinaz = ::sin(_leanaz);
    cosaz = ::cos(_leanaz);

    /*
     *This is Wf, the flow W axis in the sonic UVW system.
     */
    _mat[2][0] = sinlean * cosaz;
    _mat[2][1] = sinlean * sinaz;
    _mat[2][2] = coslean;


    if (UP_IS_SONIC_W) {

      /* Uf is cross product of Vs (sonic V axis = 0,1,0) with Wf */
      mag = ::sqrt(coslean*coslean + sinlean*sinlean*cosaz*cosaz);

      _mat[0][0] = coslean / mag;
      _mat[0][1] = 0.0f;
      _mat[0][2] = -sinlean * cosaz / mag;
    }
    else {
      {
	double WfXUs[3];
        /* cross product of Wf and Us */
	WfXUs[0] = 0.0f;
	WfXUs[1] = coslean;
	WfXUs[2] = -sinlean * sinaz;

        /* Uf is cross of above with Wf */
	_mat[0][0] = WfXUs[1] * _mat[2][2] - WfXUs[2] * _mat[2][1];
	_mat[0][1] = WfXUs[2] * _mat[2][0] - WfXUs[0] * _mat[2][2];
	_mat[0][2] = WfXUs[0] * _mat[2][1] - WfXUs[1] * _mat[2][0];

	mag = ::sqrt(_mat[0][0]*_mat[0][0] + _mat[0][1]*_mat[0][1] + _mat[0][2]*_mat[0][2]);
	_mat[0][0] /= mag;
	_mat[0][1] /= mag;
	_mat[0][2] /= mag;
      }
    }

    /*  Vf = Wf cross Uf. */
    _mat[1][0] = _mat[2][1] * _mat[0][2] - _mat[2][2] * _mat[0][1];
    _mat[1][1] = _mat[2][2] * _mat[0][0] - _mat[2][0] * _mat[0][2];
    _mat[1][2] = _mat[2][0] * _mat[0][1] - _mat[2][1] * _mat[0][0];
}

