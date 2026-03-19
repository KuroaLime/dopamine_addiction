# DevLog (Index)

## 2026-01
- 2026-01-01: Subst X:로 UE 경로 이슈 해결 + VS Code 설치 + DevLog 구조 생성 → [20260101.md](20260101.md)

- 2026-01-02: 재부팅 후 X: 드라이브(subst) 자동 매핑 배치 적용 + 한글 경로/인코딩 깨짐(Target path not found) 해결 → [20260102.md](20260102.md)

- 2026-01-02: UE 멀티 테스트 & 색상 Replication 시도(미해결) → [20260105.md](20260105.md)

- 2026-01-06: IOCP 로비 서버 프로젝트 생성 + 기본 뼈대(WSAInit/Listen/IOCP/Worker/Accept/PostRecv) 작성 및 빌드 준비 → [20260106.md](20260106.md)

- 2026-01-07: IOCP 로비 서버 뼈대 구현(세션 구조/Accept 후 IOCP 연결/워커 스레드 완료 통지 루프) → [20260107.md](20260107.md)

- 2026-01-08: IOCP 로비 서버 패킷 경계 처리 + Send 경로 추가 + Disconnect/에러 처리 강화 → [20260108.md](20260108.md)

- 2026-01-22: IOCP 서버 송신 경로 고도화(SendQueue/in-flight + Partial Send) + 패킷 경계 처리(PacketHeader/Dispatcher) 적용 → [20260122.md](20260122.md)

- 2026-01-23: 로비 서버 기본 로직(세션ID WELCOME/방 목록 자동 전송/방 목록 재요청) + LobbyService로 로비 로직 분리 → [20260123.md](20260123.md)

- 2026-01-26: 방 입장(Join) 로직 구현 + 4인 충족 시 상태 자동 변경 + 실시간 상태 브로드캐스트 동기화 → [20260126.md](20260126.md)

- 2026-01-27: Dedicated Server 포트 풀링(Alloc/Free) 구현 + 4인 매칭 시 게임 서버 이동 명령(Handover) 패킷 적용 → [20260127.md](20260127.md)