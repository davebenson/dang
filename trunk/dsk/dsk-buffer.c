/* DSK - a library to write servers
    Copyright (C) 1999-2000 Dave Benson

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

    Contact:
        daveb@ffem.org <Dave Benson>
*/

/* Free blocks to hold around to avoid repeated mallocs... */
#define MAX_RECYCLED		16

/* Size of allocations to make. */
#define BUF_CHUNK_SIZE		32768

/* Max fragments in the iovector to writev. */
#define MAX_FRAGMENTS_TO_WRITE	16

/* This causes fragments not to be transferred from buffer to buffer,
 * and not to be allocated in pools.  The result is that stack-trace
 * based debug-allocators work much better with this on.
 *
 * On the other hand, this can mask over some abuses (eg stack-based
 * foreign buffer fragment bugs) so we disable it by default.
 */ 
#define DSK_DEBUG_BUFFER_ALLOCATIONS	0

#include <alloca.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "dsk-common.h"
#include "dsk-buffer.h"

/* --- DskBufferFragment implementation --- */
static inline int 
dsk_buffer_fragment_avail (DskBufferFragment *frag)
{
  return frag->buf_max_size - frag->buf_start - frag->buf_length;
}
static inline uint8_t *
dsk_buffer_fragment_start (DskBufferFragment *frag)
{
  return frag->buf + frag->buf_start;
}
static inline uint8_t *
dsk_buffer_fragment_end (DskBufferFragment *frag)
{
  return frag->buf + frag->buf_start + frag->buf_length;
}

/* --- DskBufferFragment recycling --- */
#if !DSK_DEBUG_BUFFER_ALLOCATIONS
static int num_recycled = 0;
static DskBufferFragment* recycling_stack = 0;

#endif

static DskBufferFragment *
new_native_fragment()
{
  DskBufferFragment *frag;
#if DSK_DEBUG_BUFFER_ALLOCATIONS
  frag = (DskBufferFragment *) g_malloc (BUF_CHUNK_SIZE);
  frag->buf_max_size = BUF_CHUNK_SIZE - sizeof (DskBufferFragment);
#else  /* optimized (?) */
  if (recycling_stack)
    {
      frag = recycling_stack;
      recycling_stack = recycling_stack->next;
      num_recycled--;
    }
  else
    {
      frag = (DskBufferFragment *) dsk_malloc (BUF_CHUNK_SIZE);
      frag->buf_max_size = BUF_CHUNK_SIZE - sizeof (DskBufferFragment);
    }
#endif	/* !DSK_DEBUG_BUFFER_ALLOCATIONS */
  frag->buf_start = frag->buf_length = 0;
  frag->next = 0;
  frag->buf = (uint8_t *) (frag + 1);
  frag->is_foreign = 0;
  return frag;
}

static DskBufferFragment *
new_foreign_fragment (unsigned             length,
                      const void          *ptr,
		      DskDestroyNotify     destroy,
		      void                *ddata)
{
  DskBufferFragment *fragment;
  fragment = dsk_malloc (sizeof (DskBufferFragment));
  fragment->is_foreign = 1;
  fragment->buf_start = 0;
  fragment->buf_length = length;
  fragment->buf_max_size = length;
  fragment->next = NULL;
  fragment->buf = (uint8_t *) ptr;
  fragment->destroy = destroy;
  fragment->destroy_data = ddata;
  return fragment;
}

#if DSK_DEBUG_BUFFER_ALLOCATIONS
#define recycle(frag) G_STMT_START{ \
    if (frag->is_foreign) { \
      { if (frag->destroy) frag->destroy (frag->destroy_data); \
        g_slice_free(DskBufferFragment, frag); } \
    else g_free (frag); \
   }G_STMT_END
#else	/* optimized (?) */
static void
recycle(DskBufferFragment* frag)
{
  if (frag->is_foreign)
    {
      if (frag->destroy)
        frag->destroy (frag->destroy_data);
      dsk_free (frag);
      return;
    }
#if defined(MAX_RECYCLED)
  if (num_recycled >= MAX_RECYCLED)
    {
      dsk_free (frag);
      return;
    }
#endif
  frag->next = recycling_stack;
  recycling_stack = frag;
  num_recycled++;
}
#endif	/* !DSK_DEBUG_BUFFER_ALLOCATIONS */

/* --- Global public methods --- */
/**
 * _dsk_buffer_cleanup_recycling_bin:
 * 
 * Free unused buffer fragments.  (Normally some are
 * kept around to reduce strain on the global allocator.)
 */
void
_dsk_buffer_cleanup_recycling_bin ()
{
#if !DSK_DEBUG_BUFFER_ALLOCATIONS
  while (recycling_stack != NULL)
    {
      DskBufferFragment *next;
      next = recycling_stack->next;
      dsk_free (recycling_stack);
      recycling_stack = next;
    }
  num_recycled = 0;
#endif
}
      
