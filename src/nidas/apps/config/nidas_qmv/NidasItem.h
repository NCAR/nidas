
#ifndef _NIDAS_ITEM_H
#define _NIDAS_ITEM_H

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>

#include "NidasModel.h"
#include <xercesc/dom/DOMNode.hpp>


using namespace nidas::core;

#include <QVariant>

class NidasItem
{
public:

    NidasItem(Project *project, int row, NidasModel *model, NidasItem *parent = 0);
    NidasItem(Site *site, int row, NidasModel *model, NidasItem *parent = 0);
    NidasItem(DSMConfig *dsm, int row, NidasModel *model, NidasItem *parent = 0);
    NidasItem(DSMSensor *sensor, int row, NidasModel *model, NidasItem *parent = 0);
    NidasItem(SampleTag *sampleTag, int row, NidasModel *model, NidasItem *parent = 0);
    NidasItem(Variable *variable, int row, NidasModel *model, NidasItem *parent = 0);

    ~NidasItem();

    NidasItem *child(int i);
    NidasItem *parent();

    virtual int row() const;
    virtual int childCount();
    virtual int childColumnCount() const;

    bool removeChildren(int first, int last);

    const QVariant & childLabel(int column) const;

    QString dataField(int column);

        /// for debugging only
    ///int getNidasType() const { return nidasType; }

    bool pointsTo(void *vp) const { return nidasObject == vp; }

    /*!
     *
     * Asks the model to create an index for this item.
     * Requires us to be a friend of NidasModel and for the model
     * to ask NidasItem for the parent item.
     *
     * \return a QModelIndex for this NidasItem
     */
    QModelIndex createIndex() { return model->createIndex(rowNumber,0,this); }
    /*
     * recursive implementation:
        if (parentItem)
            return model->index(rowNumber,0,parentItem->createIndex())
        else return QModelIndex()
     *
     * or cache the index as a QPersistentModelIndex and rebuild all
     *  childItems indexes when rowNumbers change (insert/remove)
     */

protected:
    QString name();
    QString value();
    std::string getSerialNumberString(DSMSensor *sensor);

    //virtual xercesc::DOMNode *findDOMNode()=0;
    //virtual xercesc::DOMNode *findDOMNode() { return 0; }

        // pointers to actual models
    enum { PROJECT, SITE, DSMCONFIG, SENSOR, SAMPLE, VARIABLE } nidasType;
    void *nidasObject;
    xercesc::DOMNode *domNode;

        // tree navigation pointers
    QList<NidasItem*> childItems;
    NidasItem *parentItem;
    int rowNumber;

        // the Qt Model that owns/"controls" the items
    NidasModel *model; 

    static const QVariant _Project_Label;
    static const QVariant _Site_Label;
    static const QVariant _DSM_Label;
     static const QVariant _Device_Label;
     static const QVariant _SN_Label;
     static const QVariant _ID_Label;
    static const QVariant _Sensor_Label;
    static const QVariant _Sample_Label;
    static const QVariant _Variable_Label;
    static const QVariant _Name_Label;

    void clearChildItems();
    friend class NidasModel;

private:
    /// don't let anybody create a default/empty object, we always want a good nidasObject
    /// maybe add throw InvalidConstructorException for libraries' templated code?
    NidasItem() { };

};

#endif
