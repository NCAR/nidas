
#include "NidasItem.h"
#include "ProjectItem.h"
#include "SiteItem.h"
#include "DSMItem.h"
#include "SensorItem.h"
#include "VariableItem.h"

#include <iostream>
#include <fstream>

const QVariant NidasItem::_Project_Label(QString("Project"));
const QVariant NidasItem::_Site_Label(QString("Site"));
const QVariant NidasItem::_DSM_Label(QString("DSM Loc. [Name]"));
const QVariant NidasItem::_Suffix_Label(QString("Sufx"));
const QVariant NidasItem::_Device_Label(QString("Device"));
const QVariant NidasItem::_DevChan_Label(QString("DChan"));
const QVariant NidasItem::_SN_Label(QString("S/N"));
const QVariant NidasItem::_ID_Label(QString("ID"));
const QVariant NidasItem::_Sensor_Label(QString("Sensor"));
const QVariant NidasItem::_Sample_Label(QString("SampID"));
const QVariant NidasItem::_Rate_Label(QString("Rate"));
const QVariant NidasItem::_Volt_Label(QString("Vrange"));
const QVariant NidasItem::_Variable_Label(QString("Variable"));
const QVariant NidasItem::_CalCoef_Label(QString("Cal Coefs"));;
const QVariant NidasItem::_Name_Label(QString("Name"));
const QVariant NidasItem::_Channel_Label(QString("Chan"));

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

/* maybe try:
 * Project * NidasItem::operator static_cast<Project*>()
 *    { return static_cast<Project*>this->nidasObject; }
 * so we can: Project *project = (Project*)this;
 */

    //int NidasItem::row() const { std::cerr << "NidasItem::row("<<rowNumber<<"\n";return rowNumber; }

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
