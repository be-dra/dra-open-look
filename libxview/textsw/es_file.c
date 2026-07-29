#ifndef lint
char     es_file_c_sccsid[] = "@(#)es_file.c 20.49 93/06/28 DRA: $Id: es_file.c,v 4.8 2026/07/29 03:53:56 dra Exp $ ";
#endif

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE
 *	file for terms of the license.
 */

/*
 * Entity stream implementation for disk files.
 *
 * A file is acting as one of three kinds of streams: 1) a read-only source of
 * characters (an "original" stream for ps_impl.c) or, 2) as a write_only
 * sink of characters (a backup or checkpoint), or 3) as a read-write edit
 * history (a "scratch" stream). A scratch stream can get VERY large, and
 * could choose to "wrap around" and re-use the bytes of the underlying file
 * to conserve file space. However, since ps_impl.c must support large
 * scratch streams for memory streams as well, it implements the wrap-around,
 * thereby changing the expected access pattern to the stream implemented in
 * this module.
 *
 * Based on the above, the implementation uses two buffers: 1) a read buffer,
 * that contains the current insertion point and some portion of the
 * characters before and after it, and 2) a write buffer, that contains the
 * last point at which more than 4 bytes were written at once, plus some of
 * the characters around it. This peculiar requirement for more than 4 bytes
 * is because ps_impl.c keeps updating the count of the number of characters
 * in the last contiguous insertion sequence. The read buffer always matchs
 * an existing portion of the file as it exists on disk, but the write buffer
 * can be "off the end", containing characters that have not yet been sent to
 * disk.  Thus, there can be valid indices in [length_on_disk..length) that
 * are not valid positions for the underlying file! For an original stream,
 * the write buffer is NULL, and length == length_on_disk.
 *
 * --- Misc. notes that may result in changes ... --- An empty original stream
 * need not be open except to prevent another process or some piece of client
 * code ripping it out from under this module. However, delayed opening moves
 * where certain error conditions have to be handled by clients. A original
 * stream pointed at a non-existent file could treat it as auto-creation of
 * an empty stream, and then act as appropriate to an empty stream.
 *
 *
 * Considerations for file consistency: Sun Unix 3.X (and BSD 4.X): For a local
 * file system, write(2) does not report success unless there is space
 * available on the disk for the data and write(2) claims that space. For a
 * NFS file system, write(2) returns as soon as it has transferred all of the
 * user data into kernel buffers.  There need not be enough space for that
 * data on the remote disk, and only a successful fsync(2) guarantees that
 * the data is on the remote disk.  If the fsync(2) fails, there is no
 * indication of which data did not make it to the disk! Sun Unix 4.X (the
 * VM-rewrite): With mapped files, the local file system may have the same
 * problem as the NFS file system. NFS replacing ND: For diskless clients,
 * the local file system has the same problem as the NFS file system unless
 * it is using the (optional) RAM-disk /tmp. AT&T System V R ? There is a
 * notion of "synchronous" files, where the write(2) does not report success
 * until the data is on the disk.  SunOS 4.X provides this feature, but
 * "synchronous" files completely bypass the kernel disk cache, and thus
 * significantly slow read(2) as well as write(2). stdio fwrite(3) is not
 * guarantee to write(2) unless the stream is unbuffered. fflush(3) forces
 * the write(2), but does not provide enough information to caller for it to
 * figure out what did not get written.  Worse yet, fseek(3) can call
 * fflush(3) as a side-effect, and does not even report an error if the
 * fflush(3) fails!
 *
 * --- And now for some history ... --- Prior to "-r 10.12", es_file used stdio
 * and only needed to work for 3.X versions of the SunOS.  The following
 * strategy was employed. To get around the delayed nature of the calls to
 * write(2), we force ES_WRITE_BUF_LEN > BUFSIZE, forcing all full-buffer
 * fwrite(3) calls to call write(2) immediately.  (We don't just want to make
 * the stream unbuffered because we need the buffering for reading).  Since
 * the only remaining partial calls to fwrite(3) should be in es_commit and
 * es_destroy, this reduces the number of places that have to be very careful
 * about disk consistency to entity_stream shutdown and es_replace callers.
 *
 * Other problems with using stdio: Since stdio may be looking at stdin (or some
 * other file that is being asynchronously extended), stdio does a lot of
 * lseek(2) calls to see if the file has been so extended.  For the textsw,
 * this is unnecessary and costly functionality. Stdio interacts poorly with
 * ps_impl.c, because fseek fflush's the writable scratch file a lot.
 */

#include <string.h>
#include <fcntl.h>
#ifdef SVR4
#include <stdlib.h>
#include <dirent.h>
#else
#include <sys/dir.h>
#endif /* SVR4 */
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview_private/primal.h>
#include <xview_private/es.h>
#include <xview/generic.h>
#include <xview/server.h>
#include <xview_private/txt_18impl.h>

extern char *xv_app_name;
#define XX(_a_) fprintf(stderr, "%s`%s`%s-%d: %s", xv_app_name, __FILE__, __FUNCTION__, __LINE__, _a_)
		
