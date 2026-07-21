# Manager 재접속/매치 종료/상점 방벽 수정

작성일: 2026-07-21

## 확인된 원인

- 카드 자동 보충은 카드 페이즈 시작 시 현재 연결된 PlayerController만 순회했다.
- 이전 라운드 중 접속이 끊긴 플레이어의 스냅샷 카드는 라운드 전환 때 정상 정리되지만, 다음 카드 페이즈가 시작된 뒤 재접속하면 3장 보충 경로가 없었다.
- 게임 종료와 동시에 IOCP에 MATCH_END를 통보하고 Dedi를 10초 뒤 종료해, 결과 화면에서 늦게 나간 클라이언트는 정상 로비 이동 전에 HostClosedConnection을 받았다.
- 네트워크 실패로 Login_Stage에 진입한 경로에서는 게임 HUD 제거와 매치 종료 로비 복귀 처리가 없었다.
- 친구의 상점 방벽 구현 커밋은 활성화 시 SetVisibility(true)를 호출해 충돌용 벽 메시까지 표시했다.

## 적용 내용

- 카드 선택 단계에 재접속한 플레이어의 서버 PlayerState가 3장 미만이면 현재 라운드의 제거 카드 기록을 우선 재사용하고, 부족분만 새 카드로 보충한다.
- 베팅 시작 후 또는 결과 확정 후에는 중도 참가 카드를 새로 만들지 않아 이미 진행 중인 베팅 상태를 변경하지 않는다.
- 매치 종료 후 모든 Dedi 클라이언트가 나갈 때까지 기다린 뒤 IOCP MATCH_END 통보와 프로세스 종료를 시작한다.
- 무한 대기를 막기 위해 기본 60초 하드 타임아웃을 유지한다.
- 최종 결과를 받은 즉시 GameInstance에 방 복귀 의도를 기록한다.
- 네트워크 종료가 Login_Stage로 보낸 경우 로그인 UI를 만들기 전에 남은 HUD를 제거하고 Lobby_Stage로 자동 복귀한다.
- MainPlayerController EndPlay에서도 모든 게임 위젯을 제거한다.
- 상점 방벽 메시의 가시성은 항상 끄고, 페이즈에 따라 충돌만 켜고 끈다.

## 기대 로그

- 재접속 보충 성공: `Reconnect CardCatchUpComplete`
- 전원 결과 화면 이탈: `MatchEndFinalize ... Reason=AllClientsReturned`
- 60초 강제 종료: `MatchEndReturnWait Timeout`
- 네트워크 실패 로비 복구: `Login fallback intercepted -> Lobby_Stage`
- 방벽 활성 중: `Visible=0 CollisionEnabled=3`

빌드 및 두 클라이언트 재현 시험은 별도로 수행해야 한다.
