/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * This file contains functions that implement the fdisk menu commands.
 */
#include "global.h"
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/dktp/fdisk.h>
#include <sys/stat.h>
#include <sys/dklabel.h>

#include "main.h"
#include "analyze.h"
#include "menu.h"
#include "menu_command.h"
#include "menu_defect.h"
#include "menu_partition.h"
#include "menu_fdisk.h"
#include "param.h"
#include "misc.h"
#include "label.h"
#include "startup.h"
#include "partition.h"
#include "prompts.h"
#include "checkdev.h"
#include "io.h"
#include "ctlr_scsi.h"
#include "auto_sense.h"

extern	struct menu_item menu_fdisk[];

/*
 * Byte swapping macros for accessing struct ipart
 *	to resolve little endian on Sparc.
 */
#if defined(sparc)
#define	les(val)	((((val)&0xFF)<<8)|(((val)>>8)&0xFF))
#define	lel(val)	(((unsigned)(les((val)&0x0000FFFF))<<16) | \
			(les((unsigned)((val)&0xffff0000)>>16)))

#elif	defined(i386)

#define	les(val)	(val)
#define	lel(val)	(val)

#else	/* defined(sparc) */

#error	No Platform defined

#endif	/* defined(sparc) */


/* Function prototypes */
#ifdef	__STDC__

#if	defined(sparc)

static int getbyte(uchar_t **);
static int getlong(uchar_t **);

#endif	/* defined(sparc) */

static int get_solaris_part(int fd, struct ipart *ipart);

#else	/* __STDC__ */

#if	defined(sparc)

static int getbyte();
static int getlong();

#endif	/* defined(sparc) */

static int get_solaris_part();

#endif	/* __STDC__ */

/*
 * Handling the alignment problem of struct ipart.
 */
static void
fill_ipart(char *bootptr, struct ipart *partp)
{
#if defined(sparc)
	/*
	 * Sparc platform:
	 *
	 * Packing short/word for struct ipart to resolve
	 *	little endian on Sparc since it is not
	 *	properly aligned on Sparc.
	 */
	partp->bootid = getbyte((uchar_t **)&bootptr);
	partp->beghead = getbyte((uchar_t **)&bootptr);
	partp->begsect = getbyte((uchar_t **)&bootptr);
	partp->begcyl = getbyte((uchar_t **)&bootptr);
	partp->systid = getbyte((uchar_t **)&bootptr);
	partp->endhead = getbyte((uchar_t **)&bootptr);
	partp->endsect = getbyte((uchar_t **)&bootptr);
	partp->endcyl = getbyte((uchar_t **)&bootptr);
	partp->relsect = getlong((uchar_t **)&bootptr);
	partp->numsect = getlong((uchar_t **)&bootptr);
#elif defined(i386)
	/*
	 * i386 platform:
	 *
	 * The fdisk table does not begin on a 4-byte boundary within
	 * the master boot record; so, we need to recopy its contents
	 * to another data structure to avoid an alignment exception.
	 */
	(void) bcopy(bootptr, partp, sizeof (struct ipart));
#else
#error  No Platform defined
#endif /* defined(sparc) */
}

/*
 * Get a correct byte/short/word routines for Sparc platform.
 */
#if defined(sparc)
static int
getbyte(uchar_t **bp)
{
	int	b;

	b = **bp;
	*bp = *bp + 1;
	return (b);
}

#ifdef DEADCODE
static int
getshort(uchar_t **bp)
{
	int	b;

	b = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	return (b);
}
#endif /* DEADCODE */

static int
getlong(uchar_t **bp)
{
	int	b, bh, bl;

	bh = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;
	bl = ((**bp) << 8) | *(*bp + 1);
	*bp += 2;

	b = (bh << 16) | bl;
	return (b);
}
#endif /* defined(sparc) */


/*
 * Convert cn[tn]dnsn to cn[tn]dnp0 path
 */
static void
get_pname(char *name)
{
	char		buf[MAXPATHLEN];
	char		*devp = "/dev/dsk";
	char		*rdevp = "/dev/rdsk";
	char		np[MAXNAMELEN];
	char		*npt;

	/*
	 * If it is a full path /dev/[r]dsk/cn[tn]dnsn, use this path
	 */
	(void) strcpy(np, cur_disk->disk_name);
	if (strncmp(rdevp, cur_disk->disk_name, strlen(rdevp)) == 0 ||
	    strncmp(devp, cur_disk->disk_name, strlen(devp)) == 0) {
		/*
		 * Skip if the path is already included with pN
		 */
		if (strchr(np, 'p') == NULL) {
			npt = strrchr(np, 's');
			/* If sN is found, do not include it */
			if (isdigit(*++npt)) {
				*--npt = '\0';
			}
			(void) snprintf(buf, sizeof (buf), "%sp0", np);
		} else {
			(void) snprintf(buf, sizeof (buf), "%s", np);
		}
	} else {
		(void) snprintf(buf, sizeof (buf), "/dev/rdsk/%sp0", np);
	}
	(void) strcpy(name, buf);
}

