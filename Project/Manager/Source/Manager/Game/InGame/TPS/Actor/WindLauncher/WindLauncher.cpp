#include "Game/InGame/TPS/Actor/WindLauncher/WindLauncher.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/InGame/MainCharacter.h"
#include "Kismet/GameplayStatics.h"

AWindLauncher::AWindLauncher()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);

	WindTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WindTrigger"));
	WindTrigger->SetupAttachment(RootComp);
	// 기본 트리거 박스 크기 (이후 에디터에서 자유롭게 조절 가능)
	WindTrigger->SetBoxExtent(FVector(500.f, 200.f, 200.f));
	WindTrigger->SetCollisionProfileName(TEXT("Trigger"));

	// 오버랩 진입/이탈 이벤트 연결
	WindTrigger->OnComponentBeginOverlap.AddDynamic(this, &AWindLauncher::OnOverlapBegin);
	WindTrigger->OnComponentEndOverlap.AddDynamic(this, &AWindLauncher::OnOverlapEnd);
}

void AWindLauncher::BeginPlay()
{
	Super::BeginPlay();
}

void AWindLauncher::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어 캐릭터만 대상 (적 AI 등 제외)
	AMainCharacter* Player = Cast<AMainCharacter>(OtherActor);
	if (!Player) return;

	// 범위 안에서 "점프 시" 발사하기 위해 진입한 플레이어의 이동모드 변경을 구독한다.
	Player->MovementModeChangedDelegate.AddUniqueDynamic(this, &AWindLauncher::OnCharacterMovementModeChanged);
}

void AWindLauncher::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AMainCharacter* Player = Cast<AMainCharacter>(OtherActor);
	if (!Player) return;

	// 비행 중인 캐릭터는 착지 시 무브먼트 값을 복원해야 하므로 구독 유지.
	// (범위를 벗어나도 아직 날고 있으면 끊지 않는다)
	if (InFlight.Contains(Player)) return;

	// 범위를 벗어나면 구독 해제 → 범위 밖에서의 점프는 무시
	Player->MovementModeChangedDelegate.RemoveDynamic(this, &AWindLauncher::OnCharacterMovementModeChanged);
}

void AWindLauncher::OnCharacterMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (!Character) return;

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move) return;

	// 점프 판정: 직전이 지상(걷기) → 지금 공중(Falling)이고, "위로 솟는 중"일 때.
	// ※ 이 델리게이트는 DoJump 내부의 SetMovementMode에서 호출돼 JumpCurrentCount 증가보다 먼저 온다
	//   → 그 시점 JumpCurrentCount는 아직 0이므로 못 씀. 대신 상승 속도로 점프를 판정한다.
	//   (점프=Velocity.Z>0이 즉시 JumpZVelocity로 세팅됨 / 절벽 낙하=Velocity.Z≈0)
	// 착지 복원: 비행 중이던 캐릭터가 땅(Walking)에 닿으면 무브먼트 값을 원래대로 되돌린다.
	if (Move->MovementMode == MOVE_Walking && InFlight.Contains(Character))
	{
		if (const FWindLaunchSaved* Saved = InFlight.Find(Character))
		{
			Move->AirControl = Saved->AirControl;
			Move->BrakingDecelerationFalling = Saved->BrakingDecelerationFalling;
		}
		InFlight.Remove(Character);

		// 더 이상 박스 안도 아니고 비행도 끝났으면 구독 해제
		if (WindTrigger && !WindTrigger->IsOverlappingActor(Character))
		{
			Character->MovementModeChangedDelegate.RemoveDynamic(this, &AWindLauncher::OnCharacterMovementModeChanged);
		}
		return;
	}

	const bool bEnteredFalling = (Move->MovementMode == MOVE_Falling);
	const bool bWasGrounded    = (PrevMovementMode == MOVE_Walking || PrevMovementMode == MOVE_NavWalking);
	const bool bJumped         = (Move->Velocity.Z > 0.f);

	if (bEnteredFalling && bWasGrounded && bJumped)
	{
		LaunchPlayer(Character);
	}
}