static Es_status es_file_commit(Es_handle esh);
static Es_handle es_file_destroy(Es_handle esh);
static Es_index es_file_get_length(Es_handle esh);
static Es_index es_file_get_position( Es_handle esh);
static Es_index es_file_set_position(Es_handle esh, register Es_index pos);
static Es_index es_file_read(Es_handle esh, int count, char *buf, int *count_read);
static Es_index es_file_replace(Es_handle esh, Es_index last_plus_one, int count, char *buf, int *count_used);
static int es_file_set(Es_handle esh, Attr_avlist attrs);

Pkg_private int es_copy_status(char *to, int fold, int *from_mode);

static struct es_ops es_file_ops = {
    es_file_commit,
    es_file_destroy,
    es_file_get,
    es_file_get_length,
    es_file_get_position,
    es_file_set_position,
    es_file_read,
    es_file_replace,
    es_file_set
};

typedef struct _es_file_buf {
    Es_index        start;	/* Disk position, valid iff used > 0 */
    unsigned        used;	/* # valid chars in buf */
    char           *chars;
} es_file_buf;
typedef es_file_buf *Es_file_buf;
#define	BUF_INVALIDATE(_buf)	(_buf)->used = 0
#define	BUF_LAST_PLUS_ONE(_buf)	((_buf)->start + (_buf)->used)
#define	BUF_CONTAINS_POS(_buf, _pos)					\
	((_buf)->used > 0 &&						\
	(_buf)->start <= (_pos) && (_pos) < (int)BUF_LAST_PLUS_ONE(_buf))

struct es_private_data {
    Es_status       status;
    char           *name;
#ifndef BACKUP_AT_HEAD_OF_LINK
    char           *true_name;	/* Non-null iff name was sym link */
#endif
    unsigned        flags, options;
    Xv_opaque       client_data;
    Es_index        length, length_on_disk, pos;
    int             fd;
#ifdef obsolete
    FILE           *file;
#endif
    es_file_buf     read_buf;	/* cache for read's */
    es_file_buf     write_buf;	/* cache for replace's */
};
typedef struct es_private_data *Es_file_data;

/* Bits for flags */
#define COMMIT_DONE	0x00000001

#define	ABS_TO_REP(esh)	(Es_file_data)esh->data


/*
 * Some invariants for read_buf and write_buf: 1) The buffers are allowed to
 * overlap, but read's must retrieve from the write_buf first, to ensure that
 * the client reads what it wrote.
 */

static char    *file_name_only_msgs[] = {
     /* 0 */ 0,
     /* 1 */ 0,
     /* 2 */ 0,
     /* 3 */ 0,
     /* 4 */ 0,
     /* 5 */ 0
};

/*
   added a static routine for checking if the start to end overlaps with
   the read buffer.  If it does, then invalidate the read buffer.
   XXX: Should actually update the read buffer to reflect the change in
   the buf.
*/

static void update_read_buf(Es_file_data private, Es_index start, Es_index end,
							char *buf)
{
	if (private->read_buf.used > 0 &&
			(unsigned)start < BUF_LAST_PLUS_ONE(&private->read_buf) &&
			end > private->read_buf.start) {
		/*
		 * There is overlap: in future, might be better to update
		 * read_buf, but for now, just discard it.
		 */
		BUF_INVALIDATE(&private->read_buf);
	}
}

Pkg_private void es_file_append_error(char *error_buf, char *file_name, Es_status status)
/* Messages appended to error_buf have no trailing newline */
{
	register char *first_free_in_buf;
	register int msg_index = 0;
	static int init_msg = 0;

	if (error_buf == 0)
		return;	/* Caller is fouled up. */

	if (!init_msg) {
		file_name_only_msgs[0] = XV_MSG("cannot read file '%s'");
		file_name_only_msgs[1] = XV_MSG("'%s' does not exist");
		file_name_only_msgs[2] = XV_MSG("not permitted to access '%s'");
		file_name_only_msgs[3] = XV_MSG("'%s' is not a text file");
		file_name_only_msgs[4] = XV_MSG("too many symbolic links from '%s'");
		file_name_only_msgs[5] = XV_MSG("out of space for file '%s'");
		init_msg = 1;
	}

	first_free_in_buf = error_buf + strlen(error_buf);
	if (status & ES_CLIENT_STATUS(0)) {
		(void)sprintf(first_free_in_buf,
				XV_MSG("INTERNAL error for file '%s', status is %ld"),
				file_name, status);
		return;
	}
	switch (ES_BASE_STATUS(status)) {
		case ES_SUCCESS:
			break;	/* Caller is REALLY lazy! */
		case ES_CHECK_ERRNO:
			switch (errno) {
				case ENOENT:
					msg_index = 1;
					goto Default;
				case EACCES:
					msg_index = 2;
					goto Default;
				case EISDIR:
					msg_index = 3;
					goto Default;

#ifndef SVR4
				case ELOOP:
					msg_index = 4;
					goto Default;
#endif /* SVR4 */

				case ENOMEM:
					strcat(error_buf, XV_MSG("alloc failure"));
					break;
				default:
					sprintf(first_free_in_buf, XV_MSG("file '%s': %s"),
							file_name, strerror(errno));
					break;
			}
			break;
		case ES_INVALID_HANDLE:
			(void)strcat(error_buf, XV_MSG("invalid es_handle"));
			break;
		case ES_SEEK_FAILED:
			(void)strcat(error_buf, XV_MSG("seek failed"));
			break;
		case ES_FLUSH_FAILED:
		case ES_FSYNC_FAILED:
		case ES_SHORT_WRITE:
			msg_index = 5;
			goto Default;
		default:
		  Default:
			(void)sprintf(first_free_in_buf, file_name_only_msgs[msg_index],
					file_name);
	}
}