/*
 * Open file descriptor for current disk (cur_file)
 *	with "p0" path or cur_disk->disk_path
 */
void
open_cur_file(int mode)
{
	char	*dkpath;
	char	pbuf[MAXPATHLEN];

	switch (mode) {
	    case FD_USE_P0_PATH:
		(void) get_pname(&pbuf[0]);
		dkpath = pbuf;
		break;
	    case FD_USE_CUR_DISK_PATH:
		dkpath = cur_disk->disk_path;
		break;
	    default:
		err_print("Error: Invalid mode option for opening cur_file\n");
		fullabort();
	}

	/* Close previous cur_file */
	(void) close(cur_file);
	/* Open cur_file with the required path dkpath */
	if ((cur_file = open_disk(dkpath, O_RDWR | O_NDELAY)) < 0) {
		err_print(
		    "Error: can't open selected disk '%s'.\n", dkpath);
		fullabort();
	}
}


/*
 * This routine implements the 'fdisk' command.  It simply runs
 * the fdisk command on the current disk.
 * Use of this is restricted to interactive mode only.
 */
int
c_fdisk()
{

	char		buf[MAXPATHLEN];
	char		pbuf[MAXPATHLEN];
	struct stat	statbuf;

	/*
	 * We must be in interactive mode to use the fdisk command
	 */
	if (option_f != (char *)NULL || isatty(0) != 1 || isatty(1) != 1) {
		err_print("Fdisk command is for interactive use only!\n");
		return (-1);
	}

	/*
	 * There must be a current disk type and a current disk
	 */
	if (cur_dtype == NULL) {
		err_print("Current Disk Type is not set.\n");
		return (-1);
	}

	/*
	 * If disk is larger than 1TB then an EFI label is required
	 * and there is no point in running fdisk
	 */
	if (cur_dtype->capacity > INFINITY) {
		err_print("This disk must use an EFI label.\n");
		return (-1);
	}

	/*
	 * Before running the fdisk command, get file status of
	 *	/dev/rdsk/cn[tn]dnp0 path to see if this disk
	 *	supports fixed disk partition table.
	 */
	(void) get_pname(&pbuf[0]);
	if (stat(pbuf, (struct stat *)&statbuf) == -1 ||
	    !S_ISCHR(statbuf.st_mode)) {
		err_print(
		"Disk does not support fixed disk partition table\n");
		return (0);
	}

	/*
	 * Run the fdisk program.
	 */
	(void) snprintf(buf, sizeof (buf), "fdisk %s\n", pbuf);
	(void) system(buf);

	/*
	 * Open cur_file with "p0" path for accessing the fdisk table
	 */
	(void) open_cur_file(FD_USE_P0_PATH);

	/*
	 * Get solaris partition information in the fdisk partition table
	 */
	if (get_solaris_part(cur_file, &cur_disk->fdisk_part) == -1) {
		err_print("No fdisk solaris partition found\n");
		cur_disk->fdisk_part.numsect = 0;  /* No Solaris */
	}

	/*
	 * Restore cur_file with cur_disk->disk_path
	 */
	(void) open_cur_file(FD_USE_CUR_DISK_PATH);

	return (0);
}

/*
 * Read MBR on the disk
 * if the Solaris partition has changed,
 *	reread the vtoc
 */
