#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdatomic.h>

#define CHUNK_SIZE 65536
#define TASK_QUEUE_CAPACITY 128

typedef struct {
    char *data;
    size_t size;
    size_t start;
    size_t end;
    char *output;
    size_t output_capacity;
    size_t output_size;
} Task;

typedef struct {
    Task *tasks[TASK_QUEUE_CAPACITY];
    int front, rear, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskQueue;

TaskQueue queue;
atomic_int job_done;

void task_queue_init(TaskQueue *queue) {
    queue->front = queue->rear = queue->count = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void task_queue_destroy(TaskQueue *queue) {
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
}

void enqueue(TaskQueue *queue, Task *task) {
    pthread_mutex_lock(&queue->lock);
    while (queue->count == TASK_QUEUE_CAPACITY) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    queue->tasks[queue->rear] = task;
    queue->rear = (queue->rear + 1) % TASK_QUEUE_CAPACITY;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

Task *dequeue(TaskQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    Task *task = queue->tasks[queue->front];
    queue->front = (queue->front + 1) % TASK_QUEUE_CAPACITY;
    queue->count--;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
    return task;
}

void *worker(void *arg) {
    while (1) {
        Task *task = dequeue(&queue);
        if (!task) {
            break;
        }
        char last_char = task->data[task->start];
        unsigned char count = 1;
        size_t write_index = 0;
        for (size_t i = task->start + 1; i < task->end; ++i) {
            if (task->data[i] == last_char && count < 255) {
                count++;
            } else {
                task->output[write_index++] = last_char;
                task->output[write_index++] = count;
                last_char = task->data[i];
                count = 1;
            }
        }
        task->output[write_index++] = last_char;
        task->output[write_index++] = count;
        task->output_size = write_index;

        atomic_fetch_sub(&job_done, 1);
        free(task);
    }
    return NULL;
}

void encode_file(const char *filename, int num_threads, char *last_char_ptr, unsigned char *last_count_ptr, int *first_chunk_ptr) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("Failed to stat file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    size_t file_size = st.st_size;
    if (file_size == 0) {
        close(fd);
        return;
    }
    char *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_data == MAP_FAILED) {
        perror("Failed to mmap file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    atomic_store(&job_done, (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE);
    for (size_t offset = 0; offset < file_size; offset += CHUNK_SIZE) {
        size_t end = offset + CHUNK_SIZE;
        if (end > file_size) {
            end = file_size;
        }

        Task *task = malloc(sizeof(Task));
        task->data = file_data;
        task->size = file_size;
        task->start = offset;
        task->end = end;
        task->output_capacity = (end - offset) * 2;
        task->output = malloc(task->output_capacity);
        task->output_size = 0;

        enqueue(&queue, task);
    }

    while (atomic_load(&job_done) > 0) {
        usleep(100);
    }

    for (size_t offset = 0; offset < file_size; offset += CHUNK_SIZE) {
        size_t end = offset + CHUNK_SIZE;
        if (end > file_size) {
            end = file_size;
        }

        Task *task = malloc(sizeof(Task));
        task->data = file_data;
        task->size = file_size;
        task->start;
        task->end = end;
        task->output_capacity = (end - offset) * 2;
        task->output = malloc(task->output_capacity);
        task->output_size = 0;

        enqueue(&queue, task);
    }
    while (atomic_load(&job_done) > 0) {
        usleep(100);
    }

    munmap(file_data, file_size);
    close(fd);
}

int main(int argc, char *argv[]) {
    int num_threads = 1;
    int opt;
    while ((opt = getopt(argc, argv, "j:")) != -1) {
        if (opt == 'j') {
            num_threads = atoi(optarg);
            if (num_threads <= 0) {
                fprintf(stderr, "Number of threads must be positive\n");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "Usage: %s [-j num_threads] file...\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected at least one file\n");
        exit(EXIT_FAILURE);
    }

    task_queue_init(&queue);
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    char last_char = 0;
    unsigned char last_count = 0;
    int first_chunk = 1;

    for (int i = optind; i < argc; ++i) {
        encode_file(argv[i], num_threads, &last_char, &last_count, &first_chunk);
    }

    if (!first_chunk) {
        fwrite(&last_char, sizeof(char), 1, stdout);
        fwrite(&last_count, sizeof(unsigned char), 1, stdout);
    }

    for (int i = 0; i < num_threads; ++i) {
        enqueue(&queue, NULL);
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    task_queue_destroy(&queue);
    return 0;
}