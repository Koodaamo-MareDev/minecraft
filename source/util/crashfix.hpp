// This is a workaround for a bug in libogc (or g++) that causes
// crashes during runtime init when using -O2 optimization level.
// It seems that adding a NOP instruction prevents the crash as it
// prevents the compiler from optimizing things too much.
#define NOP_FIX __asm__ volatile("nop")