# Manager 모듈 리팩토링 정리

이 문서는 `Gil_Refactoring` 브랜치에서 진행한 리팩토링·정리 작업 전체를 요약한다.
목표는 **"게임모드에 몰려 있던 로직을 자체 제작 GAS + 전략 패턴 구성으로 풀고, 레거시·디버그 군더더기를 정리"** 하는 것이었다.

---

## 1. 프로젝트 개요 (배경)

언리얼 엔진 5 **데디케이티드 서버 멀티플레이어** 게임. 한 매치 안에서 **TPS 배틀로얄 ↔ 섯다(화투) 카드게임**을 번갈아 진행한다.

- 로그인/로비: 별도 **IOCP 서버**(raw winsock)와 통신
- 인게임: **언리얼 데디서버**로 핸드오버
- 캐릭터 행동: **자체 제작 GAS**(`PFGASC`/`PFGAbility`)로 어빌리티화
- 게임 흐름: **페이즈 전략 패턴**으로 구성

---

## 2. 최종 아키텍처 (게임모드 분리)

```
AMainGameMode (4621 → 1639줄, 약 65%↓)   ── 얇은 Context / 오케스트레이터
  │   · 페이즈 머신(EDediServerPhase 타이머/전이) · 라이프사이클 · 접속 관리
  │   · 전략은 friend 없이 public Context API로만 GameMode를 조작
  │
  ├─ UTPSPhaseStrategy / UCardPhaseStrategy   ── 페이즈 진입(OnPhaseStart)+종료(OnPhaseEnd) 소유
  ├─ FSeotdaRuleService      ── 섯다 패 평가/우열 비교 (순수 함수, 상태 없음)
  ├─ FCardPlacementService   ── 카드 섬/사망 드롭 배치 (nav·geometry, 설정 주입)
  └─ UCardGameService        ── 카드/섯다 상태 + 도메인 로직 (1777줄)

GameState  ── 복제 상태(RemainingTime, CurrentRound, 마스터 데이터)
PlayerState── 플레이어별(돈, 보유 카드, 업그레이드)
```

**원칙**
- GameMode = Context (페이즈 흐름·매치 상태 소유), 전략에 위임
- 전략 = 페이즈별 셋업/teardown 오케스트레이션
- 서비스 = 순수 도메인 로직(규칙·배치) 또는 도메인 상태(카드게임)
- 복제는 기존과 동일(서비스는 서버 전용, PlayerState/GameState/PC RPC로 클라 전파)

---

## 3. 작업 내역 (테마별)

### 3.1 레거시 대청소 · Manager→Main 이관
- 미사용 클래스 삭제: `CSM_Componenet`, `DetectComponent`, `HUDManagerComponent`, `MyActor`
- 구 어빌리티 시스템 제거: `CustomASC`, `CustomAbility`, `GA_Interaction` (자체 GAS `PFGASC`로 일원화)
- 레거시 캐릭터/게임모드 제거: `ManagerCharacter`, `ManagerGameMode`
  → 현역 참조(`InteractionComponent`/`CoinRegulator`/`Ability_Respawn`)를 `Main*`으로 이관
- 언리얼 템플릿 잔재 에셋 삭제(`BP_ThirdPerson*` 등), 로비 스포너 `CharacterClass` 교체, ini 정리, `.bak` 제거

### 3.2 소스 인코딩 정리
- CP949로 저장돼 깨져 보이던 24개 파일을 **UTF-8(BOM)로 무손실 변환** (내용 무변경)

### 3.3 게임모드 전략 패턴 리팩토링 (핵심)
- 페이즈 진입 셋업을 `*PhaseStrategy::OnPhaseStart`로 이전
- 페이즈 종료 teardown을 `*PhaseStrategy::OnPhaseEnd`로 이전 (라이프사이클 대칭)
- 순수 로직 추출: `FSeotdaRuleService`(섯다 규칙), `FCardPlacementService`(카드 배치)
- 카드/섯다 상태+로직을 `UCardGameService`(UObject)로 이전, GameMode는 위임
- `friend` 제거 → 명시적 public Context API로 경계 정리

