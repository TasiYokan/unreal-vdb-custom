// Copyright 2022 Eidos-Montreal / Eidos-Sherbrooke

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceVdb.generated.h"

USTRUCT()
struct FVdbRay
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "Ray")
	FVector3f Origin = FVector3f::Zero();

	UPROPERTY(EditAnywhere, Category = "Ray")
	float Tmin = 0.0001;

	UPROPERTY(EditAnywhere, Category = "Ray")
	FVector3f Direction = FVector3f::UnitX();

	UPROPERTY(EditAnywhere, Category = "Ray")
	float Tmax = FLT_MAX;
};

USTRUCT()
struct FVdbLevelSetHit
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	float t = 0.f;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	float v0 = 0.f;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int i = 0;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int j = 0;
	UPROPERTY(EditAnywhere, Category = "LevelSetHit")
	int k = 0;
};


// Name of all the functions available in the data interface
static const FName InitVolumeName(TEXT("InitVolume"));
static const FName SampleVolumeName(TEXT("SampleVolume"));
static const FName SampleVolumeFastName(TEXT("SampleVolumeFast"));
static const FName SampleVolumePosName(TEXT("SampleVolumePos"));
static const FName LevelSetZeroCrossingName(TEXT("LevelSetZeroCrossing"));
static const FName LevelSetComputeNormalName(TEXT("LevelSetComputeNormal"));
static const FName RayClipName(TEXT("RayClip"));
// Spaces conversions
static const FName LocalToVdbSpaceName(TEXT("LocalToVdbSpace"));
static const FName LocalToVdbSpacePosName(TEXT("LocalToVdbSpacePos"));
static const FName LocalToVdbSpaceDirName(TEXT("LocalToVdbSpaceDir"));
static const FName VdbToLocalSpaceName(TEXT("VdbToLocalSpace"));
static const FName VdbToLocalSpacePosName(TEXT("VdbToLocalSpacePos"));
static const FName VdbToLocalSpaceDirName(TEXT("VdbToLocalSpaceDir"));
static const FName VdbSpaceToIjkName(TEXT("VdbSpaceToIjk"));
static const FName IjkToVdbSpaceName(TEXT("IjkToVdbSpace"));
// Ray operations
static const FName RayFromStartEndName(TEXT("RayFromStartEnd"));
static const FName RayFromStartDirName(TEXT("RayFromStartDir"));
// Others
static const FName GetIndexBoundsName(TEXT("GetIndexBounds"));
static const FName GetOffsetOfBoundsName(TEXT("GetOffsetOfBounds"));
static const FName GetSeqIndexBoundsName(TEXT("GetSeqIndexBounds"));
static const FName GetSeqOffsetOfBoundsName(TEXT("GetSeqOffsetOfBounds"));
static const FName GetCenterLocationName(TEXT("GetCenterLocation"));

static const FString VolumeName(TEXT("Volume_"));
static const FString IndexMinName(TEXT("IndexMin_"));
static const FString IndexMaxName(TEXT("IndexMax_"));

UCLASS(EditInlineNew, Category = "SparseVolumetrics", meta = (DisplayName = "Volume VDB"))
class VOLUMERUNTIME_API UNiagaraDataInterfaceVdb : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_NIAGARA_DI_PARAMETER();

	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<class UVdbVolumeStatic> VdbVolumeStatic;

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
	virtual void GetIndexBoundsVM(FVectorVMExternalFunctionContext& Context);
	virtual void GetOffsetOfBoundsVM(FVectorVMExternalFunctionContext& Context);
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool RequiresDepthBuffer() const override { return false; }

#if WITH_EDITORONLY_DATA
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif


protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;
};