Pkg_private Es_handle es_file_create(char *name, int options, Es_status *status)
{
	Es_handle esh = (Es_object *) calloc((size_t)1, sizeof(Es_object));
	Es_file_data private = 0;
	int open_option;
	struct stat buf;
	Es_status dummy_status;

#ifndef BACKUP_AT_HEAD_OF_LINK
	char *temp_name, true_name[MAXNAMLEN];
	int link_count, true_name_len;
#endif

	if (status == 0)
		status = &dummy_status;
	*status = ES_CHECK_ERRNO;
	errno = 0;

	/* (1) Try to allocate all necessary memory */
	if (esh == NULL)
		goto AllocFailed;
	if ((private = (struct es_private_data *)calloc((size_t)1,
							sizeof(struct es_private_data))) == NULL)
		goto AllocFailed;
	private->fd = -1;	/* In case of later AllocFailed */
	BUF_INVALIDATE(&private->read_buf);

	if ((private->read_buf.chars = MALLOC((long)ES_READ_BUF_LEN)) == NULL)
		goto AllocFailed;
	BUF_INVALIDATE(&private->write_buf);
	if (options & ES_OPT_APPEND) {
		if ((private->write_buf.chars = MALLOC((long)ES_WRITE_BUF_LEN)) == NULL)
			goto AllocFailed;
	}
	else {
		private->write_buf.chars = NULL;
	}
	if ((private->name = strdup(name)) == NULL)
		goto AllocFailed;

#ifndef BACKUP_AT_HEAD_OF_LINK
	/* (2) Chase the symbolic link if 'name' is one. */
	for (temp_name = name, link_count = 0;
			(link_count < MAXSYMLINKS) &&
			(-1 != (true_name_len =
							readlink(temp_name, true_name, sizeof(true_name))));
			temp_name = true_name, link_count++) {
		true_name[true_name_len] = '\0';
	}
	if (link_count == MAXSYMLINKS) {
		errno = ELOOP;
		goto Error_Return;
	}
	if (temp_name == name) {
		private->true_name = NULL;
	}
	else
		private->true_name = strdup(true_name);
#endif /* BACKUP_AT_HEAD_OF_LINK */

	/* (3) Open up the file and check to see it is not directory. */
	open_option = (options & ES_OPT_APPEND)
			? (O_RDWR | O_TRUNC | O_CREAT)
			: (O_RDONLY);
	private->fd = open(name, open_option, 0666);
	if (private->fd < 0) {
		goto Error_Return;
	}
	private->flags = 0;
	private->options = options;
	if ((private->options & ES_OPT_APPEND) == 0) {
		if (fstat(private->fd, &buf) == -1)
			goto Error_Return;
		if ((buf.st_mode & S_IFMT) != S_IFREG) {
			errno = EISDIR;
			goto Error_Return;
		}
		private->length = buf.st_size;
	}
	/* (4) Final fix ups. */

	private->length_on_disk = private->length;
	esh->ops = &es_file_ops;
	esh->data = (caddr_t) private;

	*status = private->status = ES_SUCCESS;
	return (esh);

  AllocFailed:
	errno = ENOMEM;
  Error_Return:
	if (esh) {
		free((char *)esh);
		esh = ES_NULL;
	}
	if (private) {
		if (private->read_buf.chars)
			free(private->read_buf.chars);
		if (private->write_buf.chars)
			free(private->write_buf.chars);
		if (private->fd >= 0)
			(void)close(private->fd);
		free((char *)private);
		private = (Es_file_data) 0;
	}
	return (esh);
}

/* ARGSUSED */
caddr_t
#ifdef ANSI_FUNC_PROTO
es_file_get(Es_handle esh, Es_attribute attribute, ...)
#else
es_file_get(esh, attribute, va_alist)
    Es_handle       esh;
    Es_attribute    attribute;
va_dcl
#endif
{
	register Es_file_data private = ABS_TO_REP(esh);

#ifndef lint
/*     va_list         args; */
#endif

	switch (attribute) {
		case ES_CLIENT_DATA:
			return ((caddr_t) (private->client_data));
		case ES_NAME:
			return ((caddr_t) (private->name));
		case ES_STATUS:
			return ((caddr_t) (private->status));
		case ES_SIZE_OF_ENTITY:
			return ((caddr_t) sizeof(char));
		case ES_TYPE:
			return ((caddr_t) ES_TYPE_FILE);
		case ES_SKIPPED:
			return ((caddr_t) 0);
		default:
			return (0);
	}
}

