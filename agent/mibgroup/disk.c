#include <config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <nlist.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#ifndef bsdi2
#include <sys/vmmeter.h>
#endif
#endif
#if HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#include <sys/param.h>
#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif
#if HAVE_SYS_FS_H
#include <sys/fs.h>
#else
#if HAVE_UFS_FS_H
#include <ufs/fs.h>
#else
#if HAVE_UFS_FFS_FS_H
#include <ufs/ffs/fs.h>
#endif
#endif
#endif
#if HAVE_MTAB_H
#include <mtab.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#if HAVE_FSTAB_H
#include <fstab.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if (!defined(HAVE_STATVFS)) && defined(HAVE_STATFS)
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#define statvfs statfs
#endif
#if HAVE_VM_SWAP_PAGER_H
#include <vm/swap_pager.h>
#endif
#if HAVE_SYS_FIXPOINT_H
#include <sys/fixpoint.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if STDC_HEADERS
#include <string.h>
#endif

#include "mibincl.h"
#include "disk.h"
#include "util_funcs.h"
#include "errormib.h"

int numdisks;
struct diskpart disks[MAXDISKS];

unsigned char *var_extensible_disk(vp, name, length, exact, var_len, write_method)
    register struct variable *vp;
/* IN - pointer to variable entry that points here */
    register oid	*name;
/* IN/OUT - input name requested, output name found */
    register int	*length;
/* IN/OUT - length of input and output oid's */
    int			exact;
/* IN - TRUE if an exact match was requested. */
    int			*var_len;
/* OUT - length of variable or 0 if function returned. */
    int			(**write_method) __P((int, u_char *, u_char, int, u_char *, oid *, int));
/* OUT - pointer to function to set variable, otherwise 0 */
{

  oid newname[30];
  int disknum=0;
#if !defined(HAVE_SYS_STATVFS_H) && !defined(HAVE_STATFS)
  double totalblks, free, used, avail, availblks;
#endif
  static long long_ret;
  static char errmsg[300];

#if defined(HAVE_STATVFS) || defined(HAVE_STATFS)
  struct statvfs vfs;
#else
#if HAVE_FSTAB_H
  int file;
  union {
     struct fs iu_fs;
     char dummy[SBSIZE];
  } sb;
#define filesys sb.iu_fs
#endif
#endif
  
  if (!checkmib(vp,name,length,exact,var_len,write_method,newname,numdisks))
    return(NULL);
  disknum = newname[*length - 1] - 1;
  switch (vp->magic) {
    case MIBINDEX:
      long_ret = disknum;
      return((u_char *) (&long_ret));
    case ERRORNAME:       /* DISKPATH */
      *var_len = strlen(disks[disknum].path);
      return((u_char *) disks[disknum].path);
    case DISKDEVICE:
      *var_len = strlen(disks[disknum].device);
      return((u_char *) disks[disknum].device);
    case DISKMINIMUM:
      long_ret = disks[disknum].minimumspace;
      return((u_char *) (&long_ret));
  }
#if defined(HAVE_SYS_STATVFS_H) || defined(HAVE_STATFS)
  if (statvfs (disks[disknum].path, &vfs) == -1) {
    fprintf(stderr,"Couldn't open device %s\n",disks[disknum].device);
    setPerrorstatus("statvfs dev/disk");
    return NULL;
  }
#if defined(HAVE_ODS)
  vfs.f_blocks = vfs.f_spare[0];
  vfs.f_bfree  = vfs.f_spare[1];
  vfs.f_bavail = vfs.f_spare[2];
#endif
  switch (vp->magic) {
    case DISKTOTAL:
      long_ret = vfs.f_blocks;
      return((u_char *) (&long_ret));
    case DISKAVAIL:
      long_ret = vfs.f_bavail;
      return((u_char *) (&long_ret));
    case DISKUSED:
      long_ret = vfs.f_blocks - vfs.f_bfree;
      return((u_char *) (&long_ret));
    case DISKPERCENT:
      long_ret = (int) (vfs.f_bavail <= 0 ? 100 :
                        ((double) (vfs.f_blocks - vfs.f_bavail) / (double) vfs.f_blocks) * 100);
      return ((u_char *) (&long_ret));
    case ERRORFLAG:
      long_ret = (vfs.f_bavail < disks[disknum].minimumspace)
        ? 1 : 0;
      return((u_char *) (&long_ret));
    case ERRORMSG:
      if (vfs.f_bavail < disks[disknum].minimumspace) 
        sprintf(errmsg,"%s: under %d left (= %d)",disks[disknum].path,
                disks[disknum].minimumspace, (int) vfs.f_bavail);
      else
        errmsg[0] = NULL;
      *var_len = strlen(errmsg);
      return((u_char *) (errmsg));
  }
#else
#if HAVE_FSTAB_H
  /* read the disk information */
  if ((file = open(disks[disknum].device,0)) < 0) {
    fprintf(stderr,"Couldn't open device %s\n",disks[disknum].device);
    setPerrorstatus("open dev/disk");
    return(NULL);
  }
  lseek(file, (long) (SBLOCK * DEV_BSIZE), 0);
  if (read(file,(char *) &filesys, SBSIZE) != SBSIZE) {
    setPerrorstatus("open dev/disk");
    fprintf(stderr,"Error reading device %s\n",disks[disknum].device);
    close(file);
    return(NULL);
  }
  close(file);
  totalblks = filesys.fs_dsize;
  free = filesys.fs_cstotal.cs_nbfree * filesys.fs_frag +
    filesys.fs_cstotal.cs_nffree;
  used = totalblks - free;
  availblks = totalblks * (100 - filesys.fs_minfree) / 100;
  avail = availblks > used ? availblks - used : 0;
  switch (vp->magic) {
    case DISKTOTAL:
      long_ret = (totalblks * filesys.fs_fsize / 1024);
      return((u_char *) (&long_ret));
    case DISKAVAIL:
      long_ret = avail * filesys.fs_fsize/1024;
      return((u_char *) (&long_ret));
    case DISKUSED:
      long_ret = used * filesys.fs_fsize/1024;
      return((u_char *) (&long_ret));
    case DISKPERCENT:
      long_ret = (double) (availblks == 0 ? 0 :
                        ((double) used / (double) availblks) * 100);
      return ((u_char *) (&long_ret));
    case ERRORFLAG:
      long_ret = (avail * filesys.fs_fsize/1024 < disks[disknum].minimumspace)
        ? 1 : 0;
      return((u_char *) (&long_ret));
    case ERRORMSG:
      if (avail * filesys.fs_fsize/1024 < disks[disknum].minimumspace) 
        sprintf(errmsg,"%s: under %d left (= %d)",disks[disknum].path,
                disks[disknum].minimumspace, avail * filesys.fs_fsize/1024);
      else
        errmsg[0] = NULL;
      *var_len = strlen(errmsg);
      return((u_char *) (errmsg));
  }
#endif
#endif
  return ((u_char *) 0);
}

