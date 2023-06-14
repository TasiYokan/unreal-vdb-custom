// Fill out your copyright notice in the Description page of Project Settings.


#include "Niagara/NiagaraDataInterfaceVdbSequence.h"
#include "Niagara/NiagaraDataInterfaceVdb.h"
#include "Rendering/VdbRenderBuffer.h"
#include "VdbVolumeSequence.h"

#include "VdbSequenceComponent.h"
#include "VdbSeqNiagaraActor.h"
#include "NiagaraComponent.h"

#include "NiagaraTypes.h"
#include "NiagaraWorldManager.h"
#include "NiagaraSystemInstance.h"
#include "ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceVdbSequence"


struct UNiagaraDataInterfaceVdbSeqProxy : public FNiagaraDataInterfaceProxy
{
	FShaderResourceViewRHIRef SrvRHI;
	FIntVector IndexMin;
	FIntVector IndexMax;

	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override { check(false); }
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }
};

struct FNiagaraDataInterfaceParametersCS_VDBSeq : public FNiagaraDataInterfaceParametersCS
{
	DECLARE_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_VDBSeq, NonVirtual);
public:
	void Bind(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const class FShaderParameterMap& ParameterMap) {
		VdbVolumeStatic.Bind(ParameterMap, *(VolumeName + ParameterInfo.DataInterfaceHLSLSymbol));
		IndexMin.Bind(ParameterMap, *(IndexMinName + ParameterInfo.DataInterfaceHLSLSymbol));
		IndexMax.Bind(ParameterMap, *(IndexMaxName + ParameterInfo.DataInterfaceHLSLSymbol));
	}

	void Set(FRHICommandList& RHICmdList, const FNiagaraDataInterfaceSetArgs& Context) const {
		check(IsInRenderingThread());

		FRHIComputeShader* ComputeShaderRHI = RHICmdList.GetBoundComputeShader();
		UNiagaraDataInterfaceVdbSeqProxy* Proxy = static_cast<UNiagaraDataInterfaceVdbSeqProxy*>(Context.DataInterface);

		if (Proxy && Proxy->SrvRHI) {
			SetSRVParameter(RHICmdList, ComputeShaderRHI, VdbVolumeStatic, Proxy->SrvRHI);
			SetShaderValue(RHICmdList, ComputeShaderRHI, IndexMin, Proxy->IndexMin);
			SetShaderValue(RHICmdList, ComputeShaderRHI, IndexMax, Proxy->IndexMax);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, VdbVolumeStatic);
	LAYOUT_FIELD(FShaderParameter, IndexMin);
	LAYOUT_FIELD(FShaderParameter, IndexMax);
};

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS_VDBSeq);
IMPLEMENT_NIAGARA_DI_PARAMETER(UNiagaraDataInterfaceVdbSequence, FNiagaraDataInterfaceParametersCS_VDBSeq);

////////////////////////////////////////////////////////////////////////////

UNiagaraDataInterfaceVdbSequence::UNiagaraDataInterfaceVdbSequence(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer) {
	Proxy.Reset(new UNiagaraDataInterfaceVdbSeqProxy());
}

