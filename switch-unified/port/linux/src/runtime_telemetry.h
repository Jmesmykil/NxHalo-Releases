#ifndef HALO_RUNTIME_TELEMETRY_H
#define HALO_RUNTIME_TELEMETRY_H
/* Main/render thread only; bounded rolling samples, never waits for the GPU. */
int halo_telemetry_enabled(void);
double halo_telemetry_time_ms(void);
void halo_telemetry_report_resources(double seconds);
void halo_telemetry_present(double entered_ms, double completed_ms, unsigned long frame);
#endif
