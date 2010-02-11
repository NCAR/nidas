
#ifndef _NIDAS_MODEL_H
#define _NIDAS_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <nidas/core/Project.h>
class NidasItem;
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


    bool insertRows(int row, int count, const QModelIndex &parent);
    bool removeRows(int row, int count, const QModelIndex &parent);

    xercesc::DOMDocument *getDOMDocument() const { return domDoc; }

    void setCurrentRootIndex(const QModelIndex &index)
    {
      _currentRootIndex = index;
    }

    NidasItem *getCurrentRootItem() { return getItem(_currentRootIndex); }

    NidasItem *getItem(const QModelIndex &index) const;

protected:

    QModelIndex findIndex(void *nidasData, NidasItem *startItem=0) const;

private:
    NidasItem *rootItem;
    xercesc::DOMDocument *domDoc;

    QPersistentModelIndex  _currentRootIndex;
};

#endif