void UNiagaraDataInterfaceVdbSequence::PostInitProperties() {
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject)) {
		ENiagaraTypeRegistryFlags Flags =
			//ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowUserVariable |
			ENiagaraTypeRegistryFlags::AllowEmitterVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceVdbSequence::PostLoad() {
	Super::PostLoad();

	MarkRenderDataDirty();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceVdbSequence::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) {
	Super::PostEditChangeProperty(PropertyChangedEvent);

	//if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVdbSequence, SourceActor)) {
	//}

	MarkRenderDataDirty();
}

#endif

bool UNiagaraDataInterfaceVdbSequence::CopyToInternal(UNiagaraDataInterface* Destination) const {
	if (!Super::CopyToInternal(Destination)) {
		return false;
	}

	UNiagaraDataInterfaceVdbSequence* DestinationDI = CastChecked<UNiagaraDataInterfaceVdbSequence>(Destination);
	DestinationDI->VdbVolumeSeqComp = VdbVolumeSeqComp;
	Destination->MarkRenderDataDirty();

	return true;
}

bool UNiagaraDataInterfaceVdbSequence::Equals(const UNiagaraDataInterface* Other) const {
	if (!Super::Equals(Other)) {
		return false;
	}

	const UNiagaraDataInterfaceVdbSequence* OtherDI = CastChecked<const UNiagaraDataInterfaceVdbSequence>(Other);
	return OtherDI->VdbVolumeSeqComp == VdbVolumeSeqComp;
}

void UNiagaraDataInterfaceVdbSequence::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) {
	auto InitSignature = [](const FName& Name, const FText& Desc) {
		FNiagaraFunctionSignature Sig;
		Sig.Name = Name;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = false;
		Sig.bSupportsGPU = true;
		Sig.SetDescription(Desc);
		return Sig;
	};

	{
		FNiagaraFunctionSignature Sig = InitSignature(InitVolumeName, LOCTEXT("InitVolume", "Mandatory function to init VDB volume sampling."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));  // fake int to make sure we force users to init volume before using it
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumeName, LOCTEXT("SampleVolume", "Sample VDB volume at IJK coordinates. Supports all grid types."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumeFastName, LOCTEXT("SampleVolume", "Sample VDB volume at IJK coordinates. Optimal way to do it, but only supports 32f grids (i.e non-quantized)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(SampleVolumePosName, LOCTEXT("SampleVolumePos", "Sample VDB volume at 3D position."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LevelSetZeroCrossingName, LOCTEXT("LevelSetZeroCrossing", "Trace ray and checks if it crosses a LevelSet in the volume. Returns if hit, which ijk index and value v."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Hit")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbLevelSetHit::StaticStruct()), TEXT("HitResults")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LevelSetComputeNormalName, LOCTEXT("LevelSetComputeNormal", "Computes LevelSet normal from successful Zero Crossing hit."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Accessor")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("GridType")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("v")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayClipName, LOCTEXT("RayClip", "Fast Ray update against Volume Bounding Box. Returns false if ray doesn't collide with volume. Updates Ray start and end according to Volume bounding box."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Hit")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
	// Space conversions
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpacePosName, LOCTEXT("LocalToVdbSpacePos", "Converts Position from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpaceDirName, LOCTEXT("LocalToVdbSpaceDir", "Converts Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(LocalToVdbSpaceName, LOCTEXT("LocalToVdbSpace", "Converts Position and Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpacePosName, LOCTEXT("VdbToLocalSpacePos", "Converts Position from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpaceDirName, LOCTEXT("VdbToLocalSpaceDir", "Converts Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbToLocalSpaceName, LOCTEXT("VdbToLocalSpace", "Converts Position and Direction from Local space to VDB space (aka index space)."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbDir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("LocalDir")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(VdbSpaceToIjkName, LOCTEXT("VdbSpaceToIjk", "Converts VDB position to ijk volume index."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(IjkToVdbSpaceName, LOCTEXT("IjkToVdbSpace", "Converts ijk volume index to VDB position."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("i")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("j")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("k")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbPos")));
		OutFunctions.Add(Sig);
	}
	// Ray operations
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayFromStartEndName, LOCTEXT("RayFromStartEnd", "Create Ray From Start and End indications."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Start")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("End")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(RayFromStartDirName, LOCTEXT("RayFromStartDir", "Create Ray From Start and Direction indications."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeStatic")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Start")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Dir")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FVdbRay::StaticStruct()), TEXT("Ray")));
		OutFunctions.Add(Sig);
	}
	// Others
	{
		FNiagaraFunctionSignature Sig = InitSignature(GetSeqIndexBoundsName, LOCTEXT("GetSeqIndexBounds", "Get index bounds"));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeSeq")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbIndexBounds")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(GetSeqOffsetOfBoundsName, LOCTEXT("GetSeqOffsetOfBounds", "Get bounds offset"));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeSeq")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbBoundsOffset")));
		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig = InitSignature(GetCenterLocationName, LOCTEXT("GetCenterLocation", "Get center location"));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bSupportsCPU = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("VdbVolumeSeq")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("VdbCenterLocation")));
		OutFunctions.Add(Sig);
	}
}


