/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file can be included many times to list all the x86 CPU features.
 * Simply define the NACL_X86_CPU_FEATURE macro to change the behavior.
 * Parameters are:
 *   - Identifier
 *   - CPU feature register
 *   - CPUID index
 *   - Fixed-feature: FixedOn, FixedOff
 *   - CPU vendor specific: NONE, AMD
 *   - Stringified name
 */

NACL_X86_CPU_FEATURE(CPUIDSupported, NONE , NONE       , FixedOn , NONE, "CPUID supported")
NACL_X86_CPU_FEATURE(CPUSupported  , NONE , NONE       , FixedOn , NONE, "CPU supported"  )
NACL_X86_CPU_FEATURE(3DNOW         , EDX_A, EDX_3DN    , FixedOff, AMD , "3DNow"          )
NACL_X86_CPU_FEATURE(AES           , ECX_I, ECX_AES    , FixedOff, NONE, "AES"            )
NACL_X86_CPU_FEATURE(AVX           , ECX_I, ECX_AVX    , FixedOff, NONE, "AVX"            )
NACL_X86_CPU_FEATURE(AVX2          , EBX_7, EBX_AVX2   , FixedOff, NONE, "AVX2"           )
NACL_X86_CPU_FEATURE(BMI1          , EBX_7, EBX_BMI1   , FixedOff, NONE, "BMI1"           )
NACL_X86_CPU_FEATURE(CLFLUSH       , EDX_I, EDX_CLFLUSH, FixedOn , NONE, "CLFLUSH"        )
NACL_X86_CPU_FEATURE(CLMUL         , ECX_I, ECX_CLMUL  , FixedOff, NONE, "CLMUL"          )
NACL_X86_CPU_FEATURE(CMOV          , EDX_I, EDX_CMOV   , FixedOn , NONE, "CMOV"           )
NACL_X86_CPU_FEATURE(CX16          , ECX_I, ECX_CX16   , FixedOn , NONE, "CMPXCHG16B"     )
NACL_X86_CPU_FEATURE(CX8           , EDX_I, EDX_CX8    , FixedOn , NONE, "CMPXCHG8B"      )
NACL_X86_CPU_FEATURE(E3DNOW        , EDX_A, EDX_E3DN   , FixedOff, AMD , "E3DNow"         )
NACL_X86_CPU_FEATURE(EMMX          , EDX_A, EDX_EMMX   , FixedOff, AMD , "EMMX"           )
NACL_X86_CPU_FEATURE(F16C          , ECX_I, ECX_F16C   , FixedOff, NONE, "F16C"           )
NACL_X86_CPU_FEATURE(FMA           , ECX_I, ECX_FMA    , FixedOff, NONE, "FMA"            )
NACL_X86_CPU_FEATURE(FMA4          , ECX_A, ECX_FMA4   , FixedOff, AMD , "FMA4"           )
NACL_X86_CPU_FEATURE(FXSR          , EDX_I, EDX_FXSR   , FixedOn , NONE, "FXSAVE/FXRSTOR" )
NACL_X86_CPU_FEATURE(LAHF          , ECX_A, ECX_LAHF   , FixedOff, NONE, "LAHF"           )
NACL_X86_CPU_FEATURE(LM            , EDX_A, EDX_LM     , FixedOff, NONE, "LongMode"       )
NACL_X86_CPU_FEATURE(LWP           , ECX_A, ECX_LWP    , FixedOff, AMD , "LWP"            )
NACL_X86_CPU_FEATURE(LZCNT         , ECX_A, ECX_ABM    , FixedOff, AMD , "LZCNT"          )
NACL_X86_CPU_FEATURE(MMX           , EDX_I, EDX_MMX    , FixedOff, NONE, "MMX"            )
NACL_X86_CPU_FEATURE(MON           , ECX_I, ECX_MON    , FixedOff, NONE, "MONITOR/MWAIT"  )
NACL_X86_CPU_FEATURE(MOVBE         , ECX_I, ECX_MOVBE  , FixedOff, NONE, "MOVBE"          )
NACL_X86_CPU_FEATURE(OSXSAVE       , ECX_I, ECX_OSXSAVE, FixedOff, NONE, "OSXSAVE"        )
NACL_X86_CPU_FEATURE(POPCNT        , ECX_I, ECX_POPCNT , FixedOff, NONE, "POPCNT"         )
NACL_X86_CPU_FEATURE(PRE           , ECX_A, ECX_PRE    , FixedOff, AMD , "3DNowPrefetch"  )
NACL_X86_CPU_FEATURE(SSE           , EDX_I, EDX_SSE    , FixedOn , NONE, "SSE"            )
NACL_X86_CPU_FEATURE(SSE2          , EDX_I, EDX_SSE2   , FixedOn , NONE, "SSE2"           )
NACL_X86_CPU_FEATURE(SSE3          , ECX_I, ECX_SSE3   , FixedOn , NONE, "SSE3"           )
NACL_X86_CPU_FEATURE(SSE41         , ECX_I, ECX_SSE41  , FixedOff, NONE, "SSE41"          )
NACL_X86_CPU_FEATURE(SSE42         , ECX_I, ECX_SSE42  , FixedOff, NONE, "SSE42"          )
NACL_X86_CPU_FEATURE(SSE4A         , ECX_A, ECX_SSE4A  , FixedOff, AMD , "SSE4A"          )
NACL_X86_CPU_FEATURE(SSSE3         , ECX_I, ECX_SSSE3  , FixedOff, NONE, "SSSE3"          )
NACL_X86_CPU_FEATURE(TBM           , ECX_A, ECX_TBM    , FixedOff, AMD , "TBM"            )
NACL_X86_CPU_FEATURE(TSC           , EDX_I, EDX_TSC    , FixedOn , NONE, "RDTSC"          )
NACL_X86_CPU_FEATURE(x87           , EDX_I, EDX_x87    , FixedOff, NONE, "x87"            )
NACL_X86_CPU_FEATURE(XOP           , ECX_A, ECX_XOP    , FixedOff, AMD , "XOP"            )
