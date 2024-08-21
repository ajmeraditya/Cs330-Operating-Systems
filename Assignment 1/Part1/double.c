#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if(argc < 2){
        printf("Unable to execute\n");
        return 0;
    }

    if (argc > 1)
    {
        // Convert the last argument to an integer
        unsigned long lastArg = atol(argv[argc - 1]);

        // Calculate the square of the number
        unsigned long num_double = lastArg * 2;

        // Convert the square back to a string
        sprintf(argv[argc - 1], "%lu", num_double);
    }

    if (argc == 2)
    {
        printf("%s\n", argv[argc - 1]);
    }
    else
    {
        // Prepare the new argument vector for exec
        char *new_argv[argc - 1];

        // Concatenate "./" with the first argument
        char executable_path[20]; // Adjust the size as needed
        snprintf(executable_path, sizeof(executable_path), "./%s", argv[1]);
        new_argv[0] = executable_path;

        // Copy the rest of the arguments
        for (int i = 1; i < argc; i++)
        {
            new_argv[i] = argv[i + 1];
        }

        // Execute the new program
        execvp(new_argv[0], new_argv);

        // If exec fails, print an error message
        printf("Unable to execute\n");
    }

    return 0;
}
