
#ifndef _NIDAS_ITEM_H
#define _NIDAS_ITEM_H

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>

using namespace nidas::core;

#include <QHash>
#include <QVariant>
#include <QModelIndex>
#include <QHeaderView>


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

    QHeaderView *getChildHeaderView();

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

    class NidasHeaderModel : QAbstractItemModel {
      public:
        NidasHeaderModel(DSMConfig *dsm, QWidget *parent=0);
        int columnCount(const QModelIndex &parent = QModelIndex()) const;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const;
        QVariant data(const QModelIndex &index, int role) const { return QVariant(); }
        //Qt::ItemFlags flags(const QModelIndex &index) const;
        QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const { return QModelIndex(); }
        QModelIndex parent(const QModelIndex &child) const { return QModelIndex(); }
        int rowCount(const QModelIndex &parent = QModelIndex()) const { return 0; }
        QAbstractItemModel *getQAIM() { return reinterpret_cast<QAbstractItemModel*>(this); }
      private:
        NidasItem *dummyItem;
    };

    static NidasHeaderModel _DSM_HeaderModel;
    static QHeaderView _DSM_HeaderView;

};

#endif