/* --- Public methods --- */
/**
 * dsk_buffer_construct:
 * @buffer: buffer to initialize (as empty).
 *
 * Construct an empty buffer out of raw memory.
 * (This is equivalent to filling the buffer with 0s)
 */
void
dsk_buffer_init(DskBuffer *buffer)
{
  buffer->first_frag = buffer->last_frag = NULL;
  buffer->size = 0;
}

#if defined(DSK_DEBUG) || DSK_DEBUG_BUFFER_ALLOCATIONS
static inline gboolean
verify_buffer (const DskBuffer *buffer)
{
  const DskBufferFragment *frag;
  unsigned total = 0;
  for (frag = buffer->first_frag; frag != NULL; frag = frag->next)
    total += frag->buf_length;
  return total == buffer->size;
}
#define CHECK_INTEGRITY(buffer)	dsk_assert (verify_buffer (buffer))
#else
#define CHECK_INTEGRITY(buffer)
#endif

/**
 * dsk_buffer_append:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @data: binary data to add to the buffer.
 * @length: length of @data to add to the buffer.
 *
 * Append data into the buffer.
 */
void
dsk_buffer_append(DskBuffer    *buffer,
		  unsigned      length,
                  const void * data)
{
  CHECK_INTEGRITY (buffer);
  buffer->size += length;
  while (length > 0)
    {
      unsigned avail;
      if (!buffer->last_frag)
	{
	  buffer->last_frag = buffer->first_frag = new_native_fragment ();
	  avail = dsk_buffer_fragment_avail (buffer->last_frag);
	}
      else
	{
	  avail = dsk_buffer_fragment_avail (buffer->last_frag);
	  if (avail <= 0)
	    {
	      buffer->last_frag->next = new_native_fragment ();
	      avail = dsk_buffer_fragment_avail (buffer->last_frag);
	      buffer->last_frag = buffer->last_frag->next;
	    }
	}
      if (avail > length)
	avail = length;
      memcpy (dsk_buffer_fragment_end (buffer->last_frag), data, avail);
      data = (const char *) data + avail;
      length -= avail;
      buffer->last_frag->buf_length += avail;
    }
  CHECK_INTEGRITY (buffer);
}

void
dsk_buffer_append_repeated_char (DskBuffer    *buffer, 
                                 size_t        count,
                                 char          character)
{
  CHECK_INTEGRITY (buffer);
  buffer->size += count;
  while (count > 0)
    {
      unsigned avail;
      if (!buffer->last_frag)
	{
	  buffer->last_frag = buffer->first_frag = new_native_fragment ();
	  avail = dsk_buffer_fragment_avail (buffer->last_frag);
	}
      else
	{
	  avail = dsk_buffer_fragment_avail (buffer->last_frag);
	  if (avail <= 0)
	    {
	      buffer->last_frag->next = new_native_fragment ();
	      avail = dsk_buffer_fragment_avail (buffer->last_frag);
	      buffer->last_frag = buffer->last_frag->next;
	    }
	}
      if (avail > count)
	avail = count;
      memset (dsk_buffer_fragment_end (buffer->last_frag), character, avail);
      count -= avail;
      buffer->last_frag->buf_length += avail;
    }
  CHECK_INTEGRITY (buffer);
}

#if 0
void
dsk_buffer_append_repeated_data (DskBuffer    *buffer, 
                                 const void * data_to_repeat,
                                 size_t         data_length,
                                 size_t         count)
{
  ...
}
#endif

/**
 * dsk_buffer_append_string:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @string: NUL-terminated string to append to the buffer.
 *  The NUL is not appended.
 *
 * Append a string to the buffer.
 */
void
dsk_buffer_append_string(DskBuffer  *buffer,
                         const char *string)
{
  dsk_return_if_fail (string != NULL, "string must be non-null");
  dsk_buffer_append (buffer, strlen (string), string);
}

/**
 * dsk_buffer_append_char:
 * @buffer: the buffer to add the byte to.
 * @character: the byte to add to the buffer.
 *
 * Append a byte to a buffer.
 */
void
dsk_buffer_append_char(DskBuffer *buffer,
		       char       character)
{
  dsk_buffer_append (buffer, 1, &character);
}

/**
 * dsk_buffer_append_string0:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @string: NUL-terminated string to append to the buffer;
 *  NUL is appended.
 *
 * Append a NUL-terminated string to the buffer.  The NUL is appended.
 */
void
dsk_buffer_append_string0      (DskBuffer    *buffer,
				const char   *string)
{
  dsk_buffer_append (buffer, strlen (string) + 1, string);
}