static int es_file_set(Es_handle esh, Attr_avlist attrs)
{
    register Es_file_data private = ABS_TO_REP(esh);
    Es_status       status_dummy = ES_SUCCESS;
    register Es_status *status = &status_dummy;

    for (; *attrs && (*status == ES_SUCCESS); attrs = attr_next(attrs)) {
	switch ((Es_attribute) * attrs) {
	  case ES_CLIENT_DATA:
	    private->client_data = attrs[1];
	    break;
	  case ES_FILE_MODE:
	    if (fchmod(private->fd, (unsigned)attrs[1]) == -1)
		*status = private->status = ES_CHECK_ERRNO;
	    break;
	  case ES_STATUS:
	    private->status = (Es_status) attrs[1];
	    break;
	  case ES_STATUS_PTR:
	    status = (Es_status *) attrs[1];
	    *status = status_dummy;
	    break;
	  default:
	    *status = ES_INVALID_ATTRIBUTE;
	    break;
	}
    }
    return ((*status == ES_SUCCESS));
}

/* ARGSUSED */
static int es_file_seek(Es_file_data private, Es_index pos, char *caller)
{

#ifdef DEBUG
	if (private->length_on_disk < pos) {
		private->status = ES_SEEK_FAILED;
		(void)fprintf(stderr,
				"%s: lseek to position %d > length_on_disk %d!!\n",
				caller, pos, private->length_on_disk);
		return (1);
	}
#endif

	if (lseek(private->fd, (off_t) pos, L_SET) == -1) {
		private->status = ES_SEEK_FAILED;

#ifdef DEBUG
		(void)fprintf(stderr, "Bad lseek in %s to position %d\n", caller, pos);
#endif

		return (1);
	}
	else {
		return (0);
	}
}

static long loop_counter;

static ssize_t es_file_fill_buf(Es_file_data private, Es_file_buf buf,
							Es_index first, Es_index last_plus_one)
{	   /* Ref jhfgewewr *//* Ref jhfgewewr */
	ssize_t read_in;

if (loop_counter >= 100 && loop_counter <= 105) {
	fprintf(stderr, "es_file_fill_buf f=%d lpo=%d\n", first, last_plus_one);
}

	if (first < last_plus_one) {	/* Ref jhfgewewr */ /* Ref jhfgewewr */
		if (es_file_seek(private, first, "es_file_fill_buf")) {
			read_in = -1;
			return read_in;
		}

		read_in = read(private->fd, buf->chars, (size_t)(last_plus_one - first));

		if (read_in == -1 || read_in != last_plus_one - first /* paranoia */ ) {
			private->status = ES_CHECK_ERRNO;
			read_in = -2;

#ifdef DEBUG
			(void)fprintf(stderr,
					"Failed read in %s of %d chars\n",
					"es_file_fill_buf", last_plus_one - first);
#endif

			return read_in;
		}
	}
	else {
		read_in = 0;

#ifdef DEBUG
		if (first != private->length)
			(void)fprintf(stderr,
					"Null read in %s at %d with length %d\n",
					"es_file_fill_buf", first, private->length);
#endif
	}
	buf->start = first;
	buf->used = read_in;
	return read_in;
}

static int es_file_flush_write_buf(Es_file_data private, Es_file_buf buf)
/*
 * This routine detects errors in attempted write, etc. but does not allow
 * for successful retry in all cases (e.g., short writes or failed fsynch).
 */
{
	register int written;

	if (buf->used == 0) {
		written = 0;
		return (written);
	}

	if (es_file_seek(private, buf->start, "es_file_flush_write_buf")) {
		return (-1);
	}
	written = write(private->fd, buf->chars, (size_t)buf->used);
	if (written == -1 || written != (int)buf->used /* paranoia */ ) {
		private->status = ES_SHORT_WRITE;	/* ES_FLUSH_FAILED instead? */
		written = -2;

#ifdef DEBUG
		(void)fprintf(stderr,
				"Failed write in %s of %d chars\n",
				"es_file_flush_write_buf", buf->used);
#endif

		return (written);
	}

	if (buf->start + written > private->length_on_disk)
		private->length_on_disk = buf->start + written;
	BUF_INVALIDATE(buf);
	return (written);
}

