// //Copyright 2020 Edwin Yung. All Rights Reserved
#include "PythonBridge.h"

UPythonBridge* UPythonBridge::Get()
{
    TArray<UClass*> PythonBridgeClasses;
    GetDerivedClasses(UPythonBridge::StaticClass(), PythonBridgeClasses);
    int32 NumClasses = PythonBridgeClasses.Num();
    if (NumClasses > 0)
    {
        return Cast<UPythonBridge>(PythonBridgeClasses[NumClasses - 1]->GetDefaultObject());
    }
    return nullptr;
};

void FTestPythonPlugin::MenuCallback2() {
    UPythonBridge* bridge = UPythonBridge::Get();
    bridge->FunctionImplementedInPython();
}

/*
#include "MyClass.h" 

MyClass::MyClass()
{
}

MyClass::~MyClass()
{
}
*/
