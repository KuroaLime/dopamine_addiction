#include "Game/InGame/TPS/Actor/WindLauncher/WindLauncher.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
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

	// 오버랩 이벤트 연결
	WindTrigger->OnComponentBeginOverlap.AddDynamic(this, &AWindLauncher::OnOverlapBegin);
}

void AWindLauncher::BeginPlay()
{
	Super::BeginPlay();
}

void AWindLauncher::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 닿은 대상이 나 자신이 아니고, 목표 섬 포인트가 지정되어 있을 때
	if (OtherActor && OtherActor != this && TargetIslandPoint)
	{
		ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
		if (PlayerCharacter)
		{
			FVector StartLoc = PlayerCharacter->GetActorLocation();
			FVector TargetLoc = TargetIslandPoint->GetActorLocation();

			FVector LaunchVelocity;
			// 목표 지점에 도달하기 위한 포물선 속도 계산
			bool bValid = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
				this,
				LaunchVelocity,
				StartLoc,
				TargetLoc,
				0.0f,    // 기본 중력 사용
				800.f    // 궤적의 최고 높이 (에디터에서 수정하거나 변수로 빼도 됨)
			);

			if (bValid)
			{
				// 캐릭터 발사 (LaunchCharacter)
				PlayerCharacter->LaunchCharacter(LaunchVelocity, true, true);
			}
			else
			{
				// 궤적 계산 실패 시 콘솔에 로그 출력
				UE_LOG(LogTemp, Warning, TEXT("AWindLauncher: Target is unreachable via custom arc!"));
			}
		}
	}
}
