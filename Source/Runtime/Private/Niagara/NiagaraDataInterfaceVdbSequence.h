// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "VdbSequenceComponent.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceVdbSequence.generated.h"

// the struct used to store our data interface data
struct FNDIVdbSeqInstanceData
{
	uint32 index = 0;
};

/**
 * 
 */
UCLASS(EditInlineNew, Category = "SparseVolumetrics", meta = (DisplayName = "Volume VDB Sequence"))
class UNiagaraDataInterfaceVdbSequence : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_NIAGARA_DI_PARAMETER();

	//UPROPERTY(EditAnywhere, Category = "Source")
	//TObjectPtr<class UVdbVolumeSequence> VdbVolumeSeq;

	//UPROPERTY(EditAnywhere, Category = "Source")
	//TLazyObjectPtr<AActor> SourceActor;


	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End
	
	//UNiagaraDataInterface Interface
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	
	// Returns the DI's functions signatures 
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual void GetSeqIndexBoundsVM(FVectorVMExternalFunctionContext& Context);
	virtual void GetSeqOffsetOfBoundsVM(FVectorVMExternalFunctionContext& Context);
	virtual void GetCenterLocationVM(FVectorVMExternalFunctionContext& Context);
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool RequiresDepthBuffer() const override { return false; }

	virtual bool HasPostSimulateTick() const override { return true; }
	virtual int32 PerInstanceDataSize()const override { return sizeof(FNDIVdbSeqInstanceData); }
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	
#if WITH_EDITORONLY_DATA
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif

	float elapsedTime = 0;
	UVdbSequenceComponent* VdbVolumeSeqComp;
	UNiagaraComponent* Owner;
	
	
protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;
};
