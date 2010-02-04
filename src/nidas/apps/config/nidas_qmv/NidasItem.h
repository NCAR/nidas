
#ifndef _NIDAS_ITEM_H
#define _NIDAS_ITEM_H

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>

using namespace nidas::core;

#include <QHash>
#include <QVariant>


class NidasItem
{
public:
    NidasItem(Project *project, int row, NidasItem *parent = 0);
    NidasItem(Site *site, int row, NidasItem *parent = 0);
    NidasItem(DSMConfig *dsm, int row, NidasItem *parent = 0);
    NidasItem(DSMSensor *sensor, int row, NidasItem *parent = 0);
    NidasItem(SampleTag *sampleTag, int row, NidasItem *parent = 0);
    NidasItem(Variable *variable, int row, NidasItem *parent = 0);

    ~NidasItem();
    NidasItem *child(int i);
    NidasItem *parent();

    int row() const;
    int childCount();
    int childColumnCount() const;

    const QVariant & childLabel(int column) const;

    QString dataField(int column);

    bool pointsTo(void *vp) const { return nidasObject == vp; }
    DOMNode *getDOMNode()=0;

protected:
    QString name();
    QString value();
    std::string getSerialNumberString(DSMSensor *sensor);

private:
    void *nidasObject;
    enum { PROJECT, SITE, DSMCONFIG, SENSOR, SAMPLE, VARIABLE } nidasType;

    DOMNode *domNode;

    QHash<int,NidasItem*> childItems;
    NidasItem *parentItem;
    int rowNumber;

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

};

#endif
