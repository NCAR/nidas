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

#ifndef _DSM_ITEM_H
#define _DSM_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMConfig.h>
#include <xercesc/dom/DOMNode.hpp>

#include <iostream>
#include <fstream>

using namespace nidas::core;


class DSMItem : public NidasItem
{

public:
    DSMItem(DSMConfig *dsm, int row, NidasModel *theModel, NidasItem *parent = 0); 

    ~DSMItem();

    NidasItem * child(int i);

    void fromDOM();

    bool removeChild(NidasItem *item);

    QString dataField(int column);
    const QVariant & childLabel(int column) const;

    int childColumnCount() const {return 6;}
//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    DSMConfig *getDSMConfig() const { return _dsm; }

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

    // Seems we don't know how to use the operator here
    //operator DSMConfig*() const { if (nidasType == DSMCONFIG)  return reinterpret_cast<DSMConfig*>(this->nidasObject); else return 0;}
    //operator xercesc::DOMNode*() { if (domNode) return domNode; else return domNode=findDOMNode(); }

protected:
    xercesc::DOMNode *findDOMNode(); 
    QString name();

private:
    DSMConfig * _dsm;
};

#endif
