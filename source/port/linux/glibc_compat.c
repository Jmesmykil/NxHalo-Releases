/* Bind two libm calls to their original symbol versions so the client runs on
   glibc older than 2.43 (SteamOS 3.8 ships 2.41). Same versions the earlier
   Crossplay18 Linux client used. */
__asm__(".symver nxhalo_fmod_2_0,fmod@GLIBC_2.0");
__asm__(".symver nxhalo_sqrtf_2_0,sqrtf@GLIBC_2.0");
extern double nxhalo_fmod_2_0(double, double);
extern float nxhalo_sqrtf_2_0(float);
double __wrap_fmod(double a, double b) { return nxhalo_fmod_2_0(a, b); }
float __wrap_sqrtf(float a) { return nxhalo_sqrtf_2_0(a); }
