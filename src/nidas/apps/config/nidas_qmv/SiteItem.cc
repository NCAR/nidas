
#include "SiteItem.h"
#include "SensorItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;



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
     if (sSiteName == site->getName()) { 
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
cerr << " deleting device " << deleteDevice << "\n";

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
    for (DSMIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
      DSMConfig* dsm = di.next();
      cerr << "found DSM with name " << dsm->getName()  << "\n";
      if (sensor->getDeviceName() == deleteDevice) {
// Talk w/Gmac - how to remove a DSM from Project Tree?
         cerr << "  calling removeDSM() except there ain't no such function...\n";
         //site->removeDSM(dsm); // do not delete, leave that for ~DSMItem()
         break; 
         }
    }

  return true;
}
