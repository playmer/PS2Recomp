void GsSetCrt(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  int interlaced = getRegU32(ctx, 4); // $a0 - 0=non-interlaced, 1=interlaced
  int videoMode = getRegU32(ctx, 5);  // $a1 - 0=NTSC, 1=PAL, 2=VESA, 3=HiVision
  int frameMode = getRegU32(ctx, 6);  // $a2 - 0=field, 1=frame

  std::cout << "PS2 GsSetCrt: interlaced=" << interlaced
            << ", videoMode=" << videoMode << ", frameMode=" << frameMode
            << std::endl;

  if (runtime) {
    GSRegisters &gs = runtime->memory().gs();

    // PMODE: EN1=1 (read circuit 1 enabled), EN2=1 (read circuit 2 enabled),
    // MMOD=1 (Alpha blending), AMOD=1 (Alpha selection)
    // For now just enable RC1/RC2.
    // gs.pmode = 0xFFFFFFFF00000003; // Simple enable
    // Better:
    uint64_t pmode = 0;
    pmode |= 1; // EN1
    pmode |= 2; // EN2
    // pmode |= (1<<2); // CRT MD
    // pmode |= (1<<3); // MMOD
    // pmode |= (1<<4); // AMOD
    // pmode |= (1<<5); // SLBG
    // pmode |= (1<<6); // ALP
    gs.pmode = pmode;

    // SMODE2: INT=interlaced, FF=frameMode
    uint64_t smode2 = 0;
    if (interlaced)
      smode2 |= 1;
    if (frameMode)
      smode2 |= 2;
    if (videoMode == 2)
      smode2 |= (2 << 2); // VESA
    // else NTSC/PAL/DTV is handled typically by VCK or just 0
    gs.smode2 = smode2;

    // Fallback: If DISPFB1 is 0, set a default config to show SOMETHING.
    // FBP=0 (Base 0), FBW=10 (640px), PSM=0 (CT32)
    if (gs.dispfb1 == 0) {
      std::cout << "PS2 GsSetCrt: DISPFB1 is 0, setting default 640x448 setup."
                << std::endl;
      uint32_t fbp = 0;
      uint32_t fbw = 10; // 640 / 64
      uint32_t psm = 0;  // PSMCT32
      gs.dispfb1 = (uint64_t)fbp | ((uint64_t)fbw << 9) | ((uint64_t)psm << 15);

      // Set DISPLAY1 too if 0
      if (gs.display1 == 0) {
        // DW=2560-1 (640*VCLK), DH=448-1?
        // Note: DW/DH are in VCLK units? No, pixels?
        // UploadFrame uses DW as pixels directly?
        // ps2_runtime reads: dw = (display >> 32) & 0xFFF.
        // So we set standard values.
        uint32_t dw = 639;
        uint32_t dh = 447;
        gs.display1 = ((uint64_t)dw << 32) | ((uint64_t)dh << 44);
      }
    }
  }
}

void GsGetIMR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint64_t imr = 0;
  if (runtime) {
    imr = runtime->memory().gs().imr;
  }

  std::cout << "PS2 GsGetIMR: Returning IMR=0x" << std::hex << imr << std::dec
            << std::endl;

  setReturnU64(ctx, imr); // Return in $v0/$v1
}

void GsPutIMR(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint64_t newImr =
      getRegU32(ctx, 4) | ((uint64_t)getRegU32(ctx, 5)
                           << 32); // $a0 = lower 32 bits, $a1 = upper 32 bits
  uint64_t oldImr = 0;
  if (runtime) {
    oldImr = runtime->memory().gs().imr;
    runtime->memory().gs().imr = newImr;
  }
  std::cout << "PS2 GsPutIMR: Setting IMR=0x" << std::hex << newImr << std::dec
            << std::endl;
  setReturnU64(ctx, oldImr);
}

void GsSetVideoMode(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  int mode = getRegU32(ctx, 4); // $a0 - video mode (various flags)

  std::cout << "PS2 GsSetVideoMode: mode=0x" << std::hex << mode << std::dec
            << std::endl;

  // Do nothing for now.
}

void GetOsdConfigParam(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint32_t paramAddr =
      getRegU32(ctx, 4); // $a0 - pointer to parameter structure

  if (!getMemPtr(rdram, paramAddr)) {
    std::cerr << "PS2 GetOsdConfigParam error: Invalid parameter address: 0x"
              << std::hex << paramAddr << std::dec << std::endl;
    setReturnS32(ctx, -1);
    return;
  }

  uint32_t *param = reinterpret_cast<uint32_t *>(getMemPtr(rdram, paramAddr));

  ensureOsdConfigInitialized();
  uint32_t raw;
  {
    std::lock_guard<std::mutex> lock(g_osd_mutex);
    raw = g_osd_config_raw;
  }

  *param = raw;

  setReturnS32(ctx, 0);
}

