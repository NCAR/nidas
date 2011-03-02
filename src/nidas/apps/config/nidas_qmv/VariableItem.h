
#ifndef _VARIABLE_ITEM_H
#define _VARIABLE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/Variable.h>
#include "SensorItem.h"

using namespace nidas::core;


class VariableItem : public NidasItem
{

public:
    VariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~VariableItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string variableName() { return this->dataField(1).toStdString(); }

    const QVariant & childLabel(int column) const { return NidasItem::_Name_Label; }
    int childColumnCount() const {return 1;}

    QString dataField(int column);

    QString name();
    QString getLongName() 
            { return QString::fromStdString(_variable->getLongName()); }
    //SampleTag *getSampleTag() { return _sampleTag; }
    SampleTag *getSampleTag() { return const_cast<SampleTag*>(_variable->getSampleTag()); }
    std::vector<std::string> getCalibrationInfo();
    xercesc::DOMNode* getSampleDOMNode() {
        if (_sampleDOMNode)
          return _sampleDOMNode;
        else return _sampleDOMNode=findSampleDOMNode();
        }
    float getRate() { return _sampleTag->getRate(); }
    unsigned int getSampleId() {std::cerr<< "in VarItem getSampleID\n";return _sampleID;}

    xercesc::DOMNode* getVariableDOMNode(QString name) {
        return _variableDOMNode=findVariableDOMNode(name);
    }

    std::string sSampleId() { return this->dataField(1).toStdString(); }

    void setDOMName(QString fromName, std::string toName);

    void fromDOM();

    SensorItem * getSensorItem() {return dynamic_cast<SensorItem*>(parent());}

protected:
        // get/convert to the underlying model pointers
    Variable *getVariable() const { return _variable; }
    xercesc::DOMNode *findSampleDOMNode();
    xercesc::DOMNode *findVariableDOMNode(QString name);
    Variable * _variable;
    SampleTag * _sampleTag;
   

private:
    xercesc::DOMNode * _sampleDOMNode;
    xercesc::DOMNode * _variableDOMNode;
    unsigned int _sampleID;
};

#endif
