# 마우스 룩 대체 드라이버 (안전판).
#
# ⚠️ 이전 버전은 매 틱 unreal.load_object(None, "<PIE 월드 경로>")를 호출했다.
#    PIE가 종료되는 순간 이 호출이 PIE 월드 패키지를 다시 로드시켜
#    WorldSubsystem이 이중 초기화되고(ensure !bInitialized, WorldSubsystem.cpp:158)
#    에디터가 EXCEPTION_ACCESS_VIOLATION으로 죽었다.
#
# 수정: 월드/컨트롤러를 시작 시 1회만 해석하고, 이후에는 유효성만 검사한다.
#       PIE가 끝나면 스스로 중단한다.

import unreal, math

CLIENT_WORLD = "/Game/InGame/System/UEDPIE_1_Main_Game_World.Main_Game_World"


class LookDriver:
    def __init__(self):
        self.t = 0.0
        self.handle = None
        self.ticks = 0
        self.pc = None

    def start(self):
        world = unreal.load_object(None, CLIENT_WORLD)   # 시작 시 1회만
        if not world:
            unreal.log_error("look: client world not found")
            return False
        self.pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not self.pc:
            unreal.log_error("look: no player controller")
            return False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        return True

    def stop(self):
        if self.handle:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.pc = None

    def tick(self, dt):
        try:
            # PIE가 끝났으면 스스로 정지 (load_object 재호출 금지)
            if self.pc is None or not unreal.SystemLibrary.is_valid(self.pc):
                self.stop()
                return
            self.t += dt
            self.ticks += 1
            yaw = 180.0 * math.sin(self.t * 0.30)
            pitch = -8.0 + 10.0 * math.sin(self.t * 0.23)
            self.pc.set_control_rotation(unreal.Rotator(0.0, pitch, yaw))
        except Exception as e:
            unreal.log_error("look: %s" % e)
            self.stop()


try:
    _perf_driver.stop()
except NameError:
    pass

_perf_driver = LookDriver()
print("look driver started:", _perf_driver.start())
