#include "patches.h"

// The game asks at boot whether a 64DD is attached (sys_main.c), by reading the drive's status register through the
// handle osLeoDiskInit makes. The runtime has neither the handle nor the register, and no 64DD, so this answers as a
// console without one does: the game then never offers the 64DD's Expansion Kit.
RECOMP_PATCH bool LeoDD_CheckPresence(void) {
    // @recomp No 64DD.
    return false;
}
