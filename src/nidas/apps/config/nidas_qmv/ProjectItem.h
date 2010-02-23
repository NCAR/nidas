
#ifndef _PROJECT_ITEM_H
#define _PROJECT_ITEM_H

#include "NidasItem.h"
#include "SiteItem.h"
#include <nidas/core/Project.h>
#include <xercesc/dom/DOMNode.hpp>

using namespace nidas::core;


class ProjectItem : public NidasItem
{

public:
    ProjectItem(Project *project, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(project,row,model,parent) {}

    ~ProjectItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item);

//protected: commented while Document still uses these

        // get/convert to the underlying model pointers
    Project *getProject() const
    {
      if (nidasType == PROJECT)
         return reinterpret_cast<Project*>(this->nidasObject);
      else return 0;
    }

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

protected:
    xercesc::DOMNode *findDOMNode() const;
    QString name();
};

#endif
