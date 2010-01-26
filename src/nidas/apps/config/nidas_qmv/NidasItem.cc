
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

  default:
    return 0;
  }

  return childItems[i];
}

int NidasItem::row() const
{
    return rowNumber;
}

int NidasItem::childCount() const
{
return 1;
}
