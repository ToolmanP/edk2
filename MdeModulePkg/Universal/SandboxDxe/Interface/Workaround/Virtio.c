#include <Interface/Workaround.h>
#include <Protocol/VirtioDevice.h>

// VIRTIO needs shared memory to do it.
// For simplicity now, we map the callee given address to given

STATIC EFI_STATUS VirtioRingCopy(IN DUPLICATE_CTX *Ctx,
                                 IN CONST REFLECT_FUNC_TYPE *Function,
                                 IN CONST REFLECT_PARAM *Param,
                                 IN CONST UINT64 *Src, OUT UINT64 *Dst) {
  return EFI_SUCCESS;
}

CONST PARAM_WORKAROUND VirtioRingWorkaround = {
    .Type = WORKAROUND_PARAM_TYPE,
    .Name = "VRING",
    .Copy = VirtioRingCopy,
};
