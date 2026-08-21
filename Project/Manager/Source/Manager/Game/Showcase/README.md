# Showcase — 소개 영상 촬영용 궤도 카메라

게임 소개 영상을 찍기 위한 **카메라 전용 레벨** 구성 요소. 플레이어 폰도, HUD도,
입력도 없이 카메라 하나가 맵을 멀리서 궤도를 따라 돈다.

기존 인게임 로직(`AMainGameMode`의 전용 서버·페이즈·IOCP 접속)과 완전히 분리되어 있어
이 레벨을 실행해도 서버에 접속을 시도하지 않는다.

## 구성

| 클래스 | 역할 |
| --- | --- |
| `AOrbitShowcaseCamera` | 액터 위치를 궤도 중심으로 삼아 일정 속도로 회전하는 카메라. 레벨 바운드를 읽어 구도를 자동으로 맞춘다. |
| `AShowcaseGameMode` | 폰·HUD를 만들지 않고, 레벨의 궤도 카메라를 찾아 뷰 타깃으로 지정한다. |
| `AShowcasePlayerController` | 커서·HUD·입력을 끄고 검은 화면에서 페이드 인한다. |

## 이미 만들어진 레벨

`Content/InGame/System/Showcase_Orbit` 이 그 결과물이다. GameMode 오버라이드와 궤도 카메라
배치, 자동 구도 맞춤까지 적용되어 있으므로 열어서 바로 실행하면 된다.

## 레벨을 새로 만드는 순서

다른 구도의 레벨을 추가로 만들거나 위 레벨을 다시 세팅해야 할 때의 절차다.

1. **컴파일** — 에디터가 켜져 있으면 `Ctrl+Alt+F11`(라이브 코딩), 아니면 에디터를 닫고 빌드한다.

2. **레벨 복제** — 콘텐츠 브라우저에서 `Content/InGame/System/Main_Game_World`를 우클릭 →
   **Duplicate**. 이름은 예를 들어 `Showcase_Orbit`.
   배틀로얄 무대가 퍼시스턴트 레벨 자체이므로 섬들이 그대로 딸려온다.

3. **게임모드 교체** — 복제한 레벨을 열고 **World Settings → GameMode Override** 를
   `ShowcaseGameMode`로 바꾼다. **이걸 빼먹으면 `AMainGameMode`가 돌아가면서 IOCP 서버 접속을 시도한다.**

4. **서브레벨 확인** — `Window → Levels`에서 `Card_Game_Stage`가 초기 로드로 잡혀 있지 않은지 본다.
   로드되면 화면 한가운데에 한옥 방이 보인다.

5. **카메라 배치** — Place Actors 패널에서 `Orbit Showcase Camera`를 검색해 뷰포트에 끌어다 놓는다.
   위치는 아무 데나 좋다. 다음 단계에서 옮겨진다.

6. **구도 잡기** — 디테일 패널의 **Frame Level Now** 버튼을 누른다.
   레벨 전체 바운드를 계산해 카메라를 중심으로 옮기고 반경을 맞춘다.

7. **다듬기** — 아래 값을 조절하며 뷰포트에서 바로 확인한다(설정을 바꿀 때마다 구도가 갱신된다).

8. **실행 후 녹화** — `Play` 드롭다운에서 **Standalone Game**을 고르면 에디터 UI 없이 뜬다.

## 주로 만지는 값

| 프로퍼티 | 설명 | 기본값 |
| --- | --- | --- |
| `OrbitSpeedDegreesPerSecond` | 초당 회전 각도. 3이면 한 바퀴 120초 | 3 |
| `OrbitPitchDegrees` | 내려다보는 각도. 크게 할수록 부감 | 22 |
| `FramingPadding` | 1보다 크면 피사체가 더 작게 잡힌다 | 1.15 |
| `FieldOfView` | 화각. 넓히면 광활해 보이고 왜곡이 는다 | 70 |
| `StartAzimuthDegrees` | 녹화 시작 방향 | 0 |
| `bReverseOrbit` | 회전 방향 반전 | false |
| `FramingPivotZOffset` | 자동 중심이 낮게 잡힐 때 위로 보정 | 0 |

### 부가 움직임

- `bEnableHeightBob` — 높이를 사인파로 흔들어 단조로움을 줄인다.
- `bEnableRadiusPulse` — 천천히 다가갔다 물러난다.

둘 다 기본은 꺼져 있다. 등속 회전만으로도 충분히 안정적이라 필요할 때만 켠다.

## 자주 겪는 문제

**섬 하나만 잡고 싶다**
자동 구도는 레벨 전체를 담으므로 섬 4개가 모두 들어온다. 대상 액터에 액터 태그를 붙이고
`FramingActorTag`에 같은 이름을 넣으면 그 액터들만으로 바운드를 계산한다.

**구도가 이상하게 멀다**
스카이 스피어처럼 거대한 프리미티브가 섞였을 수 있다. `MaxPrimitiveRadiusForFraming`(기본 5km)을
낮춰 제외 기준을 좁힌다.

**화면이 검다**
레벨에 `AOrbitShowcaseCamera`가 없으면 게임모드가 0.5초 간격으로 20회까지 찾다가 경고를 남긴다.
출력 로그에서 `[Showcase]`를 검색해 확인한다.

**여러 구도를 만들어 두고 고르고 싶다**
카메라를 여러 개 배치한 뒤 각각 액터 태그를 붙이고, 게임모드의 `ShowcaseCameraTag`로 선택한다.