#ifdef DEADCODE
static void
update_cur_parts()
{

	int i;
	register struct partition_info *parts;

	for (i = 0; i < NDKMAP; i++) {
#if defined(_SUNOS_VTOC_16)
		if (cur_parts->vtoc.v_part[i].p_tag &&
			cur_parts->vtoc.v_part[i].p_tag != V_ALTSCTR) {
			cur_parts->vtoc.v_part[i].p_start = 0;
			cur_parts->vtoc.v_part[i].p_size = 0;

#endif
			cur_parts->pinfo_map[i].dkl_nblk = 0;
			cur_parts->pinfo_map[i].dkl_cylno = 0;
			cur_parts->vtoc.v_part[i].p_tag =
				default_vtoc_map[i].p_tag;
			cur_parts->vtoc.v_part[i].p_flag =
				default_vtoc_map[i].p_flag;
#if defined(_SUNOS_VTOC_16)
		}
#endif
	}
	cur_parts->pinfo_map[C_PARTITION].dkl_nblk = ncyl * spc();

#if defined(_SUNOS_VTOC_16)
	/*
	 * Adjust for the boot partitions
	 */
	cur_parts->pinfo_map[I_PARTITION].dkl_nblk = spc();
	cur_parts->pinfo_map[I_PARTITION].dkl_cylno = 0;
	cur_parts->vtoc.v_part[C_PARTITION].p_start =
		cur_parts->pinfo_map[C_PARTITION].dkl_cylno * nhead * nsect;
	cur_parts->vtoc.v_part[C_PARTITION].p_size =
		cur_parts->pinfo_map[C_PARTITION].dkl_nblk;

	cur_parts->vtoc.v_part[I_PARTITION].p_start =
			cur_parts->pinfo_map[I_PARTITION].dkl_cylno;
	cur_parts->vtoc.v_part[I_PARTITION].p_size =
			cur_parts->pinfo_map[I_PARTITION].dkl_nblk;

#endif	/* defined(_SUNOS_VTOC_16) */
	parts = cur_dtype->dtype_plist;
	cur_dtype->dtype_ncyl = ncyl;
	cur_dtype->dtype_plist = cur_parts;
	parts->pinfo_name = cur_parts->pinfo_name;
	cur_disk->disk_parts = cur_parts;
	cur_ctype->ctype_dlist = cur_dtype;

}
#endif /* DEADCODE */

static int
get_solaris_part(int fd, struct ipart *ipart)
{
	int		i;
	struct ipart	ip;
	int		status;
	char		*bootptr;

	(void) lseek(fd, 0, 0);
	status = read(fd, (caddr_t)&boot_sec, NBPSCTR);

	if (status != NBPSCTR) {
		err_print("Bad read of fdisk partition. Status = %x\n", status);
		err_print("Cannot read fdisk partition information.\n");
		return (-1);
	}

	for (i = 0; i < FD_NUMPART; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &boot_sec.parts[ipc];
		(void) fill_ipart(bootptr, &ip);

		/*
		 * we are interested in Solaris and EFI partition types
		 */
		if (ip.systid == SUNIXOS ||
		    ip.systid == SUNIXOS2 ||
		    ip.systid == EFI_PMBR) {
			/*
			 * if the disk has an EFI label, nhead and nsect may
			 * be zero.  This test protects us from FPE's, and
			 * format still seems to work fine
			 */
			if (nhead != 0 && nsect != 0) {
				pcyl = lel(ip.numsect) / (nhead * nsect);
				xstart = lel(ip.relsect) / (nhead * nsect);
				ncyl = pcyl - acyl;
			}
#ifdef DEBUG
			else {
				err_print("Critical geometry values are zero:\n"
					"\tnhead = %d; nsect = %d\n", nhead,
					nsect);
			}
#endif /* DEBUG */

			solaris_offset = lel(ip.relsect);
			break;
		}
	}

	if (i == FD_NUMPART) {
		err_print("Solaris fdisk partition not found\n");
		return (-1);
	}

	/*
	 * compare the previous and current Solaris partition
	 * but don't use bootid in determination of Solaris partition changes
	 */
	ipart->bootid = ip.bootid;
	status = bcmp(&ip, ipart, sizeof (struct ipart));

	bcopy(&ip, ipart, sizeof (struct ipart));

	/* if the disk partitioning has changed - get the VTOC */
	if (status) {
		status = ioctl(fd, DKIOCGVTOC, &cur_parts->vtoc);
		if (status == -1) {
			i = errno;
			err_print("Bad ioctl DKIOCGVTOC.\n");
			err_print("errno=%d %s\n", i, strerror(i));
			err_print("Cannot read vtoc information.\n");
			return (-1);
		}
	}
	return (0);
}


