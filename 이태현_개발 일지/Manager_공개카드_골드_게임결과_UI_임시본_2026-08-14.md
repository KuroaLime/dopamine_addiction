# Manager 공개 카드/골드/게임 결과 UI 임시본

- 반영일: 2026-08-14
- 프로젝트: `X:\Project\Manager`
- 목적: 기존 서버 판정과 게임 에셋을 유지하면서, 실제 플레이에서 확인하기 어려웠던 네 가지 정보를 빠르게 화면에 표시한다.

## 1. 확인된 문제와 원인

### 1.1 공개 카드가 최종 패 제출 뒤에 보임

- 서버의 `RevealSeotdaCard()`는 공개 카드 선택 즉시 `bRevealConfirmed`와 공개 카드 ID를 저장한다.
- `BroadcastSeotdaState()`와 Reliable `Client_UpdateSeotdaState()`도 최종 2장 제출 전에 정상 호출된다.
- 클라이언트의 `Seat0Card`~`Seat3Card`에는 공개 카드 텍스처를 넣는 코드가 이미 있었다.
- 따라서 직접 원인은 서버 전송 지연이 아니라 기존 `WBP_Seotda`의 카드 이미지 상위 패널이 다음 UI 상태 전이 전까지 접혀 있는 표시 계층이었다.

### 1.2 골드 수량이 보이지 않음

- 서버 권위 골드는 `AMainPlayerState::CurPlayerData.HoldingGold`로 이미 복제된다.
- 섯다 `InfoTxt`에는 숫자만 넣고 있었고, 표시 상태를 보장하지 않았다.
- TPS의 `CRoundandTimerWidget::Round_Text`는 등수만 표시했다.

### 1.3 섯다 라운드 승자와 족보가 보이지 않음

- 서버는 승자, 동점, 족보, 팟, 지급액을 이미 계산한다.
- 각 클라이언트에도 `[ROUND N RESULT]` 문자열이 Reliable RPC로 도착한다.
- 기존 클라이언트 구현은 로그와 개발용 화면 메시지만 출력하고 실제 게임 UI에는 연결하지 않았다.

### 1.4 최종 승자와 순위가 보이지 않음

- 서버는 `BuildFinalGoldRanking()`으로 최종 골드 순위를 만든 뒤 `[MATCH END]` 결과를 전송한다.
- 섯다 위젯의 로비 버튼만 최종 종료 상태를 보므로, TPS 화면에서 종료되면 결과를 표시할 지속 UI가 없었다.

## 2. 임시 구현 내용

### 2.1 공개 카드 즉시 표시

- 기존 `Seat0Card`~`Seat3Card`와 최대 네 단계의 상위 패널을 공개 상태가 도착한 프레임부터 표시한다.
- 카드 이미지는 `HitTestInvisible`, 부모는 `SelfHitTestInvisible`을 사용한다.
- 따라서 공개 카드와 부모 패널은 보이면서 카드 선택/제출/베팅 버튼의 자식 입력은 유지된다.
- 기존 `UCardTextureSet`의 카드 앞면 에셋을 그대로 사용한다.

### 2.2 섯다와 TPS 골드 표시

- 섯다의 기존 `InfoTxt`: `골드 10000` 형식으로 표시한다.
- TPS의 기존 `CRoundandTimerWidget.Round_Text`: `골드 10000 | 1위` 형식으로 표시한다.
- 골드 원본은 새 변수를 만들지 않고 복제된 `HoldingGold`만 읽는다.
- TPS 순위는 현재 `GameState.PlayerArray`에서 자신보다 골드가 많은 플레이어 수로 계산하는 기존 방식을 유지한다.

### 2.3 섯다 라운드 결과 오버레이

- 기존 `Client_ShowSeotdaResult()`가 받은 서버 결과를 한국어 표시 문자열로 변환한다.
- 승자/공동 승자, 승리 족보, 팟, 각 승자 지급액을 표시한다.
- 내부 족보명(`SamPalGwangDdang`, `8Gut` 등)은 `38광땡`, `8끗` 등으로 변환한다.
- 결과는 화면 위쪽에 7초간 표시되며 입력을 가로채지 않는다.
- 판정과 정산을 클라이언트에서 다시 계산하지 않고 서버 결과를 표시만 한다.

### 2.4 최종 결과 오버레이

- `[MATCH END]`를 받으면 섯다/TPS 위젯과 독립적인 최상위 Slate 오버레이를 띄운다.
- 최종 우승자, 진행 라운드, 골드 기준 전체 순위를 표시한다.
- 기존 `/Game/InGame/UI/T_UI_FX_WinnerLaurel` 에셋을 결과 장식으로 재사용한다.
- `로비로 돌아가기` 버튼은 기존 `ReturnToLobbyFromMatchEnd()` 흐름을 호출한다.
- 최종 결과는 7초 타이머로 사라지지 않고 로비 이동 또는 월드 종료까지 유지된다.

## 3. 서버 계약 영향

- IOCP 소스 변경 없음.
- Dedi의 섯다 판정, 골드 정산, 최종 순위 계산 변경 없음.
- RPC 이름, 인자, Reliable 속성 변경 없음.
- 새 Replicated 필드 없음.
- 카드 공개/최종 패 제출 순서 변경 없음.
- 이번 패치는 서버가 이미 보내던 값을 클라이언트에서 확실히 보여주는 임시 표시 계층이다.

## 4. 변경 파일

