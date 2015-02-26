#include <stdio.h>
#include <stdlib.h>
#include "libcsv/csv.h"
#include <unistd.h>
#include <string.h>

#define CHUNK_SIZE 128
#define CSV_MODE_SET 0
#define CSV_MODE_GET 1

#define DEBUG 0

struct parse_info {
	int found_record;
	char *found_value;
	int rows;
	int cols;
	int curr_row;
	int curr_col;
	int tar_row;
	int tar_col;
	int mode;
	char *tar_col_name;
	char *headers[50];
};

int setup_text_col_selector(int col, char *input, struct parse_info *info) {
	// PROBLEM HERE! What if the column name is 0? Or any other number?
	int input_len = strlen(input);
	
	if (input_len == 1 && input[0] == '0') {
		return 0;
	}

	// User supplied a text string instead
	info->tar_col_name = malloc(sizeof(char) * input_len);
	strncpy(info->tar_col_name, input, input_len + 1);
	return -1;
}

// Bug in libcsv: final record does not trigger callback unless there is a return at the end of the file.
void parse_field_cb(void *record, size_t size, void *i) {
	struct parse_info *info = (struct parse_info*)i;

	if (info->curr_row == 0) {
		info->headers[info->curr_col] = malloc(sizeof(char) * strlen(record));
		strcpy(info->headers[info->curr_col], record);
		
		info->cols++;
	}

	if ( (info->curr_row == info->tar_row) && ((info->curr_col == info->tar_col) || (info->tar_col == -1 && strcmp(info->headers[info->curr_col], info->tar_col_name) == 0)) ) {
		info->found_record = 0;

		info->found_value = malloc(sizeof(char) * strlen(record));

		strcpy(info->found_value, record);
	}

	info->curr_col++;
}

void parse_record_cb(int term_char, void *i) {
	struct parse_info *info = (struct parse_info*)i;

	info->curr_row++;
	info->rows++;
	info->curr_col = 0;
}

int get_field_value(FILE *file, struct csv_parser *p, struct parse_info *i)
{
	char *chunk = malloc(sizeof(char) * CHUNK_SIZE);
	size_t read;
	size_t processed;

	while((read = fread(chunk, sizeof(char), CHUNK_SIZE, file)) >= 1) {
		processed = csv_parse(p, chunk, sizeof(char) * read, &parse_field_cb, &parse_record_cb, i);
	}

	free(chunk);
	return 0;
}

// TODO: Add writing capability
int set_field_value(FILE *file, struct csv_parser *p, struct parse_info *i)
{
	printf("Not implemented yet.\n");
	return 0;
}

int main(int argc, char *argv[]) {
	int rc = 0, optch;
	char *set_value = NULL;
	char *get_value = NULL;
	FILE *input;
	const char *valid_opts = "r:c:s:";
	const char *usage =
		"Usage: csvr -r <row number> -c <col number> | <col name> [-s <new value>] [<file>]";
	struct csv_parser *p = malloc(sizeof(struct csv_parser));
	struct parse_info *i = malloc(sizeof(struct parse_info));

	i->rows = i->curr_row = i->cols = i->curr_col = 0;
	i->found_record = 1;

	while((optch = getopt(argc, argv, valid_opts)) != -1) {
		switch(optch) {
			case 'r':
				i->tar_row = atoi(optarg);
				if (i->tar_row < 0) {
					printf("%s\n", "Error: -r <row> must be a positive integer (cheeky!)");
					rc = 1; // Invalid input
					goto error;
				}
				break;
			case 'c':
				i->tar_col = atoi(optarg);
				// Icky code to get around atoi limitation of returning 0 on a failed parse, which is a perfectly reasonable input
				if (i->tar_col == 0) {
					i->tar_col = setup_text_col_selector(i->tar_col, optarg, i);
				}
				break;
			case 's':
				set_value = malloc(sizeof(optarg));
				strcpy(set_value, optarg);
				break;
			default:
				printf("%s\n", usage);
				rc = 1;
				goto error;
		}
	}

	if (optind < 3) {
		printf("%s\n", usage);
		rc = 1;
		goto error;
	}

	if (i->tar_row < 0 && i->tar_col_name == NULL) {
		printf("%s\n", "Error: -c <column> must be a positive integer or a string denoting the column name");
		rc = 1;
		goto error;
	}

	if (optind < argc || argv[optind] != NULL) {
		input = fopen(argv[optind], (set_value == NULL ? "r" : "r+"));
		if (input == NULL) {
			printf("Error: file not found\n");
			rc = 2; // Missing file
			goto error;
		}
	} else {
		input = stdin;
	}

	csv_init(p, CSV_APPEND_NULL);

	if (set_value == NULL) {
		i->mode = CSV_MODE_GET;
		rc = get_field_value(input, p, i);
	} else {
		i->mode = CSV_MODE_SET;
		rc = set_field_value(input, p, i);
	}

	if (i->found_record == 0) {
		if (i->mode == CSV_MODE_GET) {
			printf("%s\n", i->found_value);
		}
	} else {
		rc = 3; // Missing record
		if (i->tar_row >= i->rows) {
			printf("Row offset does not exist.\n");
		}
		
		if (i->tar_col >= i->cols) {
			printf("Column offset does not exist.\n");
		}

		if (i->tar_col == -1 && i->tar_row) {
			//printf("Column name \"%s\" does not exist.\n", i->tar_col_name);
		}
	}

error:
	if (set_value) {
		free(set_value);
	}
	if (get_value) {
		free(get_value);
	}
	free(p);
	free(i);
	return rc;
}