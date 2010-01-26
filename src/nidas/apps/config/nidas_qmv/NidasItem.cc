
#include "NidasItem.h"

NidasItem::NidasItem(Project *project, int row, NidasItem *parent)
{
    nidasObject = (void*)project;
    nidasType = PROJECT;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::NidasItem(Site *site, int row, NidasItem *parent)
{
    nidasObject = (void*)site;
    nidasType = SITE;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::NidasItem(DSMConfig *dsm, int row, NidasItem *parent)
{
    nidasObject = (void*)dsm;
    nidasType = DSMCONFIG;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::NidasItem(DSMSensor *sensor, int row, NidasItem *parent)
{
    nidasObject = (void*)sensor;
    nidasType = SENSOR;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::NidasItem(SampleTag *sampleTag, int row, NidasItem *parent)
{
    nidasObject = (void*)sampleTag;
    nidasType = SAMPLE;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::NidasItem(Variable *variable, int row, NidasItem *parent)
{
    nidasObject = (void*)variable;
    nidasType = VARIABLE;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}

NidasItem::~NidasItem()
{
    QHash<int,NidasItem*>::iterator it;
    for (it = childItems.begin(); it != childItems.end(); ++it)
        delete it.value();
    // do not delete nidasObject; leave it in the Nidas tree for ~Project()
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
    if (childItems.contains(i))
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

  int j;
  switch(this->nidasType){

  case PROJECT:
    {
    Project *project = (Project*)this->nidasObject;
    SiteIterator it;
    for (j=0, it = project->getSiteIterator(); it.hasNext(); j++) {
        Site* site = it.next();
        NidasItem *childItem = new NidasItem(site, j, this);
        childItems[j] = childItem;
        }
    break;
    }

  case SITE:
    {
    Site *site = (Site*)this->nidasObject;
    DSMConfigIterator it;
    for (j=0, it = site->getDSMConfigIterator(); it.hasNext(); j++) {

            // XXX *** XXX (also in configwindow.cc)
            // very bad casting of const to non-const to get a mutable pointer to our dsm
            // *** NEEDS TO BE FIXED either here or in nidas::core
            //
        DSMConfig * dsm = (DSMConfig*)(it.next());

        NidasItem *childItem = new NidasItem(dsm, j, this);
        childItems[j] = childItem;
        }
    break;
    }

  case DSMCONFIG:
    {
    DSMConfig *dsm = (DSMConfig*)this->nidasObject;
    SensorIterator it;
    for (j=0, it = dsm->getSensorIterator(); it.hasNext(); j++) {
        DSMSensor* sensor = it.next();
        NidasItem *childItem = new NidasItem(sensor, j, this);
        childItems[j] = childItem;
        }
    break;
    }

  case SENSOR:
    {
    DSMSensor *sensor = (DSMSensor*)this->nidasObject;
    SampleTagIterator it;
    for (j=0, it = sensor->getSampleTagIterator(); it.hasNext(); j++) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        NidasItem *childItem = new NidasItem(sample, j, this);
        childItems[j] = childItem;
        }
    break;
    }

  case SAMPLE:
    {
    SampleTag *sampleTag = (SampleTag*)this->nidasObject;
    VariableIterator it = sampleTag->getVariableIterator();
    for (j=0; it.hasNext(); j++) {
        Variable* var = (Variable*)it.next(); // XXX cast from const
        NidasItem *childItem = new NidasItem(var, j, this);
        childItems[j] = childItem;
        }
    break;
    }

  default:
    return 0;
  }

  return childItems[i];
}

int NidasItem::row() const
{
    return rowNumber;
}

int NidasItem::childCount()
{
if (int i=childItems.count()) return(i); // childItems has children, return how many
if (child(0)) // force a buildout of childItems
 return(childItems.count()); // and then return how many
return(0);
}

QString NidasItem::name()
{
  switch(this->nidasType){

  case PROJECT:
    {
    Project *project = (Project*)this->nidasObject;
    return(QString::fromStdString(project->getName()));
    }

  case SITE:
    {
    Site *site = (Site*)this->nidasObject;
    const Project *project = site->getProject();
    std::string siteTabLabel = project->getName();
    if (project->getSystemName() != site->getName())
        siteTabLabel += "/" + project->getSystemName();
    siteTabLabel += ": " + site->getName();
    return(QString::fromStdString(siteTabLabel));
    }

  case DSMCONFIG:
    {
    DSMConfig *dsm = (DSMConfig*)this->nidasObject;
    return(QString::fromStdString(dsm->getLocation()));
    }

  case SENSOR:
    {
    DSMSensor *sensor = (DSMSensor*)this->nidasObject;
    if (sensor->getCatalogName().length() > 0)
        return(QString::fromStdString(sensor->getCatalogName()+sensor->getSuffix()));
    else return(QString::fromStdString(sensor->getClassName()+sensor->getSuffix()));
    }

  case SAMPLE:
    {
    SampleTag *sampleTag = (SampleTag*)this->nidasObject;
    return QString("Sample %1").arg(sampleTag->getSampleId());
    }

  case VARIABLE:
    {
    Variable *var = (Variable*)this->nidasObject;
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

