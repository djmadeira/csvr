#include <stdio.h>
#include <stdlib.h>
#include "libcsv/csv.h"
#include <unistd.h>
#include <string.h>

#define CHUNK_SIZE 128
#define CSV_MODE_SET 0
#define CSV_MODE_GET 1
#define MAX_HEADERS 50

#define DEBUG 0

/**
 * Usage:
 * csvr [-s <set value>] <row> <col name/offset> [file.csv]
 *
 */

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
  FILE *output_file;
  char *tar_col_name;
  char *headers[MAX_HEADERS];
};

/**
 * Create a new parse_info struct, and set it's default values
 * This struct is what is used to store the results returned from the parser.
 */
void Construct_parse_info(struct parse_info *i) {
  i->tar_col_name = NULL;
  i->found_value = NULL;
  i->tar_col = i->tar_row = -1;
  i->curr_row = i->cols = i->curr_col = 0;
  i->found_record = 0;
}

/**
 * Destroy the parse_info struct
 */
void Destruct_parse_info(struct parse_info *i) {
  int j;

  if (i->found_value) {
    free(i->found_value);
  }

  if (i->tar_col_name) {
    free(i->tar_col_name);
  }

  for (j = 0; j < MAX_HEADERS; j++) {
    if (! i->headers[j]) {
      break;
    }
    free(i->headers[j]);
  }

  free(i);
}

/**
 * Figure out if the column given is a string or a number,
 * and save the value to the parse_info struct
 */
int setup_text_col_selector(int col, char *input, struct parse_info *info) {
  // PROBLEM HERE! What if the column **name** in the file is 0? Or any other number?
  int input_len = strlen(input);

  if (input_len == 1 && input[0] == '0') {
    return 0;
  }

  // User supplied a text string instead
  info->tar_col_name = malloc(sizeof(char) * input_len);
  strncpy(info->tar_col_name, input, input_len + 1);
  return -2;
}

/**
 * The callback passed to libcsv for each field (column)
 *
 * Bug in libcsv: final record does not trigger callback unless there is a return at the end of the file.
 */
void parse_field_cb(void *record, size_t size, void *i) {
  struct parse_info *info = (struct parse_info*)i;
  int record_len = strlen(record);

  // Set up the file headers
  // File headers can still be queried for
  if (info->curr_row == 0) {
    info->headers[info->curr_col] = malloc(sizeof(char) * record_len + 1);
    strncpy(info->headers[info->curr_col], record, record_len);
    info->headers[info->curr_col][record_len] = '\0';

    info->cols++;
  }

  if (info->mode == CSV_MODE_SET) {
    if ( (info->curr_row == info->tar_row) &&
        ((info->curr_col == info->tar_col) || (info->tar_col == -2 &&
          strcmp(info->headers[info->curr_col], info->tar_col_name) == 0)) ) {
      info->found_record = 1;
      csv_fwrite(info->output_file, info->found_value, strlen(info->found_value));

    } else {
      csv_fwrite(info->output_file, record, size);
    }
    putc(',', info->output_file);

  } else {
    if ( (info->curr_row == info->tar_row) &&
        ((info->curr_col == info->tar_col) || (info->tar_col == -2 &&
          strcmp(info->headers[info->curr_col], info->tar_col_name) == 0)) ) {
      info->found_record = 1;
      info->found_value = malloc(sizeof(char) * record_len + 1);

      strncpy(info->found_value, record, record_len);
      info->found_value[record_len] = '\0';
    }
  }

  info->curr_col++;
}

/**
 * The callback that is passed to libcsv at the end of a record (row)
 * Simply increments the counters and resets the col index
 */
void parse_record_cb(int term_char, void *i) {
  struct parse_info *info = (struct parse_info*)i;

  if (info->mode == CSV_MODE_SET) {
    // Fix writing an extra , at the end of each line
    fseek(info->output_file, -1, SEEK_CUR);
    putc('\n', info->output_file);
  }

  info->curr_row++;
  info->rows++;
  info->curr_col = 0;
}

/**
 * Gets a single field value from the target CSV file
 * Kicks off libcsv to do so
 * TODO: add error message for a not found header name
 */
int get_field_value(FILE *file, struct csv_parser *p, struct parse_info *i)
{
  char *chunk = malloc(sizeof(char) * CHUNK_SIZE);
  size_t read;
  size_t processed;

  // Main loop
  while((read = fread(chunk, sizeof(char), CHUNK_SIZE, file)) >= 1) {
    processed = csv_parse(p, chunk, sizeof(char) * read, &parse_field_cb, &parse_record_cb, i);
  }

  csv_fini(p, &parse_field_cb, &parse_record_cb, i);
  csv_free(p);
  free(chunk);
  return 0;
}

