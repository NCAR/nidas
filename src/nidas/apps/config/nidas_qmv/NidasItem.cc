
#include "NidasItem.h"
#include "DSMItem.h"

#include <iostream>
#include <fstream>


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

NidasItem::~NidasItem()
{
clearChildItems();
// do not delete nidasObject; leave it in the Nidas tree for ~Project()
}

void NidasItem::clearChildItems()
{
    while (!childItems.isEmpty())
         delete childItems.takeFirst();
}

NidasItem *NidasItem::parent()
{
    return parentItem;
}


/* maybe try:
 * Project * NidasItem::operator static_cast<Project*>()
 *    { return static_cast<Project*>this->nidasObject; }
 * so we can: Project *project = (Project*)this;
 */

NidasItem *NidasItem::child(int i)
{
std::cerr << "NidasItem::child(" << i << ") with size " << childItems.size() << " of type " << nidasType << "\n";

    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    /*
     * when we don't have row/child i then build all of the cached childItems
     *  we expect (at least 1st time) for all children/rows to be requested in sequence
     *  so building/caching all is worth it
     *
     * based on QT4 examples/itemviews/simpledommodel/domitem.cpp
     *  domitem builds only the new item requested and adds it to childItems
     * XXX figure out the short-circuit return above esp re deleted rows/children
     *     and i>childItems.size()
     */

  clearChildItems();

  int j;
  switch(this->nidasType){

  case PROJECT:
    {
    Project *project = reinterpret_cast<Project*>(this->nidasObject);
    SiteIterator it;
    for (j=0, it = project->getSiteIterator(); it.hasNext(); j++) {
        Site* site = it.next();
        NidasItem *childItem = new NidasItem(site, j, model, this);
        childItems.append( childItem);
        }
    break;
    }

  case SITE:
    {
    Site *site = reinterpret_cast<Site*>(this->nidasObject);
    DSMConfigIterator it;
    for (j=0, it = site->getDSMConfigIterator(); it.hasNext(); j++) {

            // XXX *** XXX (also in configwindow.cc)
            // very bad casting of const to non-const to get a mutable pointer to our dsm
            // *** NEEDS TO BE FIXED either here or in nidas::core
            //
        DSMConfig * dsm = (DSMConfig*)(it.next());

        NidasItem *childItem = new DSMItem(dsm, j, model, this);
        childItems.append( childItem);
        }
    break;
    }

  case DSMCONFIG:
    {
    DSMConfig *dsm = reinterpret_cast<DSMConfig*>(this->nidasObject);
    SensorIterator it;
    for (j=0, it = dsm->getSensorIterator(); it.hasNext(); j++) {
        DSMSensor* sensor = it.next();
        NidasItem *childItem = new NidasItem(sensor, j, model, this);
        childItems.append( childItem);
        }
    break;
    }

  case SENSOR:
    {
    DSMSensor *sensor = reinterpret_cast<DSMSensor*>(this->nidasObject);
    SampleTagIterator it;
    for (j=0, it = sensor->getSampleTagIterator(); it.hasNext(); j++) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        NidasItem *childItem = new NidasItem(sample, j, model, this);
        childItems.append( childItem);
        }
    break;
    }

  case SAMPLE:
    {
    SampleTag *sampleTag = reinterpret_cast<SampleTag*>(this->nidasObject);
    VariableIterator it = sampleTag->getVariableIterator();
    for (j=0; it.hasNext(); j++) {
        Variable* var = (Variable*)it.next(); // XXX cast from const
        NidasItem *childItem = new NidasItem(var, j, model, this);
        childItems.append( childItem);
        }
    break;
    }

  default:
    return 0;
  }

    // we tried to build childItems but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
if ((i<0) || (i>=childItems.size())) return 0;

    // we built childItems, return child i from it
return childItems[i];
}

int NidasItem::row() const
{
    return rowNumber;
}

int NidasItem::childCount()
{
if (int i=childItems.size()) return(i); // childItems has children, return how many
if (child(0)) // force a buildout of childItems
 return(childItems.size()); // and then return how many
return(0);
}

QString NidasItem::name()
{
  switch(this->nidasType){

  case PROJECT:
    {
    Project *project = reinterpret_cast<Project*>(this->nidasObject);
    return(QString::fromStdString(project->getName()));
    }

  case SITE:
    {
    Site *site = reinterpret_cast<Site*>(this->nidasObject);
    const Project *project = site->getProject();
    std::string siteTabLabel = project->getName();
    if (project->getSystemName() != site->getName())
        siteTabLabel += "/" + project->getSystemName();
    siteTabLabel += ": " + site->getName();
    return(QString::fromStdString(siteTabLabel));
    }

  case DSMCONFIG:
    {
    DSMConfig *dsm = reinterpret_cast<DSMConfig*>(this->nidasObject);
    return(QString::fromStdString(dsm->getLocation()));
    }

  case SENSOR:
    {
    DSMSensor *sensor = reinterpret_cast<DSMSensor*>(this->nidasObject);
    if (sensor->getCatalogName().length() > 0)
        return(QString::fromStdString(sensor->getCatalogName()+sensor->getSuffix()));
    else return(QString::fromStdString(sensor->getClassName()+sensor->getSuffix()));
    }

  case SAMPLE:
    {
    SampleTag *sampleTag = reinterpret_cast<SampleTag*>(this->nidasObject);
    return QString("Sample %1").arg(sampleTag->getSampleId());
    }

  case VARIABLE:
    {
    Variable *var = reinterpret_cast<Variable*>(this->nidasObject);
    return QString::fromStdString(var->getName());
    }

  default:
    return QString();
  }

return QString();
}

QString NidasItem::value()
{
return QString("value");
}

QString NidasItem::dataField(int column)
{
if (column == 0) return name();

if (this->nidasType == SENSOR) {
    DSMSensor *sensor = reinterpret_cast<DSMSensor*>(this->nidasObject);
    switch (column) {
      case 1:
        return QString::fromStdString(sensor->getDeviceName());
      case 2:
        return QString::fromStdString(getSerialNumberString(sensor));
      case 3:
        return QString("(%1,%2)").arg(sensor->getDSMId()).arg(sensor->getSensorId());
      /* default: fall thru */
    }
  }

return QString();
}

int NidasItem::childColumnCount() const
{
if (this->nidasType == DSMCONFIG) return(4);
return(1);
}

std::string NidasItem::getSerialNumberString(DSMSensor *sensor)
// maybe move this to a helper class
{
    const Parameter * parm = sensor->getParameter("SerialNumber");
    if (parm) 
        return parm->getStringValue(0);

    CalFile *cf = sensor->getCalFile();
    if (cf)
        return cf->getFile().substr(0,cf->getFile().find(".dat"));

return(std::string());
}



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


const QVariant & NidasItem::childLabel(int column) const
{
  switch(this->nidasType){

  case PROJECT:
    return _Site_Label;

  case SITE:
    return _DSM_Label;

  case DSMCONFIG:
    {
    switch (column) {
      case 0:
        return _Sensor_Label;
      case 1:
        return _Device_Label;
      case 2:
        return _SN_Label;
      case 3:
        return _ID_Label;
      /* default: fall thru */
      }
    }

  case SENSOR:
    return _Sample_Label;

  case SAMPLE:
    return _Variable_Label;

  case VARIABLE:
    return _Name_Label;

  /* default: fall thru */
  } // end switch

return _Name_Label;
}

bool NidasItem::removeChildren(int first, int last)
{
for (; first <= last; last--)
  delete childItems.takeAt(first);
for (; first < childItems.size(); first++)
  childItems[first]->rowNumber = first;
return true;
}
