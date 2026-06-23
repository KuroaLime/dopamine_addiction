#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "CardDropActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType)
class MANAGER_API ACardDropActor : public AActor
{
    GENERATED_BODY()

public:
    ACardDropActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Card")
    void InitCardDrop(int32 InCardInstanceId, ECardID InCardID);

    UFUNCTION(BlueprintCallable, Category = "Card")
    int32 GetCardInstanceId() const { return CardInstanceId; }

    UFUNCTION(BlueprintCallable, Category = "Card")
    ECardID GetCardID() const { return CardID; }

    UFUNCTION(BlueprintCallable, Category = "Card")
    bool IsPickedUp() const { return bPickedUp; }

    void MarkPickedUp();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card")
    TObjectPtr<UStaticMeshComponent> CardMesh;

    UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
    int32 CardInstanceId = 0;

    UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
    ECardID CardID = ECardID::None;

    UPROPERTY(ReplicatedUsing = OnRep_CardVisual, BlueprintReadOnly, Category = "Card")
    bool bPickedUp = false;

    UFUNCTION()
    void OnRep_CardVisual();

private:
    void RefreshVisual();
};
