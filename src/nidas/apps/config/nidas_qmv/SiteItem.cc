
#include "SiteItem.h"
#include "SensorItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;



SiteItem::SiteItem(Site *site, int row, NidasModel *theModel, NidasItem *parent)
{
    _site = site;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

SiteItem::~SiteItem()
{
std::cerr << "call to ~SiteItem() \n";
try {
NidasItem *parentItem = this->parent();
if (parentItem)
    parentItem->removeChild(this);
delete this->getSite();
} catch (...) {
    // ugh!?!
    cerr << "Caught Exception in ~SiteItem() \n";
}
}

NidasItem * SiteItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    int j;

    //Site *site = reinterpret_cast<Site*>(this->nidasObject);
    DSMConfigIterator it;
    for (j=0, it = _site->getDSMConfigIterator(); it.hasNext(); j++) {
        DSMConfig * dsm = (DSMConfig*)(it.next()); // XXX cast from const
        if (j<i) continue; // skip old cached items (after it.next())
        NidasItem *childItem = new DSMItem(dsm, j, model, this);
        childItems.append( childItem);
    }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

QString SiteItem::dataField(int column)
{
  if (column == 0) return name();

  return QString();
}

/// find the DOM node which defines this Site
DOMNode *SiteItem::findDOMNode() const
{
Site *site = getSite();
if (site == NULL) return(0);
DOMDocument *domdoc = model->getDOMDocument();
if (!domdoc) return(0);

  DOMNodeList * SiteNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("site"));
  // XXX also check "aircraft"

  DOMNode * SiteNode = 0;
  for (XMLSize_t i = 0; i < SiteNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)SiteNodes->item(i));
     const string& sSiteName = xnode.getAttributeValue("name");
     if (sSiteName == _site->getName()) { 
       cerr<<"getSiteNode - Found SiteNode with name:" << sSiteName << endl;
       SiteNode = SiteNodes->item(i);
       break;
     }
  }

return(SiteNode);
}



/*!
 * \brief remove the DSM \a item from this Site's Nidas and DOM trees
 *
 * current implementation confused between returning bool and throwing exceptions
 * due to refactoring from Document
 *
 */
bool SiteItem::removeChild(NidasItem *item)
{
cerr << "SiteItem::removeChild\n";
DSMConfig *dsmConfig = dynamic_cast<DSMConfig*>(item);
string deleteDSM = dsmConfig->getName();
// Probably need dsmId too to be sure
cerr << " deleting DSM " << deleteDSM << "\n";

  Site *site = this->getSite();
  if (!site)
    throw InternalProcessingException("null site");

    // get the DOM node for this Site
  xercesc::DOMNode *siteNode = this->getDOMNode();
  if (!siteNode) {
    throw InternalProcessingException("null site DOM node");
  }

    // delete the matching DSM Node
  xercesc::DOMNode* child;
  xercesc::DOMNodeList* siteChildren = siteNode->getChildNodes();
  XMLSize_t numChildren, index;
  numChildren = siteChildren->getLength();
  for (index = 0; index < numChildren; index++ )
  {
      if (!(child = siteChildren->item(index))) continue;
      if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
      nidas::core::XDOMElement xchild((xercesc::DOMElement*) child);

      const string& elname = xchild.getNodeName();
      if (elname == "dsm")
      {

        const string & dsmName = xchild.getAttributeValue("name");
        cerr << "found node with name " << elname  << " and DSM name " << dsmName << "\n";

          if (dsmName == deleteDSM) 
          {
             xercesc::DOMNode* removableChld = siteNode->removeChild(child);
             removableChld->release();
          }
      }
  }

    // delete dsm from nidas model (Project tree)
    for (DSMConfigIterator di = _site->getDSMConfigIterator(); di.hasNext(); ) {
      DSMConfig* dsm = const_cast <DSMConfig*> (di.next());
      cerr << "found DSM with name " << dsm->getName()  << "\n";
      if (dsm->getName() == deleteDSM) {
// Talk w/Gmac - how to remove a DSM from Project Tree?
         cerr << "  calling removeDSM() except there ain't no such function...\n";
         //_site->removeDSM(dsm); // do not delete, leave that for ~DSMItem()
         break; 
         }
    }

  return true;
}

QString SiteItem::name()
{
    //Site *site = reinterpret_cast<Site*>(this->nidasObject);
    const Project *project = _site->getProject();
    std::string siteTabLabel = project->getName();
    if (project->getSystemName() != _site->getName())
        siteTabLabel += "/" + project->getSystemName();
    siteTabLabel += ": " + _site->getName();
    return(QString::fromStdString(siteTabLabel));
}
