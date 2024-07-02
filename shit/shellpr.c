#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Function declarations
int process_arglist(int count, char **arglist);
int prepare(void);
int finalize(void);

int main(int argc, char *argv[])
{
	if (prepare() != 0)
		exit(1);

	if (argc == 3 && strcmp(argv[1], "-c") == 0)
	{
		char *command = argv[2];
		printf("Executing command: %s\n", command); // Debug print
		char **arglist = NULL;
		char *line = strdup(command);
		// size_t size;
		int count = 0;

		arglist = (char **)malloc(sizeof(char *));
		if (arglist == NULL)
		{
			printf("malloc failed: %s\n", strerror(errno));
			exit(1);
		}
		arglist[0] = strtok(line, " \t\n");

		while (arglist[count] != NULL)
		{
			++count;
			arglist = (char **)realloc(arglist, sizeof(char *) * (count + 1));
			if (arglist == NULL)
			{
				printf("realloc failed: %s\n", strerror(errno));
				exit(1);
			}
			arglist[count] = strtok(NULL, " \t\n");
		}

		if (count != 0)
		{
			if (!process_arglist(count, arglist))
			{
				free(line);
				free(arglist);
				exit(1);
			}
		}

		free(line);
		free(arglist);
	}
	else
	{
		while (1)
		{
			char **arglist = NULL;
			char *line = NULL;
			size_t size;
			int count = 0;

			if (getline(&line, &size, stdin) == -1)
			{
				free(line);
				break;
			}

			arglist = (char **)malloc(sizeof(char *));
			if (arglist == NULL)
			{
				printf("malloc failed: %s\n", strerror(errno));
				exit(1);
			}
			arglist[0] = strtok(line, " \t\n");

			while (arglist[count] != NULL)
			{
				++count;
				arglist = (char **)realloc(arglist, sizeof(char *) * (count + 1));
				if (arglist == NULL)
				{
					printf("realloc failed: %s\n", strerror(errno));
					exit(1);
				}
				arglist[count] = strtok(NULL, " \t\n");
			}

			if (count != 0)
			{
				if (!process_arglist(count, arglist))
				{
					free(line);
					free(arglist);
					break;
				}
			}

			free(line);
			free(arglist);
		}
	}

	if (finalize() != 0)
		exit(1);

	return 0;
}
