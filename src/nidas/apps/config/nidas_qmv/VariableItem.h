
#ifndef _VARIABLE_ITEM_H
#define _VARIABLE_ITEM_H

#include "NidasItem.h"
#include <nidas/core/Variable.h>

using namespace nidas::core;


class VariableItem : public NidasItem
{

public:
    VariableItem(Variable *variable, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(variable,row,model,parent) {}

    ~VariableItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string variableName() { return this->dataField(1).toStdString(); }

    const QVariant & childLabel(int column) const { return NidasItem::_Name_Label; }
    int childColumnCount() const {return 1;}

    QString dataField(int column);

protected:
        // get/convert to the underlying model pointers
    Variable *getVariable() const {
     if (nidasType == VARIABLE)
         return reinterpret_cast<Variable*>(this->nidasObject);
     else return 0;
     }
   
     QString name();

};

#endif