// this provides the cpu vm with the correct function to call
void UNiagaraDataInterfaceVdbSequence::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) {
	if (BindingInfo.Name == GetSeqIndexBoundsName) {
		//check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVdbSequence::GetSeqIndexBoundsVM);
	}
	if (BindingInfo.Name == GetSeqOffsetOfBoundsName) {
		//check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVdbSequence::GetSeqOffsetOfBoundsVM);
	}
	if (BindingInfo.Name == GetCenterLocationName) {
		//check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVdbSequence::GetCenterLocationVM);
	}
	else {
		UE_LOG(LogTemp, Display, TEXT("Could not find data interface external function in %s. Received Name: %s"), *GetPathNameSafe(this), *BindingInfo.Name.ToString());
	}
}

// implementation called by the vectorVM
void UNiagaraDataInterfaceVdbSequence::GetSeqIndexBoundsVM(FVectorVMExternalFunctionContext& Context) {

	VectorVM::FUserPtrHandler<FNDIVdbSeqInstanceData> InstData(Context);
	FNDIOutputParam<float> X(Context);
	FNDIOutputParam<float> Y(Context);
	FNDIOutputParam<float> Z(Context);	

	if (VdbVolumeSeqComp == nullptr) {
		for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
			X.SetAndAdvance(128);
			Y.SetAndAdvance(128);
			Z.SetAndAdvance(128);
		}
		return;
	}

	uint32 id = VdbVolumeSeqComp->GetFrameIndexFromElapsedTime();
	auto volumeSeq = VdbVolumeSeqComp->GetPrincipalSequence();

	FVdbRenderBuffer* RT_Resource = volumeSeq ? volumeSeq->GetRenderInfos(id)->GetRenderResource() : nullptr;
	FIntVector IndexMin = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMin() : FIntVector();
	FIntVector IndexMax = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMax() : FIntVector();
	FIntVector IndexBounds = IndexMax - IndexMin;

	//UE_LOG(LogTemp, Display, TEXT("Found vdb: %d, %d, %d"), IndexBounds.X, IndexBounds.Y, IndexBounds.Z);
	for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
		X.SetAndAdvance(IndexBounds.X);
		Y.SetAndAdvance(IndexBounds.Y);
		Z.SetAndAdvance(IndexBounds.Z);
	}
}

void UNiagaraDataInterfaceVdbSequence::GetSeqOffsetOfBoundsVM(FVectorVMExternalFunctionContext& Context) {
	VectorVM::FUserPtrHandler<FNDIVdbSeqInstanceData> InstData(Context);
	FNDIOutputParam<float> X(Context);
	FNDIOutputParam<float> Y(Context);
	FNDIOutputParam<float> Z(Context);

	if (VdbVolumeSeqComp == nullptr) 
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
			X.SetAndAdvance(0.0);
			Y.SetAndAdvance(0.0);
			Z.SetAndAdvance(0.0);
		}
		return;
	}

	uint32 id = VdbVolumeSeqComp->GetFrameIndexFromElapsedTime();
	auto volumeSeq = VdbVolumeSeqComp->GetPrincipalSequence();

	FVdbRenderBuffer* RT_Resource = volumeSeq ? volumeSeq->GetRenderInfos(id)->GetRenderResource() : nullptr;
	FIntVector IndexMin = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMin() : FIntVector();
	FIntVector IndexMax = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMax() : FIntVector();
	FMatrix44f IndexToLocal = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexToLocal() : FMatrix44f::Identity;
	FIntVector IndexBounds = IndexMax - IndexMin;
	FIntVector IndexCenter = IndexMax + IndexMin;

	//FVector center = FVector(IndexCenter.X, IndexCenter.Y, IndexCenter.Z) * 0.5f;
	//float minBounds = FMath::Min3(IndexBounds.X, IndexBounds.Y, IndexBounds.Z);
	//center *= 100.0f / minBounds;
	//UE_LOG(LogTemp, Display, TEXT("center Offset: %f, %f, %f"), center.X, center.Y, center.Z);
	for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
		X.SetAndAdvance(float(IndexCenter.X) / float(IndexBounds.X) * -0.5f);
		Y.SetAndAdvance(float(IndexCenter.Y) / float(IndexBounds.Y) * -0.5f);
		Z.SetAndAdvance(float(IndexCenter.Z) / float(IndexBounds.Z) * -0.5f);
	}
}