static int es_file_move_write_buf(Es_file_data private, Es_index include, Es_index also_include, char **include_offset)
/*
 * Caller ensures: include <= also_include < include+ES_WRITE_BUF_LEN <
 * ES_INFINITY, include <= private->length Return values: < 0 indicate
 * various errors, = 0 implies write_buf already correctly positioned, > 0
 * implies write_buf written then moved. Assumptions: if you have to write
 * any bytes, you might as well write them all. if you have to read any
 * bytes, you might as well read them all.
 */
{
    register Es_file_buf buf = &private->write_buf;
    register int    written = 0;
    Es_index        buf_last_plus_one;

    if (buf->used == 0)
	goto Fill_Buffer;
    buf_last_plus_one = BUF_LAST_PLUS_ONE(buf);
#ifdef XV_DEBUG
    if (buf_last_plus_one > private->length)
	take_breakpoint();
#endif
    /*
     * Buffer must contain include and also_include. There are two possible
     * problems, even if include is contained: 1) include is too far into the
     * buffer for also_include to also be contained, or 2) there is space
     * available in the buffer for also_include, but the buffer needs to fill
     * in bytes from the disk between its current end and also_include. If
     * include is at buffer end, or also_include is at or beyond buffer end,
     * it may be possible to simply extend the buffer iff buffer is at the
     * end of the stream and it has room left.
     */
    if (include < buf->start || include > buf_last_plus_one ||
	(include == buf_last_plus_one &&
	 include >= buf->start + ES_WRITE_BUF_LEN) ||
	(also_include >= buf_last_plus_one &&
	 (buf_last_plus_one < private->length ||
	  also_include >= buf->start + ES_WRITE_BUF_LEN))
	) {
	written = es_file_flush_write_buf(private, &private->write_buf);
	if (written < 0)
	    return (written);
	/*
	 * Possible future optimizations: 1) Deal with buf_last_plus_one <=
	 * also_include < buf->start+ES_WRITE_BUF_LEN 2) Slide buffer around to
	 * avoid full read when possible 3) Copy from read_buf to avoid full
	 * read when possible
	 */
Fill_Buffer:
	if (es_file_fill_buf(private, buf, include,
			   include + ES_WRITE_BUF_LEN > private->length_on_disk
		 ? private->length_on_disk : include + ES_WRITE_BUF_LEN) < 0L) {
	    written = -4;
	    return (written);
	}
    }
    *include_offset = buf->chars + (include - buf->start);
    return (written);
}

static void es_file_maybe_truncate_buf(Es_file_buf buf, Es_index new_last_plus_one)
{
	if (buf->used > 0 && new_last_plus_one < (int)BUF_LAST_PLUS_ONE(buf)) {
		buf->used = (new_last_plus_one < buf->start)
				? 0 : new_last_plus_one - buf->start;
	}
}

static Es_status es_file_commit(Es_handle esh)
{
    register Es_file_data private = ABS_TO_REP(esh);

    if (es_file_flush_write_buf(private, &private->write_buf) < 0) {
	return (private->status);
    }
    if (fsync(private->fd) == -1) {
#ifdef DEBUG
    (void) fprintf(stderr,
       "Failed fsynch in es_file_commit!\n");
#endif
       /* BUG ALERT!  Who knows what state the file is in? */
       return (ES_FSYNC_FAILED);
    }
    private->flags |= COMMIT_DONE;
    return (ES_SUCCESS);
}


static Es_handle es_file_destroy(Es_handle esh)
{
	register Es_file_data private = ABS_TO_REP(esh);

	if (private->write_buf.chars) {

#ifdef XV_DEBUG
		if ((private->write_buf.used > 0) && (private->flags & COMMIT_DONE)) {
			/*
			 * Caller should have called es_commit in order to guarantee
			 * appropriate recovery in case of errors.
			 */
			take_breakpoint();
		}
#endif

		free(private->write_buf.chars);
	}
	(void)close(private->fd);
	private->fd = -1;

	if ((private->options & ES_OPT_APPEND) &&
			(private->flags & COMMIT_DONE) == 0) {
		(void)unlink(private->name);
	}

	free((char *)esh);
	free(private->read_buf.chars);

#ifndef BACKUP_AT_HEAD_OF_LINK
	free((char *)private->true_name);
#endif

	free(private->name);
	free((char *)private);
	return (NULL);
}

static          Es_index es_file_get_length(Es_handle esh)
{
    register Es_file_data private = ABS_TO_REP(esh);
    return (private->length);
}

static Es_index es_file_get_position( Es_handle esh)
{
    register Es_file_data private = ABS_TO_REP(esh);
    return (private->pos);
}

static Es_index es_file_set_position(Es_handle esh, register Es_index pos)
{
    register Es_file_data private = ABS_TO_REP(esh);
    if (pos > private->length)
	pos = private->length;
    private->pos = pos;
    return (private->pos);
}

static void catch_looper(int lala)
{
	char buf[200];

	sprintf(buf, "%s: loop (%ld) in es_file_read caught, dumping core\n",
				xv_app_name, loop_counter);
	write(2, buf, strlen(buf));
	abort();
}

static Es_index es_file_read(Es_handle esh, int count, char *buf,
									int *count_read)
