# Claude TODO

## Pending

- **Re-enable JIT**: JIT was disabled (`--disable-jit` in rebuild_and_test.sh) during debugging of the cosmo_once deadlock. Now that guest apps are compiled with gcc instead of cosmocc, JIT should be safe to re-enable. Test all apps after re-enabling.

- **Revert loader.c anonymous mmap change**: In `blink/blink/loader.c` around line 207, `ReserveVirtual` was changed to use `fd=-1` with `LoaderCopy` instead of fd-based lazy page mapping. This was a debug experiment that didn't fix the deadlock (the real fix was switching from cosmocc to gcc). Should be reverted to restore lazy page mapping from fd.
