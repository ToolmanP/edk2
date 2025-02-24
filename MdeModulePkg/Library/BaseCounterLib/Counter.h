#ifndef __COUNTER_H__

#if defined(__aarch64__)
STATIC inline UINT64 ReadCounter()
{
    UINT64 counter;
    __asm__ __volatile__("isb" ::: "memory");
    __asm__ __volatile__("mrs %0, pmccntr_el0" : "=r" (counter));
    // __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(counter));
    return counter;
}
#elif defined(__x86_64__)
STATIC inline UINT64 ReadCounter()
{
    UINT64 counter;
    __asm__ __volatile__("rdtsc" : "=A"(counter));
    return counter;
}
#else
STATIC inline UINT64 ReadCounter()
{
    return 0;
}
#endif
#endif
