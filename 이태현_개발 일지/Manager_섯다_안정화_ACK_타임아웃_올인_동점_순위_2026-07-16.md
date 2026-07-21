# Manager 섯다 서버 권위 안정화 및 빌드 기록

작성일: 2026-07-16

## 1. 작업 목적

공개 카드 선택 이후 서버 응답이 없거나 일부 클라이언트가 느린 경우 카드 게임이 멈추는 문제, 베팅 턴 방치와 올인 상태에서 라운드가 끝나지 않는 문제, 동점 팟 지급과 최종 순위 누락 문제를 서버 권위 기준으로 보강했다.

## 2. 공개 카드 선택 ACK

### 서버 처리

- `AMainPlayerController::Server_SubmitSeotdaSelection_Implementation`
  - RPC rate limit 실패 시 거절 ACK를 보낸다.
  - GameMode 또는 CardGameService가 없으면 거절 ACK를 보낸다.
  - CardGameService의 최종 검증 결과와 실패 사유를 클라이언트 RPC로 반환한다.
- `UCardGameService::SubmitSeotdaSelection`
  - CardGame 페이즈인지 확인한다.
  - 이미 제출했는지 확인한다.
  - 정확히 3장을 보유했는지 확인한다.
  - 공개 카드가 정확히 1장인지 확인한다.
  - 모든 카드의 서버 레코드와 소유권이 일치하는지 확인한다.

### 클라이언트 UI

- 제출 직후에는 `bLocalSelectionPending`만 설정한다.
- 서버 승인 ACK가 도착해야 제출 완료 상태가 된다.
- 거절 ACK가 오면 선택 버튼과 제출 버튼을 다시 사용할 수 있다.
- 서버의 실패 사유를 결과 텍스트에 표시한다.

주요 위치:

- `MainPlayerController.cpp:1506`
- `CardGameService.cpp:341`
- `SeotdaTempWidget.cpp:422`

## 3. 로딩 지연과 선택 Gate

`AMainGameMode::StartCardGamePhase`가 카드 지급과 상태 초기화 후 선택 타이머를 시작한다.

`UCardGameService::StartSeotdaSelectionTimeout`은 다음을 수행한다.

1. CardGame 페이즈와 서버 권위를 확인한다.
2. 기존 선택 타이머를 지운다.
3. 일반 CardGame 페이즈 타이머를 해제한다.
4. 선택 기본 제한시간 60초를 시작한다.

일반 페이즈 타이머를 해제한 이유는 무거운 맵을 로드하는 원격 클라이언트가 늦게 준비될 때 선택 전용 제한시간보다 기존 페이즈 시간이 먼저 끝나는 경쟁 조건을 막기 위해서다.

제한시간 만료 시 미제출 플레이어에게 첫 번째 보유 카드를 서버가 자동 공개한다. 자동 제출 후에도 진행 조건이 충족되지 않으면 결과를 정리하고 다음 페이즈로 진행한다.

주요 위치:

- `MainGameMode.cpp:2544`
- `CardGameService.cpp:526`
- `CardGameService.cpp:564`

## 4. 베팅 턴 타임아웃

- 기본 제한시간: 30초
- 콜 필요 금액이 0이면 자동 `Check`
- 콜이 필요하면 자동 `Die`
- 요청 Controller를 찾지 못하거나 자동 액션 제출이 실패해도 서버가 fold/advance fallback으로 라운드를 계속 진행한다.
- 턴 목록이 비거나 유효한 다음 플레이어가 없으면 결과를 정리하고 CardGame 페이즈를 끝낸다.

주요 위치:

- `CardGameService.cpp:795`
- `CardGameService.cpp:829`
- `CardGameService.cpp:1070`

## 5. 올인 상태

`FSeotdaPlayerRoundState`에 `bAllIn`을 추가했다.

- 베팅 후 보유 골드가 0이면 올인으로 설정한다.
- 올인 플레이어는 다음 턴 탐색에서 제외한다.
- 타임아웃 함수가 올인 플레이어를 현재 턴으로 발견해도 액션을 강제하지 않고 다음 턴으로 넘긴다.
- 레이즈 시 다른 일반 플레이어의 acted 상태만 초기화하며 올인 플레이어는 그대로 둔다.
- 베팅 완료 판정에서는 올인 플레이어가 최고 베팅액보다 적더라도 추가 액션이 필요 없는 상태로 처리한다.

현재 구현은 단일 팟 규칙이다. 서로 다른 금액의 복수 올인에 포커식 main/side pot을 적용하려면 별도의 게임 규칙 결정과 정산 로직이 필요하다.

