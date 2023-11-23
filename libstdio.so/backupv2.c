#include "so_stdio.h"
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>


#define BUFFER_SIZE 4096

typedef struct _so_file
{
    int fd;
    long cursor;
    long index_buffer;
    int mode;
    unsigned char buffer[BUFFER_SIZE];
    int isEOF;
    int error;
    long read_bytes;
    int last_operation;
    int fflush;
    int child_pid;
    int from_fread;
}SO_FILE;

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    SO_FILE* handle = (SO_FILE*)malloc(sizeof(SO_FILE));
    if(handle == NULL)
        return NULL;
    memset(handle->buffer, 0, sizeof(handle->buffer));
    handle->cursor = 0;
    handle->error = 0;
    handle->isEOF = 0;
    handle->read_bytes = 0;
    handle->mode = 0;
    handle->child_pid = -1;
    handle->last_operation = 0;
    handle->index_buffer = 0;
    handle->fflush = 0;
    handle->from_fread = 0;
    int fd;
    if(strcmp(mode, "r") == 0)
    {
        handle->mode = 1;
        fd = open(pathname, O_RDONLY);
    }
    else if(strcmp(mode, "r+") == 0)
    {
        handle->mode = 2;
        fd = open(pathname, O_RDWR);
    }
    else if(strcmp(mode, "w") == 0)
    {
        handle->mode = 3;
        fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    }
    else if(strcmp(mode, "w+") == 0)
    {
        handle->mode = 4;
        fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0600);
    }
    else if(strcmp(mode, "a") == 0)
    {
        handle->mode = 5;
        fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0600);
    }
    else if(strcmp(mode, "a+") == 0)
    {
        handle->mode = 6;
        fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0600);
    }
    if(handle->mode == 0)
    {
        handle->error = 1;
        free(handle);
        handle = NULL;
        return NULL;
    }
    if(fd < 0)
    {
        handle->error = 1;
        free(handle);
        handle = NULL;
        return NULL;
    }
    handle->fd = fd;
    return handle;
}

int so_fclose(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;
    if(stream->last_operation == 1 && stream->fflush == 0)
    {
        stream->read_bytes = so_fflush(stream);
        if(stream->read_bytes < 0)
        {
            stream->error = 1;
            free(stream);
            stream = NULL;
            return SO_EOF;
        }
    }
    if(close(stream->fd) < 0)
    {
        free(stream);
        stream = NULL;
        return SO_EOF;
    }
    free(stream);
    stream = NULL;
    return 0;
}

int so_fgetc(SO_FILE *stream)
{
    if(stream->mode == 1 || stream->mode == 2 || stream->mode == 4 || stream->mode == 6)
    {
        /*
        if(stream->last_operation == 1 && stream->fflush == 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        */
        if(stream->index_buffer == 0 || stream->index_buffer == BUFFER_SIZE || stream->last_operation == 1)
        {
            memset(stream->buffer, 0, BUFFER_SIZE);
            stream->read_bytes = read(stream->fd, stream->buffer, BUFFER_SIZE);
            if(stream->read_bytes < 0)
            {
                stream->error = 1;
                return SO_EOF;
            }
            else if(stream->read_bytes == 0)
            {
                stream->isEOF = 1;
                return SO_EOF;
            }
            else if(stream->read_bytes > 0)
            {
                stream->last_operation = 2;
                stream->index_buffer = 0;
            }
        }
        if(stream->from_fread == 0 && stream->buffer[stream->index_buffer] == '\0')
        {
            stream->isEOF = 1;
            return SO_EOF;
        }
        /*
        if(stream->from_fread == 1 && stream->buffer[stream->index_buffer] == '\0')
        {
            stream->isEOF = 1;
            stream->error = 1;
            return SO_EOF;
        }
        */
        stream->last_operation = 2;
        stream->index_buffer++;
        //if(stream->buffer[stream->index_buffer] == '\0' && stream->read_bytes < BUFFER_SIZE)//
        //    stream->isEOF = 1;
        stream->cursor++;
        //stream->fflush = 0;
        return (int)(stream->buffer[stream->index_buffer - 1]);
    }
    else
    {
        stream->error = 1;
        return SO_EOF;
    }
}

int so_fputc(int c, SO_FILE *stream)
{
    if(stream->mode == 2 || stream->mode == 3 || stream->mode == 4 || stream->mode == 5 ||stream->mode == 6)
    {
        /*
        if(stream->last_operation == 2 && stream->fflush == 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        */
        if(stream->index_buffer == BUFFER_SIZE)
        {
            stream->fflush = 0;
            stream->read_bytes = so_fflush(stream);
            //stream->fflush = 1;
            if(stream->read_bytes < 0)
            {
                stream->error = 1;
                return SO_EOF;
            }
        }
        stream->buffer[stream->index_buffer] = (unsigned char)c;
        stream->index_buffer++;
        stream->cursor++;
        stream->fflush = 0;
    }
    else
    {
        stream->error = 1;
        return SO_EOF;
    }
    stream->last_operation = 1;
    return c;
}

