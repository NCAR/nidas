
#include "ProjectItem.h"
#include "SensorItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;



/* Should never be called - we don't give access to the Project in the UI */
ProjectItem::~ProjectItem()
{
std::cerr << "call to ~ProjectItem() \n";
try {
      Project *project = reinterpret_cast<Project*>(this->nidasObject);
      project->destroyInstance();
} catch (...) {
    // ugh!?!
    cerr << "Caught Exception in ~Projectitem() \n";
}
}



/// find the DOM node which defines this Project
DOMNode *ProjectItem::findDOMNode() const
{
DOMDocument *domdoc = model->getDOMDocument();
if (!domdoc) return 0;

  DOMNodeList * ProjectNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("project"));

  DOMNode * ProjectNode = 0;
  for (XMLSize_t i = 0; i < ProjectNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)ProjectNodes->item(i));
     const string& sProjectName = xnode.getAttributeValue("name");
     //string projectName = (this->name().toStdString());
     Project *project = reinterpret_cast<Project*>(this->nidasObject);
     string projectName = project->getName();
     if (sProjectName == projectName) { 
       cerr<<"getProjectNode - Found ProjectNode with name:" << sProjectName << endl;
       ProjectNode = ProjectNodes->item(i);
       break;
     }
  }

  return ProjectNode;

}



/*!
 * \brief remove the site \a item from this Project's Nidas and DOM trees
 *
 * current implementation confused between returning bool and throwing exceptions
 * due to refactoring from Document
 *
 */
bool ProjectItem::removeChild(NidasItem *item)
{
cerr << "ProjectItem::removeChild\n";
Site *site = dynamic_cast<Site*>(item);
string deleteSite = site->getName();
cerr << " deleting site " << deleteSite << "\n";

  Project *project = this->getProject();
    // get the DOM node for this Project
  xercesc::DOMNode *projectNode = this->getDOMNode();
  if (!projectNode) {
    throw InternalProcessingException("null project DOM node");
  }

    // delete the site from this Projects DOM node
  xercesc::DOMNode* child;
  xercesc::DOMNodeList* projectChildren = projectNode->getChildNodes();
  XMLSize_t numChildren, index;
  numChildren = projectChildren->getLength();
  for (index = 0; index < numChildren; index++ )
  {
      if (!(child = projectChildren->item(index))) continue;
      if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
      nidas::core::XDOMElement xchild((xercesc::DOMElement*) child);

      const string& elname = xchild.getNodeName();
      if (elname == "site") 
      {
        const string & name = xchild.getAttributeValue("name");
        cerr << "found node with name " << elname  << " and name attribute " << name << endl;

          if (name == deleteSite) 
          {
             xercesc::DOMNode* removableChld = projectNode->removeChild(child);
             removableChld->release();
          }
      }
  }

/* Cannot delete a site from the project
    // delete site from nidas model (Project tree)
    for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
      Site* site = si.next();
      cerr << "found site with name " << site->getName() << "\n";
      if (site->getname() == deleteSite) {
         cerr << "  calling removeSite()\n";
         project->removeSite(site); // do not delete, leave that for ~SiteItem()
         break; // Nidas has only 1 object per site, regardless of how many XML has
         }
    }
*/

  return true;
}

QString ProjectItem::name()
{
    cerr << "name() called from ProjectItem class \n";
    Project *project = reinterpret_cast<Project*>(this->nidasObject);
    return(QString::fromStdString(project->getName()));
}