void UNiagaraDataInterfaceVdbSequence::GetCenterLocationVM(FVectorVMExternalFunctionContext& Context) {
	VectorVM::FUserPtrHandler<FNDIVdbSeqInstanceData> InstData(Context);
	FNDIOutputParam<float> X(Context);
	FNDIOutputParam<float> Y(Context);
	FNDIOutputParam<float> Z(Context);

	if (VdbVolumeSeqComp == nullptr) {
		for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
			X.SetAndAdvance(0.0);
			Y.SetAndAdvance(0.0);
			Z.SetAndAdvance(0.0);
		}
		return;
	}

	uint32 id = VdbVolumeSeqComp->GetFrameIndexFromElapsedTime();
	auto volumeSeq = VdbVolumeSeqComp->GetPrincipalSequence();

	FVdbRenderBuffer* RT_Resource = volumeSeq ? volumeSeq->GetRenderInfos(id)->GetRenderResource() : nullptr;
	FIntVector IndexMin = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMin() : FIntVector();
	FIntVector IndexMax = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMax() : FIntVector();
	FMatrix44f IndexToLocal = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexToLocal() : FMatrix44f::Identity;
	FIntVector IndexBounds = IndexMax - IndexMin;
	FIntVector IndexCenter = IndexMax + IndexMin;

	//FVector center = FVector(IndexCenter.X, IndexCenter.Y, IndexCenter.Z) * 0.5f;
	//float minBounds = FMath::Min3(IndexBounds.X, IndexBounds.Y, IndexBounds.Z);
	//center *= 100.0f / minBounds;
	for (int32 i = 0; i < Context.GetNumInstances(); ++i) {
		X.SetAndAdvance(float(IndexCenter.X) * 0.5f);
		Y.SetAndAdvance(float(IndexCenter.Y) * 0.5f);
		Z.SetAndAdvance(float(IndexCenter.Z) * 0.5f);
	}
}

