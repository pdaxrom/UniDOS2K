/***********************************************************************/
/*                                                                     */
/*  SYSCALLS.C:  System Calls Remapping                                */
/*  most of this is from newlib-lpc and a Keil-demo                    */
/*                                                                     */
/*  these are "reentrant functions" as needed by                       */
/*  the WinARM-newlib-config, see newlib-manual                        */
/*  collected and modified by Martin Thomas                            */
/*  TODO: some more work has to be done on this                        */
/***********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <errno.h>
#include <dirent.h>
#include <setjmp.h>

#include "swi.h"

jmp_buf	__exit_jump_buf__;
int 	__exit_jump_stat__;

#if 0
static inline int do_SystemSWI (int reason, void *arg)
{
  int value;
  asm volatile ("mov r0, %1; mov r1, %2; swi %a3; mov %0, r0"
       : "=r" (value) /* Outputs */
       : "r" (reason), "r" (arg), "i" (SystemSWI) /* Inputs */
       : "r0", "r1", "lr"
		/* Clobbers r0 and r1, and lr if in supervisor mode */);
  return value;
}
#endif

int _open_r(struct _reent *r, const char *pathname, int flags, int mode)
{
    int volatile block[4];

    block[0] = (int) r;
    block[1] = (int) pathname;
    block[2] = flags;
    block[3] = mode;
    return do_SystemSWI(SWI_NEWLIB_Open_r, (void *)block);
}

_ssize_t _read_r(struct _reent *r, int file, void *ptr, size_t len)
{
    int volatile block[4];

    block[0] = (int) r;
    block[1] = file;
    block[2] = (int) ptr;
    block[3] = len;
    return do_SystemSWI(SWI_NEWLIB_Read_r, (void *)block);
}

_ssize_t _write_r (struct _reent *r, int file, const void *ptr, size_t len)
{
    int volatile block[4];

    block[0] = (int) r;
    block[1] = file;
    block[2] = (int) ptr;
    block[3] = len;
    return do_SystemSWI(SWI_NEWLIB_Write_r, (void *)block);
}

int _close_r(struct _reent *r, int file)
{
    int volatile block[2];

    block[0] = (int) r;
    block[1] = file;
    return do_SystemSWI(SWI_NEWLIB_Close_r, (void *)block);
}

_off_t _lseek_r(struct _reent *r, int file, _off_t ptr, int dir)
{
    int volatile block[4];

    block[0] = (int) r;
    block[1] = file;
    block[2] = ptr;
    block[3] = dir;
    return do_SystemSWI(SWI_NEWLIB_Lseek_r, (void *)block);
}

int _fstat_r(struct _reent *r, int file, struct stat *st)
{
    int volatile block[3];

    block[0] = (int) r;
    block[1] = file;
    block[2] = (int) st;
    return do_SystemSWI(SWI_NEWLIB_Fstat_r, (void *)block);
}

int _stat_r(struct _reent *r, const char *name, struct stat *s)
{
    int volatile block[3];

    block[0] = (int) &r;
    block[1] = (int) name;
    block[2] = (int) s;
    return do_SystemSWI(SWI_NEWLIB_Stat_r, (void *)block);
}

int lstat(const char *name, struct stat *s)
{
    struct _reent r;
    int ret = _stat_r(&r, name, s);
    errno = r._errno;
    return ret;
}

int _isatty_r(struct _reent *r, int file)
{
    int volatile block[2];

    block[0] = (int)r;
    block[1] = file;
    return do_SystemSWI(SWI_NEWLIB_Isatty_r, (void *)block);
}

int _isatty(int file)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int)&r;
    block[1] = file;
    int ret = do_SystemSWI(SWI_NEWLIB_Isatty_r, (void *)block);
    errno = r._errno;
    return ret;
}

int ioctl(int fd, unsigned int request, void *value)
{
    int volatile block[3];
    block[0] = fd;
    block[1] = (int)request;
    block[2] = (int)value;
    return do_SystemSWI(SWI_NEWLIB_Ioctl, (void *)block);
}

int _gettimeofday_r(struct _reent *r, struct timeval *tp, void *tzp)
{
    int volatile block[3];

    block[0] = (int) r;
    block[1] = (int) tp;
    block[2] = (int) tzp;
    return do_SystemSWI(SWI_NEWLIB_Gettimeofday_r, (void *)block);
}

