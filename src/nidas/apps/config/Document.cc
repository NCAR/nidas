
#include "Document.h"


#include <sys/param.h>
#include <libgen.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>

#include <nidas/core/XMLParser.h>


using namespace std;
using namespace xercesc;
using namespace nidas::core;



static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
static const XMLCh gNull[] = { chNull };



const char *Document::getDirectory() const
{
static char buf[MAXPATHLEN];

strncpy(buf,filename->c_str(),MAXPATHLEN-1);
return dirname(buf);
}



void Document::writeDocument()
{
LocalFileFormatTarget *target;

    try {
        target = new LocalFileFormatTarget(filename->c_str());
        if (!target) {
            cerr << "target is null" << endl;
            return;
            }
    } catch (...) {
        cerr << "LocalFileFormatTarget new exception" << endl;
        return;
    }

writeDOM(target,domdoc);
delete target;
return;
}



bool Document::writeDOM( XMLFormatTarget * const target, const DOMNode * node )
{
DOMImplementation *domimpl;
DOMImplementationLS *lsimpl;
DOMWriter *myWriter;


    try {
        domimpl = XMLImplementation::getImplementation();
        //DOMImplementation *domimpl = DOMImplementationRegistry::getDOMImplementation(gLS);
    } catch (...) {
        cerr << "getImplementation exception" << endl;
        return(false);
    }
        if (!domimpl) {
            cerr << "xml implementation is null" << endl;
            return(false);
            }

    try {
        lsimpl =
        // (DOMImplementationLS*)domimpl;
         (domimpl->hasFeature(gLS,gNull)) ? (DOMImplementationLS*)domimpl : 0;
    } catch (...) {
        cerr << "hasFeature/cast exception" << endl;
        return(false);
    }

        if (!lsimpl) {
            cerr << "dom implementation is null" << endl;
            return(false);
            }

    try {
        myWriter = lsimpl->createDOMWriter();
        if (!myWriter) {
            cerr << "writer is null" << endl;
            return(false);
            }
    } catch (...) {
        cerr << "createDOMWriter exception" << endl;
        return(false);
    }

        if (myWriter->canSetFeature(XMLUni::fgDOMWRTValidation, true))
            myWriter->setFeature(XMLUni::fgDOMWRTValidation, true);
        if (myWriter->canSetFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true))
            myWriter->setFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true);

        myWriter->setErrorHandler(&errorHandler);

    try {
        if (!myWriter->writeNode(target,*node)) {
            cerr << "writeNode returns false" << endl;
            }
    } catch (...) {
        cerr << "writeNode exception" << endl;
        return(false);
    }

        target->flush();
        myWriter->release();

        return(true);
}



void Document::parseFile()
{
        cerr << "Document::parseFile()" << endl;
        if (!filename) return;

        XMLParser * parser = new XMLParser();
    
        // turn on validation
        parser->setDOMValidation(true);
        parser->setDOMValidateIfSchema(true);
        parser->setDOMNamespaces(true);
        parser->setXercesSchema(true);
        parser->setXercesSchemaFullChecking(true);
        parser->setDOMDatatypeNormalization(false);
        parser->setXercesUserAdoptsDOMDocument(true);

        cerr << "parsing: " << *filename << endl;
        domdoc = parser->parse(*filename);
        cerr << "parsed" << endl;
        delete parser;

        project = Project::getInstance();
        cerr << "doing fromDOMElement" << endl;
        project->fromDOMElement(domdoc->getDocumentElement());
        cerr << "fromDOMElement done" << endl;
}
