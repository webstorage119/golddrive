#include <assert.h>
#include "global.h"
#include "util.h"
#include "gd.h"

//#if defined(FSP_FUSE_USE_STAT_EX)
//static inline uint32_t MapFileAttributesToFlags(UINT32 FileAttributes)
//{
//	uint32_t flags = 0;
//
//	if (FileAttributes & FILE_ATTRIBUTE_READONLY)
//		flags |= FSP_FUSE_UF_READONLY;
//	if (FileAttributes & FILE_ATTRIBUTE_HIDDEN)
//		flags |= FSP_FUSE_UF_HIDDEN;
//	if (FileAttributes & FILE_ATTRIBUTE_SYSTEM)
//		flags |= FSP_FUSE_UF_SYSTEM;
//	if (FileAttributes & FILE_ATTRIBUTE_ARCHIVE)
//		flags |= FSP_FUSE_UF_ARCHIVE;
//
//	return flags;
//}
//
//static inline UINT32 MapFlagsToFileAttributes(uint32_t flags)
//{
//	UINT32 FileAttributes = 0;
//
//	if (flags & FSP_FUSE_UF_READONLY)
//		FileAttributes |= FILE_ATTRIBUTE_READONLY;
//	if (flags & FSP_FUSE_UF_HIDDEN)
//		FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
//	if (flags & FSP_FUSE_UF_SYSTEM)
//		FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
//	if (flags & FSP_FUSE_UF_ARCHIVE)
//		FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
//
//	return FileAttributes;
//}
//#endif

