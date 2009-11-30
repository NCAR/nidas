#include "DSMDisplayWidget.h"
#include "DSMTableWidget.h"

#include <QVBoxLayout>
#include <QLabel>

#include <iostream>
#include <fstream>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XDOM.h>


using namespace std;
using namespace xercesc;
using namespace nidas::core;



/* Constructor - create table, set column headers and width */
DSMDisplayWidget::DSMDisplayWidget( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QString & label, QWidget *parent)
       : QGroupBox(parent)
{
    dsmConfig = dsm;
    domdoc = doc;
    dsmDomNode = 0;

    setObjectName("DSM");

    dsmOtherTable = new DSMOtherTable(dsm,doc);
    dsmAnalogTable = new DSMAnalogTable(dsm,doc);

    QVBoxLayout *DSMLayout = new QVBoxLayout;
    QLabel *DSMLabel = new QLabel(label);
    DSMLayout->addWidget(DSMLabel);

    _dsmId = dsm->getId();

    DSMLayout->addWidget(dsmOtherTable);
    DSMLayout->addWidget(dsmAnalogTable);
    this->setLayout(DSMLayout);

}

// Return a pointer to the node which defines this DSM
DOMNode * DSMDisplayWidget::getDSMNode()
{
if (dsmDomNode) return(dsmDomNode);
cerr << "DSMDisplayWidget::getDSMNode()\n";
if (!domdoc) return(0);

  DOMNodeList * DSMNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("dsm"));
  DOMNode * DSMNode = 0;
  cerr << "nodes length = " << DSMNodes->getLength() << "\n";
  for (XMLSize_t i = 0; i < DSMNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)DSMNodes->item(i));
     const std::string& sDSMId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sDSMId.c_str()) == _dsmId) { 
       cerr<<"getDSMNode - Found DSMNode with id:" << sDSMId << endl;
       DSMNode = DSMNodes->item(i);
     }
  }

  return(dsmDomNode=DSMNode);

}