### 3.4 플레이어 행동 GAS 일원화
- `Crouch`를 GAS 어빌리티(`UAbility_Crouch`)로 전환 → 모든 캐릭터 행동이 어빌리티 경유
  (Jump/Fire/Aim/Reload/PickUp/Shop/Crouch/Death/Respawn)
- `PickUp` 어빌리티가 서버에서 카드 서비스를 직접 호출 → 군더더기 RPC 홉·죽은 코드 제거
- (참고) 이동/카메라는 직접 처리(어빌리티 대상 아님), 카드 베팅/선택은 턴제 게임규칙이라 PC RPC가 적합

### 3.5 디버그 출력 게이팅
- `[DS]` 트레이스 로그 138개 → **`DS_LOG`** (LogManager/Verbose)
- 온스크린 디버그 39개 → **`DS_SCREEN`** (가변인자)
- **로그 삭제 0** — 전부 보존하되 "디버깅할 때만" 동작하게 게이팅
- `[DS]` Error 15개는 실제 오류라 유지

### 3.6 죽은 코드/의존성 정리
- `MainGameMode.h` stale include 제거(리팩토링 후 미사용)
- `SeotdaTypes.h` 정리: 정의 9개 중 `EBettingAction`만 실사용 → 나머지 8개(옛 섯다 모델) 제거 (280→24줄)
- 의존성 분석 결과: **헤더 순환 0, 결합도 낮음** — 구조적으로 양호함을 확인

---

## 4. 컨벤션

### 디버그 로그
```cpp
DS_LOG(TEXT("[DS] ... %d"), Value);            // 평소 비표시. 콘솔 `Log LogManager Verbose`로 ON. Shipping 제거
DS_SCREEN(-1, 2.f, FColor::Green, TEXT("..."));// 화면 디버그. Shipping 완전 제거
```
- 정의: `Source/Manager/Manager.h`
- 실제 오류는 `UE_LOG(LogTemp, Error, ...)`로 항상 표시

### 빌드 구성
- **Development**(개발): 디버그 로그 포함하되 평소 꺼짐
- **Shipping**(출시): Verbose 로그·온스크린 디버그가 컴파일에서 완전 제거(비용 0)

---

## 5. 커밋 히스토리 (이번 세션, `Gil_Refactoring`)

| 커밋 | 내용 |
|---|---|
| `1e070c5` | 레거시 제거 및 Manager→Main 이관, 템플릿/BP 정리 |
| `88dd073` | 소스 인코딩 CP949 → UTF-8(BOM) 변환 |
| `447c89c` | GameMode 페이즈 전략 패턴 리팩토링 (Context + Strategy + 도메인 서비스) |
| `705ca32` | 전략 OnPhaseEnd 대칭: 페이즈 종료 로직을 전략으로 이전 |
| `4a2e54c` | Crouch를 GAS 어빌리티로 전환 (플레이어 행동 GAS 일원화) |
| `6802ea4` | PickUp 어빌리티가 카드 서비스를 직접 호출 + 죽은 RPC 경로 제거 |
| `6de8669` | Crouch 어빌리티 BP 등록 |
| `6d2c1ba` | 디버그 로그/화면출력 게이팅 (DS_LOG / DS_SCREEN) |
| `0ef8419` | MainGameMode.h stale include 제거 |
| `77a6a28` | SeotdaTypes.h 죽은 옛 섯다 모델 제거 |

---

## 6. 남은 권장 작업 (미진행)

- **빌드 산출물 `.gitignore`**: `Server/IOCP_Server/x64/` 출력물(.log/.idb/.ilk/.recipe/.tlog)이 git에 추적돼 매 빌드마다 status가 지저분함 → 추적 해제 권장
- **섯다 규칙 단위 테스트**: `FSeotdaRuleService`가 순수 함수로 분리됐으므로, 복잡한 규칙(38광땡/땡/구사/암행어사)의 회귀 방지 테스트가 효과적
- **죽은 코드 추가 제거**: `Protocol_D.h::LoginResultToFString`(호출처 0, `new FString` 누수)
- (참고) `UUManagerGameInstance` 클래스명 U 중복 오타 — 동작엔 무해, 리네임은 ini/BP 참조 수정 필요(위험 대비 이득 작음)
