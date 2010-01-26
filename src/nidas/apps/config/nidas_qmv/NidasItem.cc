
#include "NidasItem.h"

NidasItem::NidasItem(Project *project, int row, NidasItem *parent)
{
    nidasObject = (void*)project;
    nidasType = PROJECT;
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

  switch(this->nidasType){

  case PROJECT:
    Project *project = (Project*)this;
    for (int j=0, SiteIterator it = getSiteIterator(); it.hasNext(); j++) {
        Site* site = it.next();
        NidasItem *childItem = new NidasItem(site, j, this);
        childItems[j] = childItem;
        }
    }
    break;

  case SITE:
    Site *site = (Site*)this;
    for (int j=0, DSMConfigIterator it = getDSMConfigIterator(); it.hasNext(); j++) {
        DSMConfig* dsm = it.next();
        NidasItem *childItem = new NidasItem(dsm, j, this);
        childItems[j] = childItem;
        }
    }
    break;

  default:
    return 0;
  }

  return childItems[i];
}

int NidasItem::row()
{
    return rowNumber;
}
