/* Switch content-only filesystem adapter. Directory tokens never become libc fds. */
#ifndef HALO_CONTENT_FS_H
#define HALO_CONTENT_FS_H
#ifdef HALO_ANDROID
int content_open(const char *, int, ...);
int content_openat(int, const char *, int, ...);
int content_close(int);
int content_dup(int);
char *content_mkdtemp(char *);
int content_fsync(int);
int content_unlinkat(int,const char *,int);
int content_rmdir(const char *);
int content_renameat(int,const char *,int,const char *);
int content_fstat(int,struct posix_file_information *);
int content_regular(int);
int content_mkdirat(int,const char *);
int content_rename_noreplace(int,const char *,const char *);
void *content_diropenfd(int);
int content_identity_fd(int,struct posix_directory_identity *);
int content_identity_at(int,const char *,struct posix_directory_identity *);
int content_identity_path(const char *,struct posix_directory_identity *);
#define open content_open
#define openat content_openat
#define close content_close
#define dup content_dup
#define mkdtemp content_mkdtemp
#define fsync content_fsync
#define unlinkat content_unlinkat
#define rmdir content_rmdir
#define renameat content_renameat
#define posix_fstat content_fstat
#define posix_file_is_regular content_regular
#define posix_make_private_directory_at content_mkdirat
#define posix_rename_noreplace_at content_rename_noreplace
#define posix_directory_open_fd content_diropenfd
#define posix_directory_identity_fd content_identity_fd
#define posix_directory_identity_at content_identity_at
#define posix_directory_identity_path content_identity_path
#endif
#endif
