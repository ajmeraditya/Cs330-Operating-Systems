#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_PATH_LENGTH 4096

unsigned long calculate_directory_size(const char *path, char *root_path)
{
    unsigned long total_size = 0;
    DIR *dir = opendir(path);

    if (dir == NULL)
    {
        printf("Unable to execute\n");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_path[MAX_PATH_LENGTH];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(entry_path, &st) == -1)
        {
            printf("Unable to execute\n");
            exit(EXIT_FAILURE);
        }

        if (S_ISDIR(st.st_mode))
        {
            // Check if the directory is directly under the root
            if (strcmp(path, root_path) == 0)
            {
                int pipefd[2];
                if (pipe(pipefd) == -1)
                {
                    perror("Unable to execute");
                    exit(EXIT_FAILURE);
                }

                // Fork a child process for this subdirectory
                pid_t child_pid = fork();
                if (child_pid == -1)
                {
                    perror("Unable to execute");
                    exit(EXIT_FAILURE);
                }
                else if (child_pid == 0)
                {
                    // Child process calculates the size for this subdirectory
                    unsigned long sub_dir_size = st.st_size + calculate_directory_size(entry_path, root_path);
                    close(pipefd[0]); // Close the read end of the pipe
                    write(pipefd[1], &sub_dir_size, sizeof(unsigned long));
                    close(pipefd[1]); // Close the write end of the pipe
                    exit(EXIT_SUCCESS);
                }
                else
                {
                    // Parent process collects the size returned by the child
                    close(pipefd[1]); // Close the write end of the pipe
                    unsigned long child_size;
                    waitpid(child_pid, NULL, 0);
                    read(pipefd[0], &child_size, sizeof(unsigned long));
                    close(pipefd[0]); // Close the read end of the pipe
                    total_size += child_size;
                }
            }
            else
            {
                unsigned long sub_dir_size = calculate_directory_size(entry_path, root_path);
                total_size = total_size + sub_dir_size + st.st_size;
            }
        }
        else if (S_ISLNK(st.st_mode))
        {
            char target_path[MAX_PATH_LENGTH];
            ssize_t len = readlink(entry_path, target_path, sizeof(target_path) - 1);
            if (len == -1)
            {
                printf("Unable to execute\n");
                exit(EXIT_FAILURE);
            }
            target_path[len] = '\0'; // Null-terminate the target_path string

            char absolute_target_path[2 * MAX_PATH_LENGTH];
            snprintf(absolute_target_path, sizeof(absolute_target_path), "%s/%s", path, target_path);

            if (lstat(absolute_target_path, &st) == -1)
            {
                printf("Unable to execute\n");
                exit(EXIT_FAILURE);
            }

            if (S_ISDIR(st.st_mode))
            {
                unsigned long sub_dir_size = calculate_directory_size(absolute_target_path, root_path);
                total_size = total_size + sub_dir_size + st.st_size;
            }
            else if (S_ISREG(st.st_mode))
            {
                total_size = total_size + st.st_size;
            }
            else
            {
                printf("Unable to execute\n");
                exit(EXIT_FAILURE);
            }
        }

        else if (S_ISREG(st.st_mode))
        {
            total_size += st.st_size;
        }
    }

    closedir(dir);

    return total_size;
}

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        printf("Unable to execute\n");
        exit(EXIT_FAILURE);
    }

    unsigned long root_size = 0;
    struct stat root_st;

    if (lstat(argv[1], &root_st) == -1)
    {
        printf("Unable to execute\n");
        exit(EXIT_FAILURE);
    }
    root_size = calculate_directory_size(argv[1], argv[1]);
    root_size += root_st.st_size; // Adding the Size of the current directory

    printf("%lu\n", root_size);

    return 0;
}