## 6. 동점 팟 분배

기존 단일 승자 선택 대신 최고 패와 같은 모든 플레이어를 수집한다.

- 팟을 동점자 수로 나눈 기본 지급액을 계산한다.
- 나머지는 이름 대소문자 무시 정렬 후 PlayerId 순서로 1골드씩 지급한다.
- 결과 문자열에 동점 여부와 플레이어별 실제 지급액을 기록한다.
- 지급 총합은 기존 팟 총액과 일치한다.

주요 위치:

- `CardGameService.cpp:1116`

## 7. 최종 골드 순위 원장

기존에는 최종 정산 시점의 접속자와 남아 있는 재접속 스냅샷만 수집했다. 재접속 유예가 만료되어 스냅샷이 제거된 참가자는 순위에서 빠질 수 있었다.

이를 막기 위해 Dedi ticket 기준 `MatchParticipantGoldLedger`를 추가했다.

갱신 시점:

- 경기 시작 시 현재 참가자 등록
- 경기 진행 중 PostLogin 및 재접속
- Logout 스냅샷 저장
- 재접속 유예 만료 직전

최종 정산은 원장, 현재 접속자, 재접속 스냅샷을 ticket 기준으로 병합한다.

주요 위치:

- `MainGameMode.cpp:1058`
- `MainGameMode.cpp:2606`

## 8. 공개 카드 이미지 패널

임시 섯다 위젯이 `GameState.PlayerArray`를 순회하여 플레이어별 공개 카드 패널을 만든다.

- 플레이어 이름
- 공개 카드 텍스처
- 카드 인스턴스 ID와 CardID
- 공개 전 `Waiting`

`CardImageMap`을 재사용하며, 플레이어/공개 카드 상태 서명이 변경된 경우에만 자식 위젯을 다시 생성한다.

주요 위치:

- `SeotdaTempWidget.cpp:248`
- `SeotdaTempWidget.cpp:273`

## 9. 변경 파일

- `Source/Manager/Manager.Build.cs`
- `Source/Manager/Game/InGame/MainGameMode.h`
- `Source/Manager/Game/InGame/MainGameMode.cpp`
- `Source/Manager/Game/InGame/MainPlayerController.h`
- `Source/Manager/Game/InGame/MainPlayerController.cpp`
- `Source/Manager/Game/InGame/Card/CardGameService.h`
- `Source/Manager/Game/InGame/Card/CardGameService.cpp`
- `Source/Manager/Game/InGame/Card/UI/SeotdaTempWidget.h`
- `Source/Manager/Game/InGame/Card/UI/SeotdaTempWidget.cpp`

## 10. 빌드 검증

빌드는 긴 실제 OneDrive 경로 대신 기존 `X:` 가상 드라이브를 사용했다.

```text
ManagerEditor Win64 Development: Succeeded
Manager Win64 Development: Succeeded
ManagerServer Win64 Development: Succeeded
IOCP Debug x64: Succeeded
```

최종 산출물:

- `X:\Project\Manager\Binaries\Win64\UnrealEditor-Manager.dll`
- `X:\Project\Manager\Binaries\Win64\Manager.exe`
- `X:\Project\Manager\Binaries\Win64\ManagerServer.exe`
- `X:\Project\Manager\Server\x64\Debug\IOCP_Server.exe`

설치형 UE에서 긴 한글 OneDrive 경로로 빌드하면 SharedPCH 출력 파일 생성이 실패했지만, 동일 프로젝트를 `X:\Project\Manager`로 빌드하면 성공했다.

`Manager.Build.cs`에서 실제로 존재하지 않는 Variant include 경로 13개를 제거해 Unreal 빌드 경고를 없앴다.

IOCP는 성공했지만 `ServerMain.cpp`에 C4819 인코딩 경고 2건이 남는다. Server 폴더에는 UTF-8과 비 UTF-8 파일이 혼재하므로 `/utf-8` 전체 강제는 적용하지 않았다.

## 11. 남은 검증

컴파일과 링크는 모두 성공했지만 실제 네트워크 동작은 다음 2PC 시험이 필요하다.

1. 느린 원격 클라이언트의 카드 선택 진입
2. 선택 승인/거절과 재시도
3. 60초 선택 자동 처리
4. 30초 베팅 자동 처리
5. 올인 후 턴 진행
6. 동점 팟 총액 보존
7. 연결 해제 참가자의 최종 순위 유지
8. 상대 공개 카드만 보이고 숨은 2장은 노출되지 않는지 확인
