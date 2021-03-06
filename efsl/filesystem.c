#ifndef _FILESYSTEM_C_
#define _FILESYSTEM_C_

#include <stdio.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <dirent.h>

#include "../efsl/Inc/disc.h"
#include "../efsl/Inc/partition.h"
#include "../efsl/Inc/fs.h"
#include "../efsl/Inc/file.h"
#include "../efsl/Inc/mkfs.h"
#include "../efsl/Inc/ioman.h"
#include "../efsl/Inc/ui.h"
#include "../efsl/Inc/ls.h"

#define MAX_FILEDESCR	32

typedef struct {
    File *file;
    int used;
} filedesc;

static filedesc fd_list[MAX_FILEDESCR];

static int inited = 0;

static void wrap_fs_check_is_inited(void)
{
    int i;
    if (!inited) {
	// reserved fds
	fd_list[0].used = 1; // stdin
	fd_list[1].used = 1; // stdout
	fd_list[2].used = 1; // stderr
	for (i = 3; i < MAX_FILEDESCR; i++)
	    fd_list[i].used = 0;
	inited = 1;
    }
}

static int wrap_fs_get_fd(void)
{
    int i;
    for (i = 0; i < MAX_FILEDESCR; i++) {
	if (!fd_list[i].used)
	    return i;
    }
    return -1;
}

static hwInterface *lfile = NULL;
static IOManager *ioman = NULL;
static Disc *disc = NULL;
static Partition *part = NULL;
static FileSystem *fs = NULL;

static int wrap_fs_mountfs(struct _reent *r, const char *pathname)
{
    if (fs) {
	r->_errno = EBUSY;
	return -1;
    }

    lfile = malloc(sizeof(*lfile));

    if (if_initInterface(lfile,"mci0:")) {
	free(lfile);
	r->_errno = EACCES;
	return -1;
    }
    ioman = malloc(sizeof(*ioman));
    disc = malloc(sizeof(*disc));
    part = malloc(sizeof(*part));
    
    ioman_init(ioman,lfile,0);
    disc_initDisc(disc,ioman);
    memClr(disc->partitions,sizeof(PartitionField)*4);
    disc->partitions[0].type=0x0B;
    disc->partitions[0].LBA_begin=0;
    disc->partitions[0].numSectors=lfile->sectorCount;
    part_initPartition(part,disc);

    fs = malloc(sizeof(*fs));
    if( (fs_initFs(fs,part)) != 0) {
	 printf("Unable to init the filesystem\n");
	 return(-1);
    }
    return 0;
}

static int wrap_fs_unmountfs(struct _reent *r, const char *pathname)
{
    if (!fs) {
	r->_errno = EINVAL;
	return -1;
    }
    fs_umount(fs);
    free(part);
    free(disc);
    free(ioman);
    free(fs);
    if_finiInterface(lfile);
    free(lfile);
    fs = NULL;
    lfile = NULL;
    return 0;
}

static int wrap_fs_open(struct _reent *r, const char *pathname, int flags, int mode)
{
    wrap_fs_check_is_inited();
    mode = mode;
    int fd = wrap_fs_get_fd();
    if (fd >= 0) {
	euint8 m = 0;
	flags &= 0xffff;
	if (flags == O_RDONLY)
	    m = MODE_READ;
	else if (flags & O_APPEND)
	    m = MODE_APPEND;
	else if (flags & O_WRONLY) {
	    if ((flags & O_CREAT) || (flags & O_TRUNC))
		fs_rmfile(fs, (euint8*)pathname);
	    m = MODE_WRITE;
	}

	fprintf(stderr, "open(%s) flag: %08X mode: %c\n", pathname, flags, m);
	if (!m) {
	    fprintf(stderr, "unknown mode\n");
	    r->_errno = EACCES;
	    return -1;
	}

	fd_list[fd].file = (File *)malloc(sizeof(File));
	if (!fd_list[fd].file) {
	    fprintf(stderr, "No memory!\n");
	    r->_errno = ENOMEM;
	    return -1;
	}
	if ( (file_fopen(fd_list[fd].file, fs, (esint8*)pathname, m)) != 0) {
	    fprintf(stderr, "Unable to open the file\n");
	    r->_errno = EACCES;
	    return(-1);
	}
	fd_list[fd].used = 1;
	return fd;
    }
    r->_errno = ENFILE;
    return -1;
}

static _ssize_t wrap_fs_read(struct _reent *r, int file, void *ptr, size_t len)
{
    if (fd_list[file].used) {
	return file_read(fd_list[file].file, len, (euint8*)ptr);
    }
    r->_errno = EBADF;
    return -1;
}

static _ssize_t wrap_fs_write(struct _reent *r, int file, const void *ptr, size_t len)
{
    if (fd_list[file].used) {
	return file_write(fd_list[file].file, len, (euint8*)ptr);
    }
    r->_errno = EBADF;
    return -1;
}

static int wrap_fs_close(struct _reent *r, int file)
{
    if (fd_list[file].used) {
	int ret = 0;
	fprintf(stderr, "close file\n");
	if (file_fclose(fd_list[file].file)) {
	    fprintf(stderr, "f_close %d\n", ret);
	    r->_errno = EIO;
	    ret = -1;
	}
	free(fd_list[file].file);
	fd_list[file].file = NULL;
	fd_list[file].used = 0;
	return 0;
    }
    r->_errno = EBADF;
    return -1;
}

