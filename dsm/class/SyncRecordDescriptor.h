/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

class SyncRecordDescripter
{
public:
    
    SyncRecordDescripter(DSMConfig* dsm);

    SyncRecordDescripter(Aircraft* aircraft);

    const std::vector<Variable*> getVariables() const;
    int getNumValues(int i) const;

    float getValue(int ivar, int ival);

    void setDataBuffer(const char* ptr);
    
    int getOffset(int i) const;
    int getStride(int i) const;

    /*
     * Format of sync record:
     * A2D sensors:
     *	  variables, fastest sampled first
     * e.g.: 50hz, variables A,B (order specified in xml)
     *       10hz, variables C,D,E
     *  A[0],B[0],A[1],B[1],...,A[49],B[49]
     *  C[0],D[0],E[0],...,C[9],D[9],E[9]
     *  calibration status byte?
     *
     * Arinc sensors:
     *     similar to A2D, fastest labels first
     * Serial sensors
     *
    const list<DSMSensor*> sensors = getDSMConfig()->getSensors();

    map<int,DSMSensor*> sensorMap;

    for (list<DSMSensor*>::const_iterator si = sensors.begin();
    	si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;
	const vector<const Variable*>& vars = sensor->getVariables();
	DSMSerialSensor* ssensor = dynamic_cast<DSMSerialSensor*>(sensor);
	if (ssensor)
	{
	    int nvars = vars.size();
	}
    }


