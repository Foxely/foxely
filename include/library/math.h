#ifndef FOX_MathPlugin_H
#define FOX_MathPlugin_H


#include "scy/pluga/plugin_api.h"

/// Test plugin implementation
class MathPlugin : public fox::pluga::IModule
{
public:
    MathPlugin(VM* oVM);
    virtual ~MathPlugin();

    virtual const char* GetClassName() const;
};


#endif