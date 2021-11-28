
// prevent linker error: for missing __sync_synchronize
#ifdef ARDUINO_ARCH_RP2040
extern "C" void __sync_synchronize() {
}
#endif
