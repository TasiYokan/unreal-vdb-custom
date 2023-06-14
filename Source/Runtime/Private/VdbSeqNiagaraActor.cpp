#include "VdbSeqNiagaraActor.h"

#include "VdbCommon.h"
#include "VdbVolumeBase.h"
#include "VdbAssetComponent.h"
#include "VdbSequenceComponent.h"
#include "NiagaraComponent.h"
//#include "NiagaraSystem.h"

AVdbSeqNiagaraActor::AVdbSeqNiagaraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssetComponent = CreateDefaultSubobject<UVdbAssetComponent>(TEXT("AssetComponent"));

	SequenceComponent = CreateDefaultSubobject<UVdbSequenceComponent>(TEXT("SequenceComponent"));
	SequenceComponent->SetVdbAssets(AssetComponent);
}

#if WITH_EDITOR
bool AVdbSeqNiagaraActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	AssetComponent->GetReferencedContentObjects(Objects);

	return true;
}
#endif

UVdbSequenceComponent* AVdbSeqNiagaraActor::GetSeqComp() {
	return SequenceComponent.Get();
}