void SetOsdConfigParam(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint32_t paramAddr =
      getRegU32(ctx, 4); // $a0 - pointer to parameter structure

  if (!getConstMemPtr(rdram, paramAddr)) {
    std::cerr << "PS2 SetOsdConfigParam error: Invalid parameter address: 0x"
              << std::hex << paramAddr << std::dec << std::endl;
    setReturnS32(ctx, -1);
    return;
  }

  const uint32_t *param =
      reinterpret_cast<const uint32_t *>(getConstMemPtr(rdram, paramAddr));
  uint32_t raw = param ? *param : 0;
  raw = sanitizeOsdConfigRaw(raw);
  {
    std::lock_guard<std::mutex> lock(g_osd_mutex);
    g_osd_config_raw = raw;
    g_osd_config_initialized = true;
  }

  setReturnS32(ctx, 0);
}

void GetRomName(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint32_t bufAddr = getRegU32(ctx, 4); // $a0
  size_t bufSize = getRegU32(ctx, 5);   // $a1
  char *hostBuf = reinterpret_cast<char *>(getMemPtr(rdram, bufAddr));
  const char *romName = "ROMVER 0100";

  if (!hostBuf) {
    std::cerr << "GetRomName error: Invalid buffer address" << std::endl;
    setReturnS32(ctx, -1); // Error
    return;
  }
  if (bufSize == 0) {
    setReturnS32(ctx, 0);
    return;
  }

  strncpy(hostBuf, romName, bufSize - 1);
  hostBuf[bufSize - 1] = '\0';

  // returns the length of the string (excluding null?) or error
  setReturnS32(ctx, (int32_t)strlen(hostBuf));
}

void SifLoadElfPart(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  const uint32_t pathAddr = getRegU32(ctx, 4); // $a0 - path
  const uint32_t secNameAddr =
      getRegU32(ctx, 5); // $a1 - section name ("all" typically)
  const uint32_t execDataAddr = getRegU32(ctx, 6); // $a2 - t_ExecData*

  std::string secName =
      readGuestCStringBounded(rdram, secNameAddr, kLoadfileArgMaxBytes);
  if (secName.empty()) {
    secName = "all";
  }

  const int32_t ret =
      runSifLoadElfPart(rdram, ctx, runtime, pathAddr, secName, execDataAddr);
  setReturnS32(ctx, ret);
}

void sceSifLoadElf(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  const uint32_t pathAddr = getRegU32(ctx, 4);     // $a0 - path
  const uint32_t execDataAddr = getRegU32(ctx, 5); // $a1 - t_ExecData*
  const int32_t ret =
      runSifLoadElfPart(rdram, ctx, runtime, pathAddr, "all", execDataAddr);
  setReturnS32(ctx, ret);
}

void sceSifLoadElfPart(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  SifLoadElfPart(rdram, ctx, runtime);
}

void sceSifLoadModule(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  // Use the same tracker as SifLoadModule so both APIs return the same module
  // IDs.
  SifLoadModule(rdram, ctx, runtime);
}

void sceSifLoadModuleBuffer(uint8_t *rdram, R5900Context *ctx,
                            PS2Runtime *runtime) {
  const uint32_t bufferAddr = getRegU32(ctx, 4); // $a0
  if (!rdram || bufferAddr == 0u) {
    setReturnS32(ctx, -1);
    return;
  }

  // Match buffer-based module loads to stable synthetic tags so module ID
  // lookup remains deterministic.
  const std::string moduleTag = makeSifModuleBufferTag(rdram, bufferAddr);
  const int32_t moduleId = trackSifModuleLoad(moduleTag);
  if (moduleId <= 0) {
    setReturnS32(ctx, -1);
    return;
  }

  uint32_t refs = 0;
  {
    std::lock_guard<std::mutex> lock(g_sif_module_mutex);
    auto it = g_sif_modules_by_id.find(moduleId);
    if (it != g_sif_modules_by_id.end()) {
      refs = it->second.refCount;
    }
  }
  logSifModuleAction("load-buffer", moduleId, moduleTag, refs);
  setReturnS32(ctx, moduleId);
}