void _exit(int n)
{
#ifndef USE_SWI_EXIT
    __exit_jump_stat__ = n;
    longjmp(__exit_jump_buf__, 1);
#else
    int volatile block[1];

    block[0] = n;
    do_SystemSWI(SWI_NEWLIB_Exit, (void *)block);
#endif
}

/* "malloc clue function" */

/**** Locally used variables. ****/
extern char *__stack_end__; 	/* Defined by startup                   */

extern char __end__[];      /*  end is set in the linker command 	*/
				/* file and is the end of statically 	*/
				/* allocated data (thus start of heap).	*/

/*static*/ char *heap_ptr;		/* Points to current end of the heap.	*/

#define STACK_BUFFER 16384

/************************** _sbrk_r *************************************/
/*  Support function.  Adjusts end of heap to provide more memory to	*/
/* memory allocator. Simple and dumb with no sanity checks.		*/
/*  struct _reent *r	-- re-entrancy structure, used by newlib to 	*/
/*			support multiple threads of operation.		*/
/*  ptrdiff_t nbytes	-- number of bytes to add.			*/
/*  Returns pointer to start of new heap area.				*/
/*  Note:  This implementation is not thread safe (despite taking a	*/
/* _reent structure as a parameter).  					*/
/*  Since _s_r is not used in the current implementation, the following	*/
/* messages must be suppressed.						*/

void * _sbrk_r(struct _reent *r, ptrdiff_t nbytes)
{
    char  *base;		/*  errno should be set to  ENOMEM on error	*/

    if (!heap_ptr) {	/*  Initialize if first time through.		*/
	heap_ptr = __end__;
#ifdef DEBUG
	printf("heap_ptr = %p\n", __end__);
	printf("stack_ptr = %p\n", __stack_end__);
#endif
    }

    if ((__stack_end__ - (heap_ptr + nbytes)) > STACK_BUFFER) {
	base = heap_ptr;	/*  Point to end of heap.			*/
	heap_ptr += nbytes;	/*  Increase heap.				*/
	return base;		/*  Return pointer to start of new heap area.	*/
    }

    r->_errno = ENOMEM;
    return (void *) -1;
}

int _kill_r(struct _reent *r, int pid, int sig)
{
    r->_errno = EINVAL;
    return -1;
    pid = pid;
    sig = sig;
}

int _getpid_r(struct _reent *r)
{
    return 1;
    r = r;
}

#ifdef REENT_MKDIR
int _mkdir_r(struct _reent *r, const char *path, int mode)
{
    int volatile block[3];

    block[0] = (int) r;
    block[1] = (int) path;
    block[2] = mode;
    return do_SystemSWI(SWI_NEWLIB_Mkdir_r, (void *)block);
}
#else
int mkdir(const char *path, mode_t mode)
{
    struct _reent r;
    int volatile block[3];

    block[0] = (int) &r;
    block[1] = (int) path;
    block[2] = (int) mode;
    return do_SystemSWI(SWI_NEWLIB_Mkdir_r, (void *)block);
}
#endif

#ifdef REENT_RMDIR
int _rmdir_r(struct _reent *r, const char *path)
{
    int volatile block[2];

    block[0] = (int) r;
    block[1] = (int) path;
    return do_SystemSWI(SWI_NEWLIB_Rmdir_r, (void *)block);
}
#else
int rmdir(const char *path)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) path;
    return do_SystemSWI(SWI_NEWLIB_Rmdir_r, (void *)block);
}
#endif

int _unlink_r(struct _reent *r, const char *path)
{
    int volatile block[2];

    block[0] = (int) r;
    block[1] = (int) path;
    return do_SystemSWI(SWI_NEWLIB_Unlink_r, (void *)block);
}

int _rename_r(struct _reent *r, const char *path, const char *newpath)
{
    int volatile block[3];

    block[0] = (int) r;
    block[1] = (int) path;
    block[2] = (int) newpath;
    return do_SystemSWI(SWI_NEWLIB_Rename_r, (void *)block);
}

int chdir(const char *path)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) path;
    return do_SystemSWI(SWI_NEWLIB_Chdir_r, (void *)block);
}