// TODO: Add writing capability
int set_field_value(FILE *file, struct csv_parser *p, struct parse_info *i)
{
  char *chunk = malloc(sizeof(char) * CHUNK_SIZE);
  size_t read;
  size_t processed;

  // Main loop
  while((read = fread(chunk, sizeof(char), CHUNK_SIZE, file)) >= 1) {
    processed = csv_parse(p, chunk, sizeof(char) * read, &parse_field_cb, &parse_record_cb, i);
  }

  csv_fini(p, &parse_field_cb, &parse_record_cb, i);
  csv_free(p);
  free(chunk);
  return 0;
}

/**
 * Main function
 * Processes options and then kicks off the file search if the file is a valid
 * file pointer.
 */
int main(int argc, char *argv[]) {
  int rc = 0, optch;
  char *set_value = NULL;
  FILE *input;
  const char *valid_opts = "r:c:s:";
  const char *usage =
    "Usage: csvr [-s <new value>] <row> <col index/name> [<file>]";
  struct csv_parser *p = malloc(sizeof(struct csv_parser));
  struct parse_info *i = malloc(sizeof(struct parse_info));
  Construct_parse_info(i);


  // Iterate over all the -x options
  while((optch = getopt(argc, argv, valid_opts)) != -1) {
    switch(optch) {
      // Row
      // Legacy; row & col offset should be passed without the -r / -c flags
      case 'r':
        i->tar_row = atoi(optarg);
        if (i->tar_row < 0) {
          printf("%s\n", "Error: -r <row> must be a positive integer (cheeky!)");
          rc = 1; // Invalid input
          goto error;
        }
        break;
      // Column
      case 'c':
        i->tar_col = atoi(optarg);
        // Icky code to get around atoi limitation of returning 0 on a failed parse,
        // which is a perfectly reasonable input
        if (i->tar_col == 0) {
          i->tar_col = setup_text_col_selector(i->tar_col, optarg, i);
        }
        break;
      // Set to a value
      case 's':
        i->mode = CSV_MODE_SET;
        i->found_value = malloc(sizeof(char) * (strlen(optarg) + 1));
        strncpy(i->found_value, optarg, strlen(optarg) + 1);
        break;
      default:
        printf("%s\n", usage);
        rc = 1;
        goto error;
    }
  }
  // Skip forward
  argc -= optind;
  argv += optind;

  if (!i->mode) {
    i->mode = CSV_MODE_GET;
  }

  int index = 0;

  // Too many options
  if (argc > 3) {
    printf("%s\n", usage);
    rc = 1;
    goto error;
  } 

  switch(argc) {
    // Just the file or nothing
    case 1:
      goto parsefile;
      break;
    // The col and row, or both + file
    case 2:
    case 3:
      goto parsetarget;
  }

parsetarget:
  // Parse row
  i->tar_row = atoi(argv[index]);
  if (i->tar_row < 0) {
    printf("%s\n", "Error: -r <row> must be a positive integer (cheeky!)");
    rc = 1; // Invalid input
    goto error;
  }
  index++;

  i->tar_col = atoi(argv[index]);
  // Icky code to get around atoi limitation of returning 0 on a failed parse,
  // which is a perfectly reasonable input
  if (i->tar_col == 0) {
    i->tar_col = setup_text_col_selector(i->tar_col, argv[index], i);
  }
  index++;

parsefile:
  if (index < argc) {
    input = fopen(argv[index], "r"); 
    if (input == NULL) {
      printf("Error: file not found\n");
      rc = 2; // Missing file
      goto error;
    }
  } else {
    input = stdin;
  }

  if (i->tar_col < 0 && i->tar_col_name == NULL) {
    printf("Error: -c <column> must be a positive integer or a string denoting the column names\n");
    rc = 1;
    goto error;
  }

  // If the target never got set because the option wasn't passed
  if (i->tar_row == -1 || i->tar_col == -1) {
    printf("Error: row or column not set\n");
    printf("%s\n", usage);
    rc = 1;
    goto error;
  }

  csv_init(p, CSV_APPEND_NULL);


  if (i->mode == CSV_MODE_GET) {
    rc = get_field_value(input, p, i);
  } else {
    i->output_file = tmpfile();
    rc = set_field_value(input, p, i);
    fseek(i->output_file, 0, SEEK_SET);
    fclose(input);
    FILE *output = fopen(argv[index], "w");
    if (!output) {
      output = stdout;
    }
    char *chunk = malloc(sizeof(char) * 100);
    int read = 0;
    while((read = fread(chunk, sizeof(char), 100, i->output_file)) > 0) {
      fwrite(chunk, sizeof(char), read, output);
    }
  }

  if (i->found_record == 1) {
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
  Destruct_parse_info(i);
  return rc;
}
