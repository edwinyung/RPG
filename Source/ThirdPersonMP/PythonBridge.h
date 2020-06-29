// //Copyright 2020 Edwin Yung. All Rights Reserved

//https://forums.unrealengine.com/unreal-engine/unreal-studio/1506630-running-a-python-script-with-c

#pragma once
//#include "CoreMinimal.h"

#include "Engine.h"
#include "PythonBridge.generated.h"

UCLASS(Blueprintable)
class UPythonBridge : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = Python)
        static UPythonBridge* Get();

    UFUNCTION(BlueprintImplementableEvent, Category = Python)
        void FunctionImplementedInPython() const;
};

/**
 * 
class THIRDPERSONMP_API MyClass
{
public:
	MyClass();
	~MyClass();
};
