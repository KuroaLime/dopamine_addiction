# Manager Ability_Fire 타이머 크래시 수정

- 작성일: 2026-07-21
- 대상: `Source/Manager/Default/Ability/Ability_Fire.h`, `Ability_Fire.cpp`
- 상태: 코드 수정 번들 작성 완료, 실제 프로젝트 적용 후 빌드 필요

## 1. 확인된 실제 장애

Dedicated Server 로그의 치명적 종료는 다음 호출 경로에서 발생했다.

- 예외: `EXCEPTION_ACCESS_VIOLATION writing`
- 위치: `UAbility_Fire::ActivateAbility`가 만든 타이머 람다
- 크래시 지점: 기존 `Ability_Fire.cpp` 217행의 `LastServerFireTime` 기록

타이머가 Ability 종료 또는 소유 객체 정리 뒤에도 실행될 수 있는데, 기존 타이머가 원시 `this`를 캡처했다. 람다는 캐릭터의 약한 포인터만 검사한 뒤 이미 무효가 된 Ability의 멤버에 기록할 수 있었다. Dedi가 종료된 뒤 클라이언트에 나타난 `CreateSavedMove: Hit limit of 96`과 접속 시간 초과는 이 서버 종료의 후속 증상이었다.

## 2. 적용한 수정

1. 클라이언트와 서버의 원시 `[this]` 타이머 람다를 모두 제거했다.
2. 타이머를 `UAbility_Fire` 멤버 함수에 직접 바인딩해 UObject 수명 검사를 적용했다.
3. 즉시 발사, 재시도 발사, 자동 연사마다 `Server_ExecuteFire()` 반환 직후 Ability 활성 상태를 재검사한다.
4. 조준 해제, 권한/월드/인터페이스/카메라/무기 상실 시 `EndAbility(true)`를 호출한다.
5. `EndAbility()`가 소유 캐릭터 유효성이나 Authority 여부와 무관하게 타이머 정리와 `Super::EndAbility()`를 수행하도록 변경했다.
6. 사격 간격을 최소 `0.01`초로 제한해 잘못된 데이터로 0초 타이머가 만들어지는 것을 막았다.
7. `ApplyDamage()`가 사망이나 라운드 종료를 동기적으로 발생시킨 경우, 다음 펠릿과 발사 피드백 전에 Ability 및 무기 상태를 다시 검사한다.

## 3. 보존되는 동작

- 자동 사격은 입력을 누르는 동안 무기 FireRate 간격으로 반복한다.
- 반자동 사격은 클릭당 한 발만 처리한다.
- FireRate가 아직 남았을 때 입력하면 짧게 재검사하다 준비되는 순간 발사한다.
- 장전 중에는 발사를 보류하며, 탄약이 0이면 Ability를 종료한다.
- 트리거를 놓으면 서버 사격 타이머와 블룸 누적값을 초기화한다.

## 4. 적용 후 검증 항목

1. IOCP, `Manager`, `ManagerServer`를 같은 소스로 다시 빌드한다.
2. Dedi에서 자동화기 연사 중 상대를 사망시켜도 프로세스가 유지되는지 확인한다.
3. 사격 중 조준 해제, 재장전, 탄약 0, 라운드 종료를 각각 시험한다.
4. 로그에 `Ability_Fire` 액세스 위반과 Dedi `exitCode=1`이 다시 나타나지 않는지 확인한다.
5. 클라이언트의 `CreateSavedMove: Hit limit of 96` 및 서버 시간 초과가 재발하지 않는지 확인한다.

## 5. 다음 작업

카드 게임은 다음 순서로 별도 수정한다.

1. 공개할 카드 1장 선택 및 공개 확정
2. 공개 선택과 독립적으로 최종 제출 카드 2장 선택 및 제출 확정
3. 모든 플레이어의 최종 제출이 끝난 뒤 베팅 시작

공개 카드 선택이 곧 최종 제출로 처리되던 현재 결합 구조를 두 개의 서버 상태와 두 개의 요청 경로로 분리해야 한다.