void AWindLauncher::LaunchPlayer(ACharacter* PlayerCharacter)
{
	if (!PlayerCharacter || !TargetIslandPoint) return;

	// 서버 권위 + 조종 클라 예측만 실행.
	// (다른 클라의 시뮬레이트 프록시는 CharacterMovement 복제로 똑같이 날아감)
	if (!HasAuthority() && !PlayerCharacter->IsLocallyControlled()) return;

	const FVector StartLoc  = PlayerCharacter->GetActorLocation();
	const FVector TargetLoc = TargetIslandPoint->GetActorLocation();

	UCharacterMovementComponent* Move = PlayerCharacter->GetCharacterMovement();

	// 중력 크기(양수). 캐릭터의 GravityScale까지 반영해야 실제로 목표에 정확히 떨어진다.
	float GravityZ = Move ? Move->GetGravityZ() : (GetWorld() ? GetWorld()->GetGravityZ() : -980.f);
	const float G = FMath::Max(FMath::Abs(GravityZ), 1.f);

	// 플레이어↔목표의 수평/수직 관계로 발사력을 자동 산출한다.
	const FVector Delta = TargetLoc - StartLoc;
	const FVector HorizDelta(Delta.X, Delta.Y, 0.f);
	const float HorizDist = HorizDelta.Size();
	const FVector HorizDir = (HorizDist > KINDA_SMALL_NUMBER) ? (HorizDelta / HorizDist) : FVector::ZeroVector;

	// 아크 정점 높이: 수평 거리에 비례(멀수록 높게), 최소/최대로 클램프.
	const float Clearance = FMath::Clamp(HorizDist * ApexHeightRatio, MinApexHeight, MaxApexHeight);
	// 기본 정점은 시작점과 목표점 중 높은 쪽보다 Clearance 만큼 위
	const float PeakZ         = FMath::Max(StartLoc.Z, TargetLoc.Z) + Clearance;
	const float RiseFromStart = FMath::Max(PeakZ - StartLoc.Z, 1.f);   // 시작점에서 기본 정점까지 상승

	// 수직 속도에 배율 적용 → 더 높고 세게 솟음. (점프만으로도 확실히 목표로 향하게)
	const float Vz = FMath::Sqrt(2.f * G * RiseFromStart) * VerticalPowerScale;

	// 배율을 반영한 "실제" 정점에서 목표까지 다시 계산 → 비행시간이 늘어 수평속도가 보정되므로 목표엔 그대로 착지
	const float ActualPeakZ  = StartLoc.Z + (Vz * Vz) / (2.f * G);
	const float DropToTarget = FMath::Max(ActualPeakZ - TargetLoc.Z, 1.f);

	const float TimeUp    = Vz / G;
	const float TimeDown  = FMath::Sqrt(2.f * DropToTarget / G);
	const float TotalTime = TimeUp + TimeDown;

	// 수평 속도 = 수평거리 / 비행시간 → 정확히 목표 위에서 떨어짐
	const float Vxy = (TotalTime > KINDA_SMALL_NUMBER) ? (HorizDist / TotalTime) : 0.f;

	const FVector LaunchVelocity = HorizDir * Vxy + FVector(0.f, 0.f, Vz);

	// XY/Z 모두 덮어써서, 방금 시작된 일반 점프를 계산된 포물선으로 교체
	PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);

	// 비행 동안: ① 수평 감속을 0으로 → 발사로 준 수평 속도가 안 죽고 목표까지 그대로 날아감
	//            ② 공중 조작력을 키워 → 원하면 착지 지점을 바꿀 수 있게
	// 원래 값을 기억해 두고(중복 발사 시 덮어쓰지 않음) 착지 시 복원한다.
	if (Move)
	{
		if (!InFlight.Contains(PlayerCharacter))
		{
			FWindLaunchSaved Saved;
			Saved.AirControl = Move->AirControl;
			Saved.BrakingDecelerationFalling = Move->BrakingDecelerationFalling;
			InFlight.Add(PlayerCharacter, Saved);
		}
		Move->AirControl = LaunchAirControl;
		Move->BrakingDecelerationFalling = 0.f;   // 수평 속도 보존 (W 안 눌러도 목표로 날아감)
	}

	// BP 연출 훅 (사운드/VFX 등)
	OnPlayerLaunched(PlayerCharacter);
}
