#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GoldDropActor.generated.h"

class AMainPlayerState;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType)
class MANAGER_API AGoldDropActor : public AActor
{
	GENERATED_BODY()

public:
	AGoldDropActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitGoldDrop(
		int32 InGoldAmount,
		AMainPlayerState* InSourcePlayerState,
		float SourcePickupLockSeconds);

	UFUNCTION(BlueprintPure, Category = "Gold Drop")
	int32 GetGoldAmount() const { return GoldAmount; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gold Drop")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gold Drop")
	TObjectPtr<UStaticMeshComponent> GoldMesh;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Gold Drop")
	int32 GoldAmount = 0;

private:
	UFUNCTION()
	void OnPickupSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	TWeakObjectPtr<AMainPlayerState> SourcePlayerState;
	double SourcePickupUnlockTimeSeconds = 0.0;
	bool bCollected = false;
};
