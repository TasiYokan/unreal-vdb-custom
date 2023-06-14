#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraActor.h"

#include "VdbSeqNiagaraActor.generated.h"

//Actor that dynamically transfers VDB grids (from OpenVDB or NanoVDB files) 
//into a Volume Texture Render Target
UCLASS(ClassGroup = Rendering, Meta = (ComponentWrapperClass))
class AVdbSeqNiagaraActor : public ANiagaraActor
{
	GENERATED_UCLASS_BODY()

private:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Volumetric, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbAssetComponent> AssetComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Volumetric, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UVdbSequenceComponent> SequenceComponent;

public:
	UVdbSequenceComponent* GetSeqComp();
};