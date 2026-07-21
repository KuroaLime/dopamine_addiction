# Manager ID 8자 제한 구현 기록

작성일: 2026-07-21

## 목적

현재 Manager 프로젝트는 별도 닉네임 입력을 받지 않고, 최초 로그인에 사용한 ID를 자동 가입 시 닉네임으로 그대로 저장한다. 따라서 지나치게 긴 ID는 방 멤버 목록과 게임 내 플레이어 이름에도 그대로 노출된다. ID와 표시 이름의 계약을 최대 8자로 통일한다.

## 변경 사항

1. `Server/Protocol_D.h`
   - 실제 문자 수 한도 `MAX_ID_CHAR_LEN=8`, `MAX_NICKNAME_CHAR_LEN=8`을 추가한다.
   - UTF-8 전송 한도는 문자당 최대 4바이트를 고려해 각각 32바이트로 둔다.
   - IOCP의 로그인 및 자동 회원가입 패킷 파서는 9자 이상의 ID를 `INVALID_FORMAT`으로 거절한다.
   - 방 멤버 목록에 실리는 닉네임도 최대 8자로 제한한다.

2. `Server/LobbyService.cpp`
   - `MultiByteToWideChar(..., MB_ERR_INVALID_CHARS, ...)`로 UTF-8 유효성과 실제 UTF-16 문자 길이를 함께 검증한다.
   - UI를 우회한 패킷도 ID와 별도 닉네임 모두 8자 초과 시 거절한다.

3. `Source/Manager/Game/Protocol_Client/Protocol_D.h`, `UManagerGameInstance.cpp`
   - 클라이언트도 문자 수 8자와 UTF-8 최대 32바이트를 분리한다.
   - `UUManagerGameInstance::SendAuth()`가 `FString::Len()`을 검사하므로 UI를 우회한 9자 이상 ID도 송신하지 않는다.

4. `Source/Manager/Game/Login/UI/LoginWidget.cpp`
   - UE 5.7의 `UEditableTextBox`에는 `SetMaxTextLength()`가 없으므로 `OnTextChanged` 이벤트를 사용한다.
   - 입력이 8자를 넘으면 `Left(8)` 결과를 다시 설정해 아홉 번째 문자 또는 긴 붙여넣기를 즉시 잘라낸다.
   - 일반 로그인 화면뿐 아니라 UE 송신과 IOCP 수신에서도 독립적으로 8자 제한을 검증한다.

## 동작 계약

- 허용: 비어 있지 않은 ID, 최대 8자
- 거절: 9자 이상 ID
- 비밀번호 한도 16은 변경하지 않는다.
- 문자 수와 UTF-8 바이트 수를 분리했으므로 영문/숫자뿐 아니라 한글 ID도 8자까지 허용된다.

## 기존 계정 영향

IOCP의 현재 사용자 저장소는 프로세스 메모리에 있으므로 서버를 재시작하면 시험용 계정이 초기화된다. 새 빌드부터 9자 이상 ID는 로그인 및 자동 가입이 불가능하다. 최근 시험 로그의 `IDasdfasd`는 9자이므로 새 제한 적용 후에는 `IDasdfas`처럼 8자 이하로 시험해야 한다.

## 빌드 및 확인

IOCP 서버와 UE 클라이언트를 모두 다시 빌드한다. 다음 세 조건을 확인한다.

1. 8자 ID는 자동 가입 및 로그인에 성공한다.
2. 로그인 화면에서 아홉 번째 문자가 입력되지 않는다.
3. 변조 패킷 또는 UI 우회 호출로 9자 ID를 보내도 UE 송신 또는 IOCP 수신 검증에서 거절된다.

## 최신 실행 로그 확인

- Dedi는 설치형 UE 5.7.4의 `UnrealEditor.exe`로 정상 실행됐다.
- 세 라운드 모두 카드 20장에 대해 `LocationFound=20`, `Spawned=20`, `NavPlaced=20`, 실패 0을 기록했다.
- 공개 카드 선택은 `Revealed=1, Submitted=0`, 최종 제출은 이후 `Submitted=1`로 분리되어 정상 처리됐다.
- MatchEnd 통지가 성공한 뒤 10초 후 Dedi가 종료된 것은 정상 회수 절차다.
- 현재 남은 로그 잡음은 로그인/로비 UI의 `InputMode:UIOnly - Attempting to focus Non-Focusable widget`이며, 이번 실행 중단이나 카드 게임 실패 원인은 아니다.
