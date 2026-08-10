# 성능 측정·최적화 작업 노트 (브랜치 `Claude_PerfOpt`)

2026-08-11 기준. 측정 환경을 세우는 과정에서 얻은 사실과, 아직 열려 있는 질문을 남긴다.

## 0. ⭐ 최우선 결론 — 이 게임은 **렌더 스레드 바운드**다 (2026-08-11 Standalone 측정)

Standalone(`-game`, 1600x900)에서 `stat unit` A→B→A 측정:

| 지표 | A: 100% | B: 70% | A2: 100% 복귀 |
|---|---|---|---|
| Frame | 15.82 / 15.63 | 16.03 / 15.39 | 16.01 / 15.95 |
| Game | 6.57 / 6.71 | 8.41 / 7.89 | 7.19 / 6.88 |
| **Draw (렌더 스레드)** | **15.82 / 16.62** | **16.03 / 16.39** | **16.00 / 15.94** |
| GPU Time | 14.36 / 14.01 | **10.67 / 10.58** | 14.59 / 14.48 |
| RenderRes | 100% (1600x900) | 70% (1120x630) | 100% |
| Draws | 825 / 837 | 781 / 833 | 683 / 687 |

`r.ScreenPercentage 70`으로 픽셀을 49%로 줄이면 **GPU는 14.0 → 10.6ms (-3.9ms, -27%)** 떨어지고
100%로 되돌리면 14.5ms로 복귀한다 — 인과 확실. **그런데 Frame은 15.8 → 16.0ms로 변하지 않는다.**
`Draw`(렌더 스레드)가 모든 조건에서 약 16ms로 고정돼 프레임을 캡하기 때문(약 62 FPS).

> **⚠️ 아래 §1의 "병목은 GPU다"는 에디터 PIE 기준이라 틀렸다.**
> 에디터 CSV에서는 `RenderThreadTime`이 0에 가깝게 잡혀(병렬 렌더링 귀속) 렌더 스레드를 볼 수 없었다.
> **GPU 최적화(해상도·TSR·Lumen·VolumetricCloud)는 FPS를 전혀 올리지 못한다.**
> 렌더 스레드를 먼저 내려야 GPU 여유가 프레임으로 전환된다.

게임 스레드도 Standalone에서 **6.6~7.2ms**로, 에디터 PIE의 14.5ms는 에디터 오버헤드가 부풀린 값이었다.
게임 로직은 여유 있다.

**다음 조사 대상 (렌더 스레드 CPU):** 드로우콜 제출량(683~837), VSM 페이지 관리·Non-Nanite 그림자 셋업,
Slate/UI 렌더, Lumen 씬 업데이트. `ProfileGPU`가 아니라 **Unreal Insights / `stat startfile`** 로
렌더 스레드를 떠야 한다.

---

## 1. 에디터 PIE 기준 측정 (참고 — 병목 판정은 §0이 우선)

### 1-1. 에디터에서는 GPU가 커 보였다
| 조건 | GPUTime | GameThread | DrawCalls | UI(GT) |
|---|---|---|---|---|
| TPS 페이즈, 섬, 시점 스윕 | **39.3 ms** (P90 46.3) | 14.61 ms | 562 (P90 1807) | 5.43 ms |
| 카드 페이즈, 밀폐 실내 | **22.5 ms** | 14.46 ms | 507 (P90 718) | 5.23 ms |
| TPS, 카메라 고정 | 22 ms | 14.4 ms | 550~560 | 4~5 ms |

### 1-2. UI는 씬과 무관한 고정 비용이다 ⭐
카드룸은 거의 아무것도 그리지 않는데도 `Exclusive/GameThread/UI`가 **5.23 ms**로 섬(5.43 ms)과 같다.
씬 복잡도가 아니라 **위젯 틱/바인딩 자체의 오버헤드**라는 뜻. 게임스레드 14.5 ms의 약 36%.
→ 게임스레드 쪽 최우선 조사 대상.

### 1-3. GPU 26.1 ms 프레임 분해 (ProfileGPU, TPS 섬)
| 항목 | ms | % |
|---|---|---|
| PostProcessing (그중 TSR 5.44) | 6.25 | 23.9% |
| ShadowDepths (그중 VSM **Non-Nanite 4.05**) | 5.90 | 22.6% |
| RenderDeferredLighting | 3.88 | 14.8% |
| VolumetricCloud | 2.75 | 10.5% |
| Lumen (뷰에 따라 최대 10.45 ms까지 폭증) | 1.14~ | 4.4%~ |

