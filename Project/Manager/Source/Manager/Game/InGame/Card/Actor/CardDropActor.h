#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/Protocol_Client/Protocol_InGame.h"
#include "CardDropActor.generated.h"

class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogManagerCard, Log, All);

UCLASS(Blueprintable, BlueprintType)
class MANAGER_API ACardDropActor : public AActor
{
    GENERATED_BODY()

public:
    ACardDropActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;

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

    // 바닥에 떨어진 카드가 둥둥 떠다니는 느낌을 주는 연출용 파라미터. 순수 로컬(비복제)
    // 연출이라 클라이언트마다 각자 계산 — 픽업 판정(TryPickupNearestCard)은 카드의
    // 루트 액터 위치만 보므로 여기서 CardMesh만 흔들어도 게임플레이엔 영향 없다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Motion")
    float FloatAmplitude = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Motion")
    float FloatPeriodSeconds = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Motion")
    float SpinDegreesPerSecond = 30.0f;

    // 카드의 기본 세로/가로 크기(비율). 실제 3D 회전 대신 너비 스케일을 코사인으로 오가게
    // 해서 "뒤집히는" 느낌을 낸다(죽었을 때 뜨는 UMG 카드 연출과 같은 방식) — 카메라 각도와
    // 무관하게 항상 세워진 자세 그대로 보인다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Motion")
    float CardHeightScale = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Motion")
    float CardWidthScale = 0.65f;

    UFUNCTION()
    void OnRep_CardVisual();

private:
    void RefreshVisual();
    void LogClientReplicationOnce(const TCHAR* Context);

    bool bInitialReplicationLogged = false;

    // 스폰 위치에서 결정되는(리플리케이트 불필요, 모든 클라이언트가 동일하게 계산) 흔들림 위상차.
    // 여러 카드가 한꺼번에 똑같이 박자 맞춰 흔들리지 않도록 카드마다 다르게 어긋나게 한다.
    float FloatPhaseOffset = 0.0f;
    float FloatElapsedSeconds = 0.0f;
};
