#include "ps2_syscalls.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "ps2_stubs.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h> // for mkdir
#include <unistd.h>   // for unlink,rmdir,chdir
#endif
#include <ThreadNaming.h>

std::string translatePs2Path(const char *ps2Path);

#include "syscalls/helpers/ps2_syscalls_helpers_loader.inl"
#include "syscalls/helpers/ps2_syscalls_helpers_path.inl"
#include "syscalls/helpers/ps2_syscalls_helpers_runtime.inl"
#include "syscalls/helpers/ps2_syscalls_helpers_state.inl"

namespace ps2_syscalls {
#include "syscalls/ps2_syscalls_interrupt.inl"
#include "syscalls/ps2_syscalls_system.inl"

bool dispatchNumericSyscall(uint32_t syscallNumber, uint8_t *rdram,
                            R5900Context *ctx, PS2Runtime *runtime) {
  // Log all syscalls for trace (temp debug)
  // std::cout << "Syscall: 0x" << std::hex << syscallNumber << std::dec <<
  // std::endl; Use filtered logging to avoid spam if inside loop, but for init
  // it's fine. Let's log unique syscalls or just all of them for now.
  static uint32_t lastSyscall = 0xFFFFFFFF;
  if (syscallNumber != lastSyscall || syscallNumber == 0x7B) {
    std::cout << "[Syscall] 0x" << std::hex << syscallNumber << std::dec
              << std::endl;
    lastSyscall = syscallNumber;
  }

  switch (syscallNumber) {
  case 0x01:
    ResetEE(rdram, ctx, runtime);
    return true;
  case 0x02:
    GsSetCrt(rdram, ctx, runtime);
    return true;
  case 0x04:
    ExitThread(rdram, ctx, runtime);
    return true;
  case 0x10:
    AddIntcHandler(rdram, ctx, runtime);
    return true;
  case 0x11:
    RemoveIntcHandler(rdram, ctx, runtime);
    return true;
  case 0x12:
    AddDmacHandler(rdram, ctx, runtime);
    return true;
  case 0x13:
    RemoveDmacHandler(rdram, ctx, runtime);
    return true;
  case 0x14:
    EnableIntc(rdram, ctx, runtime);
    return true;
  case 0x15:
    DisableIntc(rdram, ctx, runtime);
    return true;
  case 0x16:
    EnableDmac(rdram, ctx, runtime);
    return true;
  case 0x17:
    DisableDmac(rdram, ctx, runtime);
    return true;
  case 0x18:
  case 0xFC:
    SetAlarm(rdram, ctx, runtime);
    return true;
  case 0x19:
  case 0xFE:
    CancelAlarm(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x1E):
  case static_cast<uint32_t>(-0xFD):
    iSetAlarm(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x1F):
  case static_cast<uint32_t>(-0xFF):
    iCancelAlarm(rdram, ctx, runtime);
    return true;
  case 0x20:
    CreateThread(rdram, ctx, runtime);
    return true;
  case 0x21:
    DeleteThread(rdram, ctx, runtime);
    return true;
  case 0x22:
    StartThread(rdram, ctx, runtime);
    return true;
  case 0x23:
    ExitThread(rdram, ctx, runtime);
    return true;
  case 0x24:
    ExitDeleteThread(rdram, ctx, runtime);
    return true;
  case 0x25:
    TerminateThread(rdram, ctx, runtime);
    return true;
  case 0x29:
  case static_cast<uint32_t>(-0x2A):
    ChangeThreadPriority(rdram, ctx, runtime);
    return true;
  case 0x2B:
  case static_cast<uint32_t>(-0x2C):
    RotateThreadReadyQueue(rdram, ctx, runtime);
    return true;
  case 0x2D:
    ReleaseWaitThread(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x2E):
    iReleaseWaitThread(rdram, ctx, runtime);
    return true;
  case 0x2F:
  case static_cast<uint32_t>(-0x2F):
    GetThreadId(rdram, ctx, runtime);
    return true;
  case 0x30:
  case static_cast<uint32_t>(-0x31):
    ReferThreadStatus(rdram, ctx, runtime);
    return true;
  case 0x32:
    SleepThread(rdram, ctx, runtime);
    return true;
  case 0x33:
    WakeupThread(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x34):
    iWakeupThread(rdram, ctx, runtime);
    return true;
  case 0x35:
    CancelWakeupThread(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x36):
    iCancelWakeupThread(rdram, ctx, runtime);
    return true;
  case 0x37:
  case static_cast<uint32_t>(-0x38):
    SuspendThread(rdram, ctx, runtime);
    return true;
  case 0x39:
  case static_cast<uint32_t>(-0x3A):
    ResumeThread(rdram, ctx, runtime);
    return true;
  case 0x3C:
    SetupThread(rdram, ctx, runtime);
    return true;
  case 0x3D:
    SetupHeap(rdram, ctx, runtime);
    return true;
  case 0x3E:
    EndOfHeap(rdram, ctx, runtime);
    return true;
  case 0x40:
    CreateSema(rdram, ctx, runtime);
    return true;
  case 0x41:
  case static_cast<uint32_t>(-0x49):
    DeleteSema(rdram, ctx, runtime);
    return true;
  case 0x42:
    SignalSema(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x43):
    iSignalSema(rdram, ctx, runtime);
    return true;
  case 0x44:
    WaitSema(rdram, ctx, runtime);
    return true;
  case 0x45:
    PollSema(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x46):
    iPollSema(rdram, ctx, runtime);
    return true;
  case 0x47:
    ReferSemaStatus(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x48):
    iReferSemaStatus(rdram, ctx, runtime);
    return true;
  case 0x4A:
    SetOsdConfigParam(rdram, ctx, runtime);
    return true;
  case 0x4B:
    GetOsdConfigParam(rdram, ctx, runtime);
    return true;
  case 0x50:
    CreateEventFlag(rdram, ctx, runtime);
    return true;
  case 0x51:
    DeleteEventFlag(rdram, ctx, runtime);
    return true;
  case 0x52:
    SetEventFlag(rdram, ctx, runtime);
    return true;
  case 0x53:
    iSetEventFlag(rdram, ctx, runtime);
    return true;
  case 0x54:
    ClearEventFlag(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x55):
    iClearEventFlag(rdram, ctx, runtime);
    return true;
  case 0x56:
    WaitEventFlag(rdram, ctx, runtime);
    return true;
  case 0x57:
    PollEventFlag(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x58):
    iPollEventFlag(rdram, ctx, runtime);
    return true;
  case 0x59:
    ReferEventFlagStatus(rdram, ctx, runtime);
    return true;
  case static_cast<uint32_t>(-0x5A):
    iReferEventFlagStatus(rdram, ctx, runtime);
    return true;
  case 0x5A:
    QueryBootMode(rdram, ctx, runtime);
    return true;
  case 0x5B:
    GetThreadTLS(rdram, ctx, runtime);
    return true;
  case 0x5C:
  case static_cast<uint32_t>(-0x5C):
    EnableIntcHandler(rdram, ctx, runtime);
    return true;
  case 0x5D:
  case static_cast<uint32_t>(-0x5D):
    DisableIntcHandler(rdram, ctx, runtime);
    return true;
  case 0x5E:
  case static_cast<uint32_t>(-0x5E):
    EnableDmacHandler(rdram, ctx, runtime);
    return true;
  case 0x5F:
  case static_cast<uint32_t>(-0x5F):
    DisableDmacHandler(rdram, ctx, runtime);
    return true;
  case 0x64:
    FlushCache(rdram, ctx, runtime);
    return true;
  case 0x70:
  case static_cast<uint32_t>(-0x70):
    GsGetIMR(rdram, ctx, runtime);
    return true;
  case 0x71:
  case static_cast<uint32_t>(-0x71):
    GsPutIMR(rdram, ctx, runtime);
    return true;
  case 0x74:
    RegisterExitHandler(rdram, ctx, runtime);
    return true;
  case 0x83: // ReturnFromException (eret) / Unknown Timer?
  {
    // Update v0 to simulate a timer/counter for the loop in sub_0011A598
    // Start at a high value to avoid underflow in game logic (T - 524)
    static uint32_t fake_timer = 2000000;
    fake_timer +=
        164; // Exact increment to match the offset difference (524-360)
    SET_GPR_U32(ctx, 2, fake_timer); // Set $v0

    // Clear EXL bit in Status register (bit 1) per eret spec - this re-enables
    // interrupts
    ctx->cop0_status &= ~0x2;

    if (ctx->cop0_epc != 0) {
      ctx->pc = ctx->cop0_epc;
    } else {
      // If EPC is 0, we assume this is just a request to leave exception level
      // (EXL=0). We fall through to the next instruction (handled by returning
      // true to handleSyscall and NOT changing ctx->pc). std::cout << "Syscall:
      // eret (0x83) with EPC=0. Clearing EXL and falling through." <<
      // std::endl;
    }
    return true;
  }
  case 0x85:
    SetMemoryMode(rdram, ctx, runtime);
    return true;
  default:
    return false;
  }
}

#include "syscalls/ps2_syscalls_fileio.inl"
#include "syscalls/ps2_syscalls_flags.inl"
#include "syscalls/ps2_syscalls_rpc.inl"
#include "syscalls/ps2_syscalls_thread.inl"

void checkEvents(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  static auto last = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  // 60Hz ~ 16ms
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
          .count() > 16) {
    last = now;
    // Iterate registered handlers
    for (auto &pair : g_intcHandlers) {
      // Cause 2 = VSYNC_START, Cause 3 = VSYNC_END
      if (pair.second.enabled &&
          (pair.second.cause == 2 || pair.second.cause == 3)) {
        // Log for debug (once per sec?) - no, spammy.
        std::cout << "[INTC] Dispatching VSync handler " << std::hex
                  << pair.second.handler << std::dec << std::endl;

        // Simulate Exception Entry
        // 1. Set EPC to current PC
        ctx->cop0_epc = ctx->pc;
        // 2. Set Status.EXL = 1 (Exception Level)
        ctx->cop0_status |= 0x2;
        // 3. Set Cause (0 = Interrupt)
        ctx->cop0_cause = (ctx->cop0_cause & ~0x7Cu) | (0 << 2);

        // 4. Jump to Handler
        ctx->pc = pair.second.handler;

        // 5. Set A0 = argument
        setRegU32(ctx, 4, pair.second.arg);

        // Only dispatch one interrupt per check
        return;
      }
    }
  }
}
} // namespace ps2_syscalls
