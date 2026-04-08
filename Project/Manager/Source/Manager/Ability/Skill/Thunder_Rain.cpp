// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Skill/Thunder_Rain.h"
#include "ManagerCharacter.h"
#include "CustomASC.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

UThunder_Rain::UThunder_Rain()
{
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Skill00")));
	Detect = CreateDefaultSubobject<UDetectComponent>(TEXT("DETECT"));

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FXRef(
        TEXT("/Script/Niagara.NiagaraSystem'/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_04_Green.NS_Zap_04_Green'")
    );

    if (FXRef.Succeeded())
    {
        DetectHitFX = FXRef.Object;
    }
}

void UThunder_Rain::ActivateAbility()
{
    Super::ActivateAbility();

	AManagerCharacter* MC = Cast<AManagerCharacter>(OwnerCharacter);
	if (!MC) return;
	DetectedResult = Detect->Detect(MC->GetActorLocation(),400.0f);

    PlayDetectFXOnTargets(DetectedResult);
}

void UThunder_Rain::PlayDetectFXOnTargets(const TArray<AActor*>& Targets)
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (AActor* Target : Targets)
    {
        if (!Target) continue;

        const FVector Loc = Target->GetActorLocation()-FVector(0.0f,0.0f, 70.0f);

        UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, DetectHitFX, Loc, FRotator::ZeroRotator, FVector(1.f), true, true
        );

        if (Comp)
        {
            // 나이아가라에서 User.TargetPos 같은 파라미터를 만들어둔 경우
            Comp->SetVariableVec3(TEXT("User.TargetPos"), Loc);
        }
    }
}