static _off_t wrap_fs_lseek(struct _reent *r, int file, _off_t ptr, int dir)
{
    if (fd_list[file].used) {
	euint32 offset = 0;
	switch (dir) {
	case SEEK_SET:
	    offset = ptr;
	    break;
	case SEEK_CUR:
	    offset = fd_list[file].file->FilePtr + ptr;
	    break;
	case SEEK_END:
	    offset = fd_list[file].file->FileSize + ptr;
	    break;
	default:
	    r->_errno = EINVAL;
	    return -1;
	}
	if (file_setpos(fd_list[file].file, offset)) {
	    r->_errno = EINVAL;
	    return -1;
	}
	return offset;
    }
    r->_errno = EBADF;
    return -1;
}

static int wrap_fs_fstat(struct _reent *r, int file, struct stat *st)
{
    if (fd_list[file].used) {
//	FRESULT ret;
	st->st_size = fd_list[file].file->FileSize;
	st->st_blksize = 512;
	st->st_blocks = ((st->st_size - 1) / 512) + 1;
//	if (s.attr & DIR_ATTR_DIRECTORY)
//	    st->st_mode = S_IFDIR;
//	else
	    st->st_mode = S_IFREG;

//	if (s.attr & DIR_ATTR_READ_ONLY)
//	    st->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH);
//	else
	    st->st_mode |= (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	return 0;
    }
    r->_errno = EBADF;
    return -1;
}

static int wrap_fs_isatty(int file)
{
    if (fd_list[file].used) {
	errno = EINVAL;
	return 0;
    }

    errno = EBADF;
    return 0;
}

static int wrap_fs_mkdir(struct _reent *r, char *path, uint32_t mode)
{
    int rc = fs_mkdir(fs, (eint8*)path);
    if (!rc)
	return 0;

    if (rc == -1) {
	r->_errno = EEXIST;
    } else if (rc == -3) {
	r->_errno = ENOSPC;
    } else
	r->_errno = EACCES;

    return -1;
}

static int wrap_fs_rmdir(struct _reent *r, char *path)
{
    if (!fs_rmfile(fs, (euint8*)path)) {
	return 0;
    }
    r->_errno = EACCES;

    return -1;
}

static int wrap_fs_unlink(struct _reent *r, char *path)
{
    if (!fs_rmfile(fs, (euint8*)path)) {
	return 0;
    }
    r->_errno = EACCES;

    return -1;
}

static int wrap_fs_rename(struct _reent *r, char *oldpath, char *newpath)
{
#if 0
    char tmpath[256];
    if ((oldpath[0] != '/') && (oldpath[0] != '\\'))
	snprintf(tmpath, 256, "%s/%s", current_path, oldpath);
    else
	strcpy(tmpath, oldpath);
    normalize_path(tmpath);

    char tmpath1[256];
    if ((newpath[0] != '/') && (newpath[0] != '\\'))
	snprintf(tmpath1, 256, "%s/%s", current_path, newpath);
    else
	strcpy(tmpath1, newpath);
    normalize_path(tmpath1);

    FF_ERROR error = FF_Move(pIoman, tmpath, tmpath1);

    if (!error)
	return 0;
#endif

    r->_errno = EACCES;
    return -1;
}

static char *wrap_fs_getcwd(struct _reent *r, char *buf, int size)
{
    if (buf && !size) {
	r->_errno = EINVAL;
	return NULL;
    }
    if (!buf)
	buf = malloc(size);
    if (!buf) {
	r->_errno = EINVAL;
	return NULL;
    }

#if 0
    strncpy(buf, current_path, size - 1);
#else
    strncpy(buf, "/", size - 1);
#endif
    return buf;
}

static int wrap_fs_chdir(struct _reent *r, char *path)
{
#if 0
    FF_DIRENT dir;
    char tmpath[256];
    if ((path[0] != '/') && (path[0] != '\\'))
	snprintf(tmpath, 256, "%s/%s", current_path, path);
    else
	strcpy(tmpath, path);
    normalize_path(tmpath);

    xfprintf(stderr, "chdir(%s)\n", tmpath);

    if (!FF_FindFirst(pIoman, &dir, tmpath)) {
	strcpy(current_path, tmpath);
	return 0;
    }
#endif

    r->_errno = ENOTDIR;
    return -1;
}

static void *wrap_fs_opendir(struct _reent *r, char *path)
{
    DirList *list = malloc(sizeof(DirList));
    if (!list) {
	r->_errno = ENOMEM;
	return NULL;
    }
    if (ls_openDir(list, fs, path) != 0) {
	r->_errno = EACCES;
	return NULL;
    }
    return list;
}

static int wrap_fs_readdir_r(struct _reent *r, void *dirp, struct dirent *entry, struct dirent **result)
{
    DirList *list = dirp;
    if (ls_getNext(list) != 0) {
	*result = NULL;
	return 0;
    }
//    strncpy(entry->d_name, (char *)list->currentEntry.FileName, NAME_MAX - 1);
// OMG we use 8+3!
    strncpy(entry->d_name, (char *)list->currentEntry.FileName, 8 + 3);
    entry->d_name[8 + 3] = 0;
    entry->d_size = list->currentEntry.FileSize;
    if (list->currentEntry.Attribute & ATTR_DIRECTORY)
	entry->d_type |= DT_DIR;
    else
	entry->d_type |= DT_REG;
    *result = entry;
    return 0;
}

static int wrap_fs_closedir(struct _reent *r, void *dirp)
{
#warning "SWI wrap_fs_closedir"
    free(dirp);
    return 0;
}

#endif
