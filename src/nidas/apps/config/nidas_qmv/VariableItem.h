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

#ifndef _VARIABLE_ITEM_H
#define _VARIABLE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/Variable.h>
#include "SensorItem.h"

using namespace nidas::core;


class VariableItem : public NidasItem
{

public:
    VariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~VariableItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string variableName() { return this->dataField(1).toStdString(); }

    const QVariant & childLabel(int column) const { 
      return NidasItem::_Name_Label; 
    }
    int childColumnCount() const {return 1;}

    QString dataField(int column);

    QString name();
    QString getLongName() 
            { return QString::fromStdString(_variable->getLongName()); }
    std::string getBaseName();

    Variable *getVariable() const { return _variable; }

    //SampleTag *getSampleTag() { return _sampleTag; }
    SampleTag *getSampleTag() { 
      return const_cast<SampleTag*>(_variable->getSampleTag()); 
    }
    std::vector<std::string> getCalibrationInfo();
    xercesc::DOMNode* getSampleDOMNode() {
        if (_sampleDOMNode)
          return _sampleDOMNode;
        else return _sampleDOMNode=findSampleDOMNode();
        }
    float getRate() { return _sampleTag->getRate(); }
    unsigned int getSampleId() {
      std::cerr<< "in VarItem getSampleID\n";return _sampleID;
    }

    xercesc::DOMNode* getVariableDOMNode(QString name) {
        return _variableDOMNode=findVariableDOMNode(name);
    }

    void clearVarItem();

    std::string sSampleId() { return this->dataField(1).toStdString(); }

    void setDOMName(QString fromName, std::string toName);

    void fromDOM();

    SensorItem * getSensorItem() {return dynamic_cast<SensorItem*>(parent());}

    QString getCalValues();
    QString getCalSrc();
    QString getCalDate();

protected:
        // get/convert to the underlying model pointers
    xercesc::DOMNode *findSampleDOMNode();
    xercesc::DOMNode *findVariableDOMNode(QString name);
    Variable * _variable;
    SampleTag * _sampleTag;
   

private:
    xercesc::DOMNode * _sampleDOMNode;
    xercesc::DOMNode * _variableDOMNode;
    unsigned int _sampleID;

    VariableConverter * _varConverter;
    CalFile * _calFile;
    std::string _calFileName;
    bool _gotCalDate, _gotCalVals;
    std::string _calDate, _calVals;
};

#endif