void TODO(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime,
          uint32_t encodedSyscallId) {
  // a bit more detail mayber reomve old logic, lets get it more raw
  std::cerr << "[Syscall TODO]"
            << " encoded=0x" << std::hex << encodedSyscallId << " v1=0x"
            << getRegU32(ctx, 3) << " v0=0x" << getRegU32(ctx, 2) << " a0=0x"
            << getRegU32(ctx, 4) << " a1=0x" << getRegU32(ctx, 5) << " a2=0x"
            << getRegU32(ctx, 6) << " a3=0x" << getRegU32(ctx, 7) << " pc=0x"
            << ctx->pc << std::dec << std::endl;

  const uint32_t v0 = getRegU32(ctx, 2);
  const uint32_t v1 = getRegU32(ctx, 3);
  const uint32_t caller_ra = getRegU32(ctx, 31);
  uint32_t syscallId = encodedSyscallId;
  if (syscallId == 0u) {
    syscallId = (v0 != 0u) ? v0 : v1;
  }

  std::cerr << "Warning: Unimplemented PS2 syscall called. PC=0x" << std::hex
            << ctx->pc << ", RA=0x" << caller_ra << ", Encoded=0x"
            << encodedSyscallId << ", v0=0x" << v0 << ", v1=0x" << v1
            << ", Chosen=0x" << syscallId << std::dec << std::endl;

  std::cerr << "  Args: $a0=0x" << std::hex << getRegU32(ctx, 4) << ", $a1=0x"
            << getRegU32(ctx, 5) << ", $a2=0x" << getRegU32(ctx, 6)
            << ", $a3=0x" << getRegU32(ctx, 7) << std::dec << std::endl;

  // Common syscalls:
  // 0x04: Exit
  // 0x06: LoadExecPS2
  // 0x07: ExecPS2
  if (syscallId == 0x04u) {
    std::cerr << "  -> Syscall is Exit(), calling ExitThread stub."
              << std::endl;
    ExitThread(rdram, ctx, runtime);
    return;
  }

  static std::mutex s_unknownMutex;
  static std::unordered_map<uint32_t, uint64_t> s_unknownCounts;
  {
    std::lock_guard<std::mutex> lock(s_unknownMutex);
    const uint64_t count = ++s_unknownCounts[syscallId];
    if (count == 1 || (count % 5000u) == 0u) {
      std::cerr << "  -> Unknown syscallId=0x" << std::hex << syscallId
                << " hits=" << std::dec << count << std::endl;
    }
  }

  // Bootstrap default: avoid hard-failing loops that probe syscall
  // availability.
  setReturnS32(ctx, 0);
}

// 0x3C SetupThread: returns stack pointer (stack + stack_size)
// args: $a0 = stack base, $a1 = stack size, $a2 = gp, $a3 = entry point
void SetupThread(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint32_t stackBase = getRegU32(ctx, 4);
  uint32_t stackSize = getRegU32(ctx, 5);
  uint32_t sp = stackBase + stackSize;
  setReturnS32(ctx, sp);
}

// 0x3D SetupHeap: returns heap base/start pointer
void SetupHeap(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  const uint32_t heapBase = getRegU32(ctx, 4); // $a0
  const uint32_t heapSize = getRegU32(ctx, 5); // $a1 (optional size)

  if (runtime) {
    uint32_t heapLimit = PS2_RAM_SIZE;
    if (heapSize != 0u && heapBase < PS2_RAM_SIZE) {
      const uint64_t candidateLimit =
          static_cast<uint64_t>(heapBase) + static_cast<uint64_t>(heapSize);
      heapLimit = static_cast<uint32_t>(
          std::min<uint64_t>(candidateLimit, PS2_RAM_SIZE));
    }
    runtime->configureGuestHeap(heapBase, heapLimit);
    setReturnU32(ctx, runtime->guestHeapBase());
    return;
  }

  setReturnU32(ctx, heapBase);
}

// 0x3E EndOfHeap: commonly returns current heap end; keep it stable for now.
void EndOfHeap(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  if (runtime) {
    setReturnU32(ctx, runtime->guestHeapEnd());
    return;
  }

  setReturnU32(ctx, getRegU32(ctx, 4));
}

// 0x5A QueryBootMode (stub): return 0 for now
void QueryBootMode(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  uint32_t mode = getRegU32(ctx, 4);
  ensureBootModeTable(rdram);
  uint32_t addr = 0;
  {
    std::lock_guard<std::mutex> lock(g_bootmode_mutex);
    auto it = g_bootmode_addresses.find(static_cast<uint8_t>(mode));
    if (it != g_bootmode_addresses.end())
      addr = it->second;
  }
  setReturnU32(ctx, addr);
}

// 0x5B GetThreadTLS (stub): return 0
void GetThreadTLS(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) {
  auto info = ensureCurrentThreadInfo(ctx);
  if (!info) {
    setReturnU32(ctx, 0);
    return;
  }

  if (info->tlsBase == 0) {
    info->tlsBase = allocTlsAddr(rdram);
  }

  setReturnU32(ctx, info->tlsBase);
}

// 0x74 RegisterExitHandler (stub): return 0
void RegisterExitHandler(uint8_t *rdram, R5900Context *ctx,
                         PS2Runtime *runtime) {
  uint32_t func = getRegU32(ctx, 4);
  uint32_t arg = getRegU32(ctx, 5);
  if (func == 0) {
    setReturnS32(ctx, -1);
    return;
  }

  int tid = g_currentThreadId;
  {
    std::lock_guard<std::mutex> lock(g_exit_handler_mutex);
    g_exit_handlers[tid].push_back({func, arg});
  }

  setReturnS32(ctx, 0);
}