char *getcwd(char *buf, size_t size)
{
    struct _reent r;
    int volatile block[3];

    block[0] = (int) &r;
    block[1] = (int) buf;
    block[2] = (int) size;
    return (char *) do_SystemSWI(SWI_NEWLIB_Getcwd_r, (void *)block);
}

char *realpath(const char *path, char *resolved_path)
{
    int volatile block[2];

    block[0] = (int) path;
    block[1] = (int) resolved_path;
    return (char *) do_SystemSWI(SWI_NEWLIB_Realpath, (void *)block);
}

DIR *opendir(const char *name)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) name;
    return (DIR *)do_SystemSWI(SWI_NEWLIB_Opendir_r, (void *)block);
}

struct dirent *readdir(DIR *dirp)
{
    struct _reent r;
    static struct dirent entry;
    struct dirent *result;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) dirp;
    block[2] = (int) &entry;
    block[3] = (int) &result;
    do_SystemSWI(SWI_NEWLIB_Readdir_r, (void *)block);
    return result;
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    struct _reent r;
    int volatile block[4];

    block[0] = (int) &r;
    block[1] = (int) dirp;
    block[2] = (int) entry;
    block[3] = (int) result;
    return do_SystemSWI(SWI_NEWLIB_Readdir_r, (void *)block);
}

int closedir(DIR *dirp)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) dirp;
    return do_SystemSWI(SWI_NEWLIB_Closedir_r, (void *)block);
}

int mount_fs(char *fs)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) fs;
    return do_SystemSWI(SWI_MountFS, (void *)block);
}

int umount_fs(char *fs)
{
    struct _reent r;
    int volatile block[2];

    block[0] = (int) &r;
    block[1] = (int) fs;
    return do_SystemSWI(SWI_UmountFS, (void *)block);
}

int exec_mem(char *elfarg, void *entry, void *sp);
asm (
    "exec_mem:		\n\t"
    "push {r1-r12, lr}	\n\t"
    "mov r3, sp		\n\t"
    "mov sp, r2		\n\t"
    "bic sp, sp, #7	\n\t"
    "push {r3, r4}	\n\t"
    "mov lr, pc		\n\t"
    "bx  r1		\n\t"
    "pop {r3, r4}	\n\t"
    "mov sp, r3		\n\t"
    "pop {r1-r12, lr}	\n\t"
    "bx  lr		\n\t"
);

#define DEFAULT_ELF_LOAD_ADDR	0xa0080000

int _system(const char *cmdline)
{
    void *addr = (void *) DEFAULT_ELF_LOAD_ADDR;
    void *entry = NULL;
    int volatile block[3];

    block[0] = (int) addr;
    block[1] = (int) cmdline;
    block[2] = (int) &entry;
    int ret = do_SystemSWI(SWI_ELF_Load, (void *)block);
    if (ret == -1)
	return ret;

    size_t stacksize = do_SystemSWI(SWI_ELF_GetStackSize, (void *)block);

    char *elfarg = (char *)alloca(strlen(cmdline) + 1);
    strcpy(elfarg, cmdline);

    errno = exec_mem(elfarg, entry, addr + stacksize);

    /* restore stdin */
    {
	struct termios raw;
	if (tcgetattr(0, &raw) != -1) {
	    raw.c_lflag |=  (ECHO | ICANON | IEXTEN | ISIG);
	    raw.c_cc[VMIN] = 1;
	    raw.c_cc[VTIME] = 0;
	    tcsetattr(0, TCSAFLUSH, &raw);
	}
    }

    return 0;
}

int utime(const char *filename, const struct utimbuf *times)
{
    errno = EACCES;
    return -1;
}

mode_t umask(mode_t __mask)
{
    static mode_t mask;
    mask = __mask;
    return mask;
}

int chmod(const char *__path, mode_t __mode)
{
    errno = EACCES;
    return -1;
}

int chown(const char *__path, uid_t __owner, gid_t __group)
{
    errno = EACCES;
    return -1;
}

int _fileno(FILE *file)
{
    return file->_file;
}

#warning "_setmode() ?"
int _setmode (int handle, int mode)
{
    errno = EINVAL;
    return -1;
}
