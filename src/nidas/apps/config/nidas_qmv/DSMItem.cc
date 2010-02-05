
#include "DSMItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

using namespace xercesc;
using namespace std;

DOMNode *DSMItem::findDOMNode() const
// find the DOM node which defines this DSM
{
DSMConfig *dsmConfig = getDSMConfig();
if (dsmConfig == NULL) return(0);
DOMDocument *domdoc = model->getDOMDocument();
if (!domdoc) return(0);

  DOMNodeList * SiteNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("site"));
  // XXX also check "aircraft"

  DOMNode * SiteNode = 0;
  for (XMLSize_t i = 0; i < SiteNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)SiteNodes->item(i));
     const std::string& sSiteName = xnode.getAttributeValue("name");
     if (sSiteName == dsmConfig->getSite()->getName()) { 
       cerr<<"getSiteNode - Found SiteNode with name:" << sSiteName << endl;
       SiteNode = SiteNodes->item(i);
       break;
     }
  }


  DOMNodeList * DSMNodes = SiteNode->getChildNodes();

  DOMNode * DSMNode = 0;
  int dsmId = dsmConfig->getId();

  for (XMLSize_t i = 0; i < DSMNodes->getLength(); i++) 
  {
     DOMNode * siteChild = DSMNodes->item(i);
     if ((std::string)XMLStringConverter(siteChild->getNodeName()) != std::string("dsm")) continue;

     XDOMElement xnode((DOMElement *)DSMNodes->item(i));
     const std::string& sDSMId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sDSMId.c_str()) == dsmId) { 
       cerr<<"getDSMNode - Found DSMNode with id:" << sDSMId << endl;
       DSMNode = DSMNodes->item(i);
       break;
     }
  }

return(DSMNode);
}