/*
 * Needed characters may be in the read_buf, the write_buf, or on disk, or
 * some combination of all three.
 */
{
	Es_file_data private = ABS_TO_REP(esh);
	Es_index pos = private->pos, lpo;
	int to_read, still_needed;
	es_file_buf dummy_read_buf;
__sighandler_t origsig;

	/*
	 * Client may request more bytes than are available, so count cannot be
	 * trusted in the following code.
	 */
	*count_read = (count > private->length - pos)
						? (private->length - pos)
						: count;

/* hier kamen wir in der 64-Bit-Version in eine (vermutlich)
 * Endlosschleife - die wollen wir fangen.
 * Siehe auch textsw/txt_sel.c (Ref lkjetrfgklertg)
 */
origsig = signal(SIGALRM, catch_looper);
alarm(5);
loop_counter = 0;

	for (still_needed = *count_read; still_needed > 0;
			still_needed -= to_read, pos += to_read) {
++loop_counter;
		/*
		 * Figure out where the next set of characters is coming from. The
		 * write_buf has precedence over the read_buf in the tests so that
		 * overlap range reads from the write_buf!
		 */
		if (BUF_CONTAINS_POS(&private->write_buf, pos)) {	/* Ref jhfgewewr */
			to_read = BUF_LAST_PLUS_ONE(&private->write_buf) - pos;
			if (to_read > still_needed)
				to_read = still_needed;
			BCOPY(private->write_buf.chars + (pos - private->write_buf.start),
					buf + (*count_read - still_needed), (size_t)to_read);
		}
		else if (BUF_CONTAINS_POS(&private->read_buf, pos)) {
			to_read = BUF_LAST_PLUS_ONE(&private->read_buf) - pos;
			if (to_read > still_needed)
				to_read = still_needed;
			BCOPY(private->read_buf.chars + (pos - private->read_buf.start),
					buf + (*count_read - still_needed), (size_t)to_read);
		}
		else if (still_needed <= ES_READ_BUF_LEN) {
			/*
			 * Since we have to read from disk, might as well get as many
			 * characters as possible.
			 */
			lpo = pos + ES_READ_BUF_LEN;
			if (lpo > private->length_on_disk) lpo = private->length_on_disk;
			/* Overlap with write_buf is avoided for good hygiene. */
			if (private->write_buf.used > 0 &&
					pos < private->write_buf.start &&
					lpo > private->write_buf.start)
				lpo = private->write_buf.start;
			if (es_file_fill_buf(private, &private->read_buf, pos, lpo) < 0L) {
				*count_read = 0;
				pos = private->pos;
				goto Return;
			}
			/*
			 * Go around again and characters will get copied from read_buf
			 * in earlier part of loop.
			 */
			to_read = 0;
		}
		else {
			/* Read directly from the disk */
			dummy_read_buf.chars = buf + (*count_read - still_needed);
			lpo = pos + still_needed;
			if (lpo > private->length_on_disk)
				lpo = private->length_on_disk;
			/* Overlap with write_buf is forbidden. */
			if (private->write_buf.used > 0 && lpo > private->write_buf.start)
				lpo = private->write_buf.start;
			if (es_file_fill_buf(private, &dummy_read_buf, pos, lpo)	/* Ref jhfgewewr */
					< 0L) {
				*count_read = 0;
				pos = private->pos;
				goto Return;
			}
			to_read = dummy_read_buf.used;
if (loop_counter >= 100 && loop_counter <= 105) {
	fprintf(stderr, "es_file_read to_read=%d\n", to_read);
}
			if (to_read == 0) {
if (getenv("XVIEW_DEBUG")) XX("to_read==0, avoiding assertion and endless loops\n");
				break;
			}
/* 			assert(to_read != 0); */
		}
	}
  Return:

alarm(0);
signal(SIGALRM, origsig);

	private->pos = pos;
	return (pos);
}

/* Following enumeration details the three possible types of replace. */
typedef enum {
    esfr_truncate, esfr_overwrite, esfr_insert
}               Esfr_mode;

