/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#ifndef _A2DVARIABLE_ITEM_H
#define _A2DVARIABLE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/Variable.h>
#include <nidas/core/SampleTag.h>
#include <iostream>
#include <vector>
#include <fstream>

using namespace nidas::core;


class A2DVariableItem : public NidasItem
{

public:
    A2DVariableItem(Variable *variable, SampleTag *sampleTag, int row,
                    NidasModel *theModel, NidasItem *parent = 0) ;

    ~A2DVariableItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string variableName() { return this->dataField(0).toStdString(); }
    std::string getVarNamePfx();
    std::string getVarNameSfx();

    const QVariant & childLabel(int column) const
                      { return NidasItem::_Name_Label; }
    int childColumnCount() const {return 1;}

    QString dataField(int column);

    QString name();
    SampleTag *getSampleTag() const { return _sampleTag; }
    xercesc::DOMNode* getSampleDOMNode() {
        if (_sampleDOMNode)
          return _sampleDOMNode;
        else return _sampleDOMNode=findSampleDOMNode();
    }

    std::string sSampleId() { return this->dataField(1).toStdString(); }

    int getA2DChannel() { return _variable->getA2dChannel(); }
    int getGain();
    int getBipolar();
    QString getLongName()
            { return QString::fromStdString(_variable->getLongName()); }
    float getRate() { return _sampleTag->getRate(); }
    std::vector<std::string> getCalibrationInfo();
    const std::string & getUnits() {return _variable->getUnits();}

    void setDOMName(QString fromName, std::string toName);

protected:
        // get/convert to the underlying model pointers
    Variable *getVariable() const { return _variable; }
    xercesc::DOMNode *findSampleDOMNode();
    xercesc::DOMNode *findVariableDOMNode(QString name);
    Variable * _variable;
    SampleTag * _sampleTag;


private:
    xercesc::DOMNode * _sampleDOMNode;
    xercesc::DOMNode * _variableDOMNode;
    bool _gotCalDate, _gotCalVals;
    std::string _calDate, _calVals;
};

#endif