bool UNiagaraDataInterfaceVdbSequence::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) {
	elapsedTime += DeltaSeconds;
	//elapsedTime = SystemInstance->GetAge();

	//VdbVolumeSeq.Get()->VdbSequenceComponents[0]->SetManualTick(true);
	//VdbVolumeSeq.Get()->VdbSequenceComponents[0]->TickAtThisTime(elapsedTime, true, false, true);
	//uint32 id = 0;
	//if (VdbVolumeSeq.Get()->VdbSequenceComponents[0]->IsPlaying() == false && VdbVolumeSeq.Get()->VdbSequenceComponents.Num() > 1)
	//	id = VdbVolumeSeq.Get()->VdbSequenceComponents[1]->GetFrameIndexFromElapsedTime();
	//else
	//	id = VdbVolumeSeq.Get()->VdbSequenceComponents[0]->GetFrameIndexFromElapsedTime();
	//UE_LOG(LogSparseVolumetrics, Log, TEXT("Change TickTime, elapsedTime: %f, id is %d"), VdbVolumeSeq.Get()->VdbSequenceComponents[0]->GetElapsedTime(), id);

	Owner = Cast<UNiagaraComponent>(SystemInstance->GetInstanceParameters().GetOwner());
	AVdbSeqNiagaraActor* ObjectActor = Cast<AVdbSeqNiagaraActor>(Owner->GetOwner());
	//AVdbSeqNiagaraActor* ObjectActor = Cast<AVdbSeqNiagaraActor>(SourceActor.Get());
	//VdbVolumeSeqComp = Cast<UVdbSequenceComponent>(ObjectActor->FindComponentByClass(UVdbSequenceComponent::StaticClass()));

	if(ObjectActor != nullptr)
		VdbVolumeSeqComp = ObjectActor->GetSeqComp();

	MarkRenderDataDirty();
	//this->PushToRenderThread();
	return false;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceVdbSequence::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) {
	auto FormatString = [&](const TCHAR* Format) {
		TMap<FString, FStringFormatArg> Args = {
			{ TEXT("FunctionName"), FunctionInfo.InstanceName },
			{ TEXT("VolumeName"), VolumeName + ParamInfo.DataInterfaceHLSLSymbol },
			{ TEXT("IndexMin"), IndexMinName + ParamInfo.DataInterfaceHLSLSymbol },
			{ TEXT("IndexMax"), IndexMaxName + ParamInfo.DataInterfaceHLSLSymbol },
		};
		OutHLSL += FString::Format(Format, Args);
		return true;
	};

	if (FunctionInfo.DefinitionName == InitVolumeName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(out int Accessor, out int GridType) 
			{
				InitVolume({VolumeName}, GridType);
				Accessor = 1; // useless, but usefull for consistency and readability
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumeName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, int i, int j, int k, out float Out_Value) 
			{
				Out_Value = ReadCompressedValue({VolumeName}, VdbAccessor, GridType, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumeFastName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, int i, int j, int k, out float Out_Value) 
			{
				Out_Value = ReadValue({VolumeName}, VdbAccessor, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == SampleVolumePosName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in float3 Position, out float Out_Value) 
			{
				pnanovdb_coord_t ijk = pnanovdb_hdda_pos_to_ijk(Position);
				Out_Value = ReadCompressedValue({VolumeName}, VdbAccessor, GridType, ijk);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LevelSetZeroCrossingName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in VdbRay Ray, out bool Hit, out VdbLevelSetHit HitResults) 
			{
				int3 ijk;
				Hit = pnanovdb_hdda_zero_crossing_improved(GridType, {VolumeName}, VdbAccessor, Ray.Origin, Ray.Tmin, Ray.Direction, Ray.Tmax, HitResults.t, HitResults.v0, ijk);
				HitResults.i = ijk.x; HitResults.j = ijk.y; HitResults.k = ijk.z;
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LevelSetComputeNormalName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in int Accessor, in int GridType, in float v, in int i, in int j, in int k, out float3 Normal) 
			{
				Normal = ZeroCrossingNormal(GridType, {VolumeName}, VdbAccessor, v, int3(i, j, k));
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == RayClipName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in VdbRay InRay, out bool Hit, out VdbRay OutRay) 
			{
				OutRay = InRay;
				Hit = pnanovdb_hdda_ray_clip({IndexMin}, {IndexMax}, OutRay.Origin, OutRay.Tmin, OutRay.Direction, OutRay.Tmax);
			}
		)");
		return FormatString(Format);
	}
	// Space conversions
	else if (FunctionInfo.DefinitionName == LocalToVdbSpacePosName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalPos, out float3 VdbPos) 
			{
				VdbPos = LocalToIndexPos({VolumeName}, LocalPos);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LocalToVdbSpaceDirName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalDir, out float3 VdbDir) 
			{
				VdbDir = LocalToIndexDir({VolumeName}, LocalDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == LocalToVdbSpaceName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 LocalPos, in float3 LocalDir, out float3 VdbPos, out float3 VdbDir) 
			{
				VdbPos = LocalToIndexPos({VolumeName}, LocalPos);
				VdbDir = LocalToIndexDir({VolumeName}, LocalDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpacePosName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, out float3 LocalPos) 
			{
				LocalPos = IndexToLocalPos({VolumeName}, VdbPos);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpaceDirName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbDir, out float3 LocalDir) 
			{
				LocalDir = IndexToLocalDir({VolumeName}, VdbDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbToLocalSpaceName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, in float3 VdbDir, out float3 LocalPos, out float3 LocalDir) 
			{
				LocalPos = IndexToLocalPos({VolumeName}, VdbPos);
				LocalDir = IndexToLocalDir({VolumeName}, VdbDir);
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == VdbSpaceToIjkName) {
		static const TCHAR* Format = TEXT(R"(
			void {FunctionName}(in float3 VdbPos, out int i, out int j, out int k) 
			{
				int3 ijk = IndexToIjk(VdbPos);
				i = ijk.x;
				j = ijk.y;
				k = ijk.z;
			}
		)");
		return FormatString(Format);
	}
	else if (FunctionInfo.DefinitionName == IjkToVdbSpaceName) {
		static const TCHAR* Format = TEXT(R"(
				void {FunctionName}(in int i, in int j, in int k, out float3 VdbPos) 
				{
					VdbPos = pnanovdb_coord_to_vec3(int3(i, j, k));
				}
			)");
		return FormatString(Format);
	}

	return false;
}

void UNiagaraDataInterfaceVdbSequence::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) {
	OutHLSL += TEXT("StructuredBuffer<uint> ") + VolumeName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("int3 ") + IndexMinName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("int3 ") + IndexMaxName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");
	OutHLSL += TEXT("\n");
}

void UNiagaraDataInterfaceVdbSequence::GetCommonHLSL(FString& OutHLSL) {
	OutHLSL += TEXT("#include \"/Plugin/VdbVolume/Private/NiagaraDataInterfaceVdb.ush\"\n");
}

bool UNiagaraDataInterfaceVdbSequence::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const {
	if (!Super::AppendCompileHash(InVisitor))
		return false;

	FSHAHash Hash = GetShaderFileHash((TEXT("/Plugin/VdbVolume/Private/NiagaraDataInterfaceVdb.ush")), EShaderPlatform::SP_PCD3D_SM5);
	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceVdbHLSLSource"), Hash.ToString());
	return true;
}
#endif

void UNiagaraDataInterfaceVdbSequence::PushToRenderThreadImpl() {
	UNiagaraDataInterfaceVdbSeqProxy* NDIProxy = GetProxyAs<UNiagaraDataInterfaceVdbSeqProxy>();

	//if(VdbVolumeSeqComp.Get() != nullptr)
	//	const UVdbVolumeSequence* seqPtr = VdbVolumeSeqComp.Get()->GetPrincipalSequence();

	//uint32 id = (int32(elapsedTime*10) % VdbVolumeSeq->GetNbFrames());
	//uint32 id = VdbVolumeSeq.Get()->GetFrameIndexFromFirstAvailableComponentTime();
	if (VdbVolumeSeqComp == nullptr)
		return;

	uint32 id = VdbVolumeSeqComp->GetFrameIndexFromElapsedTime();
	auto volumeSeq = VdbVolumeSeqComp->GetPrincipalSequence();

	FVdbRenderBuffer* RT_Resource = volumeSeq ? volumeSeq->GetRenderInfos(id)->GetRenderResource() : nullptr;
	FIntVector IndexMin = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMin() : FIntVector();
	FIntVector IndexMax = volumeSeq ? volumeSeq->GetFrameInfos(id)->GetIndexMax() : FIntVector();

	if (RT_Resource == nullptr) {
		UE_LOG(LogSparseVolumetrics, Log, TEXT("RT_Resource is nullptr %d, elapsedTime is: %f"), id, elapsedTime);
		return;
	}

	ENQUEUE_RENDER_COMMAND(FPushDIVolumeVdbToRT)
		(
			[NDIProxy, RT_Resource, IndexMin, IndexMax](FRHICommandListImmediate& RHICmdList) {
		NDIProxy->SrvRHI = RT_Resource ? RT_Resource->GetBufferSRV() : nullptr;
		NDIProxy->IndexMin = IndexMin;
		NDIProxy->IndexMax = IndexMax;
	}
	);
}

#undef LOCTEXT_NAMESPACE