int so_fflush(SO_FILE* stream)
{
    if(stream->last_operation == 1 && stream->index_buffer != 0 && stream->fflush == 0)
    {
        stream->read_bytes = write(stream->fd, stream->buffer, stream->index_buffer);
        //memset(stream->buffer, 0, BUFFER_SIZE);
        if(stream->read_bytes < 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        stream->index_buffer = 0;
        stream->fflush = 1;
        return 0;
    }
    else
    {
        memset(stream->buffer, 0, BUFFER_SIZE);
        stream->index_buffer = 0;
        //stream->fflush = 1;
        return 0;
    }
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
    if(stream == NULL)
    {
        //stream->error = 1;
        return SO_EOF;
    }
    if(so_fflush(stream) < 0)
    {
        //stream->error = 1;
        return SO_EOF;
    }
    int pos = lseek(stream->fd, 0, SEEK_CUR);
    pos =lseek(stream->fd, -(pos-stream->cursor), SEEK_CUR);
    pos = lseek(stream->fd, offset, whence);
    if(pos < 0)
        return SO_EOF;
    stream->cursor = pos;
    return 0;
}

long so_ftell(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;
    if(stream->error == 1)
        return SO_EOF;
    return stream->cursor;
}

int so_fileno(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;
    return stream->fd;
}

int so_feof(SO_FILE *stream)
{
    if(stream == NULL)
        return SO_EOF;
    return stream->isEOF;
}

int so_ferror(SO_FILE *stream)
{
    return stream->error;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
  char *data = (char*)ptr;
  size_t total = size * nmemb;
  size_t count = total;

  if (!stream || !ptr || size == 0 || nmemb == 0) {
    return 0;
  }

  while (count != 0) {
    if (stream->index_buffer > 0 &&
        stream->cursor < stream->index_buffer) {
      size_t nbytes = (count < (size_t)(stream->index_buffer - stream->cursor)) ? count : (size_t)(stream->index_buffer - stream->cursor);
      memcpy(data, stream->buffer + stream->cursor, nbytes);

      count -= nbytes;
      stream->cursor += nbytes;
      data += nbytes;
    } else if (count >= BUFFER_SIZE) {
      size_t nbytes = BUFFER_SIZE;
      ssize_t nread = read(stream->fd, data, nbytes);

      if (nread == 0) {
        stream->isEOF = 1;
        return (total - count) / size;
      } else if (nread < 0) {
        stream->error = 1;
        return (total - count) / size;
      }

      count -= nread;
      data += nread;
    } else {
      ssize_t nread = read(stream->fd, stream->buffer, BUFFER_SIZE);

      if (nread == 0) {
        stream->isEOF = 1;
        return (total - count) / size;
      } else if (nread < 0) {
        stream->error = 1;
        return (total - count) / size;
      }

      stream->index_buffer = nread;
      stream->cursor = 0;
    }
  }

  return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    size_t nr = size * nmemb;
    char* buff = (char*)ptr;
    size_t count = 0;
    stream->fflush = 0;
    if(stream->mode == 6 || stream->mode == 5)
    {
        if(so_fseek(stream, 0, SEEK_END) < 0)
        {
            stream->error = 1;
            return 0;
        }
    }
    for(size_t i = 0; i < nmemb; i++)
    {
        int x = so_fputc((int)buff[i], stream);
        if(stream->error == 1)
        {
            return 0;
        }
        count++;
    }
    stream->last_operation = 1;
    if(count == 0)
    {
        stream->error = 1;
        return 0;
    }
    return count;
}

SO_FILE *so_popen(const char *command, const char *type)
{
    int pfd[2];
	if(pipe(pfd) < 0) 
    {
		return NULL;
	}

    int fd;
	int pid = fork();
    if(pid < 0) 
    {
		close(pfd[0]);
		close(pfd[1]);
		return NULL;
	}
	if (pid == 0) 
    {
		if(type[0] == 'r') 
        {
			close(pfd[0]);
			dup2(pfd[1], STDOUT_FILENO);
			close(pfd[1]);
		}
		else if(type[0] == 'w')
        { 
			close(pfd[1]);
			dup2(pfd[0], STDIN_FILENO);
			close(pfd[0]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(0);
	}
    if(pid > 0)
    {
        SO_FILE* handle = (SO_FILE*)malloc(sizeof(SO_FILE));
        if(handle == NULL) 
        {
            //close(fd);
            close(pfd[0]);
            close(pfd[1]);
            return NULL;
        }

        if(type[0] == 'r') 
        {
            close(pfd[1]);
            fd = pfd[0];
        }
        else 
        { 
            close(pfd[0]);
            fd = pfd[1];
        }

        handle->fd = fd;

        if(type[0] == 'r')
        {
            handle->mode = 1;
        }
        else if(type[0] == 'w')
        {
            handle->mode = 3;
        }
        else
        {
            close(fd);
            free(handle);
            handle = NULL;
            return NULL;
        }

        handle->last_operation=0;
        handle->cursor = 0;
        handle->isEOF = 0;
        handle->index_buffer = 0;
        handle->error = 0;
        handle->child_pid = pid;
        handle->fflush=0;
        handle->from_fread=0;
        //memset(handle->buffer, 0, BUFFER_SIZE);
        return handle;
    }
}

int so_pclose(SO_FILE *stream)
{
    if(stream == NULL)
    {
        return SO_EOF;
    }
    int wstatus;
    int ret = waitpid(stream->child_pid, &wstatus, 0);//aici se blocheaza la so_popen write
    if(ret < 0)
    {
        free(stream);
        stream = NULL;
        return SO_EOF;
    }
    else if(so_fclose(stream) < 0)
    {
        return SO_EOF;
    }
    //free(stream);
    //stream = NULL;
    return wstatus;
}
