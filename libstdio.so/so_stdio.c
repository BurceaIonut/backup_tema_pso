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
    char buffer[BUFFER_SIZE];
    int isEOF;
    int error;
    int read_bytes;
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
        handle->cursor = 0;
        fd = open(pathname, O_RDONLY);
    }
    else if(strcmp(mode, "r+") == 0)
    {
        handle->mode = 2;
        handle->cursor = 0;
        fd = open(pathname, O_RDWR);
    }
    else if(strcmp(mode, "w") == 0)
    {
        handle->mode = 3;
        handle->cursor = 0;
        fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    }
    else if(strcmp(mode, "w+") == 0)
    {
        handle->mode = 4;
        handle->cursor = 0;
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
        handle->cursor = 0;
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
    if(stream == NULL)
        return SO_EOF;
    if(close(stream->fd) < 0)
    {
        if(stream != NULL)
        {
            free(stream);
            stream = NULL;
        }
        return SO_EOF;
    }
    if(stream != NULL)
    {
        free(stream);
        stream = NULL;
    }
    else return SO_EOF;
    return 0;
}

int so_fgetc(SO_FILE *stream)
{
    if(stream->mode == 1 || stream->mode == 2 || stream->mode == 4 || stream->mode == 6)
    {
        if(stream->last_operation == 1 && stream->fflush == 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        if(stream->index_buffer == 0 || stream->index_buffer == BUFFER_SIZE || stream->last_operation == 1)
        {
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
        if(stream->from_fread == 1 && stream->buffer[stream->index_buffer] == '\0')
        {
            stream->isEOF = 1;
            stream->error = 1;
            return SO_EOF;
        }
        stream->last_operation = 2;
        stream->index_buffer++;
        stream->cursor++;
        stream->fflush = 0;
        return (unsigned char)stream->buffer[stream->index_buffer - 1];
    }
    else
    {
        stream->last_operation = 2;
        stream->error = 1;
        return SO_EOF;
    }
}

int so_fputc(int c, SO_FILE *stream)
{
    if(stream->mode == 2 || stream->mode == 3 || stream->mode == 4 || stream->mode == 5 ||stream->mode == 6)
    {
        if(stream->mode == 5)
        {
            so_fseek(stream, 0, SEEK_END);
        }
        if(stream->last_operation == 2 && stream->fflush == 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        if(stream->index_buffer == BUFFER_SIZE)
        {
            stream->fflush = 0;
            stream->read_bytes = so_fflush(stream);
            if(stream->read_bytes < 0)
            {
                stream->error = 1;
                return SO_EOF;
            }
        }
        stream->buffer[stream->index_buffer] = (unsigned char)c;
        stream->index_buffer++;
        stream->cursor++;
        stream->last_operation = 1;
        stream->fflush = 0;
        return (unsigned char)c;
    }
    else
    {
        stream->error = 1;
        stream->last_operation = 1;
        return SO_EOF;
    }
}

int so_fflush(SO_FILE* stream)
{
    if(stream->last_operation == 1 && stream->index_buffer != 0)
    {
        stream->read_bytes = write(stream->fd, stream->buffer, stream->index_buffer);
        memset(stream->buffer, 0, BUFFER_SIZE);
        if(stream->read_bytes < 0)
        {
            stream->error = 1;
            return SO_EOF;
        }
        stream->fflush = 1;
    }
    else if(stream->last_operation == 2 && stream->index_buffer != 0)
    {
        memset(stream->buffer, 0, BUFFER_SIZE);
        stream->index_buffer = 0;
        stream->fflush = 1;
    }
    return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
    stream->read_bytes = so_fflush(stream);
    if(stream->read_bytes < 0)
    {
        stream->error = 1;
        return SO_EOF;
    }
    if(stream == NULL)
    {
        stream->error = 1;
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
    if(stream == NULL)
        return SO_EOF;
    if(stream->error)
        return SO_EOF;
    return 0;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    long nr = size * nmemb;
    char* buff = (char*)malloc(sizeof(char)*nr);
    if(buff == NULL)
    {
        stream->error = 1;
        return 0;
    }
    stream->from_fread = 1;
    long count = 0;
    for(long i = 0; i < nr; i++)
    {
        if(!stream->isEOF)
        {
            char x = so_fgetc(stream);
            if(stream->error == 1)
            {
                free(buff);
                stream->from_fread = 0;
                return 0;
            }
            buff[i] = x;
            count++;
        }
        else
        {
            stream->last_operation = 2;
            memcpy(ptr, buff, nr);
            free(buff);
            stream->from_fread = 0;
            return count;
        }
    }
    stream->last_operation = 2;
    memcpy(ptr, buff, nr);
    free(buff);
    stream->from_fread = 0;
    return count;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    long nr = size * nmemb;
    char* buff = (char*)ptr;
    long count = 0;
    if(stream->mode == 6 || stream->mode == 5)
    {
        so_fseek(stream, 0, SEEK_END);
    }
    for(long i = 0; i < nmemb; i++)
    {
        char x = so_fputc(buff[i], stream);
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
    /*
    int pipe_fd[2];
    int fd;
    if(pipe(pipe_fd) < 0)
    {
        return NULL;
    }
    int pid = fork();
    if(pid < 0)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return NULL;
    }
    if(pid == 0)
    {
        if(strcmp(type, "r") == 0)
        {
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);
        }
        else if(strcmp(type, "w") == 0)
        {
            close(pipe_fd[1]);
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);
        }
        execlp("sh", "sh", "-c", command, NULL);
        exit(1);
    }
    if(pid > 0)
    {
        if(strcmp(type, "r") == 0)
        {
            close(pipe_fd[1]);
            fd = pipe_fd[0];
        }
        else
        {
            close(pipe_fd[0]);
            fd = pipe_fd[1];
        }
    }
    SO_FILE* f = (SO_FILE*)malloc(sizeof(SO_FILE));
    if(!f)
    {
        free(f);
        f = NULL;
        return NULL;
    }
    f->fd = fd;
    if(strcmp(type, "r") == 0)
        f->mode = 1;
    else if(strcmp(type, "w") == 0)
        f->mode = 3;
    else
    {
        free(f);
        f = NULL;
        return NULL;
    }
    f->last_operation = 0;
    f->cursor = 0;
    f->isEOF = 0;
    f->index_buffer = 0;
    f->error = 0;
    f->child_pid = pid;
    f->fflush = 0;
    f->from_fread = 0;
    memset(f->buffer, 0, BUFFER_SIZE); // IMI DA MEMCHECK DUPA CE AM IMPLEMENTAT so_popen
    return f;
    */
   return NULL;
}

int so_pclose(SO_FILE *stream)
{
    return 0;
}
