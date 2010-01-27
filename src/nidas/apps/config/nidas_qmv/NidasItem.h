
#ifndef _NIDAS_ITEM_H
#define _NIDAS_ITEM_H

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>

using namespace nidas::core;

#include <QHash>

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

    QString dataField(int column);

protected:
    QString name();
    QString value();
    std::string getSerialNumberString(DSMSensor *sensor);

private:
    void *nidasObject;
    enum { PROJECT, SITE, DSMCONFIG, SENSOR, SAMPLE, VARIABLE } nidasType;

    QHash<int,NidasItem*> childItems;
    NidasItem *parentItem;
    int rowNumber;
};

#endif