int
copy_solaris_part(struct ipart *ipart)
{

	int		status, i, fd;
	struct mboot	mboot;
	struct ipart	ip;
	char		buf[MAXPATHLEN];
	char		*bootptr;
	struct stat	statbuf;

	(void) get_pname(&buf[0]);
	if (stat(buf, &statbuf) == -1 ||
	    !S_ISCHR(statbuf.st_mode) ||
	    ((cur_label == L_TYPE_EFI) &&
		(cur_disk->disk_flags & DSK_LABEL_DIRTY))) {
		/*
		 * Make sure to reset solaris_offset to zero if it is
		 *	previously set by a selected disk that
		 *	supports the fdisk table.
		 */
		solaris_offset = 0;
		/*
		 * Return if this disk does not support fdisk table or
		 * if it uses an EFI label but has not yet been labelled.
		 * If the EFI label has not been written then the open
		 * on the partition will fail.
		 */
		return (0);
	}

	if ((fd = open(buf, O_RDONLY)) < 0) {
		err_print("Error: can't open disk '%s'.\n", buf);
		return (-1);
	}

	status = read(fd, (caddr_t)&mboot, sizeof (struct mboot));

	if (status != sizeof (struct mboot)) {
		err_print("Bad read of fdisk partition.\n");
		(void) close(fd);
		return (-1);
	}

	for (i = 0; i < FD_NUMPART; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &mboot.parts[ipc];
		(void) fill_ipart(bootptr, &ip);

		if (ip.systid == SUNIXOS ||
		    ip.systid == SUNIXOS2 ||
		    ip.systid == EFI_PMBR) {
			solaris_offset = lel(ip.relsect);
			bcopy(&ip, ipart, sizeof (struct ipart));

			/*
			 * if the disk has an EFI label, we typically won't
			 * have values for nhead and nsect.  format seems to
			 * work without them, and we need to protect ourselves
			 * from FPE's
			 */
			if (nhead != 0 && nsect != 0) {
				pcyl = lel(ip.numsect) / (nhead * nsect);
				ncyl = pcyl - acyl;
			}
#ifdef DEBUG
			else {
				err_print("Critical geometry values are zero:\n"
					"\tnhead = %d; nsect = %d\n", nhead,
					nsect);
			}
#endif /* DEBUG */

			break;
		}
	}

	(void) close(fd);
	return (0);

}

#if defined(_FIRMWARE_NEEDS_FDISK)
int
auto_solaris_part(struct dk_label *label)
{

	int		status, i, fd;
	struct mboot	mboot;
	struct ipart	ip;
	char		buf[80];
	char		*bootptr;

	(void) snprintf(buf, sizeof (buf), "/dev/rdsk/%sp0", x86_devname);
	if ((fd = open(buf, O_RDONLY)) < 0) {
		err_print("Error: can't open selected disk '%s'.\n", buf);
		return (-1);
	}
	status = read(fd, (caddr_t)&mboot, sizeof (struct mboot));

	if (status != sizeof (struct mboot)) {
		err_print("Bad read of fdisk partition.\n");
		return (-1);
	}

	for (i = 0; i < FD_NUMPART; i++) {
		int	ipc;

		ipc = i * sizeof (struct ipart);

		/* Handling the alignment problem of struct ipart */
		bootptr = &mboot.parts[ipc];
		(void) fill_ipart(bootptr, &ip);

		/*
		 * if the disk has an EFI label, the nhead and nsect fields
		 * the label may be zero.  This protects us from FPE's, and
		 * format still seems to work happily
		 */
		if (ip.systid == SUNIXOS ||
		    ip.systid == SUNIXOS2 ||
		    ip.systid == EFI_PMBR) {
			if ((label->dkl_nhead != 0) &&
			    (label->dkl_nsect != 0)) {
				label->dkl_pcyl = lel(ip.numsect) /
				    (label->dkl_nhead * label->dkl_nsect);
				label->dkl_ncyl = label->dkl_pcyl -
				    label->dkl_acyl;
			}
#ifdef DEBUG
			else {
				err_print("Critical label fields aren't "
					"non-zero:\n"
					"\tlabel->dkl_nhead = %d; "
					"label->dkl_nsect = "
					"%d\n", label->dkl_nhead,
					label->dkl_nsect);
			}
#endif /* DEBUG */

		solaris_offset = lel(ip.relsect);
		break;
		}
	}

	(void) close(fd);

	return (0);
}
#endif	/* defined(_FIRMWARE_NEEDS_FDISK) */


int
good_fdisk()
{
	char		buf[MAXPATHLEN];
	struct stat	statbuf;

	(void) get_pname(&buf[0]);
	if (stat(buf, &statbuf) == -1 ||
	    !S_ISCHR(statbuf.st_mode) ||
	    cur_label == L_TYPE_EFI) {
		/*
		 * Return if this disk does not support fdisk table or
		 * if the disk is labeled with EFI.
		 */
		return (1);
	}

	if (lel(cur_disk->fdisk_part.numsect) > 0)
		return (1);
	else
		return (0);
}
