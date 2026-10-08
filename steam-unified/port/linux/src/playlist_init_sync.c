/* Serialize the game's default playlist first-use check across worker/UI.
 * No process-lifetime once flag: the game's own lifecycle flag remains the
 * authority when saved-game state is reinitialized. */
#include <pthread.h>
static pthread_mutex_t playlist_defaults_mutex = PTHREAD_MUTEX_INITIALIZER;
void platform_playlist_defaults_lock(void)
{
    pthread_mutex_lock(&playlist_defaults_mutex);
}
void platform_playlist_defaults_unlock(void)
{
    pthread_mutex_unlock(&playlist_defaults_mutex);
}