static Es_index es_file_replace(Es_handle esh, Es_index last_plus_one, int count, char *buf, int *count_used)
{
    register Es_file_data private = ABS_TO_REP(esh);
    register Esfr_mode mode;
    char           *offset;
    es_file_buf     dummy_write_buf;

    /* Ensure that the operation is consistent with the options. */
    if ((private->options & ES_OPT_APPEND) == 0) {
	private->status = ES_INCONSISTENT_POS;
#ifdef DEBUG
	(void) fprintf(stderr, "es_file_replace: read-only stream\n");
#endif
	/* Error */
	return (ES_CANNOT_SET);
    }
    if (private->pos < private->length) {
	if (last_plus_one <= private->length) {
	    mode = esfr_overwrite;
	} else {
	    mode = esfr_truncate;
	    if (count != 0) {
		private->status = ES_INVALID_ARGUMENTS;
#ifdef DEBUG
		(void) fprintf(stderr,
			       "%s: non-zero (%d) count in truncate\n",
			       "es_file_replace", count);
#endif
		/* Error */
		return (ES_CANNOT_SET);
	    }
	    if (private->pos < private->length_on_disk) {
		private->status = ES_INVALID_ARGUMENTS;
#ifdef DEBUG
		(void) fprintf(stderr,
			       "%s: truncate @ %d when length_on_disk %d\n",
			       "es_file_replace", private->pos,
			       private->length_on_disk);
#endif
		/* Error */
		return (ES_CANNOT_SET);
	    }
	}
	if ((private->options & ES_OPT_OVERWRITE) == 0 ||
	    ((mode == esfr_overwrite) &&
	     (count != last_plus_one - private->pos))) {
	    private->status = ES_INVALID_ARGUMENTS;
#ifdef DEBUG
	    (void) fprintf(stderr, "%s last_plus_one is %d, len is %d\n",
			   "es_file_replace position error:",
			   last_plus_one, private->length);
#endif
	    /* Error */
	    return (ES_CANNOT_SET);
	}
    } else {
	mode = esfr_insert;
    }

    /* Do the replace */
    if (mode == esfr_truncate) {
	es_file_maybe_truncate_buf(&private->read_buf, 0);	/* ??? */
	es_file_maybe_truncate_buf(&private->write_buf, 0);	/* ??? */
	*count_used = 0;
    } else if (mode == esfr_overwrite) {
	/*
	 * If new bytes will fit in the write_buf, position the write_buf to
	 * accomodate them (unless there are 4 or fewer bytes) and overwrite.
	 * Otherwise, flush out the write_buf and write the new bytes
	 * directly.
	 */
	if (count <= ES_WRITE_BUF_LEN) {
	    if (count < 5 &&
		(last_plus_one < private->write_buf.start ||
		 private->pos >= (int)BUF_LAST_PLUS_ONE(&private->write_buf))) {
		goto Write_Direct;
	    }
	    if (es_file_move_write_buf(private, private->pos,
				       last_plus_one, &offset) < 0) {
		/* Error */
		return (ES_CANNOT_SET);
	    }
	    BCOPY(buf, offset, (size_t)count);
	    *count_used = count;
	    update_read_buf(private,private->pos,private->pos+count,buf);
	} else {
	    goto Flush_And_Write_Direct;
	}
    } else {			/* mode == es_insert */
	/*
	 * Insert only happens at end-of-stream, thus it always increases
	 * private->write_buf.used by the size of the insertion. If new bytes
	 * will fit in the write_buf, position the write_buf to accomodate
	 * them and add them.  Otherwise, flush out the write_buf and write
	 * the new bytes directly.
	 */
	if (count <= ES_WRITE_BUF_LEN) {
	    if (es_file_move_write_buf(private, private->pos,
				       private->pos + count, &offset) < 0) {
		/* Error */
		return (ES_CANNOT_SET);
	    }
	    BCOPY(buf, offset, (size_t)count);
	    private->write_buf.used += count;
	    update_read_buf(private,private->pos,private->pos+count,buf);
	} else {
    Flush_And_Write_Direct:
	    if (es_file_flush_write_buf(private,
					&private->write_buf) < 0) {
		/* Error */
		return (ES_CANNOT_SET);
	    }
    Write_Direct:
	    /* Fake up a write buffer */
	    dummy_write_buf.used = count;
	    dummy_write_buf.start = private->pos;
	    dummy_write_buf.chars = buf;
	    if (es_file_flush_write_buf(private,
					&dummy_write_buf) <= 0) {
		/* Error */
		return (ES_CANNOT_SET);
	    }
	    /*
	     * Correct overlap with the read_buf that is not masked by
	     * overlap with the write_buf.
	     */
	    if (private->read_buf.used > 0 &&
		private->pos < (Es_index)BUF_LAST_PLUS_ONE(&private->read_buf) &&
		private->read_buf.start < private->pos + count) {
		/*
		 * There is overlap: in future, might be better to update
		 * read_buf, but for now, just discard it.
		 */
		BUF_INVALIDATE(&private->read_buf);
	    }
	}
	*count_used = count;
    }

    private->pos += *count_used;
    if (mode != esfr_overwrite)
	private->length = private->pos;
    return (private->pos);
}

Pkg_private int es_file_copy_status(Es_handle esh, char *to)
{
    Es_file_data    private = ABS_TO_REP(esh);
    int             dummy;
    return (es_copy_status(to, private->fd, &dummy));
}

Pkg_private Es_handle es_file_make_backup(Es_handle esh, char *backup_pattern, Es_status *status)
/* Currently backup_pattern must be of the form "%s<suffix>" */
{
    register Es_file_data private;
    char            backup_name[MAXNAMLEN];
    int             fd, len, retrying = FALSE;
    Es_status       dummy_status;
    Es_handle       result;

    if (status == 0)
	status = &dummy_status;
    if ((esh == NULL) || (esh->ops != &es_file_ops)) {
	*status = ES_INVALID_HANDLE;
	return (NULL);
    }
    *status = ES_CHECK_ERRNO;
    errno = 0;
    private = ABS_TO_REP(esh);
#ifdef BACKUP_AT_HEAD_OF_LINK
    (void) SPRINTF(backup_name, backup_pattern, private->name);
#else
	if (backup_pattern[0] == '/') {
		char *nam = ((private->true_name) ? private->true_name : private->name);
    	char *ptr = strrchr(nam, (int)'/' );

		if (ptr) sprintf(backup_name, backup_pattern, ptr + 1);
		else sprintf(backup_name, backup_pattern, nam);
	}
	else {
    	SPRINTF(backup_name, backup_pattern,
					(private->true_name) ? private->true_name : private->name);
	}
#endif
    fd = private->fd;
    len = lseek(fd, (off_t)0, 1);
    if (lseek(fd, (off_t)0, 0) != 0)
	goto Lseek_Failed;
Retry:
    if (es_copy_fd(private->name, backup_name, fd) != 0) {
	if ((!retrying) && (errno == EACCES)) {
	    /*
	     * It may be that the backup_name is already taken by a file that
	     * cannot be overwritten, so try to remove it first.
	     */
	    if (unlink(backup_name) == 0) {
		retrying = TRUE;
		goto Retry;
	    }
	    if (errno == ENOENT)
		/*
		 * backup_name does not exist, so problem with es_copy_fd
		 * really is unfixable access error, which that needs to be
		 * reported to caller, so set errno back!
		 */
		errno = EACCES;
	}
	return (NULL);
    }
    if (lseek(fd, (off_t)len, 0) != len)
	goto Lseek_Failed;
    result = es_file_create(backup_name, 0, status);
    *status = ES_SUCCESS;
    return (result);

Lseek_Failed:
    *status = ES_SEEK_FAILED;
    return (NULL);
}

