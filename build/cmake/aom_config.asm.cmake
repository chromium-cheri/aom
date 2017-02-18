ARCH_ARM equ ${ARCH_ARM}
ARCH_MIPS equ ${ARCH_MIPS}
ARCH_X86 equ ${ARCH_X86}
ARCH_X86_64 equ ${ARCH_X86_64}
HAVE_EDSP equ ${HAVE_EDSP}
HAVE_MEDIA equ ${HAVE_MEDIA}
HAVE_NEON equ ${HAVE_NEON}
HAVE_NEON_ASM equ ${HAVE_NEON_ASM}
HAVE_MIPS32 equ ${HAVE_MIPS32}
HAVE_DSPR2 equ ${HAVE_DSPR2}
HAVE_MSA equ ${HAVE_MSA}
HAVE_MIPS64 equ ${HAVE_MIPS64}
HAVE_MMX equ ${HAVE_MMX}
HAVE_SSE equ ${HAVE_SSE}
HAVE_SSE2 equ ${HAVE_SSE2}
HAVE_SSE3 equ ${HAVE_SSE3}
HAVE_SSSE3 equ ${HAVE_SSSE3}
HAVE_SSE4_1 equ ${HAVE_SSE4_1}
HAVE_AVX equ ${HAVE_AVX}
HAVE_AVX2 equ ${HAVE_AVX2}
HAVE_AOM_PORTS equ ${HAVE_AOM_PORTS}
HAVE_PTHREAD_H equ ${HAVE_PTHREAD_H}
HAVE_UNISTD_H equ ${HAVE_UNISTD_H}
CONFIG_DEPENDENCY_TRACKING equ ${CONFIG_DEPENDENCY_TRACKING}
CONFIG_EXTERNAL_BUILD equ ${CONFIG_EXTERNAL_BUILD}
CONFIG_INSTALL_DOCS equ ${CONFIG_INSTALL_DOCS}
CONFIG_INSTALL_BINS equ ${CONFIG_INSTALL_BINS}
CONFIG_INSTALL_LIBS equ ${CONFIG_INSTALL_LIBS}
CONFIG_INSTALL_SRCS equ ${CONFIG_INSTALL_SRCS}
CONFIG_DEBUG equ ${CONFIG_DEBUG}
CONFIG_GPROF equ ${CONFIG_GPROF}
CONFIG_GCOV equ ${CONFIG_GCOV}
CONFIG_RVCT equ ${CONFIG_RVCT}
CONFIG_GCC equ ${CONFIG_GCC}
CONFIG_MSVS equ ${CONFIG_MSVS}
CONFIG_PIC equ ${CONFIG_PIC}
CONFIG_BIG_ENDIAN equ ${CONFIG_BIG_ENDIAN}
CONFIG_CODEC_SRCS equ ${CONFIG_CODEC_SRCS}
CONFIG_DEBUG_LIBS equ ${CONFIG_DEBUG_LIBS}
CONFIG_DEQUANT_TOKENS equ ${CONFIG_DEQUANT_TOKENS}
CONFIG_DC_RECON equ ${CONFIG_DC_RECON}
CONFIG_RUNTIME_CPU_DETECT equ ${CONFIG_RUNTIME_CPU_DETECT}
CONFIG_POSTPROC equ ${CONFIG_POSTPROC}
CONFIG_AV1_POSTPROC equ ${CONFIG_AV1_POSTPROC}
CONFIG_MULTITHREAD equ ${CONFIG_MULTITHREAD}
CONFIG_INTERNAL_STATS equ ${CONFIG_INTERNAL_STATS}
CONFIG_AV1_ENCODER equ ${CONFIG_AV1_ENCODER}
CONFIG_AV1_DECODER equ ${CONFIG_AV1_DECODER}
CONFIG_AV1 equ ${CONFIG_AV1}
CONFIG_ENCODERS equ ${CONFIG_ENCODERS}
CONFIG_DECODERS equ ${CONFIG_DECODERS}
CONFIG_STATIC_MSVCRT equ ${CONFIG_STATIC_MSVCRT}
CONFIG_SPATIAL_RESAMPLING equ ${CONFIG_SPATIAL_RESAMPLING}
CONFIG_REALTIME_ONLY equ ${CONFIG_REALTIME_ONLY}
CONFIG_ONTHEFLY_BITPACKING equ ${CONFIG_ONTHEFLY_BITPACKING}
CONFIG_ERROR_CONCEALMENT equ ${CONFIG_ERROR_CONCEALMENT}
CONFIG_SHARED equ ${CONFIG_SHARED}
CONFIG_STATIC equ ${CONFIG_STATIC}
CONFIG_SMALL equ ${CONFIG_SMALL}
CONFIG_POSTPROC_VISUALIZER equ ${CONFIG_POSTPROC_VISUALIZER}
CONFIG_OS_SUPPORT equ ${CONFIG_OS_SUPPORT}
CONFIG_UNIT_TESTS equ ${CONFIG_UNIT_TESTS}
CONFIG_WEBM_IO equ ${CONFIG_WEBM_IO}
CONFIG_LIBYUV equ ${CONFIG_LIBYUV}
CONFIG_ACCOUNTING equ ${CONFIG_ACCOUNTING}
CONFIG_DECODE_PERF_TESTS equ ${CONFIG_DECODE_PERF_TESTS}
CONFIG_ENCODE_PERF_TESTS equ ${CONFIG_ENCODE_PERF_TESTS}
CONFIG_MULTI_RES_ENCODING equ ${CONFIG_MULTI_RES_ENCODING}
CONFIG_TEMPORAL_DENOISING equ ${CONFIG_TEMPORAL_DENOISING}
CONFIG_COEFFICIENT_RANGE_CHECKING equ ${CONFIG_COEFFICIENT_RANGE_CHECKING}
CONFIG_AOM_HIGHBITDEPTH equ ${CONFIG_AOM_HIGHBITDEPTH}
CONFIG_BETTER_HW_COMPATIBILITY equ ${CONFIG_BETTER_HW_COMPATIBILITY}
CONFIG_EXPERIMENTAL equ ${CONFIG_EXPERIMENTAL}
CONFIG_SIZE_LIMIT equ ${CONFIG_SIZE_LIMIT}
CONFIG_AOM_QM equ ${CONFIG_AOM_QM}
CONFIG_FP_MB_STATS equ ${CONFIG_FP_MB_STATS}
CONFIG_EMULATE_HARDWARE equ ${CONFIG_EMULATE_HARDWARE}
CONFIG_CLPF equ ${CONFIG_CLPF}
CONFIG_DERING equ ${CONFIG_DERING}
CONFIG_VAR_TX equ ${CONFIG_VAR_TX}
CONFIG_RECT_TX equ ${CONFIG_RECT_TX}
CONFIG_REF_MV equ ${CONFIG_REF_MV}
CONFIG_DUAL_FILTER equ ${CONFIG_DUAL_FILTER}
CONFIG_EXT_TX equ ${CONFIG_EXT_TX}
CONFIG_TX64X64 equ ${CONFIG_TX64X64}
CONFIG_SUB8X8_MC equ ${CONFIG_SUB8X8_MC}
CONFIG_EXT_INTRA equ ${CONFIG_EXT_INTRA}
CONFIG_FILTER_INTRA equ ${CONFIG_FILTER_INTRA}
CONFIG_EXT_INTER equ ${CONFIG_EXT_INTER}
CONFIG_EXT_REFS equ ${CONFIG_EXT_REFS}
CONFIG_GLOBAL_MOTION equ ${CONFIG_GLOBAL_MOTION}
CONFIG_NEW_QUANT equ ${CONFIG_NEW_QUANT}
CONFIG_SUPERTX equ ${CONFIG_SUPERTX}
CONFIG_ANS equ ${CONFIG_ANS}
CONFIG_EC_MULTISYMBOL equ ${CONFIG_EC_MULTISYMBOL}
CONFIG_LOOP_RESTORATION equ ${CONFIG_LOOP_RESTORATION}
CONFIG_EXT_PARTITION equ ${CONFIG_EXT_PARTITION}
CONFIG_EXT_PARTITION_TYPES equ ${CONFIG_EXT_PARTITION_TYPES}
CONFIG_UNPOISON_PARTITION_CTX equ ${CONFIG_UNPOISON_PARTITION_CTX}
CONFIG_EXT_TILE equ ${CONFIG_EXT_TILE}
CONFIG_MOTION_VAR equ ${CONFIG_MOTION_VAR}
CONFIG_WARPED_MOTION equ ${CONFIG_WARPED_MOTION}
CONFIG_ENTROPY equ ${CONFIG_ENTROPY}
CONFIG_BIDIR_PRED equ ${CONFIG_BIDIR_PRED}
CONFIG_BITSTREAM_DEBUG equ ${CONFIG_BITSTREAM_DEBUG}
CONFIG_ALT_INTRA equ ${CONFIG_ALT_INTRA}
CONFIG_PALETTE equ ${CONFIG_PALETTE}
CONFIG_DAALA_EC equ ${CONFIG_DAALA_EC}
CONFIG_PVQ equ ${CONFIG_PVQ}
CONFIG_XIPHRC equ ${CONFIG_XIPHRC}
CONFIG_CB4X4 equ ${CONFIG_CB4X4}
CONFIG_FRAME_SIZE equ ${CONFIG_FRAME_SIZE}
CONFIG_DELTA_Q equ ${CONFIG_DELTA_Q}
CONFIG_ADAPT_SCAN equ ${CONFIG_ADAPT_SCAN}
CONFIG_FILTER_7BIT equ ${CONFIG_FILTER_7BIT}
CONFIG_PARALLEL_DEBLOCKING equ ${CONFIG_PARALLEL_DEBLOCKING}
CONFIG_TILE_GROUPS equ ${CONFIG_TILE_GROUPS}
CONFIG_EC_ADAPT equ ${CONFIG_EC_ADAPT}
CONFIG_SIMP_MV_PRED equ ${CONFIG_SIMP_MV_PRED}
CONFIG_RD_DEBUG equ ${CONFIG_RD_DEBUG}
CONFIG_REFERENCE_BUFFER equ ${CONFIG_REFERENCE_BUFFER}
CONFIG_COEF_INTERLEAVE equ ${CONFIG_COEF_INTERLEAVE}
CONFIG_ENTROPY_STATS equ ${CONFIG_ENTROPY_STATS}
CONFIG_MASKED_TX equ ${CONFIG_MASKED_TX}
CONFIG_DAALA_DIST equ ${CONFIG_DAALA_DIST}
