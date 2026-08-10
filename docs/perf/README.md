# 성능 측정·최적화 작업 노트 (브랜치 `Claude_PerfOpt`)

2026-08-11 기준. 측정 환경을 세우는 과정에서 얻은 사실과, 아직 열려 있는 질문을 남긴다.

## 1. 확정된 사실

### 1-1. 병목은 GPU다
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