#ifdef HERE_ENDLESS_LOOP_KILLED

#4  es_file_fill_buf (private=0x1d6a1d0, buf=0x7ffd33696470, first=2120, 
    last_plus_one=20) at es_file.c:625
#5  0x00007fb1be611456 in es_file_read (esh=0x1d679e0, count=2096, 
    buf=0x7ffd33696600 "0ni3ý\177", count_read=0x7ffd336964fc)
    at es_file.c:930
#6  0x00007fb1be6a02e5 in ps_read (esh=0x1d67960, len=2096, 
    bufp=0x7ffd33696600 "0ni3ý\177", resultp=0x7ffd3369657c) at ps_impl.c:582

#12 0x00007fb1be53f64c in note_error (cl=30337392, fd=15) at process.c:142



#4  es_file_fill_buf (private=0x5fa880, buf=0x7ffdd5fd5080, first=6262, 
    last_plus_one=20) at es_file.c:614
#5  0x00007fb8a7195511 in es_file_read (esh=0x5fa860, count=2096, 
    buf=0x7ffdd5fd5260 "\001", count_read=0x7ffdd5fd5164) at es_file.c:956



#4  0x00007f3d193433e1 in es_file_read (esh=0x8dd3d0, count=2096, 
    buf=0x7fff285ad200 "", count_read=0x7fff285ad104) at es_file.c:915



#4  0x00007f01a0be4e64 in es_file_fill_buf (private=0x140f980, 
    buf=0x7ffe9371d460, first=6262, last_plus_one=20) at es_file.c:565
#5  0x00007f01a0be5528 in es_file_read (esh=0x140f960, count=2096, 
    buf=0x7ffe9371d640 "", count_read=0x7ffe9371d544) at es_file.c:957



#4  0x00007fefd42dce6e in es_file_fill_buf (private=0x23f3980, 
    buf=0x7ffc23548190, first=6262, last_plus_one=20) at es_file.c:567
#5  0x00007fefd42dd56a in es_file_read (esh=0x23f3960, count=2096, 
    buf=0x7ffc23548370 "", count_read=0x7ffc23548274) at es_file.c:962


#4  es_file_fill_buf (private=0x1b9c980, buf=0x7ffdcf73c250, first=6502, 
    last_plus_one=20) at es_file.c:629
#5  0x00007f88f59e056a in es_file_read (esh=0x1b9c960, count=2096, 
    buf=0x7ffdcf73c430 "¨L\231õ\210\177", count_read=0x7ffdcf73c334)
    at es_file.c:962


#4  0x00007f8a0fc75f3a in es_file_fill_buf (private=0xc81880, 
    buf=0x7ffd72ea1250, first=6262, last_plus_one=20) at es_file.c:630
#5  0x00007f8a0fc76629 in es_file_read (esh=0xc81860, count=2096, 
    buf=0x7ffd72ea1430 "¨¬Â\017\212\177", count_read=0x7ffd72ea1334)
    at es_file.c:971



#3  0x00007f30575b3592 in __assert_fail () from /lib64/libc.so.6
#4  0x00007f3057de267c in es_file_read (esh=0x1333960, count=2096, 
    buf=0x7fff16b61190 "\223\036", count_read=0x7fff16b61094) at es_file.c:982
#5  0x00007f3057e70ea4 in ps_scratch_read (esh=0x1333960, len=2096, 
    bufp=0x7fff16b61190 "\223\036", resultp=0x7fff16b61094) at ps_impl.c:309
#6  0x00007f3057e7148c in ps_read (esh=0x1334a00, len=2096, 
    bufp=0x7fff16b61190 "\223\036", resultp=0x7fff16b6110c) at ps_impl.c:560



#3  0x00007fc120266592 in __assert_fail () from /lib64/libc.so.6
#4  0x00007fc120a9567c in es_file_read (esh=0x988860, count=2096, 
    buf=0x7ffde8c1fcd0 "¨\234¤ Á\177", count_read=0x7ffde8c1fbd4)
    at es_file.c:982


#endif /* HERE_ENDLESS_LOOP_KILLED */
