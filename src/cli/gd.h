#pragma once
#include "config.h"
#include "jsmn.h"


GDSSH *gd_init_ssh(void);
int gd_finalize(int);
int gd_stat(const char *path, struct fuse_stat *stbuf);
int gd_fstat(intptr_t fd, struct fuse_stat *stbuf);
int gd_readlink(const char* path, char* buf, size_t size);
int gd_mkdir(const char *path, fuse_mode_t mode);
int gd_unlink(const char *path);
int gd_rmdir(const char * path);
int gd_rename(const char *from, const char *to);
int gd_truncate(const char *path, fuse_off_t size);
int gd_ftruncate(intptr_t fd, fuse_off_t size);
intptr_t gd_open(const char *path, int flags, unsigned int mode);
GDDIR* gd_opendir(const char* path);
int gd_read(intptr_t fd, void *buf, size_t size, fuse_off_t offset);
int gd_write(intptr_t fd, const void *buf, size_t size, fuse_off_t offset);
int gd_statvfs(const char * path, struct fuse_statvfs *stbuf);
int gd_close(intptr_t fd);
void gd_rewinddir(GDDIR* dirp);
struct GDDIRENT * gd_readdir(GDDIR *dirp);
int gd_closedir(GDDIR *dirp);
intptr_t gd_dirfd(GDDIR *dirp);
int gd_check_hlink(const char *path);
int gd_utimens(const char* path, const struct fuse_timespec tv[2], struct fuse_file_info* fi);
int gd_fsync(intptr_t fd);
int gd_flush(intptr_t fd);
//int gd_chmod(const char* path, fuse_mode_t mode);
//int gd_chown(const char* path, fuse_uid_t uid, fuse_gid_t gid);
//int gd_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);
//int gd_getxattr(const char* path, const char* name, char* value, size_t size);
//int gd_listxattr(const char* path, char* namebuf, size_t size);
//int gd_removexattr(const char* path, const char* name);

void gd_log(const char* fmt, ...);
int jsoneq(const char* json, jsmntok_t* tok, const char* s);
int load_json(GDCONFIG* conf);

// Send message to UsageUrl in config.json
// The http post also includes user and host 
HANDLE* gd_usage(const char* message);

DWORD WINAPI _post_background(LPVOID data);
int _post(const char* url, const char* data);
int get_ssh_error(GDSSH* ssh);
int map_error(int rc);
void mode_human(unsigned long mode, char* human);
void get_filetype(unsigned long perm, char* filetype);
int run_command(const char* cmd, char* out, char* err);
int run_command_channel_exec(const char* cmd, char* out, char* err);
int waitsocket(GDSSH* sanssh);

void libssh2_logger(LIBSSH2_SESSION* session, void* context,
	const char* data, size_t length);
void copy_attributes(struct fuse_stat* stbuf, LIBSSH2_SFTP_ATTRIBUTES* attrs);
//void print_permissions(const char* path, LIBSSH2_SFTP_ATTRIBUTES* attrs);
//void print_stat(const char* path, LIBSSH2_SFTP_ATTRIBUTES* attrs);
//void print_statvfs(const char* path, LIBSSH2_SFTP_STATVFS* st);

// message queue
typedef struct GDQUEUE {
	int front, rear, size;
	unsigned capacity;
	char** data;
} GDQUEUE;

GDQUEUE* gd_create_queue(unsigned capacity);
int gd_queue_is_full(GDQUEUE* queue);
int gd_queue_is_empty(GDQUEUE* queue);
void gd_enqueue(GDQUEUE* queue, char* item);
char* gd_dequeue(GDQUEUE* queue);
