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

#ifndef _PROJECT_ITEM_H
#define _PROJECT_ITEM_H

#include "NidasItem.h"
#include "SiteItem.h"
#include <nidas/core/Project.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class ProjectItem : public NidasItem
{

public:
    ProjectItem(Project *project, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~ProjectItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item);

    QString dataField(int column);

    const QVariant & childLabel(int column) const { return NidasItem::_Site_Label; }
    int childColumnCount() const {return 1;}

//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    Project *getProject() const { return _project; }

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

protected:
    xercesc::DOMNode *findDOMNode(); 
    QString name();

private:
    Project* _project;
};

#endif
