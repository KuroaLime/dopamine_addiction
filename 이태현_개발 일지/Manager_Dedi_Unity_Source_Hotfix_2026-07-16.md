# Manager Dedi Unity Build 소스 Hotfix

- 날짜: 2026-07-16
- 대상: `Source\Manager\Game\InGame\MainPlayerController.cpp`
- 목적: `ManagerServer Win64 Development` Unity Build 중복 정의 제거

## 발생한 오류

실제 `X:\Project\Manager`에서 `ManagerServer`를 빌드하자 UnrealHeaderTool은 통과했고 14개 컴파일 작업이 시작됐다. Dedi Unity Build는 `MainGameMode.cpp`와 `MainPlayerController.cpp`를 같은 `Module.Manager.6.cpp`에 포함했다.

두 파일의 익명 namespace에 다음 보조 함수 이름이 동시에 존재해 C2084 중복 정의가 발생했다.

- `GetRotationErrorDegrees`
- `GetPersistentMainWorldLevelName`
- `IsPersistentMainWorldTarget`

Editor의 adaptive/non-unity 빌드에서는 두 `.cpp`가 별도로 컴파일되어 드러나지 않았고, Dedi Unity 묶음에서만 확정적으로 발생한 빌드 오류다.

## 수정

`MainPlayerController.cpp`의 보조 함수와 해당 파일 내부 호출부만 다음과 같이 이름을 바꿨다.

- `GetRotationErrorDegrees` -> `GetControllerRotationErrorDegrees`
- `GetPersistentMainWorldLevelName` -> `GetControllerPersistentMainWorldLevelName`
- `IsPersistentMainWorldTarget` -> `IsControllerPersistentMainWorldTarget`

함수 내용, 인자, 반환값, 호출 순서에는 변화가 없다. 서버 위치 보정과 영구 메인 월드 판정 동작은 그대로이며 Unity Build 심볼 충돌만 제거한다.

## 검증

- 실제 실패 당시의 `Module.Manager.6.cpp` 사용
- 실제 Dedi의 `Manager.Shared.rsp`, PCH, Definitions 및 MSVC 옵션 사용
- `MainPlayerController.cpp`만 수정본으로 교체한 전체 Unity 모듈 컴파일 성공
- 생성 object를 실제 `ManagerServer.exe.rsp`에 넣은 별도 전체 링크 성공
- 수정본 `ManagerEditor Win64 Development` 컴파일 및 링크 성공

사용자 요청에 따라 검증용 Dedi 실행 파일을 `X:`에 복사하지 않고 소스만 교체한다. 공식 `ManagerServer` 빌드는 사용자가 별도로 실행한다.

## 소스 해시

- 수정 전: `FF039B7573E59926C26D1321C607FA99F44E55E0C826288A4066B723F6E0CEF5`
- 수정 후: `25927676A3582D90DCFCB20F7FB444BBC19C3482F808331B6E2D3F65E7F68EC5`

## 빌드 명령

```powershell
& 'S:\UE\UE_5.7_Source\Engine\Build\BatchFiles\Build.bat' `
  ManagerServer Win64 Development `
  -Project='X:\Project\Manager\Manager.uproject' `
  -WaitMutex -NoUBA
```