/**
 * dsk_buffer_read:
 * @buffer: the buffer to read data from.
 * @data: buffer to fill with up to @max_length bytes of data.
 * @max_length: maximum number of bytes to read.
 *
 * Removes up to @max_length data from the beginning of the buffer,
 * and writes it to @data.  The number of bytes actually read
 * is returned.
 *
 * returns: number of bytes transferred.
 */
unsigned
dsk_buffer_read(DskBuffer    *buffer,
		unsigned      max_length,
                void         *data)
{
  unsigned rv = 0;
  unsigned orig_max_length = max_length;
  CHECK_INTEGRITY (buffer);
  while (max_length > 0 && buffer->first_frag)
    {
      DskBufferFragment *first = buffer->first_frag;
      if (first->buf_length <= max_length)
	{
	  memcpy (data, dsk_buffer_fragment_start (first), first->buf_length);
	  rv += first->buf_length;
	  data = (char *) data + first->buf_length;
	  max_length -= first->buf_length;
	  buffer->first_frag = first->next;
	  if (!buffer->first_frag)
	    buffer->last_frag = NULL;
	  recycle (first);
	}
      else
	{
	  memcpy (data, dsk_buffer_fragment_start (first), max_length);
	  rv += max_length;
	  first->buf_length -= max_length;
	  first->buf_start += max_length;
	  data = (char *) data + max_length;
	  max_length = 0;
	}
    }
  buffer->size -= rv;
  dsk_assert (rv == orig_max_length || buffer->size == 0);
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * dsk_buffer_peek:
 * @buffer: the buffer to peek data from the front of.
 *    This buffer is unchanged by the operation.
 * @data: buffer to fill with up to @max_length bytes of data.
 * @max_length: maximum number of bytes to peek.
 *
 * Copies up to @max_length data from the beginning of the buffer,
 * and writes it to @data.  The number of bytes actually copied
 * is returned.
 *
 * This function is just like dsk_buffer_read() except that the 
 * data is not removed from the buffer.
 *
 * returns: number of bytes copied into data.
 */
unsigned
dsk_buffer_peek     (const DskBuffer *buffer,
		     unsigned         max_length,
                     void            *data)
{
  int rv = 0;
  DskBufferFragment *frag = (DskBufferFragment *) buffer->first_frag;
  CHECK_INTEGRITY (buffer);
  while (max_length > 0 && frag)
    {
      if (frag->buf_length <= max_length)
	{
	  memcpy (data, dsk_buffer_fragment_start (frag), frag->buf_length);
	  rv += frag->buf_length;
	  data = (char *) data + frag->buf_length;
	  max_length -= frag->buf_length;
	  frag = frag->next;
	}
      else
	{
	  memcpy (data, dsk_buffer_fragment_start (frag), max_length);
	  rv += max_length;
	  data = (char *) data + max_length;
	  max_length = 0;
	}
    }
  return rv;
}

/**
 * dsk_buffer_read_line:
 * @buffer: buffer to read a line from.
 *
 * Parse a newline (\n) terminated line from
 * buffer and return it as a newly allocated string.
 * The newline is changed to a NUL character.
 *
 * If the buffer does not contain a newline, then NULL is returned.
 *
 * returns: a newly allocated NUL-terminated string, or NULL.
 */
char *
dsk_buffer_read_line(DskBuffer *buffer)
{
  int len = 0;
  char *rv;
  DskBufferFragment *at;
  int newline_length;
  CHECK_INTEGRITY (buffer);
  for (at = buffer->first_frag; at; at = at->next)
    {
      uint8_t *start = dsk_buffer_fragment_start (at);
      uint8_t *got;
      got = memchr (start, '\n', at->buf_length);
      if (got)
	{
	  len += got - start;
	  break;
	}
      len += at->buf_length;
    }
  if (at == NULL)
    return NULL;
  rv = dsk_malloc (len + 1);
  /* If we found a newline, read it out, truncating
   * it with NUL before we return from the function... */
  if (at)
    newline_length = 1;
  else
    newline_length = 0;
  dsk_buffer_read (buffer, len + newline_length, rv);
  rv[len] = 0;
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * dsk_buffer_parse_string0:
 * @buffer: buffer to read a line from.
 *
 * Parse a NUL-terminated line from
 * buffer and return it as a newly allocated string.
 *
 * If the buffer does not contain a newline, then NULL is returned.
 *
 * returns: a newly allocated NUL-terminated string, or NULL.
 */
char *
dsk_buffer_parse_string0(DskBuffer *buffer)
{
  int index0 = dsk_buffer_index_of (buffer, '\0');
  char *rv;
  if (index0 < 0)
    return NULL;
  rv = dsk_malloc (index0 + 1);
  dsk_buffer_read (buffer, index0 + 1, rv);
  return rv;
}

/**
 * dsk_buffer_peek_byte:
 * @buffer: buffer to peek a single byte from.
 *
 * Get the first byte in the buffer as a positive or 0 number.
 * If the buffer is empty, -1 is returned.
 * The buffer is unchanged.
 *
 * returns: an unsigned character or -1.
 */
int
dsk_buffer_peek_byte(const DskBuffer *buffer)
{
  const DskBufferFragment *frag;

  if (buffer->size == 0)
    return -1;

  for (frag = buffer->first_frag; frag; frag = frag->next)
    if (frag->buf_length > 0)
      break;
  return * (const uint8_t *) (dsk_buffer_fragment_start ((DskBufferFragment*)frag));
}

/**
 * dsk_buffer_read_byte:
 * @buffer: buffer to read a single byte from.
 *
 * Get the first byte in the buffer as a positive or 0 number,
 * and remove the character from the buffer.
 * If the buffer is empty, -1 is returned.
 *
 * returns: an unsigned character or -1.
 */
int
dsk_buffer_read_byte (DskBuffer *buffer)
{
  uint8_t c;
  return (dsk_buffer_read (buffer, 1, &c) == 0) ? -1 : c;
}

/**
 * dsk_buffer_discard:
 * @buffer: the buffer to discard data from.
 * @max_discard: maximum number of bytes to discard.
 *
 * Removes up to @max_discard data from the beginning of the buffer,
 * and returns the number of bytes actually discarded.
 *
 * returns: number of bytes discarded.
 */
int
dsk_buffer_discard(DskBuffer *buffer,
                   unsigned      max_discard)
{
  int rv = 0;
  CHECK_INTEGRITY (buffer);
  while (max_discard > 0 && buffer->first_frag)
    {
      DskBufferFragment *first = buffer->first_frag;
      if (first->buf_length <= max_discard)
	{
	  rv += first->buf_length;
	  max_discard -= first->buf_length;
	  buffer->first_frag = first->next;
	  if (!buffer->first_frag)
	    buffer->last_frag = NULL;
	  recycle (first);
	}
      else
	{
	  rv += max_discard;
	  first->buf_length -= max_discard;
	  first->buf_start += max_discard;
	  max_discard = 0;
	}
    }
  buffer->size -= rv;
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * dsk_buffer_writev:
 * @read_from: buffer to take data from.
 * @fd: file-descriptor to write data to.
 *
 * Writes as much data as possible to the
 * given file-descriptor using the writev(2)
 * function to deal with multiple fragments
 * efficiently, where available.
 *
 * returns: the number of bytes transferred,
 * or -1 on a write error (consult errno).
 */
int
dsk_buffer_writev (DskBuffer       *read_from,
		   int              fd)
{
  int rv;
  struct iovec *iov;
  int nfrag, i;
  DskBufferFragment *frag_at = read_from->first_frag;
  CHECK_INTEGRITY (read_from);
  for (nfrag = 0; frag_at != NULL
#ifdef MAX_FRAGMENTS_TO_WRITE
       && nfrag < MAX_FRAGMENTS_TO_WRITE
#endif
       ; nfrag++)
    frag_at = frag_at->next;
  iov = (struct iovec *) alloca (sizeof (struct iovec) * nfrag);
  frag_at = read_from->first_frag;
  for (i = 0; i < nfrag; i++)
    {
      iov[i].iov_len = frag_at->buf_length;
      iov[i].iov_base = dsk_buffer_fragment_start (frag_at);
      frag_at = frag_at->next;
    }
  rv = writev (fd, iov, nfrag);
  if (rv < 0 && (errno == EINTR || errno == EAGAIN))
    return 0;
  if (rv <= 0)
    return rv;
  dsk_buffer_discard (read_from, rv);
  return rv;
}

/**
 * dsk_buffer_writev_len:
 * @read_from: buffer to take data from.
 * @fd: file-descriptor to write data to.
 * @max_bytes: maximum number of bytes to write.
 *
 * Writes up to max_bytes bytes to the
 * given file-descriptor using the writev(2)
 * function to deal with multiple fragments
 * efficiently, where available.
 *
 * returns: the number of bytes transferred,
 * or -1 on a write error (consult errno).
 */
int
dsk_buffer_writev_len (DskBuffer *read_from,
		       int        fd,
		       unsigned      max_bytes)
{
  int rv;
  struct iovec *iov;
  int nfrag, i;
  unsigned bytes;
  DskBufferFragment *frag_at = read_from->first_frag;
  CHECK_INTEGRITY (read_from);
  for (nfrag = 0, bytes = 0; frag_at != NULL && bytes < max_bytes
#ifdef MAX_FRAGMENTS_TO_WRITE
       && nfrag < MAX_FRAGMENTS_TO_WRITE
#endif
       ; nfrag++)
    {
      bytes += frag_at->buf_length;
      frag_at = frag_at->next;
    }
  iov = (struct iovec *) alloca (sizeof (struct iovec) * nfrag);
  frag_at = read_from->first_frag;
  for (bytes = max_bytes, i = 0; i < nfrag && bytes > 0; i++)
    {
      unsigned frag_bytes = frag_at->buf_length;
      if (frag_bytes > bytes)
        frag_bytes = bytes;
      iov[i].iov_len = frag_bytes;
      iov[i].iov_base = dsk_buffer_fragment_start (frag_at);
      frag_at = frag_at->next;
      bytes -= frag_bytes;
    }
  rv = writev (fd, iov, i);
  if (rv < 0 && (errno == EINTR || errno == EAGAIN))
    return 0;
  if (rv <= 0)
    return rv;
  dsk_buffer_discard (read_from, rv);
  return rv;
}

/**
 * dsk_buffer_read_in_fd:
 * @write_to: buffer to append data to.
 * @read_from: file-descriptor to read data from.
 *
 * Append data into the buffer directly from the
 * given file-descriptor.
 *
 * returns: the number of bytes transferred,
 * or -1 on a read error (consult errno).
 */
/* TODO: zero-copy! */
int
dsk_buffer_readv(DskBuffer *write_to,
                      int        read_from)
{
  char buf[8192];
  int rv = read (read_from, buf, sizeof (buf));
  if (rv < 0)
    return rv;
  dsk_buffer_append (write_to, rv, buf);
  return rv;
}

/**
 * dsk_buffer_clear:
 * @to_destroy: the buffer to empty.
 *
 * Remove all fragments from a buffer, leaving it empty.
 * The buffer is guaranteed to not to be consuming any resources,
 * but it also is allowed to start using it again.
 */
void
dsk_buffer_clear(DskBuffer *to_destroy)
{
  DskBufferFragment *at = to_destroy->first_frag;
  CHECK_INTEGRITY (to_destroy);
  while (at)
    {
      DskBufferFragment *next = at->next;
      recycle (at);
      at = next;
    }
  to_destroy->first_frag = to_destroy->last_frag = NULL;
  to_destroy->size = 0;
}

/**
 * dsk_buffer_index_of:
 * @buffer: buffer to scan.
 * @char_to_find: a byte to look for.
 *
 * Scans for the first instance of the given character.
 * returns: its index in the buffer, or -1 if the character
 * is not in the buffer.
 */
int
dsk_buffer_index_of(DskBuffer *buffer,
                    char       char_to_find)
{
  DskBufferFragment *at = buffer->first_frag;
  int rv = 0;
  while (at)
    {
      uint8_t *start = dsk_buffer_fragment_start (at);
      uint8_t *saught = memchr (start, char_to_find, at->buf_length);
      if (saught)
	return (saught - start) + rv;
      else
	rv += at->buf_length;
      at = at->next;
    }
  return -1;
}

/**
 * dsk_buffer_str_index_of:
 * @buffer: buffer to scan.
 * @str_to_find: a string to look for.
 *
 * Scans for the first instance of the given string.
 * returns: its index in the buffer, or -1 if the string
 * is not in the buffer.
 */
int 
dsk_buffer_str_index_of (DskBuffer *buffer,
                         const char *str_to_find)
{
  DskBufferFragment *frag = buffer->first_frag;
  unsigned rv = 0;
  for (frag = buffer->first_frag; frag; frag = frag->next)
    {
      const uint8_t *frag_at = frag->buf + frag->buf_start;
      unsigned frag_rem = frag->buf_length;
      while (frag_rem > 0)
        {
          DskBufferFragment *subfrag;
          const uint8_t *subfrag_at;
          unsigned subfrag_rem;
          const char *str_at;
          if (*frag_at != str_to_find[0])
            {
              frag_at++;
              frag_rem--;
              rv++;
              continue;
            }
          subfrag = frag;
          subfrag_at = frag_at + 1;
          subfrag_rem = frag_rem - 1;
          str_at = str_to_find + 1;
          if (*str_at == '\0')
            return rv;
          while (subfrag != NULL)
            {
              while (subfrag_rem == 0)
                {
                  subfrag = subfrag->next;
                  if (subfrag == NULL)
                    goto bad_guess;
                  subfrag_at = subfrag->buf + subfrag->buf_start;
                  subfrag_rem = subfrag->buf_length;
                }
              while (*str_at != '\0' && subfrag_rem != 0)
                {
                  if (*str_at++ != *subfrag_at++)
                    goto bad_guess;
                  subfrag_rem--;
                }
              if (*str_at == '\0')
                return rv;
            }
bad_guess:
          frag_at++;
          frag_rem--;
          rv++;
        }
    }
  return -1;
}

/**
 * dsk_buffer_drain:
 * @dst: buffer to add to.
 * @src: buffer to remove from.
 *
 * Transfer all data from @src to @dst,
 * leaving @src empty.
 *
 * returns: the number of bytes transferred.
 */
#if DSK_DEBUG_BUFFER_ALLOCATIONS
unsigned
dsk_buffer_drain (DskBuffer *dst,
		  DskBuffer *src)
{
  unsigned rv = src->size;
  DskBufferFragment *frag;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  for (frag = src->first_frag; frag; frag = frag->next)
    dsk_buffer_append (dst,
                       dsk_buffer_fragment_start (frag),
                       frag->buf_length);
  dsk_buffer_discard (src, src->size);
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#else	/* optimized */
unsigned
dsk_buffer_drain (DskBuffer *dst,
		  DskBuffer *src)
{
  unsigned rv = src->size;

  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  if (src->first_frag == NULL)
    return rv;

  dst->size += src->size;

  if (dst->last_frag != NULL)
    {
      dst->last_frag->next = src->first_frag;
      dst->last_frag = src->last_frag;
    }
  else
    {
      dst->first_frag = src->first_frag;
      dst->last_frag = src->last_frag;
    }
  src->size = 0;
  src->first_frag = src->last_frag = NULL;
  CHECK_INTEGRITY (dst);
  return rv;
}
#endif

/**
 * dsk_buffer_transfer:
 * @dst: place to copy data into.
 * @src: place to read data from.
 * @max_transfer: maximum number of bytes to transfer.
 *
 * Transfer data out of @src and into @dst.
 * Data is removed from @src.  The number of bytes
 * transferred is returned.
 *
 * returns: the number of bytes transferred.
 */
#if DSK_DEBUG_BUFFER_ALLOCATIONS
unsigned
dsk_buffer_transfer(DskBuffer *dst,
		    DskBuffer *src,
		    unsigned max_transfer)
{
  unsigned rv = 0;
  DskBufferFragment *frag;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  for (frag = src->first_frag; frag && max_transfer > 0; frag = frag->next)
    {
      unsigned len = frag->buf_length;
      if (len >= max_transfer)
        {
          dsk_buffer_append (dst, dsk_buffer_fragment_start (frag), max_transfer);
          rv += max_transfer;
          break;
        }
      else
        {
          dsk_buffer_append (dst, dsk_buffer_fragment_start (frag), len);
          rv += len;
          max_transfer -= len;
        }
    }
  dsk_buffer_discard (src, rv);
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#else	/* optimized */
unsigned
dsk_buffer_transfer(DskBuffer *dst,
		    DskBuffer *src,
		    unsigned max_transfer)
{
  unsigned rv = 0;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  while (src->first_frag && max_transfer >= src->first_frag->buf_length)
    {
      DskBufferFragment *frag = src->first_frag;
      src->first_frag = frag->next;
      frag->next = NULL;
      if (src->first_frag == NULL)
	src->last_frag = NULL;

      if (dst->last_frag)
	dst->last_frag->next = frag;
      else
	dst->first_frag = frag;
      dst->last_frag = frag;

      rv += frag->buf_length;
      max_transfer -= frag->buf_length;
    }
  dst->size += rv;
  if (src->first_frag && max_transfer)
    {
      DskBufferFragment *frag = src->first_frag;
      dsk_buffer_append (dst, max_transfer, dsk_buffer_fragment_start (frag));
      frag->buf_start += max_transfer;
      frag->buf_length -= max_transfer;
      rv += max_transfer;
    }
  src->size -= rv;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#endif	/* !DSK_DEBUG_BUFFER_ALLOCATIONS */

/* --- foreign data --- */
/**
 * dsk_buffer_append_foreign:
 * @buffer: the buffer to append into.
 * @data: the data to append.
 * @length: length of @data.
 * @destroy: optional method to call when the data is no longer needed.
 * @destroy_data: the argument to the destroy method.
 *
 * This function allows data to be placed in a buffer without
 * copying.  It is the callers' responsibility to ensure that
 * @data will remain valid until the destroy method is called.
 * @destroy may be omitted if @data is permanent, for example,
 * if appended a static string into a buffer.
 */
void dsk_buffer_append_foreign (DskBuffer        *buffer,
				unsigned          length,
                                const void *     data,
				DskDestroyNotify    destroy,
				void *          destroy_data)
{
  DskBufferFragment *fragment;

  CHECK_INTEGRITY (buffer);

  fragment = new_foreign_fragment (length, data, destroy, destroy_data);
  fragment->next = NULL;

  if (buffer->last_frag == NULL)
    buffer->first_frag = fragment;
  else
    buffer->last_frag->next = fragment;

  buffer->last_frag = fragment;
  buffer->size += length;

  CHECK_INTEGRITY (buffer);
}

#if 0
/**
 * dsk_buffer_printf:
 * @buffer: the buffer to append to.
 * @format: printf-style format string describing what to append to buffer.
 * @Varargs: values referenced by @format string.
 *
 * Append printf-style content to a buffer.
 */
void     dsk_buffer_printf              (DskBuffer    *buffer,
					 const char   *format,
					 ...)
{
  va_list args;
  va_start (args, format);
  dsk_buffer_vprintf (buffer, format, args);
  va_end (args);
}

/**
 * dsk_buffer_vprintf:
 * @buffer: the buffer to append to.
 * @format: printf-style format string describing what to append to buffer.
 * @args: values referenced by @format string.
 *
 * Append printf-style content to a buffer, given a va_list.
 */
void     dsk_buffer_vprintf             (DskBuffer    *buffer,
					 const char   *format,
					 va_list       args)
{
  size_t size = g_printf_string_upper_bound (format, args);
  if (size < 1024)
    {
      char buf[1024];
      g_vsnprintf (buf, sizeof (buf), format, args);
      dsk_buffer_append_string (buffer, buf);
    }
  else
    {
      char *buf = g_strdup_vprintf (format, args);
      dsk_buffer_append_foreign (buffer, buf, strlen (buf), g_free, buf);
    }
}
#endif

/* --- dsk_buffer_polystr_index_of implementation --- */
/* Test to see if a sequence of buffer fragments
 * starts with a particular NUL-terminated string.
 */
static dsk_boolean
fragment_n_str(DskBufferFragment   *frag,
               unsigned                frag_index,
               const char          *string)
{
  unsigned len = strlen (string);
  for (;;)
    {
      unsigned test_len = frag->buf_length - frag_index;
      if (test_len > len)
        test_len = len;

      if (memcmp (string,
                  dsk_buffer_fragment_start (frag) + frag_index,
                  test_len) != 0)
        return DSK_FALSE;

      len -= test_len;
      string += test_len;

      if (len <= 0)
        return DSK_TRUE;
      frag_index += test_len;
      if (frag_index >= frag->buf_length)
        {
          frag = frag->next;
          if (frag == NULL)
            return DSK_FALSE;
        }
    }
}

/**
 * dsk_buffer_polystr_index_of:
 * @buffer: buffer to scan.
 * @strings: NULL-terminated set of string.
 *
 * Scans for the first instance of any of the strings
 * in the buffer.
 *
 * returns: the index of that instance, or -1 if not found.
 */
int     
dsk_buffer_polystr_index_of    (DskBuffer    *buffer,
                                char        **strings)
{
  uint8_t init_char_map[16];
  int num_strings;
  int num_bits = 0;
  int total_index = 0;
  DskBufferFragment *frag;
  memset (init_char_map, 0, sizeof (init_char_map));
  for (num_strings = 0; strings[num_strings] != NULL; num_strings++)
    {
      uint8_t c = strings[num_strings][0];
      uint8_t mask = (1 << (c % 8));
      uint8_t *rack = init_char_map + (c / 8);
      if ((*rack & mask) == 0)
        {
          *rack |= mask;
          num_bits++;
        }
    }
  if (num_bits == 0)
    return 0;
  for (frag = buffer->first_frag; frag != NULL; frag = frag->next)
    {
      const uint8_t *frag_start;
      const uint8_t *at;
      int remaining = frag->buf_length;
      frag_start = dsk_buffer_fragment_start (frag);
      at = frag_start;
      while (at != NULL)
        {
          const uint8_t *start = at;
          if (num_bits == 1)
            {
              at = memchr (start, strings[0][0], remaining);
              if (at == NULL)
                remaining = 0;
              else
                remaining -= (at - start);
            }
          else
            {
              while (remaining > 0)
                {
                  uint8_t i = (uint8_t) (*at);
                  if (init_char_map[i / 8] & (1 << (i % 8)))
                    break;
                  remaining--;
                  at++;
                }
              if (remaining == 0)
                at = NULL;
            }

          if (at == NULL)
            break;

          /* Now test each of the strings manually. */
          {
            char **test;
            for (test = strings; *test != NULL; test++)
              {
                if (fragment_n_str(frag, at - frag_start, *test))
                  return total_index + (at - frag_start);
              }
            at++;
          }
        }
      total_index += frag->buf_length;
    }
  return -1;
}

#if 0
/* --- DskBufferIterator --- */

/**
 * dsk_buffer_iterator_construct:
 * @iterator: to initialize.
 * @to_iterate: the buffer to walk through.
 *
 * Initialize a new #DskBufferIterator.
 */
void 
dsk_buffer_iterator_construct (DskBufferIterator *iterator,
			       DskBuffer         *to_iterate)
{
  iterator->fragment = to_iterate->first_frag;
  if (iterator->fragment != NULL)
    {
      iterator->in_cur = 0;
      iterator->cur_data = (uint8_t*)dsk_buffer_fragment_start (iterator->fragment);
      iterator->cur_length = iterator->fragment->buf_length;
    }
  else
    {
      iterator->in_cur = 0;
      iterator->cur_data = NULL;
      iterator->cur_length = 0;
    }
  iterator->offset = 0;
}

/**
 * dsk_buffer_iterator_peek:
 * @iterator: to peek data from.
 * @out: to copy data into.
 * @max_length: maximum number of bytes to write to @out.
 *
 * Peek data from the current position of an iterator.
 * The iterator's position is not changed.
 *
 * returns: number of bytes peeked into @out.
 */
unsigned
dsk_buffer_iterator_peek      (DskBufferIterator *iterator,
			       void *           out,
			       unsigned              max_length)
{
  DskBufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;
  uint8_t *out_at = out;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  memcpy (out_at, frag_data + in_frag, out_remaining);
	  out_remaining = 0;
	  break;
	}

      memcpy (out_at, frag_data + in_frag, frag_remaining);
      out_remaining -= frag_remaining;
      out_at += frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) dsk_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      in_frag = 0;
    }
  return max_length - out_remaining;
}

/**
 * dsk_buffer_iterator_read:
 * @iterator: to read data from.
 * @out: to copy data into.
 * @max_length: maximum number of bytes to write to @out.
 *
 * Peek data from the current position of an iterator.
 * The iterator's position is updated to be at the end of
 * the data read.
 *
 * returns: number of bytes read into @out.
 */
unsigned
dsk_buffer_iterator_read      (DskBufferIterator *iterator,
			       void *           out,
			       unsigned              max_length)
{
  DskBufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;
  uint8_t *out_at = out;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  memcpy (out_at, frag_data + in_frag, out_remaining);
	  in_frag += out_remaining;
	  out_remaining = 0;
	  break;
	}

      memcpy (out_at, frag_data + in_frag, frag_remaining);
      out_remaining -= frag_remaining;
      out_at += frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) dsk_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      in_frag = 0;
    }
  iterator->in_cur = in_frag;
  iterator->fragment = fragment;
  iterator->cur_length = frag_length;
  iterator->cur_data = frag_data;
  iterator->offset += max_length - out_remaining;
  return max_length - out_remaining;
}

