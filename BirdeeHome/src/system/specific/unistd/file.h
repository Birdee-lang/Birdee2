 
#define mode_t uint
#define dev_t uint
#define off_t ulong
int creat(const char *pathname, mode_t mode);

int openat(int dirfd, const char *pathname, int flags);
int openat(int dirfd, const char *pathname, int flags, mode_t mode);

int name_to_handle_at(int dirfd, const char *pathname,
                        struct file_handle *handle,
                        int *mount_id, int flags);

int open_by_handle_at(int mount_fd, struct file_handle *handle,
                        int flags);

int memfd_create(const char *name, uint flags);
int mknod(const char *pathname, mode_t mode, dev_t dev);

int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);
int rename(const char *oldpath, const char *newpath);

int renameat(int olddirfd, const char *oldpath,
            int newdirfd, const char *newpath);

int renameat2(int olddirfd, const char *oldpath,
                int newdirfd, const char *newpath, uint flags);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
int fallocate(int fd, int mode, off_t offset, off_t len);

int mkdir(const char *pathname, mode_t mode);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
int rmdir(const char *pathname);
char *getcwd(char *buf, size_t size);

char *getwd(char *buf);

char *get_current_dir_name(void);
int chdir(const char *path);
int fchdir(int fd);
int chroot(const char *path);
int lookup_dcookie(ulong cookie, char *buffer, size_t len);

int link(const char *oldpath, const char *newpath);

int linkat(int olddirfd, const char *oldpath,
            int newdirfd, const char *newpath, int flags);

int symlink(const char *target, const char *linkpath);

int symlinkat(const char *target, int newdirfd, const char *linkpath);
        int unlink(const char *pathname);
int unlinkat(int dirfd, const char *pathname, int flags);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *pathname,
char *buf, size_t bufsiz);
mode_t umask(mode_t mask);
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);

int fstatat(int dirfd, const char *pathname, struct stat *statbuf,
    int flags);

int chmod(const char *pathname, mode_t mode);
int fchmod(int fd, mode_t mode);
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);

int chown(const char *pathname, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *pathname, uid_t owner, gid_t group);

int fchownat(int dirfd, const char *pathname,
    uid_t owner, gid_t group, int flags);
int utime(const char *filename, const struct utimbuf *times);

int utimes(const char *filename, const struct timeval* times);
int futimesat(int dirfd, const char *pathname,
        const struct timeval* times);
int utimensat(int dirfd, const char *pathname,
        const struct timespec* times, int flags);

int futimens(int fd, const struct timespec* times);
int access(const char *pathname, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
