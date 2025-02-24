#include "Init/ArchInit.h"
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include "Uefi/UefiBaseType.h"

#define ARMV8_PMUSERENR_EN_EL0  (1 << 0) // Enable EL0 access
#define ARMV8_PMUSERENR_ER      (1 << 1) // Enable event counter read
#define ARMV8_PMUSERENR_CR      (1 << 2) // Enable cycle counter read

#define ARMV8_PMCR_P            (1 << 1) // Reset event counter
#define ARMV8_PMCR_C            (1 << 2) // Reset cycle counter
#define ARMV8_PMCR_E            (1 << 0) // Enable performance monitoring

#define ARMV8_PMCNTENSET_EL0_ENABLE (1 << 31) // Enable cycle counter

VOID
EnableUserModeCounters (
  VOID
  )
{
    UINT64 Value;

    // Enable user-mode access to counters (PMUSERENR_EL0)
    Value = ARMV8_PMUSERENR_EN_EL0 | ARMV8_PMUSERENR_ER | ARMV8_PMUSERENR_CR;
    __asm__ volatile (
        "msr pmuserenr_el0, %0"
        : /* no output */
        : "r" (Value)
    );

    // Initialize & Reset PMNC (PMCR_EL0): C and P bits
    Value = ARMV8_PMCR_P | ARMV8_PMCR_C;
    __asm__ volatile (
        "msr pmcr_el0, %0"
        : /* no output */
        : "r" (Value)
    );

    // Enable PMINTENSET_EL1 (Performance Monitor Interrupt Enable Set)
    Value = 0 << 31; // No interrupt
    __asm__ volatile (
        "msr pmintenset_el1, %0"
        : /* no output */
        : "r" (Value)
    );

    // Enable the cycle counter in PMCNTENSET_EL0
    Value = ARMV8_PMCNTENSET_EL0_ENABLE;
    __asm__ volatile (
        "msr pmcntenset_el0, %0"
        : /* no output */
        : "r" (Value)
    );

    // Enable performance monitoring (set E bit in PMCR_EL0)
    UINT64 CurrentValue;
    __asm__ volatile (
        "mrs %0, pmcr_el0"    // Read PMCR_EL0
        : "=r" (CurrentValue)
    );

    Value = CurrentValue | ARMV8_PMCR_E;
    __asm__ volatile (
        "msr pmcr_el0, %0"    // Write back modified PMCR_EL0
        : /* no output */
        : "r" (Value)
    );

    DEBUG ((DEBUG_INFO, "User-mode counters enabled.\n"));
}

EFI_STATUS ArchInit(VOID) {
    EnableUserModeCounters();
    return EFI_SUCCESS;
}