/**
 * dsk_buffer_iterator_find_char:
 * @iterator: to advance.
 * @c: the character to look for.
 *
 * If it exists,
 * skip forward to the next instance of @c and return TRUE.
 * Otherwise, do nothing and return FALSE.
 *
 * returns: whether the character was found.
 */

gboolean
dsk_buffer_iterator_find_char (DskBufferIterator *iterator,
			       char               c)
{
  DskBufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;
  unsigned new_offset = iterator->offset;

  if (fragment == NULL)
    return -1;

  for (;;)
    {
      unsigned frag_remaining = frag_length - in_frag;
      const uint8_t * ptr = memchr (frag_data + in_frag, c, frag_remaining);
      if (ptr != NULL)
	{
	  iterator->offset = (ptr - frag_data) - in_frag + new_offset;
	  iterator->fragment = fragment;
	  iterator->in_cur = ptr - frag_data;
	  iterator->cur_length = frag_length;
	  iterator->cur_data = frag_data;
	  return TRUE;
	}
      fragment = fragment->next;
      if (fragment == NULL)
	return FALSE;
      new_offset += frag_length - in_frag;
      in_frag = 0;
      frag_length = fragment->buf_length;
      frag_data = (uint8_t *) fragment->buf + fragment->buf_start;
    }
}

/**
 * dsk_buffer_iterator_skip:
 * @iterator: to advance.
 * @max_length: maximum number of bytes to skip forward.
 *
 * Advance an iterator forward in the buffer,
 * returning the number of bytes skipped.
 *
 * returns: number of bytes skipped forward.
 */
unsigned
dsk_buffer_iterator_skip      (DskBufferIterator *iterator,
			       unsigned              max_length)
{
  DskBufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  in_frag += out_remaining;
	  out_remaining = 0;
	  break;
	}

      out_remaining -= frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) dsk_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      else
	{
	  frag_data = NULL;
	  frag_length = 0;
	}
      in_frag = 0;
    }
  iterator->in_cur = in_frag;
  iterator->fragment = fragment;
  iterator->cur_length = frag_length;
  iterator->cur_data = frag_data;
  iterator->offset += max_length - out_remaining;
  return max_length - out_remaining;
}
#endif