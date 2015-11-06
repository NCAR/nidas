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

#ifndef _NIDAS_MODEL_H
#define _NIDAS_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <nidas/core/Project.h>
class NidasItem;
class ProjectItem;
#include <xercesc/dom/DOMDocument.hpp>


class NidasModel : public QAbstractItemModel
{
    Q_OBJECT

    friend class NidasItem;

public:
    NidasModel(nidas::core::Project *project, xercesc::DOMDocument *doc, /*QModelIndex & index = QModelIndex(),*/ QObject *parent = 0);
    ~NidasModel();

    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const
       { return headerData(section, orientation, role, _currentRootIndex); }; 

    QVariant headerData(int section, Qt::Orientation orientation, int role,
        const QModelIndex &parent) const;


    bool appendChild(NidasItem *parentItem);
    bool insertRows(int row, int count, const QModelIndex &parent);

    bool removeIndexes(QModelIndexList indexList);
    bool removeChildren(std::list <int> & selectedRows, NidasItem *parentItem);
    bool removeRows(int row, int count, const QModelIndex &parent);

    xercesc::DOMDocument *getDOMDocument() const { return domDoc; }

    void setCurrentRootIndex(const QModelIndex &index)
    {
      _currentRootIndex = index;
    }

    NidasItem *getCurrentRootItem() { return getItem(_currentRootIndex); }

    NidasItem *getItem(const QModelIndex &index) const;

    NidasItem *getRootItem() const { return rootItem; };

protected:

    //QModelIndex findIndex(void *nidasData, NidasItem *startItem=0) const;

private:
    NidasItem *rootItem;
    xercesc::DOMDocument *domDoc;

    QPersistentModelIndex  _currentRootIndex;
};

#endif
