
#include "NidasItem.h"
#include "ProjectItem.h"
#include "SiteItem.h"
#include "DSMItem.h"
#include "SensorItem.h"
#include "SampleItem.h"
#include "VariableItem.h"

#include <iostream>
#include <fstream>

const QVariant NidasItem::_Project_Label(QString("Project"));
const QVariant NidasItem::_Site_Label(QString("Site"));
const QVariant NidasItem::_DSM_Label(QString("DSM"));
 const QVariant NidasItem::_Device_Label(QString("Device"));
 const QVariant NidasItem::_SN_Label(QString("S/N"));
 const QVariant NidasItem::_ID_Label(QString("ID"));
const QVariant NidasItem::_Sensor_Label(QString("Sensor"));
const QVariant NidasItem::_Sample_Label(QString("Sample"));
const QVariant NidasItem::_Variable_Label(QString("Variable"));
const QVariant NidasItem::_Name_Label(QString("Name"));

/*!
 * NidasItem is a proxy for the actual Nidas objects in the Project tree
 * and their corresponding DOMNodes. Qt indexing
 * is mapped to the Project tree via parent/child with row the ordinal number of the siblings
 * and columns used for data fields (attributes/properties) of each level of Nidas object.
 *
 * Pointers to Nidas objects are void* since Nidas uses different classes at every level.
 * NidasItem is (should/will be) subclassed for specialized Items corresponding
 * to each kind of Nidas object in the Project tree and implementing their own logic for the API
 * used by NidasModel (like the data and header returned for Qt columns).
 *
 * The NidasItem tree is a mirror/proxy of the Nidas object Project tree,
 * with children lazily initialized by child().
 * Row number, parent pointer, and a list of children are cached.
 *
 * N.B. can only add children to end of list due to Nidas API
 * and use of children list in child()
 *
 * NidasItem could also become a QObject to get the parent/children stuff.
 * This could be useful in future for findChild()/findChildren().
 * Except for the fact that it didn't work - something about Qt's cleanup
 * of children caused problems for us.
 *
 */

NidasItem::NidasItem(Project *project, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)project;
    nidasType = PROJECT;
    domNode = 0;

    // Record the item's location within its parent.
    rowNumber = row; 
    parentItem = parent;
    model = theModel;
}

NidasItem::NidasItem(Site *site, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)site;
    nidasType = SITE;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

NidasItem::NidasItem(DSMConfig *dsm, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)dsm;
    nidasType = DSMCONFIG;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

NidasItem::NidasItem(DSMSensor *sensor, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)sensor;
    nidasType = SENSOR;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

NidasItem::NidasItem(SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)sampleTag;
    nidasType = SAMPLE;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

NidasItem::NidasItem(Variable *variable, int row, NidasModel *theModel, NidasItem *parent)
{
    nidasObject = (void*)variable;
    nidasType = VARIABLE;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

/* maybe try:
 * Project * NidasItem::operator static_cast<Project*>()
 *    { return static_cast<Project*>this->nidasObject; }
 * so we can: Project *project = (Project*)this;
 */

int NidasItem::childCount()
{
    if (int i=childItems.size()) return(i); // children, return how many
    if (child(0)) // force a buildout of children
        return(childItems.size()); // and then return how many
    return(0);
}

bool NidasItem::removeChildren(int first, int last)
{
    // XXX check first/last within QList range and/or catch Qt's exception
    for (; first <= last; last--)
        delete childItems.takeAt(first);
    for (; first < childItems.size(); first++)
        childItems[first]->rowNumber = first;
    return true;
}