gdssh_t * gd_init_ssh(const char* hostname, int port, const char* username, const char* pkey)
{
	int rc;
	char *errmsg = 0;
	int errlen;
	SOCKADDR_IN sin;
	HOSTENT *he;
	SOCKET sock;
	LIBSSH2_SESSION* ssh = NULL;
	//LIBSSH2_CHANNEL *channel;
	LIBSSH2_SFTP* sftp = NULL;
	int thread = GetCurrentThreadId();

	// initialize windows socket
	WSADATA wsadata;
	rc = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if (rc != 0) {
		gd_log("%zd: %d :ERROR: %s: %d: WSAStartup failed, rc=%d\n",
			time_mu(), thread, __func__, __LINE__, rc);
		return 0;
	}

	// resolve hostname	
	he = gethostbyname(hostname);
	if (!he) {
		gd_log("%zd: %d :ERROR: %s: %d: host not found: %s\n",
			time_mu(), thread, __func__, __LINE__, hostname);
		return 0;
	}
	sin.sin_addr.s_addr = **(int**)he->h_addr_list;
	sin.sin_family = AF_INET;
	sin.sin_port = htons((u_short)port);

	// init ssh
	rc = libssh2_init(0);
	if (rc) {
		gd_log("%zd: %d :ERROR: %s: %d: "
			"failed to initialize crypto library, rc=%d\n",
			time_mu(), thread, __func__, __LINE__, rc);
		return 0;
	}

	/* create socket  */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	rc = connect(sock, (SOCKADDR*)(&sin), sizeof(SOCKADDR_IN));
	if (rc) {
		gd_log("%zd: %d :ERROR: %s: %d: "
			"failed to open socket, connect() rc=%d\n",
			time_mu(), thread, __func__, __LINE__, rc);
		return 0;
	}

	/* Create a session instance */
	ssh = libssh2_session_init();
	if (!ssh) {
		gd_log("%zd: %d :ERROR: %s: %d: "
			"failed allocate memory for ssh session\n",
			time_mu(), thread, __func__, __LINE__);
		return 0;
	}

	/* non-blocking */
	libssh2_session_set_blocking(ssh, g_fs.block);
	/* blocking */
	//libssh2_session_set_blocking(ssh, 1);

	/* ... start it up. This will trade welcome banners, exchange keys,
	* and setup crypto, compression, and MAC layers	*/
	while ((rc = libssh2_session_handshake(ssh, sock)) ==
		LIBSSH2_ERROR_EAGAIN);
	
	
	if (rc) {
		rc = libssh2_session_last_error(ssh, &errmsg, &errlen, 0);
		gd_log("%zd: %d :ERROR: %s: %d: "
			"failed to complete ssh handshake [rc=%d, %s]\n",
			time_mu(), thread, __func__, __LINE__, rc, errmsg);
		return 0;
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */
	//char* fingerprint = libssh2_hostkey_hash(ssh, LIBSSH2_HOSTKEY_HASH_SHA1);
	//gd_log("Fingerprint: ");
	//for (int i = 0; i < 20; i++) {
	//	gd_log("%02X ", (unsigned char)fingerprint[i]);
	//}
	//gd_log("\n");

	///* check what authentication methods are available */
	//char* userauthlist = libssh2_userauth_list(ssh, username, strlen(username));
	//gd_log("Authentication methods: %s\n", userauthlist);

	//if (strstr(userauthlist, "publickey") == NULL) {
	//	gd_log("Publick key authentication not available in server.\n");
	//	return 0;
	//}


	// authenticate
	while ((rc = libssh2_userauth_publickey_fromfile(
			ssh, username, NULL, pkey, NULL)) ==
			LIBSSH2_ERROR_EAGAIN);
	if (rc) {
		rc = libssh2_session_last_error(ssh, &errmsg, &errlen, 0);
		gd_log("%zd: %d :ERROR: %s: %d: "
			"authentication by public key failed [rc=%d, %s]\n",
			time_mu(), thread, __func__, __LINE__, rc, errmsg);
		return 0;
	}

	// init sftp channel
	do {
		sftp = libssh2_sftp_init(ssh);
		if ((!sftp) && (libssh2_session_last_errno(ssh) !=
			LIBSSH2_ERROR_EAGAIN))
		{
			gd_log("%zd: %d :ERROR: %s: %d: "
				"failed to start sftp session [rc=%d, %s]\n",
				time_mu(), thread, __func__, __LINE__, rc, errmsg);
			return 0;
		}
	} while (!sftp);
	

	/* default mode is blocking */
	//libssh2_session_set_blocking(session, 1);
	g_ssh = malloc(sizeof(gdssh_t));
	g_ssh->socket = sock;
	g_ssh->ssh = ssh;
	g_ssh->sftp = sftp;
	g_ssh->thread = GetCurrentThreadId();
	//if (!InitializeCriticalSectionAndSpinCount(&sanssh->lock, 0x00000400))
	//	return 0;
	return g_ssh;
}

int gd_finalize(void)
{
	log_info("FINALIZE\n");

	while (libssh2_sftp_shutdown(g_ssh->sftp) ==
		LIBSSH2_ERROR_EAGAIN);
	while (libssh2_session_disconnect(g_ssh->ssh, "ssh session disconnected") ==
		LIBSSH2_ERROR_EAGAIN);
	while (libssh2_session_free(g_ssh->ssh) ==
		LIBSSH2_ERROR_EAGAIN);

	libssh2_exit();
	closesocket(g_ssh->socket);
	free(g_ssh);


	return 0;
}

int gd_stat(const char * path, struct fuse_stat *stbuf)
{
	log_info("%s\n", path);
	int rc = 0;
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	assert(g_ssh);
	assert(g_ssh->sftp);
	memset(stbuf, 0, sizeof * stbuf);

	gd_lock();
	while ((rc = libssh2_sftp_stat_ex(g_ssh->sftp, path, (int)strlen(path), LIBSSH2_SFTP_LSTAT, &attrs)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	log_debug("rc=%d, %s\n", rc, path);
	if (rc < 0) {
		gd_error(path);
		rc = error();
	}
	copy_attributes(stbuf, &attrs);
	
	return rc;
	
}

int gd_fstat(intptr_t fd, struct fuse_stat* stbuf)
{

	int rc = 0;
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	assert(handle);

	LIBSSH2_SFTP_ATTRIBUTES attrs;
	memset(stbuf, 0, sizeof * stbuf);

	gd_lock();
	while ((rc = libssh2_sftp_fstat_ex(handle, &attrs, 0)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	log_debug("rc=%d, %s\n", rc, sh->path);
	if (rc < 0) {
		gd_error(sh->path);
		rc = error();
	}

	copy_attributes(stbuf, &attrs);
	log_info("%lld, size=%lld\n", (size_t)handle, attrs.filesize);
	return rc;
}

int gd_readlink(const char* path, char* buf, size_t size)
{
	log_info("%s, size=%lld\n", path, size);
	log_debug("%s, size=%lld, buf=%s\n", path, size, buf);
	int rc;
	assert(size > 0);

	char* target = malloc(MAX_PATH);
	// rc is number of bytes in target
	gd_lock();
	while ((rc = libssh2_sftp_symlink_ex(g_ssh->sftp, path, (int)strlen(path),
		target, MAX_PATH, LIBSSH2_SFTP_READLINK)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();
	
	log_debug("rc=%d, %s\n", rc, path);
	if (rc < 0) {
		free(target);
		if (strcmp(path, g_fs.root) != 0)
			gd_error(path);
		return error();
	}
	assert(rc < size);

	//transform_symlink(path, &target);

	strncpy(buf, target, rc);
	buf[rc] = '\0';
	free(target);
	return 0;
}

int gd_mkdir(const char* path, fuse_mode_t mode)
{
	int rc;
	log_info("%s, mode=%u\n", path, mode);

	gd_lock();
	while ((rc = libssh2_sftp_mkdir_ex(g_ssh->sftp, path, (int)strlen(path), mode)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}

	gd_unlock();

	if (rc < 0) {
		gd_error(path);
		rc = error();
	}
	return rc;
}

int gd_unlink(const char *path)
{
	int rc;
	log_info("%s\n", path);

	gd_lock();
	if (g_fs.block) {
		rc = libssh2_sftp_unlink_ex(g_ssh->sftp, path, (int)strlen(path));
		g_sftp_calls++;
	}
	else {
		while ((rc = libssh2_sftp_unlink_ex(g_ssh->sftp, path, (int)strlen(path))) ==
			LIBSSH2_ERROR_EAGAIN) {
			waitsocket(g_ssh);
			g_sftp_calls++;
		}
	}
	gd_unlock();

	if (rc) {
		gd_error(path);
		rc = error();
	}
	return rc;
}

int gd_rmdir(const char* path)
{
	// rm path/* and rmdir path
	int rc;
	log_info("%s\n", path);

	gd_lock();
	while ((rc = libssh2_sftp_rmdir_ex(g_ssh->sftp, path, (int)strlen(path))) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc < 0) {
		gd_error(path);
		rc = error();
	}
	return rc;
}

static int _gd_rename(const char *from, const char *to)
{
	int rc;
	gd_lock();
	while ((rc = libssh2_sftp_rename_ex(g_ssh->sftp,
		from, (int)strlen(from), to, (int)strlen(to),
		LIBSSH2_SFTP_RENAME_OVERWRITE)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc < 0) {
		gd_error(from);
		rc = error();
	}
	
	return rc;
}

int gd_rename(const char *from, const char *to)
{
	int rc;
	log_info("%s -> %s\n", from, to);
	size_t tolen = strlen(to);
	rc = _gd_rename(from, to);

	if (rc) {

		if (tolen + 8 < PATH_MAX) {
			char totmp[PATH_MAX];
			strcpy(totmp, to);
			gd_random_string(totmp + tolen, 8);
			rc = _gd_rename(to, totmp);

			if (!rc) {
				rc = _gd_rename(from, to);
				if (!rc)
					rc = gd_unlink(totmp);
				else
					_gd_rename(totmp, to);
			}
			if (rc) {
				gd_error(from);
				gd_error(to);
			}
		}
	}
	return rc ? -1 : 0;

}


int gd_truncate(const char* path, fuse_off_t size)
{

	int rc;
	log_info("%s\n", path);
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	attrs.flags = LIBSSH2_SFTP_ATTR_SIZE;
	attrs.filesize = size;

	gd_lock();
	while ((rc = libssh2_sftp_stat_ex(g_ssh->sftp, path, (int)strlen(path), 
		LIBSSH2_SFTP_SETSTAT, &attrs)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc) {
		gd_error(path);
		rc = error();
	}

	return rc;
}

int gd_ftruncate(intptr_t fd, fuse_off_t size)
{
	int rc = 0;
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	assert(handle);
	log_info("%lld, %s, size=%lld\n", (size_t)handle, sh->path, size);
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	attrs.flags = LIBSSH2_SFTP_ATTR_SIZE;
	attrs.filesize = size;

	gd_lock();
	while ((rc = libssh2_sftp_fstat_ex(handle, &attrs, LIBSSH2_SFTP_SETSTAT)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc) {
		gd_error(sh->path);
		rc = error();
	}

	return rc;
}

intptr_t gd_open(const char* path, int flags, unsigned int mode)
{
	int rc;
	LIBSSH2_SFTP_HANDLE* handle = 0;
	gd_handle_t* sh = malloc(sizeof(gd_handle_t));
	log_info("%s, flags=%d, mode=%u\n", path, flags, mode);

	unsigned int pflags;
	if ((flags & O_ACCMODE) == O_RDONLY) {
		pflags = LIBSSH2_FXF_READ;
	}
	else if ((flags & O_ACCMODE) == O_WRONLY) {
		pflags = LIBSSH2_FXF_WRITE;
	}
	else if ((flags & O_ACCMODE) == O_RDWR) {
		pflags = LIBSSH2_FXF_READ | LIBSSH2_FXF_WRITE;
	}
	else {
		return -EINVAL;;
	}

	if (flags & O_CREAT)
		pflags |= LIBSSH2_FXF_CREAT;

	if (flags & O_EXCL)
		pflags |= LIBSSH2_FXF_EXCL;

	if (flags & O_TRUNC)
		pflags |= LIBSSH2_FXF_TRUNC;

	if (flags & O_APPEND)
		pflags |= LIBSSH2_FXF_APPEND;

	//error("%s, pflags: %d\n", path, pflags);
	sh->flags = pflags;
	strcpy_s(sh->path, MAX_PATH, path);

	// sh->mode = 0777 | ((LIBSSH2_SFTP_S_ISDIR(mode)) ? S_IFDIR : 0);
	//sh->mode = mode & 0777;
	sh->mode = mode;
	sh->dir = 0;

	if (sh->flags == LIBSSH2_FXF_WRITE || sh->flags == (LIBSSH2_FXF_READ | LIBSSH2_FXF_WRITE)) {
		//gd_check_hlink(path);
	}
	gd_lock();
	do {
		handle = libssh2_sftp_open_ex(g_ssh->sftp, sh->path, (int)strlen(sh->path),
			sh->flags, sh->mode, LIBSSH2_SFTP_OPENFILE);
		g_sftp_calls++;
		if (!handle && libssh2_session_last_errno(g_ssh->ssh) !=
			LIBSSH2_ERROR_EAGAIN)
			break;
	} while (!handle);
	gd_unlock();

	if (!handle) {
		gd_error(sh->path);
		return error();
	}

	log_info("OPEN HANDLE : %zu:%zu: %s, flags=%d, mode=%d\n",
		(size_t)sh, (size_t)handle, sh->path, sh->flags, sh->mode);

	sh->handle = handle;
	return (intptr_t)sh;
}

int gd_read(intptr_t fd, void* buf, size_t size, fuse_off_t offset)
{
	int rc = 0;
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	size_t curpos;

	log_info("READING HANDLE: %zu size=%lld, offset=%lld\n",
		(size_t)handle, size, offset);

	gd_lock();
	curpos = libssh2_sftp_tell64(handle);
	if (offset != curpos)
		libssh2_sftp_seek64(handle, offset);
	gd_unlock();

	ssize_t total = 0;
	size_t chunk = size;
	char* pos = buf;

	do {
		gd_lock();
		while ((rc = (int)libssh2_sftp_read(handle, pos, chunk)) ==
			LIBSSH2_ERROR_EAGAIN) {
			waitsocket(g_ssh);
			g_sftp_calls++;
		}
		gd_unlock();

		if (rc > 0) {
			pos += rc;
			total += rc;
			chunk -= rc;
		}
		else {
			break;
		}
	} while (chunk);

	if (rc < 0) {
		gd_error("ERROR: Unable to read chuck of file\n");
		rc = error();
		total = -1;
	}
	if (rc == 0 && size != total)
	{
		log_warn("WARNING !!!!!!!!!!! need=%lld, actual=%lld, probably EOF\n",
			size, total);
	}
	log_debug("FINISH READING HANDLE %zu, bytes: %zu\n", (size_t)handle, total);

	return total >= 0 ? (int)total : rc;
}

int gd_write(intptr_t fd, const void* buf, size_t size, fuse_off_t offset)
{
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	size_t curpos;
	int rc;

	log_info("WRITING HANDLE: %zu size: %zu\n", (size_t)handle, size);

	gd_lock();
	curpos = libssh2_sftp_tell64(handle);
	if (offset != curpos)
		libssh2_sftp_seek64(handle, offset);
	gd_unlock();

	int total = 0;
	size_t chunk = size;
	const char* pos = buf;

	do {
		gd_lock();
		while ((rc = (int)libssh2_sftp_write(handle, pos, chunk)) ==
			LIBSSH2_ERROR_EAGAIN) {
			waitsocket(g_ssh);
			g_sftp_calls++;
		}
		gd_unlock();

		if (rc > 0) {
			pos += rc;
			total += rc;
			chunk -= rc;
		}
		else {
			break;
		}
	} while (chunk);

	if (rc == 0 && size != total)
	{
		log_warn("WARNING !!!!!!!!!!! need=%lld, actual=%d, probably EOF\n", size, total);

	}

	if (rc < 0) {
		gd_error("ERROR: Unable to write chuck of data\n");
		rc = error();
		total = -1;
	}
	log_debug("FINISH WRITING %zu, bytes: %zu\n", (size_t)handle, total);
	return total;// >= 0 ? (int)total : rc;
}

int gd_statvfs(const char* path, struct fuse_statvfs* stbuf)
{
	log_info("%s\n", path);
	int rc = 0;
	LIBSSH2_SFTP_STATVFS stvfs;

	gd_lock();
	while ((rc = libssh2_sftp_statvfs(g_ssh->sftp, path, strlen(path), &stvfs)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();


	if (rc < 0) {
		gd_error(path);
		rc = error();
	}

	memset(stbuf, 0, sizeof(struct fuse_statvfs));
	stbuf->f_bsize = stvfs.f_bsize;			/* file system block size */
	stbuf->f_frsize = stvfs.f_frsize;		/* fragment size */
	stbuf->f_blocks = stvfs.f_blocks;		/* size of fs in f_frsize units */
	stbuf->f_bfree = stvfs.f_bfree;			/* # free blocks */
	stbuf->f_bavail = stvfs.f_bavail;		/* # free blocks for non-root */
	stbuf->f_files = stvfs.f_files;			/* # inodes */
	stbuf->f_ffree = stvfs.f_ffree;			/* # free inodes */
	stbuf->f_favail = stvfs.f_favail;		/* # free inodes for non-root */
	//stbuf->f_fsid = stvfs.f_fsid;			/* file system ID */
	//stbuf->f_flag = stvfs.f_flag;			/* mount flags */
	stbuf->f_namemax = stvfs.f_namemax;		/* maximum filename length */


	// mac
	//stbuf->f_namemax = 255;
	//stbuf->f_bsize = stvfs.f_bsize;
	///*
	// * df seems to use f_bsize instead of f_frsize, so make them
	// * the same
	// */
	//stbuf->f_frsize = stbuf->f_bsize;
	//stbuf->f_blocks = stbuf->f_bfree = stbuf->f_bavail =
	//	1000ULL * 1024 * 1024 * 1024 / stbuf->f_frsize;
	//stbuf->f_files = stbuf->f_ffree = 1000000000;
	//return 0;

	return rc;
}

int gd_close(intptr_t fd)
{
	int rc;
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle;
	handle = sh->handle;
	assert(handle);

	gd_lock();
	while ((rc = libssh2_sftp_close_handle(handle)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc < 0) {
		gd_error(sh->path);
		rc = error();
	}

	log_info("CLOSE HANDLE: %zu:%zu\n", (size_t)sh, (size_t)handle);
	free(sh);
	sh = NULL;
	return rc;
}

gd_dir_t* gd_opendir(const char* path)
{
	log_info("%s\n", path);
	int rc = 0;
	LIBSSH2_SFTP_HANDLE* handle;
	gd_dir_t* dirp;
	gd_handle_t* sh = malloc(sizeof(gd_handle_t));
	unsigned int mode = 0;
	log_debug("OPEN gd_handle_t: %zu, %s\n", (size_t)sh, path);

	gd_lock();
	do {
		handle = libssh2_sftp_open_ex(g_ssh->sftp, path, (int)strlen(path), 0, 0, LIBSSH2_SFTP_OPENDIR);
		g_sftp_calls++;
		if (!handle && libssh2_session_last_errno(g_ssh->ssh) !=
			LIBSSH2_ERROR_EAGAIN)
			break;
	} while (!handle);
	gd_unlock();

	if (!handle) {
		gd_error(path);
		rc = error();
		return 0;
	}

	size_t pathlen = strlen(path);
	if (0 < pathlen && '/' == path[pathlen - 1])
		pathlen--;

	dirp = malloc(sizeof * dirp + pathlen + 2); /* sets errno */
	if (0 == dirp) {
		// fixme:
		// close handle
		return 0;
	}

	strcpy_s(sh->path, MAX_PATH, path);
	sh->handle = handle;
	sh->dir = 1;
	memset(dirp, 0, sizeof * dirp);
	dirp->handle = sh;
	memcpy(dirp->path, path, pathlen);
	dirp->path[pathlen + 0] = '/';
	dirp->path[pathlen + 1] = '\0';

	return dirp;
}

void gd_rewinddir(gd_dir_t *dirp)
{
	gd_handle_t *sh = dirp->handle;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	gd_lock();
	libssh2_sftp_seek64(handle, 0);
	g_sftp_calls++;
	gd_unlock();
}

struct gd_dirent_t* gd_readdir(gd_dir_t* dirp)
{
	int rc;
	gd_handle_t* sh = dirp->handle;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	char fname[FILENAME_MAX];
	memset(&attrs, 0, sizeof attrs);

	gd_lock();
	while ((rc = libssh2_sftp_readdir(handle, fname, FILENAME_MAX, &attrs)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();


	if (rc < 0) {
		gd_error(dirp->path);
		rc = error0();
		return 0;
	}
	if (rc == 0) {
		// no more files
		return 0;
	}


	strcpy_s(dirp->de.d_name, FILENAME_MAX, fname);
	dirp->de.dir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
	copy_attributes(&dirp->de.d_stat, &attrs);
	return &dirp->de;
}

int gd_closedir(gd_dir_t* dirp)
{
	int rc = 0;
	gd_handle_t* dirfh = dirp->handle;
	LIBSSH2_SFTP_HANDLE* handle = dirfh->handle;

	gd_lock();
	while ((rc = libssh2_sftp_close_handle(handle)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc < 0) {
		gd_error(dirfh->path);
		rc = error();
	}

	log_info("CLOSE HANDLE: %zu:%zu\n", (size_t)dirfh, (size_t)handle);
	free(dirfh);
	dirfh = NULL;
	free(dirp);
	dirp = NULL;
	return rc;
}

intptr_t gd_dirfd(gd_dir_t *dirp)
{
	return (intptr_t)dirp->handle;
}

int gd_check_hlink(const char* path)
{
	// check for hard link
	int rc = 0;
	char cmd[MAX_PATH + 10] = { 0 }, out[100] = { 0 }, err[100] = { 0 };
	sprintf_s(cmd, sizeof cmd, "stat -c%%h \"%s\"\n", path);
	run_command(cmd, out, err);
	int hlinks = atoi(out);
	if (hlinks > 1) {
		//error("opening for writing hard linked file: %s\n"
		//	"number of links: %d\n", path, hlinks);

		// backup file
		char backup[PATH_MAX];
		sprintf_s(backup, sizeof backup, "%s.%zd.hlink", path, time_mu());
		rc = gd_rename(path, backup);

		if (!rc) {
			gd_lock();
			LIBSSH2_SFTP_HANDLE* handle;
			// fixme: AND mode with -o create_umask arg
			unsigned int mode = 432; // oct 660
			unsigned flags = LIBSSH2_FXF_READ | LIBSSH2_FXF_WRITE
				| LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL; // int 43

			do {
				handle = libssh2_sftp_open_ex(g_ssh->sftp, path, (int)strlen(path),
					flags, mode, LIBSSH2_SFTP_OPENFILE);
				g_sftp_calls++;
				if (!handle && libssh2_session_last_errno(g_ssh->ssh) !=
					LIBSSH2_ERROR_EAGAIN)
					break;

			} while (!handle);
			while ((rc = libssh2_sftp_close_handle(handle)) ==
				LIBSSH2_ERROR_EAGAIN) {
				waitsocket(g_ssh);
				g_sftp_calls++;
			}
			gd_unlock();
		}
		else {
			gd_error(path);
			rc = error();
		}
	}
	return rc;
}

int gd_utimens(const char* path, const struct fuse_timespec tv[2], struct fuse_file_info* fi)
{
	log_info("%s\n", path);
	int rc = 0;

	UINT64 LastAccessTime, LastWriteTime;
	if (0 == tv)
	{
		FILETIME FileTime;
		GetSystemTimeAsFileTime(&FileTime);
		LastAccessTime = LastWriteTime = *(PUINT64)&FileTime;
	}
	else
	{
		FspPosixUnixTimeToFileTime((void*)&tv[0], &LastAccessTime);
		FspPosixUnixTimeToFileTime((void*)&tv[1], &LastWriteTime);
	}
	LIBSSH2_SFTP_ATTRIBUTES attrs;
	attrs.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
	attrs.atime = (unsigned long)LastAccessTime;
	attrs.mtime = (unsigned long)LastWriteTime;
	gd_lock();
	while ((rc = libssh2_sftp_stat_ex(g_ssh->sftp, path, (int)strlen(path),
		LIBSSH2_SFTP_SETSTAT, &attrs)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();

	if (rc < 0) {
		gd_error(path);
		rc = error();
	}

	return rc;
}

int gd_flush(intptr_t fd)
{
	return gd_fsync(fd);

	/*gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	assert(handle);
	log_info("%s\n", sh->path);
	return 0;*/
}
int gd_fsync(intptr_t fd)
{
	int rc;
	gd_handle_t* sh = (gd_handle_t*)fd;
	LIBSSH2_SFTP_HANDLE* handle = sh->handle;
	assert(handle);
	log_info("%s\n", sh->path);
	// flush file ?
	gd_lock();
	while ((rc = libssh2_sftp_fsync(handle)) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(g_ssh);
		g_sftp_calls++;
	}
	gd_unlock();
	if (rc < 0) {
		gd_error(sh->path);
		rc = error();
	}
	return rc;
}

int gd_chmod(const char* path, fuse_mode_t mode)
{
	log_info("%s, mode=%u\n", path, mode);
	/* we do not support file security */
	return 0;
}

int gd_chown(const char* path, fuse_uid_t uid, fuse_gid_t gid)
{
	log_info("%s, uid=%d, gid=%d\n", path, uid, gid);
	/* we do not support file security */
	return 0;
}

int gd_setxattr(const char* path, const char* name, const char* value, size_t size, int flags)
{
	return 0;
}

int gd_getxattr(const char* path, const char* name, char* value, size_t size)
{
	return 0;
}

int gd_listxattr(const char* path, char* namebuf, size_t size)
{
	return 0;
}

int gd_removexattr(const char* path, const char* name)
{
	return 0;
}


void copy_attributes(struct fuse_stat *stbuf, LIBSSH2_SFTP_ATTRIBUTES* attrs)
{

	
	//int isdir = LIBSSH2_SFTP_S_ISDIR(attrs->permissions);
	//memset(stbuf, 0, sizeof *stbuf);
	stbuf->st_uid = attrs->uid;
	stbuf->st_gid = attrs->gid;
	//stbuf->st_mode = attrs->permissions;
	stbuf->st_mode = 0777 | ((LIBSSH2_SFTP_S_ISDIR(attrs->permissions)) ? S_IFDIR : 0);
	stbuf->st_size = attrs->filesize;
	stbuf->st_birthtim.tv_sec = attrs->mtime;
	stbuf->st_atim.tv_sec = attrs->atime;
	stbuf->st_mtim.tv_sec = attrs->mtime;
	stbuf->st_ctim.tv_sec = attrs->mtime;
	stbuf->st_nlink = 1;
	/*if (LIBSSH2_SFTP_S_ISLNK(attrs->permissions)) {
		int a = attrs->permissions;
	}*/
#if defined(FSP_FUSE_USE_STAT_EX)
//	if (hidden) {
//		stbuf->st_flags |= FSP_FUSE_UF_READONLY;
//	}
#endif
}


int run_command(const char *cmd, char *out, char *err)
{
	LIBSSH2_CHANNEL *channel;
	int rc;
	ssize_t bytesread = 0;
	size_t offset = 0;
	char *errmsg;

	gd_lock();
	do {
			channel = libssh2_channel_open_session(g_ssh->ssh);
			g_sftp_calls++;
			if (!channel && libssh2_session_last_errno(g_ssh->ssh) !=
				LIBSSH2_ERROR_EAGAIN)
				break;
		} while (!channel);
	gd_unlock();

	if (!channel) {
		rc = libssh2_session_last_error(g_ssh->ssh, &errmsg, NULL, 0);
		log_debug("ERROR: unable to init ssh chanel, rc=%d, %s\n", rc, errmsg);
		return 1;
	}

	gd_lock();
	/*libssh2_channel_set_blocking(channel, g_fs.block);*/
	//g_sftp_calls++;

	while ((rc = libssh2_channel_exec(channel, cmd)) == LIBSSH2_ERROR_EAGAIN) {
			waitsocket(g_ssh);
			g_sftp_calls++;
		}
	gd_unlock();

	if (rc != 0) {
		rc = libssh2_session_last_error(g_ssh->ssh, &errmsg, NULL, 0);
		log_debug("ERROR: unable to execute command, rc=%d, %s\n", rc, errmsg);
		goto finish;
	}

	/* read stdout */
	gd_lock();
	out[0] = '\0';
	for (;;) {
		do {
			char buffer[0x4000];
			
			bytesread = libssh2_channel_read(channel, buffer, sizeof(buffer));
			
			if (bytesread > 0) {
				//strncat(out, buffer, bytesread);
				memcpy(out + offset, buffer, bytesread);
				offset += bytesread;
			}
		} while (bytesread > 0);

		if (bytesread == LIBSSH2_ERROR_EAGAIN)
			waitsocket(g_ssh);
		else
			break;
	}
	/* read stderr */
	err[0] = '\0';
	offset = 0;
	for (;;) {
		do {
			char buffer[0x4000];
			
			bytesread = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));
			
			if (bytesread > 0) {
				//strncat(err, buffer, bytesread);
				memcpy(out + offset, buffer, bytesread);
				offset += bytesread;
			}
		} while (bytesread > 0);

		if (bytesread == LIBSSH2_ERROR_EAGAIN)
			waitsocket(g_ssh);
		else
			break;
	}

	/* get exit code */
	while ((rc = libssh2_channel_close(channel)) == 
		LIBSSH2_ERROR_EAGAIN)
		waitsocket(g_ssh);

	//rc = libssh2_channel_close(channel);
	if (rc == 0)
		rc = libssh2_channel_get_exit_status(channel);
	else
		rc = 127;

finish:
	while((rc = libssh2_channel_free(channel))==
		LIBSSH2_ERROR_EAGAIN)
		waitsocket(g_ssh);
	gd_unlock();
	return (int)rc;
}

int gd_threads(int n, int c)
{
	// guess number of threads in this app
	// n: ThreadCount arg
	// c: number of cores
	// w: winfsp threads = n < 1 ? c : max(2, n) + 1
	// t: total = w + c + main thread
	return (n < 1 ? c : max(2, n)) + c + 2;
}

void get_filetype(unsigned long mode, char* filetype)
{
	if (LIBSSH2_SFTP_S_ISREG(mode))			strcpy_s(filetype, 4, "REG");
	else if (LIBSSH2_SFTP_S_ISDIR(mode))	strcpy_s(filetype, 4, "DIR");
	else if (LIBSSH2_SFTP_S_ISLNK(mode))	strcpy_s(filetype, 4, "LNK");
	else if (LIBSSH2_SFTP_S_ISCHR(mode))	strcpy_s(filetype, 4, "CHR");
	else if (LIBSSH2_SFTP_S_ISBLK(mode))	strcpy_s(filetype, 4, "BLK");
	else if (LIBSSH2_SFTP_S_ISFIFO(mode))	strcpy_s(filetype, 4, "FIF");
	else if (LIBSSH2_SFTP_S_ISSOCK(mode))	strcpy_s(filetype, 4, "SOC");
	else									strcpy_s(filetype, 4, "NAN");
}

void mode_human(unsigned long mode, char* human)
{
	human[0] = mode & LIBSSH2_SFTP_S_IRUSR ? 'r' : '-';
	human[1] = mode & LIBSSH2_SFTP_S_IWUSR ? 'w' : '-';
	human[2] = mode & LIBSSH2_SFTP_S_IXUSR ? 'x' : '-';
	human[3] = mode & LIBSSH2_SFTP_S_IRGRP ? 'r' : '-';
	human[4] = mode & LIBSSH2_SFTP_S_IWGRP ? 'w' : '-';
	human[5] = mode & LIBSSH2_SFTP_S_IXGRP ? 'x' : '-';
	human[6] = mode & LIBSSH2_SFTP_S_IROTH ? 'r' : '-';
	human[7] = mode & LIBSSH2_SFTP_S_IWOTH ? 'w' : '-';
	human[8] = mode & LIBSSH2_SFTP_S_IXOTH ? 'x' : '-';
	human[9] = '\0';
}



void print_permissions(const char* path, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
	char perm[10];
	char ftype[4];
	mode_human(attrs->permissions, perm);
	get_filetype(attrs->permissions, ftype);
	printf("%s %s %d %d %s\n", perm, ftype, attrs->uid, attrs->gid, path);
}

void print_stat(const char* path, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
	printf("path:  %s\n", path);
	printf("flags: %ld\n", attrs->flags);
	printf("size:  %zd\n", attrs->filesize);
	printf("uid:   %ld\n", attrs->uid);
	printf("gid:   %ld\n", attrs->gid);
	printf("mode:  %ld\n", attrs->permissions);
	printf("atime: %ld\n", attrs->atime);
	printf("mtime: %ld\n", attrs->mtime);
}

void print_statvfs(const char* path, LIBSSH2_SFTP_STATVFS *st)
{
	printf("path:    %s\n", path);
	printf("bsize:   %zd\n", st->f_bsize);    	/* file system block size */
	printf("frsize:  %zd\n", st->f_frsize);   	/* fragment size */
	printf("blocks:  %zd\n", st->f_blocks);   	/* size of fs in f_frsize units */
	printf("bfree:   %zd\n", st->f_bfree);    	/* # free blocks */
	printf("bavail:  %zd\n", st->f_bavail);   	/* # free blocks for non-root */
	printf("files:   %zd\n", st->f_files);    	/* # inodes */
	printf("ffree:   %zd\n", st->f_ffree);    	/* # free inodes */
	printf("favail:  %zd\n", st->f_favail);   	/* # free inodes for non-root */
	printf("fsid:    %zd\n", st->f_fsid);     	/* file system ID */
	printf("flag:    %zd\n", st->f_flag);     	/* mount flags */
	printf("namemax: %zd\n", st->f_namemax);  	/* maximum filename length */

}

int waitsocket(gdssh_t* ssh)
{
	struct timeval timeout;
	int rc;
	fd_set fd;
	fd_set *writefd = NULL;
	fd_set *readfd = NULL;
	int dir;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	FD_ZERO(&fd);
	FD_SET(ssh->socket, &fd);
	/* now make sure we wait in the correct direction */
	dir = libssh2_session_block_directions(ssh->ssh);
	if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
		readfd = &fd;
	if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
		writefd = &fd;
	rc = select((int)ssh->socket + 1, readfd, writefd, NULL, &timeout);
	return rc;
}

int map_ssh_error(gdssh_t* ssh, const char* path)
{
	int rc = libssh2_session_last_errno(ssh->ssh);
	if (rc > 0 || rc < -47)
		rc = -48; /* ssh unknown */
	if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL) {
		rc = libssh2_sftp_last_error(ssh->sftp);
		if (rc < 0 || rc>21)
			rc = 22; /* sftp unknown */
	}
	// rc < 0: ssh error
	// rc > 0: sftp errot
	return rc;
}

int map_error(int rc)
{
	if (rc == 0)
		return 0;
	if (rc <= -48 || rc >= 22)
		return EINVAL;

	switch (rc) {
	case LIBSSH2_FX_PERMISSION_DENIED:
	case LIBSSH2_FX_WRITE_PROTECT:
	case LIBSSH2_FX_LOCK_CONFLICT:
		return EACCES;
	case LIBSSH2_FX_NO_SUCH_FILE:
	case LIBSSH2_FX_NO_SUCH_PATH:
	case LIBSSH2_FX_INVALID_FILENAME:
	case LIBSSH2_FX_NOT_A_DIRECTORY:
	case LIBSSH2_FX_NO_MEDIA:
		return ENOENT;
	case LIBSSH2_FX_QUOTA_EXCEEDED:
	case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
		return ENOMEM;
	case LIBSSH2_FX_FILE_ALREADY_EXISTS:
		return EEXIST;
	case LIBSSH2_FX_DIR_NOT_EMPTY:
		return ENOTEMPTY;
	case LIBSSH2_FX_EOF:
		return ENOTEMPTY;
	case LIBSSH2_FX_BAD_MESSAGE:
		return EBADMSG;
	case LIBSSH2_FX_NO_CONNECTION:
		return ENOTCONN;
	case LIBSSH2_FX_CONNECTION_LOST:
		ECONNABORTED;
	case LIBSSH2_FX_OP_UNSUPPORTED:
		EOPNOTSUPP;
	case LIBSSH2_FX_FAILURE:
		return EPERM;
	default:
		//printf("ssh error?: rc=%d\n", rc);
		return EIO;
	}
}

int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	return (tok->type == JSMN_STRING
		&& (int)strlen(s) == tok->end - tok->start
		&& strncmp(json + tok->start, s, tok->end - tok->start) == 0) ? 0 : -1;
}

int load_json(fs_config * fs)
{
	// fill json with json->drive parameters
	if (!file_exists(fs->json)) {
		fprintf(stderr, "cannot read json file: %s\n", fs->json);
		return 1;
	}
	char* JSON_STRING = 0;
	size_t size = 0;
	FILE *fp = fopen(fs->json, "r");
	fseek(fp, 0, SEEK_END); /* Go to end of file */
	size = ftell(fp); /* How many bytes did we pass ? */
	rewind(fp);
	JSON_STRING = calloc(size + 1, sizeof(char*)); /* size + 1 byte for the \0 */
	fread(JSON_STRING, size, 1, fp); /* Read 1 chunk of size bytes from fp into buffer */
	JSON_STRING[size] = '\0';
	fclose(fp);

	int i, r;
	jsmn_parser p;
	jsmntok_t t[1024]; /* We expect no more than 128 tokens */
	jsmntok_t *tok;
	char* val;

	jsmn_init(&p);
	r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t) / sizeof(t[0]));
	if (r < 0) {
		fprintf(stderr, "Failed to parse JSON: %d\n", r);
		return 1;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		fprintf(stderr, "Object expected, json type=%d\n", t[0].type);
		return 1;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		/* only interested in drives key */
		if (jsoneq(JSON_STRING, &t[i], "Args") == 0) {
			tok = &t[i + 1];
			val = str_ndup(JSON_STRING + tok->start, tok->end - tok->start);
			fs->args = strdup(val);
			free(val);
			i++;
		}
		else if (jsoneq(JSON_STRING, &t[i], "Drives") == 0) {
			int size = t[i + 1].size;
			i++;
			for (int j = 0; j < size; j++) {
				tok = &t[i + 1];
				char * key = str_ndup(JSON_STRING + tok->start, tok->end - tok->start);
				jsmntok_t *v = &t[i + 2];

				if (strcmp(key, fs->drive) == 0) {
					i = i + 3;
					for (int k = 0; k < v->size; k++) {
						tok = &t[i + 1];
						if (tok->type == JSMN_STRING) {
							val = str_ndup(JSON_STRING + tok->start, tok->end - tok->start);
							if (jsoneq(JSON_STRING, &t[i], "Args") == 0) {
								fs->args = strdup(val);
								free(val);
							}
							i = i + 2;
						}
						else if (tok->type == JSMN_ARRAY) {
							i = i + tok->size + 1;
						}
					}
				}
				else {
					i = i + 3;
					for (int k = 0; k < v->size; k++) {
						tok = &t[i + 1];
						if (tok->type == JSMN_STRING) {
							i = i + 2;
						}
						else if (tok->type == JSMN_ARRAY) {
							i = i + tok->size + 1;
						}
					}
				}
				free(key);

				//else if (v->type == JSMN_ARRAY) {
				//	//fs->hostcount = v->size;
				//	//fs->hostlist = malloc(v->size);
				//	//for (int u = 0; u < v->size; u++) {
				//	//	jsmntok_t *h = &t[i+j+u+4];
				//	//	int ssize = h->end - h->start;
				//	//	//printf("  * %.*s\n", h->end - h->start, JSON_STRING + h->start); 
				//	//	fs->hostlist[u] = strndup(JSON_STRING + h->start, ssize);
				//	//	fs->hostlist[u][ssize] = '\0';
				//	//	//printf("host %d: %s\n", u+1, fs->hostlist[u]);
				//	//	
				//	//}
				//	//i += t[i + 1].size + 1;

				//	i = i + v->size + 1;
				//}
				//free(key);

			}
		}
		else {
			// assume key with 1 value, skip it
			i++;
		}
	}
	free(JSON_STRING);
	//printf("Hosts:\n");
	//i = 0;
	//while (json->hosts[i])
	//	printf("  - %s\n", json->hosts[i++]);
	//printf("Port: %s\n", json->port);
	//printf("Drive: %s\n", json->drive);
	//printf("Path: %s\n", json->path);
	return 0;
}


void gd_log(const char *fmt, ...)
{
	char message[1000];
	//memset(message, 0, 1000);
	va_list args;
	va_start(args, fmt);
	vsprintf(message, fmt, args);
	va_end(args);
	printf("%s", message);
	if (g_logfile) {
		FILE *f = fopen(g_logfile, "a");
		if (f != NULL)
			fprintf(f, "golddrive: %s", message);
		fclose(f);
	}
}
