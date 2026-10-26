#ifndef HALO_DIRECTORY_FILTER_H
#define HALO_DIRECTORY_FILTER_H
/* Local browser policy only. Does not alter listing/network layouts. */
enum { SERVER_VIEW_ALL, SERVER_VIEW_CAMPAIGN, SERVER_VIEW_MULTIPLAYER,
 SERVER_VIEW_LEGACY, SERVER_VIEW_CURRENT, SERVER_VIEW_ANNOUNCED,
 SERVER_VIEW_BROKER, SERVER_VIEW_CLASSIC_CE, SERVER_VIEW_CLASSIC_PC, SERVER_VIEW_NATIVE_V24 };
static int directory_filter_accepts(int view, int version, int source, int engine)
{
 switch (view) {
 case SERVER_VIEW_ALL: return 1;
 case SERVER_VIEW_CLASSIC_CE: return version == -2;
 case SERVER_VIEW_CLASSIC_PC: return version == -3;
 case SERVER_VIEW_NATIVE_V24: return version == 24;
 default: if (version < 0) return 0; break;
 }
 switch (view) {
 case SERVER_VIEW_CAMPAIGN: return engine == 0;
 case SERVER_VIEW_MULTIPLAYER: return engine != 0;
 case SERVER_VIEW_LEGACY: return version >= 11 && version <= 20;
 case SERVER_VIEW_CURRENT: return version == 21;
 case SERVER_VIEW_ANNOUNCED: return source == 2;
 case SERVER_VIEW_BROKER: return source == 1;
 default: return 0;
 }
}
#endif