### 1-4. 섬 부유는 성능과 무관하다 (A/B/B/A 검증)
`UFloatingMotionComponent`를 정지시켜도 `NavigationBuild`는 1.28~1.32 ms로 변화 없음.
GPU도 순서를 뒤집으면 결과가 뒤집혀 인과가 없다.
`RuntimeGeneration=Dynamic`(`DefaultEngine.ini:107`)이라 내브메시가 매 프레임 약 1.3 ms 도는 것은
사실이지만 **원인은 부유가 아니며 아직 미규명**이다.

### 1-5. 시각적 단서: Nanite 무효화
로그에 `Enchanted_Ice_Wall`의 Nanite 경고가 대량 반복된다 — 반투명
(`BLEND_TranslucentGreyTransmittance`) 머티리얼이 Nanite 메시에 물려 Nanite가 꺼진다.
1-3의 **VSM Non-Nanite 4.05 ms**와 직결될 가능성이 높다.

## 2. 측정 환경의 한계 (여기서 막혔음)

에디터 PIE로는 신뢰할 만한 렌더링 A/B가 **불가능**하다는 결론.

1. **매치 페이즈가 계속 순환한다** — TPS 5분 ↔ 카드 2분. 카드 페이즈가 되면 서버가 폰을
   카드룸으로 강제 이동시킨다. 캡처가 페이즈를 걸치면 무효.
2. **일시정지 불가** — 멀티플레이어 게임모드라 `set_game_paused`가 거부된다(`bPauseable=false`).
3. **Simulate In Editor 무효** — 게임이 아니라 에디터를 측정한다(GPU 1.47 ms, GameThread 42.8 ms).
   `r.ScreenPercentage`가 에디터 뷰포트에 적용되지 않아 100%/70%/100% A/B/A가 전부 같은 값.
4. **워밍업 오염** — PIE 시작 후 첫 ~20초는 DrawCalls 1150~1675 → 550~560으로 안정화된다.
   순차 스윕으로 재면 "나중에 잰 쪽이 항상 빠른" 착시가 생긴다.
5. **에디터 백그라운드 스로틀** — 원격 구동 시 `RenderThread/EventWait`가 324 ms로 잡히고
   FrameTime이 3 FPS로 보인다. FrameTime/FPS는 버리고 `GPUTime`·`GameThreadTime`만 읽을 것.

## 3. 다음 단계 제안

### 3-1. 측정 환경부터 (선행 조건)
**Standalone 프로세스**에서 측정해야 한다. 에디터 PIE는 위 5가지 때문에 한계.
- `-game` 또는 패키징 빌드로 클라이언트를 띄우고, 콘솔에서 `CsvProfile start/stop`
- 페이즈가 고정된 구간(예: TPS 시작 직후 30초)만 반복 측정

### 3-2. 검증할 레버 (예상 효과 순)
| 레버 | 근거 | 예상 |
|---|---|---|
| `r.ScreenPercentage` 70~80 | GPU가 해상도 고정비 지배 (빈 실내에서도 22 ms) | 최대 |
| Nanite 무효화 메시 수정 | VSM Non-Nanite 4.05 ms, 136 draw | 큼 |
| `sg.AntiAliasingQuality` 하향 | TSR 5.44 ms | 중 |
| VolumetricCloud 품질 | 2.75 ms | 중 |
| UI 위젯 틱/바인딩 | GT 5.2 ms 고정비 | 게임스레드 최대 |

### 3-3. 아직 못 푼 것
- `NavigationBuild` 1.3 ms/프레임의 원인 (부유는 아님). `RuntimeGeneration=Static` A/B 필요 — config 변경이라 합의 후.
- 섬별 비용 비교 — 워밍업 오염으로 1차 시도 무효. 재측정 필요.

## 4. 도구
- `tools/Analyze-Csv.ps1` — CsvProfiler 결과에서 필요한 컬럼만 1회 파싱해 중앙값/P90 집계.
  `Import-Csv`는 이 CSV의 중복 컬럼명(`FMsgLogf/FMsgLogfCount`) 때문에 실패하므로 쓰지 말 것.
