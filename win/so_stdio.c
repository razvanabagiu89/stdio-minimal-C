#include <string.h>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>

#elif defined(_WIN32)
#include <windows.h>
#define DLL_EXPORTS

#endif

#include "so_stdio.h"

#define READ 0
#define WRITE 1

typedef struct _so_file {

	char *read_buf;
	char *write_buf;
	int bytes_read;
	int bytes_write;
	int is_eof_or_error;
	#if defined __linux__
	int fd;
	#elif defined(_WIN32)
	HANDLE fd;
	#endif
	/* cursors for buffer */
	int read_crs;
	int write_crs;
	/* cursors for file */
	int read_cursor_file;
	int write_cursor_file;
	/* if file opened in read only mode */
	int is_read;
	/* branch switch variable for last operation (r/w) */
	int branch;
} SO_FILE;

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	#if defined(__linux__)
	int fd;
	#elif defined(_WIN32)
	HANDLE fd;
	#endif

	SO_FILE *fp = calloc(1, sizeof(SO_FILE));
	
	fp->read_buf = calloc(4096, sizeof(char));
	fp->write_buf = calloc(4096, sizeof(char));
	if (fp == NULL || fp->read_buf == NULL || fp->write_buf == NULL)
		return NULL;

	if (strcmp(mode, "r") == 0) {
		#if defined(__linux__)
		fd = open(pathname, O_RDONLY);
		fp->is_read = 1;
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else if (strcmp(mode, "r+") == 0) {
		#if defined(__linux__)
		fd = open(pathname, O_RDWR);
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else if (strcmp(mode, "w") == 0) {
		#if defined(__linux__)
		fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else if (strcmp(mode, "w+") == 0) {
		#if defined(__linux__)
		fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else if (strcmp(mode, "a") == 0) {
		#if defined(__linux__)
		fd = open(pathname,  O_APPEND | O_WRONLY | O_CREAT, 0644);
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_WRITE,
		FILE_APPEND_DATA,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else if (strcmp(mode, "a+") == 0) {
		#if defined(__linux__)
		fd = open(pathname,  O_APPEND | O_RDWR | O_CREAT, 0644);
		#elif defined(_WIN32)
		fd = CreateFile(
		pathname,
		GENERIC_WRITE | GENERIC_READ,
		FILE_APPEND_DATA,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
		#endif
	} else {
		free(fp->read_buf);
		free(fp->write_buf);
		free(fp);
		return NULL;
	}
	#if defined(__linux__)
	if (fd == -1) {
		free(fp->read_buf);
		free(fp->write_buf);
		free(fp);
		return NULL;
	}
	fp->fd = fd;
	#elif defined(_WIN32)
	if (fd == INVALID_HANDLE_VALUE) {
		free(fp->read_buf);
		free(fp->write_buf);
		free(fp);
		return NULL;
	}
	fp->fd = fd;
	#endif

	memset(fp->write_buf, 0, 4096);
	memset(fp->read_buf, 0, 4096);
	fp->bytes_read = 0;
	fp->bytes_write = 0;
	fp->is_eof_or_error = 0;
	fp->read_crs = 0;
	fp->write_crs = 0;
	fp->read_cursor_file = 0;
	fp->write_cursor_file = 0;
	fp->branch = 0;

	return fp;
}

int so_fclose(SO_FILE *stream)
{

	int status = 0;
	int closed = 0;

	/* if there are bytes left in the stream, fflush */
	if (stream->bytes_write != 0)
		status = so_fflush(stream);

	#if defined(__linux__)
	closed = close(so_fileno(stream));
	#elif defined(_WIN32)
	closed = CloseHandle(so_fileno(stream));
	#endif
	if (closed < 0 || status != 0) {
		if (stream != NULL) {
			free(stream->read_buf);
			free(stream->write_buf);
			free(stream);
		}
		return SO_EOF;
	}
	free(stream->read_buf);
	free(stream->write_buf);
	free(stream);
	return 0;
}

int so_fgetc(SO_FILE *stream)
{

	int bytes_read = 0;
	#if defined(_WIN32)
	int rc = 0;
	#endif
	/* if it's the first call load first 4096 bytes into the buffer */
	if (stream->bytes_read == 0) {
		#if defined(__linux__)
		bytes_read = read(so_fileno(stream), stream->read_buf, 4096);
		#elif defined(_WIN32)
		rc = ReadFile(
		so_fileno(stream),
		stream->read_buf,
		4096,
		&bytes_read,
		NULL);
		if (rc == 0) {
			stream->is_eof_or_error = 1;
			return SO_EOF;
		}
		#endif
		if (bytes_read <= 0) {
			stream->is_eof_or_error = 1;
			return SO_EOF;
		}
		stream->bytes_read = bytes_read;
		stream->read_crs = 0;
	}
	stream->read_crs++;
	stream->read_cursor_file++;
	stream->bytes_read = stream->bytes_read - 1;
	stream->branch = READ;
	return (unsigned char) stream->read_buf[stream->read_crs - 1];
}

int so_fputc(int c, SO_FILE *stream)
{

	int status;
	size_t max_size = 4096;
	size_t chunk = 0;
	int *aux;

	#if defined(_WIN32)
	size_t bytes_already_written = 0;
	#endif

	/* when buffer is fully loaded, write the whole content to file */
	if (stream->bytes_write == 4096) {
		while (max_size - chunk > 0) {
			#if defined(__linux__)
			status = write(so_fileno(stream), stream->write_buf,
				max_size - chunk);
			#elif defined(_WIN32)
			status = WriteFile(
			so_fileno(stream),
			stream->write_buf,
			max_size - chunk,
			&bytes_already_written,
			NULL);
			#endif
			if (status <= 0) {
				stream->is_eof_or_error = 1;
				return SO_EOF;
			}
			#if defined(__linux__)
			chunk += status;
			#elif defined(_WIN32)
			chunk += bytes_already_written;
			#endif
		}
		if (status > 0) {
			stream->write_crs = 0;
			stream->bytes_write = 0;
		} else {
			stream->is_eof_or_error = 1;
			return SO_EOF;
		}
	}

	aux = calloc(1, sizeof(int));
	if (aux == NULL)
		return -1;
	*aux = c;
	memcpy(stream->write_buf + stream->write_crs, aux, 1);
	stream->bytes_write++;
	stream->write_crs++;
	stream->write_cursor_file++;
	stream->branch = WRITE;
	free(aux);
	return (unsigned char) stream->write_buf[stream->write_crs - 1];
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{

	size_t bytes_demanded_read = size * nmemb;
	size_t bytes_read = 0;
	int read_char = 0;
	int is_eof_or_error = 0;

	while (bytes_read < bytes_demanded_read) {
		/* get 1 char from file */
		read_char = so_fgetc(stream);
		if (read_char == -1) {
			is_eof_or_error = 1;
			break;
		}
		/* store it in ptr */
		memcpy((char *)ptr + bytes_read,
			stream->read_buf + stream->read_crs - 1, 1);
		bytes_read++;
	}
	if (bytes_read > 0) {
		return bytes_read/size;
	} else if (is_eof_or_error == 1) {
		stream->is_eof_or_error = 1;
		return bytes_read/size;
	} else {
		return 0;
	}
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{

	size_t bytes_demanded_write = size * nmemb;
	size_t bytes_write = 0;
	int write_char = 0;
	int is_eof_or_error = 0;

	while (bytes_write < bytes_demanded_write) {
		/* get 1 char from ptr */
		memcpy(&write_char, (char *)ptr + bytes_write, 1);
		/* write it to stream */
		write_char = so_fputc(write_char, stream);
		if (write_char == -1) {
			is_eof_or_error = 1;
			break;
		}
		bytes_write++;
	}
	if (bytes_write > 0)
		return bytes_write/size;
	else if (is_eof_or_error == 1) {
		stream->is_eof_or_error = is_eof_or_error;
		return bytes_write/size;
	} else
		return 0;
}

#if defined(__linux__)
int so_fileno(SO_FILE *stream)
{
	if (stream == NULL) {
		stream->is_eof_or_error = 1;
		return -1;
	}
	return stream->fd;
}
#elif defined(_WIN32)
HANDLE so_fileno(SO_FILE *stream)
{
	if (stream == NULL) {
		stream->is_eof_or_error = 1;
		return INVALID_HANDLE_VALUE;
	}
	return stream->fd;
}
#endif

int so_fflush(SO_FILE *stream)
{

	size_t size;
	size_t chunk = 0;
	int status;
	#if defined(_WIN32)
	int bytes_already_written;
	#endif

	if (stream->bytes_write != 0) {
		size = (size_t) stream->bytes_write;
		while (size - chunk > 0) {
			#if defined(__linux__)
			status = write(so_fileno(stream),
				stream->write_buf, size - chunk);
			#elif defined(_WIN32)
			status = WriteFile(so_fileno(stream), stream->write_buf,
				size - chunk, &bytes_already_written, NULL);
			#endif
			if (status <= 0) {
				stream->is_eof_or_error = 1;
				return SO_EOF;
			}
			#if defined(__linux__)
			chunk += status;
			#elif defined(_WIN32)
			chunk += bytes_already_written;
			#endif
		}

		if (status > 0) {
			stream->read_crs = 0;
			stream->read_cursor_file = 0;
			stream->write_crs = 0;
			stream->write_cursor_file = 0;
			stream->bytes_write = 0;
			return 0;
		} else
			return SO_EOF;
	}
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int status;

	if (stream->bytes_write != 0)
		so_fflush(stream);

	/* if last operation is read, clear the buffer */
	if (stream->branch == 0)
		stream->bytes_read = 0;
	#if defined(__linux__)
	status = lseek(so_fileno(stream), offset, whence);
	if (status == -1)
		return -1;
	stream->read_cursor_file = status;
	stream->write_cursor_file = status;
	return 0;

	#elif defined(_WIN32)
	status = SetFilePointer(so_fileno(stream),
		offset, NULL, whence);
	if (status == INVALID_SET_FILE_POINTER)
		return -1;
	stream->read_cursor_file = status;
	stream->write_cursor_file = status;
	return 0;
	#endif
}

long so_ftell(SO_FILE *stream)
{
	if (stream == NULL) {
		stream->is_eof_or_error = 1;
		return -1;
	}
	if (stream->is_read == 1)
		return stream->read_cursor_file;
	else
		return stream->write_cursor_file;
	return -1;
}

int so_feof(SO_FILE *stream)
{
	if (stream == NULL) {
		stream->is_eof_or_error = 1;
		return -1;
	}
	if (stream->is_eof_or_error == 0)
		return 0;
	return -1;
}

int so_ferror(SO_FILE *stream)
{
	int status;

	status = so_feof(stream);
	if (status == -1 || stream->is_eof_or_error == -1)
		return -1;
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	return 0;
}