- `Source/Manager/Game/InGame/Card/UI/SeotdaTempWidget.cpp`
- `Source/Manager/Game/InGame/TPS/UI/CRoundandTimerWidget.cpp`
- `Source/Manager/Game/InGame/MainPlayerController.h`
- `Source/Manager/Game/InGame/MainPlayerController.cpp`

원본 백업:

- `X:\Project\Manager\Saved\CodexBackups\temporary_game_results_20260814-032415`

## 5. 빌드 검증

### 성공

- Launcher UE 5.7 `ManagerEditor Win64 Development`: 성공
- Launcher UE 5.7 `Manager Win64 Development`: 성공
- 최종 입력 보존 수정 뒤 `ManagerEditor` 4개 액션 증분 빌드: 성공
- 누락 런타임 DLL 복구용 `Manager` 6개 복사 액션: 성공
- `git diff --check`: 오류 없음

### Dedi 상태

- Launcher UE 배포판은 Server 타깃을 지원하지 않아 즉시 거절됐다. 코드 컴파일 오류가 아니다.
- `S:\UE\UE_5.7_Source`의 `ManagerServer` 빌드는 Source 엔진 캐시 부족으로 790개 엔진 액션을 요구해 중단했다.
- 해당 UBT/컴파일러/MSBuild 워커는 모두 종료했다.
- Source 빌드가 먼저 삭제했던 `tbb12.dll`, `tbbmalloc.dll`, D3D12 DLL 2개, `DirectML.dll`은 Launcher `Manager` 빌드로 모두 복구했다.
- 이번 변경은 클라이언트 표시 코드이고 네트워크 RPC/복제 계약은 그대로이므로 기존 Dedi와 통신 계약은 유지된다.
- 최신 소스를 포함한 새 `ManagerServer.exe` 생성 검증은 Source 엔진 캐시를 정상화한 뒤 별도로 수행해야 한다.

## 6. 2클라이언트 런타임 확인 순서

1. A가 공개 카드 1장을 확정한 직후 B 화면의 해당 좌석에 카드 앞면이 보이는지 확인한다.
2. 이 시점에 A/B가 아직 최종 패 2장을 제출하지 않았는지 확인한다.
3. 섯다 화면에서 `골드 N`이 보이고 정산 직후 숫자가 바뀌는지 확인한다.
4. TPS 화면에서 `골드 N | N위`가 보이는지 확인한다.
5. 섯다 라운드 종료 직후 두 클라이언트 모두 같은 승자/족보/팟/획득액을 7초간 보는지 확인한다.
6. 전체 게임 종료 시 어느 페이즈 화면이든 최종 우승자와 전체 순위가 유지되는지 확인한다.
7. `로비로 돌아가기`를 눌렀을 때 두 클라이언트 모두 기존 방 복귀 흐름으로 이동하는지 확인한다.

## 7. 임시본 이후 권장 정식화

- 런타임 검증이 끝나면 결과 오버레이를 별도 UMG Blueprint로 옮겨 디자이너가 크기/애니메이션/폰트를 조정할 수 있게 한다.
- 현재 한국어 포맷터는 서버의 구조화되지 않은 결과 문자열을 읽는다. 정식 구현에서는 `FSeotdaRoundResult`와 `FFinalMatchRanking` 같은 구조체 RPC로 전환하는 편이 안전하다.
- 공개 카드 부모 강제 표시는 임시 호환 처리다. `WBP_Seotda`에서 공개 카드 컨테이너를 항상 Visible로 두고 카드 이미지 자체만 접는 구조로 정리하면 부모 탐색을 제거할 수 있다.

## 8. 2026-08-14 실제 화면 확인 후 후속 보정

위 2.1과 2.2의 최초 임시 구현만으로는 실제 화면 문제가 모두 해결되지 않았다. 아래 내용이 현재 코드 상태이며 앞의 관련 설명을 대체한다.

- TPS 상단 `골드 N | N위`는 고정 폭을 넘어섰으므로 상단은 `등수 N위`만 남겼다.
- 골드는 TPS 오른쪽 아래에 기존 `T_UI_Icon_Gold`와 숫자를 독립 오버레이로 표시한다.
- 공개 상태는 첫 공개 직후 이미 서버에서 수신됐다. 남은 원인은 UI가 절대 `SeatIndex`를 무시하고 상대 배열을 압축한 것, 자기 자신을 표시하지 않은 것, 접힌 부모 패널 깊이가 충분하지 않았던 것이다.
- 상대는 서버 좌석 번호대로 배치하고, 빠진 좌석에 로컬 플레이어를 복원한다.
- 자기 공개 카드는 승인 ACK용 로컬 캐시와 복제된 `RevealedCard` 중 먼저 준비된 값을 사용한다.
- `ID:+6=>1004`는 단순 문자 치환하지 않고 필드 구조대로 읽어 `ID: +6골드 (보유 1,004골드)`로 표시한다.
- 사용자 요청에 따라 이번 후속 수정은 빌드하지 않았으며 2클라이언트 런타임 검증이 필요하다.
- 후속 상세 기록: `Manager_골드HUD_공개카드_결과문구_후속수정_2026-08-14.md`
- 후속 백업: `X:\Project\Manager\Saved\CodexBackups\ui_gold_reveal_result_20260814-041547`
- 사용자 `ManagerEditor` 증분 빌드에서 `Widgets/Layout/SOverlay.h` 경로 오류 `C1083`을 확인했고, UE 5.7 실제 경로인 `Widgets/SOverlay.h`로 수정했다. 수정 뒤 재빌드는 아직 하지 않았다.
