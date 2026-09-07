# -*- coding: utf-8 -*-
import base64
import html
import os

ROOT = r"C:\Users\benev\Documents\GitHub\dopamine_addiction"
IMG_DIR = os.path.join(ROOT, "portfolio_images_compressed")


def img_data_uri(name):
    path = os.path.join(IMG_DIR, name)
    with open(path, "rb") as f:
        data = f.read()
    return "data:image/jpeg;base64," + base64.b64encode(data).decode("ascii")


def code(lang, tag, text):
    text = text.strip("\n")
    esc = html.escape(text, quote=False)
    return (
        '<div class="code-block">'
        f'<div class="code-tag">{html.escape(tag)}</div>'
        f'<pre><code class="language-{lang}">{esc}</code></pre>'
        '</div>'
    )


def mermaid(diagram_text, caption=""):
    init = ("%%{init: {'theme':'dark', 'themeVariables': {'primaryColor':'#1c2721',"
            "'primaryTextColor':'#eee7d8','primaryBorderColor':'#dcaa4e','lineColor':'#dcaa4e',"
            "'secondaryColor':'#182019','tertiaryColor':'#182019','fontFamily':'Noto Sans KR'}}}%%\n")
    cap = f'<div class="diagram-cap">{caption}</div>' if caption else ""
    return (
        '<div class="diagram">'
        f'<pre class="mermaid">\n{init}{diagram_text.strip()}\n    </pre>'
        f'{cap}'
        '</div>'
    )


def case(num, title, tag, body_html):
    return (
        '<details class="case">'
        '<summary>'
        f'<span class="case-num">{num}</span>'
        f'<span class="case-head"><h3>{title}</h3><span class="case-tag">{tag}</span></span>'
        '<span class="chev">+</span>'
        '</summary>'
        f'<div class="case-body">{body_html}</div>'
        '</details>'
    )


