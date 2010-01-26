
#ifndef _NIDAS_ITEM_H
#define _NIDAS_ITEM_H

class NidasItem
{
public:
    NidasItem(Project *project, int row, NidasItem *parent = 0);
    NidasItem(Site *site, int row, NidasItem *parent = 0);
    NidasItem(DSMConfig *dsm, int row, NidasItem *parent = 0);
    NidasItem(Sensor *sensor, int row, NidasItem *parent = 0);
    NidasItem(Sample *sample, int row, NidasItem *parent = 0);
    NidasItem(Variable *variable, int row, NidasItem *parent = 0);

    ~NidasItem();
    NidasItem *child(int i);
    NidasItem *parent();
    int row();

private:
    void *nidasObject;
    enum { PROJECT, SITE, DSMCONFIG, SENSOR, SAMPLE, VARIABLE } nidasType;

    QHash<int,NidasItem*> childItems;
    NidasItem *parentItem;
    int rowNumber;
};

#endif