# ---------------------------------------------------------------- SYSTEM 01
sys01 = case(
    "01", "스폰 시스템과 리플리케이션", "서버 권위 · 리스폰 검증",
    '<h4>문제/배경</h4>'
    '<p>플레이어 캐릭터의 스폰(로비 입장, 사망 후 리스폰, 카드 라운드 종료 후 재배치)은 여러 시점에서 반복적으로 필요했고, '
    '스폰 지점을 서버 권위로 관리하면서 클라이언트에도 상태가 동기화되는 시스템이 필요했습니다.</p>'

    '<h4>설계 및 구현</h4>'
    '<p><code>AA_Spawn</code> 액터에 <code>bReplicates</code>, <code>bAlwaysRelevant</code>를 설정하고 '
    '<code>DOREPLIFETIME</code> + <code>OnRep_BarrierActive</code>로 점유 상태를 클라이언트에 복제해, '
    '<code>USpawnManagerComponent</code>(GameMode 소유)가 서버 권위로 전체 스폰 지점을 관리하도록 했습니다.</p>'
    '<p>최초 구현의 스폰 후보 선택 로직은 후보 배열에서 무작위 인덱스를 하나 뽑아 반환하면서 그 자리에서 배열에서 제거하는 '
    '방식이었습니다. 문제는 이 방식이 후보를 그대로 "소모"한다는 것이었습니다 — 사망→리스폰이 반복되는 매치 특성상 스폰 지점 '
    '개수만큼 뽑히고 나면 후보군이 완전히 고갈되고, 그 이후로는 리스폰 자체가 실패하는 구조적 결함이었습니다. 이를 고치기 위해 '
    '후보를 배열에서 제거하지 않고 커서로 순회하다가, 한 바퀴를 다 돈 커서가 처음으로 돌아왔을 때만 다시 섞는 방식으로 바꿨습니다.</p>'
    + code("cpp", "SpawnManagerComponent.cpp — 커서 기반 재사용 스폰 풀",
           """
AA_Spawn* USpawnManagerComponent::GetUniqueRandomSpawnActor() {
    AvailableSpawns.RemoveAll([](const AA_Spawn* SpawnPoint) {
        return !IsValid(SpawnPoint);
    });

    if (AvailableSpawns.IsEmpty())
    {
        InitializeSpawnPoints();
        AvailableSpawns.RemoveAll([](const AA_Spawn* SpawnPoint) {
            return !IsValid(SpawnPoint);
        });
    }

    if (AvailableSpawns.IsEmpty()) return nullptr;
    if (!AvailableSpawns.IsValidIndex(UniqueSpawnCursor)) UniqueSpawnCursor = 0;

    if (UniqueSpawnCursor == 0)
    {
        Algo::RandomShuffle(AvailableSpawns);
    }

    AA_Spawn* Picked = AvailableSpawns[UniqueSpawnCursor];
    UniqueSpawnCursor = (UniqueSpawnCursor + 1) % AvailableSpawns.Num();
    return Picked;
}
""")
    + '<p>이 방식으로 라운드가 아무리 반복돼도 후보군이 고갈되지 않으면서, 매번 완전 재셔플할 때보다 체감 분포도 고르게 '
      '유지할 수 있었습니다.</p>'
    + '<p>별개의 문제도 있었습니다. 스폰 지점 마커들이 별도로 스트리밍되는 서브레벨에 배치되어 있어서, '
      '<code>USpawnManagerComponent</code>가 <code>BeginPlay</code>에서 월드를 처음 스캔하는 시점에 그 레벨이 아직 '
      '로드되지 않아 후보 배열 자체가 비어버릴 수 있었습니다. 후보군이 비어 있으면 그 시점에 한 번 더 재스캔하도록 해서 '
      '해결했습니다.</p>'
    + '<p>리스폰 처리 자체도 처음에는 <code>TeleportTo</code> 호출 결과를 검사하지 않고 곧바로 성공한 것처럼 다음 로직'
      '(HP 복구, 상태 전환)으로 넘어갔습니다. 텔레포트가 지형이나 다른 오브젝트와 겹쳤을 때 이를 감지할 방법이 없었던 것입니다. '
      '이를 서버 권위 하에 목표 좌표를 검증하고 실패 시 재시도하는 구조로 바꿨습니다.</p>'
    + code("cpp", "Ability_Respawn.cpp — 텔레포트 결과 검증 및 재시도",
           """
const bool bTeleported = Pawn->TeleportTo(TargetLocation, TargetRotation, false, true);
bool bApplied = bTeleported;
if (!bApplied)
{
    bApplied = Pawn->SetActorLocationAndRotation(
        TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
}
...
const float LocationError = FVector::Dist(TargetLocation, Pawn->GetActorLocation());
const bool bWithinTolerance = LocationError <= PositionTeleportMaxLocationError && ...;

// 검증 실패 시 1초 간격으로 재시도
World->GetTimerManager().SetTimer(
    ServerRespawnRetryTimerHandle, this,
    &UAbility_Respawn::TryCompleteServerRespawn, 1.f, false);
""")
    + '<p>HP 복구와 조작 권한 반환은 이 텔레포트가 검증까지 완전히 성공한 뒤에만 실행되도록 순서를 고정해서, 겹침 상태로 '
      '어중간하게 리스폰된 채로 조작이 풀리는 상황을 막았습니다.</p>'
    + '<p>로비 캐릭터 스폰에는 같은 커서 방식 대신, 방 슬롯마다 고정된 스폰 위치가 있고 서버가 이미 슬롯 인덱스를 갖고 있는 '
      '구조적 특성을 살려 멤버 목록의 인덱스와 스폰 위치를 그대로 매칭하는 결정적(deterministic) 방식으로 처리했습니다. '
      '카드 라운드 종료 후에는 서버가 모든 PlayerController를 순회하며 권위적으로 재배치했습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>후보를 제거하는 소모형 풀은 편중보다 "고갈로 인한 실패"가 더 치명적인 문제일 수 있다는 것을 실제로 겪고 체감했습니다.</li>'
    + '<li>스트리밍 레벨을 쓰는 프로젝트에서는 <code>BeginPlay</code> 시점의 월드 스캔이 항상 성공한다고 가정하면 안 되고, 비어있을 때 재스캔하는 방어 로직이 필요함을 체감했습니다.</li>'
    + '<li>위치를 바꾸는 작업(텔레포트)은 호출 성공 여부를 검사하지 않으면 겹침 등으로 실제로는 실패해도 성공한 것처럼 넘어갈 수 있으므로, 결과를 검증하고 실패 시 재시도하는 구조가 필요함을 체감했습니다.</li>'
    + '<li>서버 권위 원칙(HasAuthority 가드)을 스폰처럼 사소해 보이는 시스템에도 일관되게 적용해야 치팅/디싱크를 막을 수 있음을 체감했습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 02
sys02 = case(
    "02", "무기·탄약 네트워크 동기화", "리플리케이션 · 레이스 컨디션",
    '<h4>문제/배경</h4>'
    '<p>탄약 수, 장착 무기, 발사 가능 여부가 서버와 클라이언트 간에 항상 일치해야 했습니다. 무기 타입별로 소지 탄약을 따로 '
    '관리해야 했고, 장착 무기가 바뀔 때 클라이언트 표현(메시 재부착)도 함께 맞춰야 하는 시스템이 필요했습니다.</p>'

    '<h4>설계 및 구현</h4>'
    '<p><code>WeaponComponent</code>의 <code>CurrentAmmo</code>를 <code>Replicated</code> 프로퍼티로 선언해 서버 권위 값을 '
    '클라이언트에 그대로 복제했습니다.</p>'
    + code("cpp", "WeaponComponent.h",
           """
UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon | Ammo")
int32 CurrentAmmo = 30;

// 장전 중 잠금 — true인 동안 사격 불가. 서버에서 set, 복제됨.
UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon | Reload")
bool bIsReloading = false;
""")
    + '<p><code>bIsReloading</code>은 처음부터 있던 필드가 아니었습니다. 재장전 로직은 원래 순수 연출용 Multicast RPC'
      '(<code>Multicast_PlayReloadFeedback</code>) 안에 탄약 이동이라는 게임플레이 판정과 뒤섞여 있었고, 그 상태에서는 '
      '사격 중에도 재장전이 걸릴 수 있는 버그가 있었습니다. 이를 전용 <code>Ability_Reload</code>로 분리하면서 '
      '<code>bIsReloading</code> 잠금을 추가하고, <code>Ability_Fire</code> 쪽에서 재장전 중이면 발사를 막도록 상호 배제시켰습니다.</p>'
    + '<p>장착 무기가 바뀔 때도 문제가 있었습니다. 탄약 수치는 리플리케이션만으로 자동으로 맞지만, 클라이언트 화면에 실제로 '
      '붙어있는 무기 메시는 별도로 다시 붙여줘야 했습니다. <code>OnRep_EquippedGun</code>에서 클라이언트가 무기 메시를 '
      '소켓에 재부착하도록 처리했습니다.</p>'
    + '<p>발사 속도 제한에도 비슷한 시행착오가 있었습니다. 초기에는 트리거를 뗐다 다시 누를 때마다 쿨다운 경과와 무관하게 '
      '즉시 한 발을 쏘고 반복 타이머를 새로 거는 구조라, 짧게 연타하면 무기의 실제 연사속도보다 빠르게 쏠 수 있는 허점이 있었습니다. '
      '<code>LastServerFireTime</code>을 기준으로 쿨다운이 끝났는지 먼저 확인하도록 고쳤습니다.</p>'
    + code("cpp", "Ability_Fire.cpp — 서버 시각 기준 발사 속도 제한",
           """
const float TimeSinceLastShot = CurrentTime - LastServerFireTime;
if (TimeSinceLastShot >= FireRate)
{
    Server_ExecuteFire();
    ...
    LastServerFireTime = World->GetTimeSeconds();
    if (!bFullAuto) return;
    World->GetTimerManager().SetTimer(
        ServerFireTimerHandle, this, &UAbility_Fire::HandleServerFireLoop, FireRate, true);
}
""")
    + '<div class="note"><b>실제 크래시로 이어진 실수 —</b> 이 반복 타이머를 처음 raw <code>this</code>를 캡처하는 람다로 '
      '걸었던 것이 한 달 뒤 실제 데디케이티드 서버 크래시로 이어졌습니다. 어빌리티가 이미 종료되고 정리된 뒤에도 타이머가 '
      '남아있다가 무효화된 멤버에 쓰기를 시도하며 <code>EXCEPTION_ACCESS_VIOLATION</code>이 났습니다. 원인을 추적해 타이머를 '
      '람다 대신 <code>UAbility_Fire</code>의 멤버 함수(<code>HandleServerFireLoop</code>)에 바인딩해 UObject 수명 검사가 '
      '걸리도록 고치고, 호출될 때마다 <code>IsValid</code>/<code>IsActive</code> 상태를 다시 검사하도록 했습니다.</div>'
    + '<p>더 근본적인 설계 문제도 하나 드러났습니다. 이 프로젝트의 자체 GAS는 클라이언트 쪽 발사 예측을 어빌리티의 CDO'
      '(클래스 전체가 공유하는 기본 오브젝트)를 통해 호출하는 구조라, 발사 관련 상태를 어빌리티 멤버에 저장하면 캐릭터별로 '
      '분리되지 않고 모든 플레이어가 상태를 공유해버렸습니다. 그래서 클라이언트 예측 상태는 어빌리티가 아니라 캐릭터별 '
      '인스턴스인 <code>WeaponComponent</code> 쪽에 저장하도록 옮겼습니다.</p>'
    + '<p>무기 타입별 소지 탄약 배열(<code>CarriedAmmoList</code>)은 <code>HasAuthority()</code> 가드로 서버에서만 '
      '수정하도록 강제하고 <code>ForceNetUpdate()</code>로 즉시 반영했습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>"서버 권위 값은 반드시 서버에서만 수정하고 클라는 복제로만 받는다"는 원칙을 탄약 시스템에서 명확히 적용했습니다.</li>'
    + '<li>클라이언트 시각적 표현(무기 재부착)과 서버 권위 데이터(탄약 수)를 분리해서 다뤄야 한다는 것을 실제로 겪으며 체득했습니다.</li>'
    + '<li>타이머 델리게이트에 raw <code>this</code>를 캡처하는 람다를 쓰면, 오브젝트가 정리된 뒤에도 타이머가 남아있다가 무효 메모리에 접근할 수 있다는 것을 실제 서버 크래시로 겪고, 멤버 함수 바인딩 위주로 쓰는 습관이 생겼습니다.</li>'
    + '<li>CDO를 경유해 호출되는 구조에서는 "이 클래스의 멤버"가 실제로는 세션 전체가 공유하는 값일 수 있다는 것을 체감했습니다 — 캐릭터별로 분리돼야 하는 상태는 반드시 인스턴스 컴포넌트 쪽에 둬야 한다는 원칙을 얻었습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 03
sys03 = case(
    "03", "통합 스탯 계산 계층", "단일 진입점 · 데이터 기반",
    '<h4>문제/배경</h4>'
    '<p>캐릭터의 실제 능력치(체력, 이동속도, 무기 데미지 등)는 기본값에 레벨 보정과 상점에서 누적한 업그레이드까지 합쳐진 값이어야 '
    '했습니다. 이 계산이 여러 곳에 흩어지지 않고 항상 같은 결과를 내도록 만드는 시스템이 필요했습니다.</p>'

    '<h4>설계 및 구현</h4>'
    '<p><code>Ability_Fire</code>가 최종 발사속도·데미지를 코드 안에서 직접 계산했는데, 그 계산식이 서로 다른 두 함수'
      '(<code>LocalActivateWithOwner()</code>와 <code>ActivateAbility()</code>)에 그대로 중복돼 있었습니다.</p>'
    + code("cpp", "Ability_Fire.cpp — 개선 전: 두 함수에 중복된 계산식",
           """
float SpeedBonus = (MainPS->WeaponData.LvFireRate * 0.1f) + MainPS->GetAccumulatedUpgrades().LvWeaponFireRate;
finalFireRate = BaseDelay / (1.f + SpeedBonus);
...
FinalDamage += MainPS->GetAccumulatedUpgrades().LvWeaponDamage;
FinalRange  += MainPS->GetAccumulatedUpgrades().LvWeaponRange;
""")
    + '<p>같은 공식이 두 곳에 복붙돼 있으면 한쪽만 고치고 다른 쪽을 놓치기 쉽다는 문제가 있어서, PlayerState에 '
      '"기본값 + 레벨 + 누적업그레이드"를 합산하는 단일 진입점 함수군을 정의했습니다.</p>'
    + code("cpp", "MainPlayerState.cpp — 단일 진입점 스탯 함수",
           """
float AMainPlayerState::GetFinalMaxHP(float BaseMaxHP) const {
    return BaseMaxHP + (PlayerData.LvHealth * 20.0f) + AccumulatedUpgrades.LvHealth;
}
float AMainPlayerState::GetFinalRegenRate(float BaseRegen) const {
    return BaseRegen + (PlayerData.LvHealthRegeneration * 1.0f) + AccumulatedUpgrades.LvHealthRegen;
}
float AMainPlayerState::GetFinalMoveSpeed(float BaseMoveSpeed) const {
    return BaseMoveSpeed + (PlayerData.LvMovementSpeed * 100.0f) + AccumulatedUpgrades.LvMoveSpeed;
}
float AMainPlayerState::GetFinalWeaponDamageMultiplier() const {
    return 1.0f + (WeaponData.LvDamage * 0.15f) + AccumulatedUpgrades.LvWeaponDamage;
}
""")
    + '<p>상점 업그레이드는 <code>FAccumulatedUpgrades</code> 구조체에 리플리케이트되어 누적되도록 했습니다.</p>'
    + '<p>여기서 값은 서버에서 바뀌는데 실제 캐릭터에는 반영되지 않는 스테일 버그가 있었습니다 — 상점에서 이동속도를 '
      '업그레이드해도 값 자체는 바뀌지만, 아무도 그 변경을 캐릭터에 통지하지 않아서 리스폰 등으로 스탯이 다시 계산될 때까지는 '
      '화면상 변화가 없었습니다. 델리게이트로 변경을 즉시 전파하도록 고쳤습니다.</p>'
    + code("cpp", "MainPlayerState.cpp — 변경 즉시 전파",
           """
if (OnAccumulatedUpgradesChangedNative.IsBound())
    OnAccumulatedUpgradesChangedNative.Broadcast(AccumulatedUpgrades);
""")
    + '<p>캐릭터 쪽에서 이 델리게이트를 구독해 값이 바뀌는 즉시 <code>UpdateCharacterStats()</code>를 다시 호출하도록 연결했습니다. '
      '값 자체도 코드에 하드코딩하지 않고 <code>FRandomUpgradeCardDataTable</code>(DataTable)에 업그레이드별 min/max 범위를 '
      '정의해두고, 클라이언트 계산에 맡기면 조작 가능성이 있기 때문에 서버가 런타임에 <code>FMath::FRandRange</code>로 굴려 '
      '누적치에 반영하는 데이터 기반 구조로 마무리했습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>같은 계산식이 여러 함수에 복붙돼 있으면 한쪽만 고치고 다른 쪽을 놓치기 쉽다는 것을 실제로 겪고 단일 진입점의 필요성을 체감했습니다.</li>'
    + '<li>계산 로직을 여러 곳에 중복시키지 않고 "단일 진입점"으로 강제한 것이 이후 기능 확장을 쉽게 만들었습니다.</li>'
    + '<li>데이터를 코드에 하드코딩하지 않고 DataTable로 분리해두면 기획 쪽에서 밸런스 값을 직접 조정할 수 있어 협업이 수월해집니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 04
sys04 = case(
    "04", "PlayerState 아키텍처 리팩토링", "생명주기 · 재접속 복원",
    '<h4>문제/배경</h4>'
    '<p>HP, 골드, 보유 카드 등 세션이 유지되는 동안 계속 살아있어야 하는 플레이어 데이터를 관리하는 구조가 필요했습니다.</p>'

    '<h4>설계 및 구현</h4>'
    '<p>처음에는 HP, 골드 같은 데이터를 폰(Pawn)에 부착된 <code>CharacterStateComponent</code>가 직접 소유했습니다.</p>'
    + code("cpp", "CharacterStateComponent.h — 개선 전",
           """
UPROPERTY(ReplicatedUsing = OnRep_CurrentHP) float CurrentHP;
UPROPERTY(ReplicatedUsing = OnRep_HoldingGold) float HoldingGold;
""")
    + '<p>문제는 폰이 사망→리스폰 시 파괴되고 새로 생성되는 객체라는 점이었습니다 — 폰이 새로 스폰될 때마다 이 컴포넌트도 '
      '통째로 새로 만들어져 HP/골드가 초기값으로 리셋되는 구조적 문제가 있었습니다. 이를 해결하기 위해 세션 내내 유지되어야 '
      '하는 데이터를 폰이 아니라, 플레이어 접속 중 계속 유지되는 PlayerState로 옮겼습니다. 다만 컴포넌트 자체를 지우지는 '
      '않고, 자기 프로퍼티는 비운 채 살아있는 PlayerState를 구독하는 얇은 어댑터로 남겨뒀습니다 — 리스폰으로 새 폰이 생길 '
      '때마다 <code>InitPlayerData()</code>에서 이 컴포넌트를 그 시점의 PlayerState에 다시 연결합니다.</p>'
    + code("cpp", "MainCharacter.cpp — 리스폰 시 어댑터 재연결",
           """
if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>())
{
    if (CharacterState) { CharacterState->BindToPlayerState(PS); }
}
""")
    + '<p>옮기면서 UI 갱신 방식도 함께 다듬었습니다. 이전 값과 새 값을 필드 단위로 비교해, 실제로 바뀐 필드에 대해서만 '
      '델리게이트를 브로드캐스트하도록 해서 안 바뀐 값까지 매번 UI를 갱신하는 낭비를 없앴습니다.</p>'
    + code("cpp", "MainPlayerState.cpp — 필드 단위 변경 감지",
           """
void AMainPlayerState::OnRep_CurPlayerData(FCurPlayerData OldCurPlayerData)
{
    if (OldCurPlayerData.HoldingGold != CurPlayerData.HoldingGold)
    {
        OnGoldChnageNative.Broadcast(CurPlayerData.HoldingGold);
    }
    if (OldCurPlayerData.CurrentHP != CurPlayerData.CurrentHP)
    {
        OnHPChnageNative.Broadcast(CurPlayerData.CurrentHP);
    }
}
""")
    + '<p>이관 자체도 매끄럽지만은 않았습니다. 병합 충돌이 정리되지 않은 채 남아 <code>ResetState()</code>와 구매 처리 함수가 '
      '각각 두 번씩 정의되어 컴파일이 깨진 적이 있었고, 같은 시점에 캐릭터 액터 자체에 <code>SetReplicates(true)</code>가 '
      '빠져있던 것도 함께 발견해 고쳤습니다. 라운드 카운터도 같은 복제+델리게이트 패턴을 적용했는데, 이건 PlayerState가 아니라 '
      '매치 전체가 공유하는 <code>AMainGameState</code>에 뒀습니다 — 라운드는 개별 플레이어가 아니라 세션 전역의 값이기 때문입니다.</p>'
    + '<p>PlayerState로 옮기고 나서도 데이터 생명주기 문제가 완전히 끝난 건 아니었습니다. 플레이어가 접속을 끊었다 재접속하면 '
      '언리얼이 새 PlayerState를 만드는데, 이번엔 Pawn이 아니라 PlayerState 자체가 파괴·재생성되며 같은 종류의 데이터 유실이 '
      '재발했습니다. <code>AMainGameMode::Logout()</code>에서 현재 PlayerState의 데이터를 스냅샷으로 캡처해두고, '
      '<code>PostLogin()</code>에서 새로 생성된 PlayerState에 그 스냅샷을 복원하는 재접속 처리를 추가해 대응했습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>"이 데이터의 생명주기가 무엇과 같이 가야 하는가"를 먼저 따지지 않으면 나중에 훨씬 큰 리팩토링 비용이 든다는 것을 직접 경험했습니다.</li>'
    + '<li>언리얼의 Pawn vs PlayerState vs GameState 생명주기 차이를 실전에서 체득했습니다.</li>'
    + '<li>데이터를 올바른 객체로 옮겨도 그 객체 자체가 파괴·재생성될 수 있다는 것(재접속 시 PlayerState 재생성)까지 고려해야 생명주기 문제가 진짜로 끝난다는 것을 겪었습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 05
sys05 = case(
    "05", "체력 재생 컴포넌트", "정밀도 · 리슨 서버 UI",
    '<h4>문제/배경</h4>'
    '<p>전투 중 데미지를 받다가 일정 시간 공격을 받지 않으면 서서히 체력이 회복되는 기능이 필요했습니다.</p>'

    '<h4>설계 및 구현</h4>'
    '<p><code>HealthRegenComponent</code>를 <code>PossessedBy</code> 시점에 동적으로 생성/등록했습니다 — 폰 소유권이 '
    '실제로 설정된 시점에 붙여야 오너 참조가 유효함을 보장하기 때문입니다. <code>OnTakeAnyDamage</code> 이벤트가 발생하면 '
    '재생 쿨다운 타이머를 리셋해 전투 중에는 회복이 시작되지 않도록 했습니다.</p>'
    '<p>회복량 계산은 처음부터 소수점 잔여값을 누적하는 방식으로 설계했습니다 — 매 틱 회복량을 정수로 즉시 반올림하면 깎이는 '
    '오차가 누적돼 실제 회복 속도가 설정값보다 느려지기 때문에, 잔여값을 계속 더하다가 1.0을 넘는 순간에만 정수 HP로 반영했습니다.</p>'
    + '<p>이 컴포넌트는 이후 다른 파일들과 병합하는 과정에서 실제 문제 두 가지를 드러냈습니다. 하나는 회복 상한이 하드코딩돼 '
      '있었다는 것입니다 — 최대 체력을 구할 때 기준값 100을 직접 박아 넣고 있었는데, 실제 캐릭터의 기준 최대 체력은 150이었습니다. '
      '그 결과 자연 회복만으로는 최대 체력의 마지막 50을 영구히 채울 수 없었습니다. 다른 하나는 리슨 서버(호스트)가 자기 자신의 '
      '화면에서는 자연 회복이 UI에 반영되지 않는 문제였습니다 — 언리얼의 리플리케이션은 값을 변경한 서버 자신에게는 '
      '<code>OnRep_*</code>을 호출해주지 않기 때문입니다. 두 문제 모두 최대 체력 기준을 하나로 통일하고, HP가 실제로 바뀐 '
      '경우에만 명시적으로 델리게이트를 브로드캐스트하도록 만들어 함께 고쳤습니다.</p>'
    + code("cpp", "HealthRegenComponent.cpp — 잔여값 누적 + 명시적 브로드캐스트",
           """
void UHealthRegenComponent::TickRegeneration()
{
    APawn* PawnOwner = Cast<APawn>(GetOwner());
    AMainPlayerState* PS = PawnOwner ? Cast<AMainPlayerState>(PawnOwner->GetPlayerState()) : nullptr;
    if (PS)
    {
        float MaxHP = PS->GetCurrentMaxHP();
        float RegenRate = PS->GetFinalRegenRate(0.0f);

        float RecoveryAmount = RegenRate * RegenInterval;
        FractionalHP += RecoveryAmount;
        if (FractionalHP >= 1.0f)
        {
            int32 AddHP = FMath::FloorToInt(FractionalHP);
            FractionalHP -= static_cast<float>(AddHP);
            const int32 OldHP = PS->CurPlayerData.CurrentHP;
            PS->CurPlayerData.CurrentHP = FMath::Min(static_cast<int32>(MaxHP), OldHP + AddHP);
            if (PS->CurPlayerData.CurrentHP != OldHP)
            {
                PS->OnHPChnageNative.Broadcast(PS->CurPlayerData.CurrentHP);
                PS->ForceNetUpdate();
            }
        }
    }
}
""")
    + '<p><code>GetCurrentMaxHP()</code>가 <code>BaseMaxHealth</code>(150) 기준으로 계산되도록 고치면서 회복 상한 문제가 '
      '풀렸고, <code>OnHPChnageNative.Broadcast()</code>를 조건부로 직접 호출하면서 리슨 서버 자신의 화면도 다른 클라이언트와 '
      '동일하게 즉시 갱신되도록 만들었습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>시간 기반으로 누적되는 값을 정수로 반영할 때는 매번 반올림하지 않고 잔여값을 누적해서 넘어갈 때만 반영해야 회복 속도 손실이 없다는 것을 설계 단계부터 고려했습니다.</li>'
    + '<li>여러 곳에서 각자 계산하던 "기준 최대체력" 값을 하나로 통일하지 않으면, 리셋/회복/UI 비율 계산이 서로 다른 기준을 참조하며 조용히 어긋날 수 있다는 것을 병합 충돌을 고치며 체감했습니다.</li>'
    + '<li>언리얼 리플리케이션에서 서버 자신은 자기가 바꾼 값에 대해 <code>OnRep</code>이 호출되지 않는다는 함정을 실제로 겪고, 값이 바뀌는 시점에 명시적으로 델리게이트를 쏴주는 패턴으로 대응하는 법을 익혔습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 06
sys06 = case(
    "06", "상점 시스템 — 페이즈 제어 · UI · 무기 개조 경제", "입력 제약 · RPC 검증",
    '<h4>문제/배경</h4>'
    '<p>라운드마다 전투(TPS) → 카드(섯다) → 상점이 순환하는 구조에서, 상점 페이즈 동안에는 이동/사격 등 전투 입력이 막혀야 '
    '했습니다. 또한 초기 상점 UI는 "랜덤 카드 강화 1개 + 캐릭터 고정 스탯 강화 3개" 버튼만 있어 화면이 비어 보였고, 무기 스탯'
    '(공격력/연사력/사거리/탄창/재장전)을 직접 구매로 올릴 수 있는 경로도 필요했습니다.</p>'

    '<h4>설계 및 구현 — 페이즈 제어</h4>'
    '<p><code>EDediServerPhase</code>에 <code>PreBattleShop</code>이라는 새 서버 페이즈를 도입하고, '
    '<code>bShopAvailable</code> 플래그를 리플리케이트해 서버(RPC 게이트)와 클라이언트(어빌리티 활성화 조건) 양쪽에서 동일한 '
    '조건으로 체크하도록 했습니다. 상점 페이즈 중에는 개별 어빌리티마다 조건문을 넣는 대신, <code>ShopInputHandler</code>로 '
    '입력 매핑 컨텍스트 자체를 나가기만 가능한 최소 입력으로 교체하는 방식을 택했습니다 — 이렇게 하면 새 어빌리티가 추가돼도 '
    '페이즈 제약을 따로 신경 쓸 필요가 없어집니다. 처음 구현했을 때는 상점에 들어가면 마우스 커서만 보이게 하고 입력 모드는 '
    '<code>FInputModeGameOnly()</code>로 남겨뒀는데, 이 모드에서는 UI가 클릭을 받지 못하고 모든 입력이 그대로 캐릭터 이동으로 '
    '새어나갔습니다. 상점 진입 시에는 <code>FInputModeGameAndUI()</code>로 바꾸도록 고쳐서 정리했습니다.</p>'
    '<p>카드 버리기(<code>Ability_CardDiscard</code>)는 짧게 누르는 것과 길게 눌러 확정하는 것을 구분해야 했습니다. 홀드 '
    '확인 상태 머신을 만들어서 일정 시간 이상 누르고 있을 때만 확정되도록 처리했습니다.</p>'
    + code("cpp", "Ability_CardDiscard.cpp — 홀드 확인 상태 머신",
           """
void UAbility_CardDiscard::StartHold(AActor* InOwner, UWorld* World)
{
    if (!World) return;
    bHoldConfirmed = false;
    HoldOwner = InOwner;

    World->GetTimerManager().SetTimer(
        HoldConfirmTimerHandle, this, &UAbility_CardDiscard::ConfirmHold,
        HoldThreshold, false);
}

void UAbility_CardDiscard::ConfirmHold()
{
    bHoldConfirmed = true;
    if (AActor* Owner = HoldOwner.Get())
    {
        if (UTpsPlayerMainHUD* HUD = ResolveHUD(Owner))
        {
            HUD->ConfirmDiscardSelectedCard();
        }
    }
    EndAbilityNow();
}
""")
    + '<p>상점 방벽은 처음엔 상점이 열리고 닫히는 두 시점에서만 콜리전을 켜고 끄는데, 그 사이에 있는 준비 라운드(Ready) '
      '구간에는 방벽을 켜는 호출이 아예 없어서 상점이 열리기 전인데도 배틀로얄 구역으로 미리 걸어 들어갈 수 있는 틈이 있었습니다. '
      '이후 상점 방벽의 콜리전+가시성을 게임 페이즈 전환 지점에 연동해, 준비 라운드와 상점 라운드에서만 활성화되고 그 외 구간'
      '(전투/카드/결과)에서는 항상 비활성화되도록 정리했습니다. 방벽 메시 자체도 원래는 콜리전 상태와 무관하게 항상 안 보이도록 '
      '만들어져 있어서 이유 없이 막히는 투명 벽처럼 보였는데, 이때 콜리전 상태와 가시성을 동기화해 막혀 있을 때는 벽도 눈에 '
      '보이도록 함께 고쳤습니다.</p>'
    + '<div class="note"><b>정직하게 짚을 점 —</b> 상점 방벽 콜리전이 켜지는 시점에 플레이어가 이미 그 자리에 텔레포트되어 있어 '
      '겹쳐서 "바닥에 박히는" 버그를 코드 추적으로 원인만 특정했고, 좌표 오프셋 보정 같은 근본 수정은 적용하지 않은 채 이동 잠금 '
      '해제로 임시 완화만 했습니다.</div>'

    + '<h4>설계 및 구현 — UI와 경제</h4>'
    + '<p>HUD가 쓰던 <code>Lv_Image[8]</code> + 다이나믹 머티리얼 인스턴스 패턴을 상점 위젯에도 동일하게 적용해서, 체력/이동속도/'
    + '체력재생 + 무기 스탯 5종의 현재 수치를 실시간 게이지로 보여주도록 이식했습니다. 이 과정에서 기존 HUD 코드에 존재 여부를 '
    + '확인하는 배열과 실제로 역참조하는 배열이 다른 잠재 널 포인터 버그를 발견했습니다 — 지금까지는 이미지 위젯이 있으면 머티리얼도 '
    + '항상 같이 있었기 때문에 우연히 문제가 없었을 뿐인데, 팀원의 블루프린트 리팩토링 커밋이 그 전제를 깨뜨리며 실제 크래시로 '
    + '드러난 것이었습니다. git 커밋 diff까지 추적해 근본 원인을 특정하고 수정했습니다.</p>'
    + '<p>상점 구매 RPC가 처음부터 이렇게 안전했던 건 아니었습니다. 캐릭터 고정 스탯 구매 RPC의 초기 구현은 골드 부족 체크가 '
    + '통째로 주석 처리된 채 가격과 무관하게 고정 5골드만 차감했고, 페이즈 체크도 상점이 아니라 사실상 반대 의미인 배틀로얄 페이즈 '
    + '여부를 검사하고 있었습니다.</p>'
    + code("cpp", "MainPlayerController.cpp — 개선 전: 골드 체크 주석 처리", """
if (CurrentPhase != EGamePhase::Shop || !GM || !GM->IsBattleRoyalePhase())
    return;
...
/*int32 Cost = GetStaticUpgradeCost(UpgradeType, CurrentLevel);
if (PS->CurPlayerData.HoldingGold < Cost) { return; }*/
PS->AddGold(-5);
""")
    + '<p>골드 체크를 되살리려던 다음 수정에서는 부호를 빠뜨려 오히려 구매할수록 골드가 늘어나는 버그를 만들었다가 같은 날 안에 '
      '다시 고쳤습니다. 이후 서버 RPC 전반에 레이트리밋을 도입하면서 인덱스 범위를 검증하는 <code>_Validate</code>, 정확한 '
      '상점 페이즈 체크, 사망 중 구매 방지까지 한꺼번에 갖추게 되었습니다. 무기 개조 상점은 이 패턴이 이미 정착된 뒤에 만들어졌기 '
      '때문에 처음부터 같은 보호 장치를 갖추고 시작할 수 있었습니다.</p>'
    + '<p>무기 개조 상점은 상점 라운드가 시작될 때마다 서버가 무기 스탯 5종 중 3종을 무작위로 뽑아 리플리케이트하는 방식으로 '
      '만들었습니다. 이 랜덤 상태를 어디에 저장할지가 고민이었는데, 상점 화면이 라운드 단위로 모든 플레이어가 같은 조건을 봐야 '
      '하는 구조라 PlayerState가 아니라 매치 전체가 공유하는 <code>AMainGameState</code>에 리플리케이트하는 쪽을 택했습니다.</p>'
    + code("cpp", "MainPlayerController.cpp — 무기 개조 구매 RPC (검증 완비)", """
bool AMainPlayerController::Server_PurchaseWeaponUpgrade_Validate(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < 3;
}

void AMainPlayerController::Server_PurchaseWeaponUpgrade_Implementation(int32 SlotIndex)
{
    if (!TryConsumeServerRpcRateLimit(
        ShopWeaponUpgradeRateLimitState,
        ShopUpgradeSelectionMinimumIntervalSeconds,
        TEXT("ShopWeaponUpgrade")))
    {
        return;
    }

    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
    AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (CurrentPhase != EGamePhase::Shop ||
        !GM ||
        !GM->IsShopRequestAllowed() ||
        !GS ||
        !PS ||
        PS->CurPlayerData.CurrentHP <= 0)
    {
        return;
    }
    // ... 이후 골드 검증 및 구매 처리
}
""")
    + '<p>무기 개조는 캐릭터 고정 스탯 강화(100 Gold)보다 저렴한 고정가(70 Gold)로 책정하고 스탯 증가폭도 랜덤 카드 강화보다 '
      '작게(+0.5) 잡아, 카드 뽑기의 가치를 깎아먹지 않는 보조 구매 수단으로 자리잡도록 마무리했습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>페이즈별 입력 제약을 각 어빌리티 내부 조건문으로 흩뿌리는 대신 입력 매핑 컨텍스트 교체로 처리하니, 새 어빌리티가 추가돼도 페이즈 제약을 따로 신경 쓸 필요가 없어졌습니다.</li>'
    + '<li>UMG <code>BindWidget</code> 프로퍼티는 디자이너의 위젯 이름과 C++ 프로퍼티 이름이 정확히 같아야 자동 바인딩되는데, 이름만 같고 <code>meta=(BindWidget)</code>을 빠뜨리면 "이미 존재하는 프로퍼티"라는 모호한 컴파일 에러가 난다는 걸 실전에서 겪고 원인을 특정했습니다.</li>'
    + '<li>팀원의 리팩토링 커밋이 제 코드의 암묵적 전제를 깨뜨릴 수 있다는 것을 널 포인터 버그를 통해 실감했습니다 — 코드의 정확성만큼이나 방어적 설계가 필요함을 체득했습니다.</li>'
    + '<li>서버 권위 랜덤 상태를 PlayerState가 아니라 GameState에 둘지 고민 후, 라운드 단위로 공유되는 화면 구조에 맞춰 GameState를 선택했습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 07
sys07 = case(
    "07", "황금 고블린 AI — \"AI 디렉터\" 구현", "스폰 페이싱 · Behavior Tree",
    '<h4>문제/배경</h4>'
    '<p>맵을 배회하다 처치하면 후한 보상을 주는 회피/도주형 레어 몬스터 "황금 고블린"을 만들면서, 몬스터를 고정된 위치나 완전히 '
    '무작위로만 스폰시키는 대신, 최근에 전투가 있었는지(소강 상태 여부)와 플레이어들의 위치를 계속 지켜보다가 "적절한 순간"에만 '
    '등장시키는 스폰 페이싱 시스템, 이른바 Left 4 Dead의 "AI 디렉터"를 이 프로젝트에 직접 구현해보는 것을 목표로 잡았습니다. '
    '이 스폰 판단을 전담할 <code>UGoldenGoblinDirectorComponent</code>를 만들고, 디렉터가 실제로 등장시킬 대상으로 황금 '
    '고블린의 캐릭터/AI 컨트롤러/Behavior Tree도 함께 만들었습니다.</p>'

    '<h4>설계 및 구현 — 스폰 페이싱 (이번 작업의 핵심 목표)</h4>'
    '<p>개별 고블린의 행동(Behavior Tree)과 "언제/어디에 등장시킬 것인가"는 처음부터 완전히 분리해서 설계했습니다. '
    '<code>UGoldenGoblinDirectorComponent</code>는 GameMode가 소유하고 BattleRoyale 페이즈 동안에만 활성화되며, '
    'Character/AIController는 이 컴포넌트의 존재 자체를 모릅니다 — Character는 <code>TakeDamage</code>에서 디렉터를 찾아 '
    '전투가 일어났다는 사실만 통지하고, 실제 스폰 판단은 전부 디렉터 쪽에 있습니다. 이 통지는 고블린 자신이 맞을 때뿐만 아니라 '
    '플레이어끼리의 TPS 교전에서 플레이어가 맞을 때도 함께 걸리도록 만들어서, "소강 상태" 판정이 맵 전체의 전투 상황을 '
    '반영하도록 했습니다.</p>'
    + mermaid("""
flowchart LR
    Player["플레이어 또는 고블린 피격"] -->|NotifyCombatEvent| Director["UGoldenGoblinDirectorComponent"]
    Director -->|4가지 조건 충족 시| Spawn["AGoldenGoblinCharacter 스폰"]
    Spawn -->|TakeDamage로 사망| Delegate["OnGoblinDied 델리게이트"]
    Delegate --> Controller["AGoldenGoblinAIController"]
    Controller -->|Blackboard IsDead 갱신| BT["BTD_IsDead가 사망 브랜치 강제"]
""")
    + '<p>스폰 조건은 네 가지를 모두 만족해야 합니다: 페이즈 시작 후 최소 대기시간, 최근 전투 이벤트가 일정 시간 이상 없는 '
      '"소강 상태", 모든 플레이어로부터 최소 거리 이상 떨어진 지점, 내비메시 상에서 실제로 도달 가능한 위치(재시도 횟수 제한). '
      '이 중 최소 대기시간은 현재 테스트 편의상 10초로 낮추어둔 값이고, 실제 밸런싱 단계에서는 60초 이상으로 늘릴 계획입니다.</p>'
    + '<p>첫 구현은 "페이즈당 1회만 스폰"하고 그 뒤로는 타이머 자체를 꺼버리는 방식이었습니다. 이 방식은 스폰된 고블린이 일찍 '
      '죽어버리면 그 라운드 내내 다시는 등장하지 않는다는 한계가 있어서, 살아있는 개체를 약한 참조 배열로 계속 추적하고, 체크 '
      '주기마다 무효화/사망한 개체를 정리한 뒤 최대 동시 개체 수(2마리) 미만이면 계속 보충 스폰을 시도하는 방식으로 바꿨습니다.</p>'
    + code("cpp", "GoldenGoblinDirectorComponent.cpp — 상시 순환 스폰 루프", """
ActiveGoblins.RemoveAll([](const TWeakObjectPtr<AGoldenGoblinCharacter>& Goblin)
{
    return !Goblin.IsValid() || Goblin->IsGoblinDead();
});
if (ActiveGoblins.Num() >= MaxConcurrentGoblins)
{
    return;
}
""")
    + '<p>"한 번 등장시키고 끝"이 아니라 "동시에 몇 마리가 존재하는가"를 디렉터가 지속적으로 관리하는 형태로 바뀐 것입니다.</p>'

    + '<h4>설계 및 구현 — 페이싱 대상: 캐릭터 / AI 컨트롤러 / Behavior Tree</h4>'
    + '<p>체력 시스템은 새로 만들지 않고 플레이어가 쓰는 것과 같은 자체 GAS를 그대로 재사용해서, 데미지 적용·죽음 판정을 '
    + '플레이어와 동일한 경로로 태웠습니다.</p>'
    + code("cpp", "GoldenGoblinAIController.cpp — Blackboard 연동", """
if (HasAuthority() && AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
{
    UPFGAttributeSet* AttributeSet = AbilitySystemComponent->AttributeSet;
    AttributeSet->SetMaxHealth(GoblinMaxHealth);
    AttributeSet->ApplyHealthDelta(GoblinMaxHealth);
}
""")
    + '<p><code>AGoldenGoblinAIController</code>는 <code>UAIPerceptionComponent</code>(Sight, 시야반경 1500 / 시야이탈 '
      '1800 / 시야각 90도)로 플레이어를 감지해 Blackboard의 TargetActor를 갱신하고, 시야를 잃으면 그 값을 지워 BT가 Patrol '
      '브랜치로 복귀하도록 했습니다. BT 흐름은 평시 배회(Patrol) → 감지 시 회피 기동(Evade) → 사망 시 아이템 드랍 + 소멸 '
      '세 단계입니다.</p>'
    + '<div class="table-wrap"><table><tr><th>클래스</th><th>역할</th></tr>'
      '<tr><td><code>BTT_GetRandomLocation</code> / <code>BTT_PatrolMoveTo</code></td><td>배회 지점 선정 및 이동</td></tr>'
      '<tr><td><code>BTT_FindEscapeLocation</code> / <code>BTT_EvasiveManeuver</code></td><td>도주 지점 계산 및 회피 기동(이동속도를 EvasiveMoveSpeed로 전환)</td></tr>'
      '<tr><td><code>BTT_PlayEscapeAction</code></td><td>도주 중 회피 몬타주 재생(재생 완료까지 대기하는 레이턴트 태스크)</td></tr>'
      '<tr><td><code>BTD_IsDead</code></td><td>Blackboard의 IsDead를 감시해 사망 브랜치 우선순위를 강제하는 Decorator</td></tr>'
      '<tr><td><code>BTT_DropItem</code> / <code>BTT_Disappear</code> / <code>BTT_StopMovement</code></td><td>사망 시 보상 드랍, 소멸 연출, 이동 정지</td></tr>'
      '</table></div>'
    + '<p>데디케이티드 서버는 렌더링을 하지 않고 파티클 스폰은 리플리케이트되지 않기 때문에, 사망 이펙트는 서버 전용 Task가 '
      'Multicast RPC를 호출해 모든 클라이언트가 각자 로컬로 이펙트를 스폰하도록 처리했습니다.</p>'
    + '<p><code>BTD_IsDead</code>를 만들면서 엔진 자체의 함정도 하나 만났습니다 — 블랙보드 키가 에셋 로드 직후에는 종종 '
      '유효하지 않은 ID로 굳어있는 현상이 있어서, 이름 기준으로 강제 리졸브하는 코드를 따로 넣어 우회했습니다. 죽음 처리 자체도 '
      '여러 컴포넌트를 가로지르는 비동기 체인이라, 각 단계마다 로그를 심어 순서대로 제대로 이어지는지 검증하면서 디버깅했습니다.</p>'

    + '<h4>설계 및 구현 — 보상: 고정값에서 범위 랜덤으로</h4>'
    + '<p>초기 구현은 고정 골드 보상(5000)이었습니다. "이 몬스터만 잡으면 그 라운드는 확정으로 유리해진다"는 식으로 공략이 '
    + '고정돼버리는 문제가 있어서, 100~3000 범위로 바꾸고 <code>BeginPlay</code> 시점에 서버 권위로 한 번만 굴려 고정해두는 '
    + '방식으로 개선했습니다.</p>'
    + code("cpp", "GoldenGoblinCharacter.cpp — 범위 랜덤 보상, 서버 1회 롤링", """
if (HasAuthority())
{
    const int32 RangeMin = FMath::Min(MinGoldRewardAmount, MaxGoldRewardAmount);
    const int32 RangeMax = FMath::Max(MinGoldRewardAmount, MaxGoldRewardAmount);
    RolledGoldRewardAmount = FMath::RandRange(RangeMin, RangeMax);
}
""")

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>개체 행동(BT)과 등장 페이싱(Director)을 완전히 분리해 설계하니, 스폰 로직만 따로 반복 개선(1회 스폰 → 동시 N마리 풀 유지)할 수 있었습니다.</li>'
    + '<li>요구사항 정의부터 클래스 구조, BT, 디렉터까지 설계·구현을 직접 완성했습니다.</li>'
    + '<li>보상을 고정값으로 두면 공략이 고정된다는 것을 실제로 체감하고, 범위 랜덤 + 서버 권위 1회 롤링으로 대응했습니다.</li>'
    + '<li>이 프로젝트 전체를 관통하는 "서버 권위" 원칙을 신규 시스템에도 처음부터 일관되게 적용했습니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 08
sys08 = case(
    "08", "렌더링 성능 근본 원인 조사", "프로파일링 · 측정 도구 자작",
    '<h4>문제/배경</h4>'
    '<p>TPS 전투 페이즈에서 프레임이 무겁다는 체감이 있었지만, 정확히 어디가 병목인지는 알려진 바가 없었습니다. 막연한 감으로 '
    'GPU 설정을 만지기 전에, 실제로 시간이 어디서 쓰이는지부터 측정으로 확인하는 것을 목표로 잡았습니다.</p>'

    '<h4>측정 도구부터 직접 만들어야 했다</h4>'
    '<p>기존 프로파일링 도구들이 이 프로젝트의 조건에 그대로 맞지 않았습니다. Unreal CsvProfiler가 뽑는 CSV는 중복 컬럼명 '
    '때문에 PowerShell의 <code>Import-Csv</code>로 파싱하면 실패해서, 필요한 컬럼만 헤더 위치로 직접 찾아 파싱하는 '
    '<code>Analyze-Csv.ps1</code>을 만들었습니다. 또한 이 게임은 멀티플레이어 게임모드(<code>bPauseable=false</code>)라 '
    '에디터 PIE에서 <code>pause</code>가 거부되고 <code>Simulate In Editor</code>도 게임이 아니라 에디터 자체를 측정해버려서, '
    '시점을 일정하게 유지한 채 반복 측정하기 위한 마우스 시점 자동화 스크립트(<code>look-driver-safe.py</code>)도 별도로 '
    '만들어야 했습니다.</p>'

    '<h4>1차 결론: "GPU가 무겁다" — 그리고 정정</h4>'
    '<p>에디터 PIE에서 <code>ProfileGPU</code>로 잰 첫 측정은 TPS 섬 구간에서 GPU 39.3ms(P90 46.3ms)까지 나와, 자연스럽게 '
    '"GPU 바운드"라는 결론으로 이어졌습니다. 그런데 이 결론은 틀렸습니다. Standalone 빌드로 다시 측정하면서 '
    '<code>r.ScreenPercentage</code>를 100→70→100으로 스윕해봤는데, 해상도를 49%로 줄이면 GPU Time은 확실히 떨어지는데도 '
    '실제 프레임 타임(렌더 스레드)은 전혀 움직이지 않았습니다.</p>'
    + code("text", "ScreenPercentage 스윕 결과 (ms)", """
GPU Time         14.36/14.01 (100%) -> 10.67/10.58 (70%) -> 14.59/14.48 (100%)
Draw(렌더스레드)  15.82/16.62 (100%) -> 16.03/16.39 (70%) -> 16.00/15.94 (100%)
""")
    + '<p>GPU를 아무리 깎아도 프레임이 그대로라는 것은 병목이 GPU가 아니라 렌더 스레드라는 뜻이었습니다. 에디터 PIE에서 GPU가 '
      '커 보였던 이유도 뒤늦게 밝혀졌습니다 — 에디터는 병렬 렌더링 처리 때문에 CSV에 렌더 스레드 시간이 거의 0으로 잡혀서, '
      '렌더 스레드가 실제로 얼마나 쓰이는지 애초에 볼 수 없는 측정 환경이었던 것입니다. <code>stat dumpframe</code>으로 렌더 '
      '스레드 14ms를 뜯어보면 그중 약 4ms는 작업이 아니라 병렬 visibility 워커와 RHI 스레드를 기다리는 CPU 스톨이었습니다.</p>'

    + '<h4>근본 원인: 씬 인스턴스 3,549개 중 3,543개가 Movable</h4>'
    + code("text", "stat dumpframe -root=initviews", """
Num Dynamic Instances : 3,543
Num Static Instances  :     1
Scene Instance Count  : 3,549
""")
    + '<p>섬을 띄우는 <code>UFloatingMotionComponent</code>는 섬 루트의 Mobility를 Movable로 요구하고, 그 아래 붙은 나무·바위·'
      '구조물이 전부 이 Mobility를 그대로 상속받습니다. 그 결과 씬 전체가 사실상 Movable이 되어 정적 컬링 캐시와 드로우 커맨드 '
      '캐싱을 쓸 수 없고, 매 프레임 relevance를 재계산하며, 레이트레이싱 dynamic update primitive 3,730개가 매 프레임 갱신됐습니다. '
      '다만 "부유 모션 자체가 무겁다"는 가설도 A/B/B/A 순서로 뒤집어 검증했는데, 컴포넌트를 껐다 켜는 순서를 바꿔도 결과가 그대로 '
      '따라와서 인과가 없었습니다. 비용은 "움직이고 있다"는 사실이 아니라 "Mobility가 Movable이다"라는 타입 자체에서 나오고 있었습니다.</p>'

    + '<h4>적용한 수정: 로프 브릿지를 Lumen에서 제외</h4>'
    + '<p>섬과 섬을 잇는 로프 브릿지는 섬이 부유할 때마다 매 프레임 다시 빌드되는데, 이게 Lumen 서페이스 캐시의 "메시 카드"를 '
    + '매 프레임 무효화시키고 있었습니다. 렌더 스레드에서 5개 다리를 기준으로 A/B/A/B로 측정한 결과, 로프만 Lumen에서 빼면 효과가 '
    + '없었고 다리 전체를 제외해야 비용이 사라졌습니다. Lumen 카드는 표면적에 비례해서 비용이 매겨지는데, 로프는 가늘어서 표면적이 '
    + '거의 없고 실제 비용은 판자와 기둥 쪽에서 나오고 있었기 때문입니다. 다리를 구성하는 모든 프리미티브 컴포넌트를 한꺼번에 순회하며 '
    + '제외하는 함수로 구현했습니다.</p>'
    + code("cpp", "RopeBridge.cpp — Lumen 전역조명 기여 제외", """
void ARopeBridge::ExcludeFromLumen(UPrimitiveComponent* Prim)
{
    if (!Prim || (!Prim->bAffectDynamicIndirectLighting && !Prim->bAffectDistanceFieldLighting))
    {
        return;
    }

    Prim->bAffectDynamicIndirectLighting = false;
    Prim->bAffectDistanceFieldLighting = false;
    if (Prim->IsRegistered())
    {
        Prim->MarkRenderStateDirty();
    }
}
""")
    + '<p>다리가 트인 공중에 걸쳐 있어 어차피 주변에 반사광(GI)을 거의 기여하지 않는다는 점도 이 최적화를 선택한 근거가 됐습니다 '
      '— 이 플래그를 끄는 것은 다리가 Lumen 전역조명에 "기여"하는 것만 막을 뿐, 다리 자체가 빛을 받아 보이는 데는 영향이 없습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>"GPU가 무겁다"는 1차 결론을 실제로는 렌더 스레드가 GPU를 기다리는 스톨이 대부분이었다는 사실로 정정하면서, 에디터 PIE 측정치를 그대로 믿으면 안 되고 Standalone 빌드 + ScreenPercentage 스윕처럼 원인을 격리하는 절차가 필요하다는 것을 체감했습니다.</li>'
    + '<li>Movable Mobility가 하위 컴포넌트 전체로 전파된다는 것, 그리고 그 비용이 "움직임" 자체가 아니라 "타입"에서 나온다는 것을 A/B 검증으로 직접 확인했습니다.</li>'
    + '<li>Lumen 카드 비용이 표면적에 비례한다는 사실을 로프/다리 A/B로 실측하면서, "작아 보이는 오브젝트"가 항상 저비용은 아니라는 것을 배웠습니다.</li>'
    + '<li>기존 도구가 이 프로젝트의 측정 조건(멀티플레이어라 일시정지 불가, CSV 중복 컬럼명)에 맞지 않아 프로파일링 도구 자체를 직접 만들어야 했습니다.</li>'
    + '<li><b>남은 과제 —</b> 근본 원인인 "씬 전체가 Movable"이라는 문제 자체는 아직 남아 있습니다. 가장 큰 효과가 기대되는 섬별 정적 장식물의 ISM/HISM 병합은 아직 착수 전이고, 로프 브릿지 Lumen 제외는 그 전에 먼저 잡을 수 있었던 개별 항목에 대한 부분적 완화입니다.</li>'
    + '</ul>'
)

# ---------------------------------------------------------------- SYSTEM 09
sys09 = case(
    "09", "카드 배치 실패 진단 로깅", "관측성 · 중복 제거",
    '<h4>문제/배경</h4>'
    '<p>TPS 전투 맵의 섬 위에 카드를 띄울 때는 내비메시 보행 가능, 존(Zone) 경계 내부, 기존 카드와 충분한 거리, 장애물에 가려지지 '
    '않음, 머리 위 공간 확보, 진입금지구역이 아닐 것까지 다섯 가지 조건을 모두 만족해야 했습니다. 이 다섯 가지 검사를 내비메시 기반 '
    '배치 경로와 그 폴백인 지면 트레이스 기반 배치 경로가 각각 따로 구현해 두었고, 실패했을 때는 "실패했다"는 로그 한 줄뿐 왜 '
    '실패했는지는 알 수 없었습니다.</p>'

    '<h4>설계 및 구현 — 중복 검사 통합</h4>'
    '<p>내비메시 경로와 지면 트레이스 폴백 경로가 각자 같은 다섯 가지 검사(Z 범위, 진입금지구역, 거리, 충돌, 머리 위 공간)를 같은 '
    '순서로 복사해 놓고 있었습니다. 검사 하나를 바꾸거나 순서를 바꿀 때 한쪽만 고치고 다른 쪽을 놓치기 쉬운 구조라, 단일 함수와 '
    '거부 사유를 나타내는 enum으로 묶었습니다.</p>'
    + code("cpp", "CardPlacementService.cpp — 판정 통합", """
enum class ECardDropReject : uint8
{
    Accepted,
    ZOutOfRange,
    InsideNoDropZone,
    TooCloseToOtherCards,
    Blocked,
    NoOverheadClearance,
};

ECardDropReject FCardPlacementService::ClassifyIslandCandidate(
    const FCardIslandDropZone& DropZone,
    float ReferenceZ,
    const FVector& Candidate,
    const TArray<FVector>& ExistingIslandLocations) const
{
    if (!IsCardDropZSane(DropZone, ReferenceZ, Candidate))      return ECardDropReject::ZOutOfRange;
    if (IsInsideNoDropZone(Candidate))                          return ECardDropReject::InsideNoDropZone;
    if (!IsFarEnoughFromIslandCards(Candidate, ExistingIslandLocations)) return ECardDropReject::TooCloseToOtherCards;
    if (!IsCardDropLocationClear(Candidate))                    return ECardDropReject::Blocked;
    if (!HasOverheadClearance(Candidate))                       return ECardDropReject::NoOverheadClearance;
    return ECardDropReject::Accepted;
}
""")
    + '<p>호출하는 쪽(내비 경로, 지면 트레이스 폴백)은 여전히 자기 흐름(return 할지 continue 할지)과 자기 실패 카운터를 직접 '
      '관리하고, 이 함수는 판정 자체만 공용으로 제공하는 형태로 역할을 나눴습니다.</p>'

    + '<h4>설계 및 구현 — 진단 로깅</h4>'
    + '<p>중복 제거만으로는 "왜 실패했는지"가 로그에 드러나지 않는 문제가 남아 있었습니다. 조건별 실패 횟수를 이름 붙은 카운터로 '
    + '누적한 뒤, 그중 값이 가장 큰 항목을 "지배적 실패 사유"로 뽑아 로그에 남기도록 했습니다.</p>'
    + code("cpp", "CardPlacementService.cpp — 지배적 실패 사유 집계", """
const TCHAR* DominantReject = TEXT("Unknown");
int32 DominantCount = 0;
const TPair<const TCHAR*, int32> RejectCounts[] =
{
    { TEXT("NavProjection"), NavFail },
    { TEXT("Bounds"), BoundsFail + GroundBoundsReject },
    { TEXT("ZRange"), ZFail + GroundZReject },
    { TEXT("Distance"), DistFail + GroundDistReject },
    { TEXT("Collision"), OverlapFail + GroundOverlapReject },
    { TEXT("Overhead"), OverheadFail + GroundOverheadReject },
    { TEXT("NoDropZone"), NoDropFail + GroundNoDropReject },
    { TEXT("GroundTrace"), GroundTraceFail },
    { TEXT("Walkability"), GroundWalkableReject },
};
for (const TPair<const TCHAR*, int32>& RejectCount : RejectCounts)
{
    if (RejectCount.Value > DominantCount)
    {
        DominantReject = RejectCount.Key;
        DominantCount = RejectCount.Value;
    }
}
FailureReason = FString::Printf(TEXT("AllCandidatesRejectedMostlyBy%s"), DominantReject);
""")
    + '<p>이 값은 최종 실패 로그에 다른 모든 개별 카운터와 함께 그대로 남아서, 로그 한 줄만 보고도 "이 섬은 후보 대부분이 Z 범위 '
      '밖이라 막혔다"거나 "대부분 다른 카드와 너무 가까워서 막혔다" 같은 결론을 바로 낼 수 있게 됐습니다.</p>'

    + '<h4>결과</h4>'
    + '<ul class="results">'
    + '<li>이 작업은 배치 판정 로직 자체를 바꾼 게 아니라, 같은 판정 결과를 더 잘 진단할 수 있게 만든 관측성(observability) 개선이라는 점을 분명히 해두고 싶습니다 — 동작은 바뀌지 않았고, 실패했을 때 원인을 알 수 있게 됐을 뿐입니다.</li>'
    + '<li>같은 다섯 가지 검사가 경로마다 따로 복사돼 있으면, 검사를 하나 추가하거나 순서를 바꿀 때 한쪽만 고치고 다른 쪽을 놓치기 쉽다는 것을 이번에도 확인했고, 판정 로직 자체를 하나로 모으는 선택을 다시 한번 재확인했습니다.</li>'
    + '<li>실패 사유를 이름 붙은 카운터로 남겨두면, 나중에 섬 크기나 카드 배치 밀도 같은 밸런스를 조정할 때 감이 아니라 로그 근거로 판단할 수 있다는 것을 체감했습니다.</li>'
    + '</ul>'
)


def main():
    tpl_path = os.path.join(ROOT, "portfolio.template.html")
    with open(tpl_path, encoding="utf-8") as f:
        tpl = f.read()

    replacements = {
        "{{IMG_TITLE}}": img_data_uri("01_title.jpg"),
        "{{IMG_LOBBY0}}": img_data_uri("02_lobby0.jpg"),
        "{{IMG_LOBBY1}}": img_data_uri("03_lobby1.jpg"),
        "{{IMG_SEOTDA_PLAY}}": img_data_uri("04_seotda_playing.jpg"),
        "{{IMG_SEOTDA_END}}": img_data_uri("05_seotda_end.jpg"),
        "{{IMG_SHOP}}": img_data_uri("06_shop.jpg"),
        "{{IMG_GOBLIN_ENC}}": img_data_uri("07_goblin_encounter.jpg"),
        "{{IMG_GOBLIN_REWARD}}": img_data_uri("08_goblin_reward.jpg"),
        "{{SYSTEM_01}}": sys01,
        "{{SYSTEM_02}}": sys02,
        "{{SYSTEM_03}}": sys03,
        "{{SYSTEM_04}}": sys04,
        "{{SYSTEM_05}}": sys05,
        "{{SYSTEM_06}}": sys06,
        "{{SYSTEM_07}}": sys07,
        "{{SYSTEM_08}}": sys08,
        "{{SYSTEM_09}}": sys09,
    }

    for k, v in replacements.items():
        if k not in tpl:
            print("WARNING: placeholder not found:", k)
        tpl = tpl.replace(k, v)

    out_path = os.path.join(ROOT, "portfolio.html")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(tpl)

    print("Wrote", out_path, "-", len(tpl) / 1024 / 1024, "MB")


if __name__ == "__main__":
    main()
