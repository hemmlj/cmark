#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "cmark.h"
#include "debug.h"
#include "bench.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <io.h>
#include <fcntl.h>
#endif

typedef enum {
	FORMAT_NONE,
	FORMAT_HTML,
	FORMAT_XML,
	FORMAT_MAN,
	FORMAT_COMMONMARK,
	FORMAT_LATEX
} writer_format;

void print_usage()
{
	printf("Usage:   cmark [FILE*]\n");
	printf("Options:\n");
	printf("  --to, -t FORMAT  Specify output format (html, xml, man, commonmark, latex)\n");
	printf("  --width WIDTH    Specify wrap width (default 0 = nowrap)\n");
	printf("  --sourcepos      Include source position attribute\n");
	printf("  --hardbreaks     Treat newlines as hard line breaks\n");
	printf("  --smart          Use smart punctuation\n");
	printf("  --normalize      Consolidate adjacent text nodes\n");
	printf("  --help, -h       Print usage information\n");
	printf("  --version        Print version\n");
}

static void print_document(cmark_node *document, writer_format writer,
                           int options, int width)
{
	char *result;

	switch (writer) {
	case FORMAT_HTML:
		result = cmark_render_html(document, options);
		break;
	case FORMAT_XML:
		result = cmark_render_xml(document, options);
		break;
	case FORMAT_MAN:
		result = cmark_render_man(document, options);
		break;
	case FORMAT_COMMONMARK:
		result = cmark_render_commonmark(document, options, width);
		break;
	case FORMAT_LATEX:
		result = cmark_render_latex(document, options, width);
		break;
	default:
		fprintf(stderr, "Unknown format %d\n", writer);
		exit(1);
	}
	printf("%s", result);
	free(result);
}

int main(int argc, char *argv[])
{
	int i, numfps = 0;
	int *files;
	char buffer[4096];
	cmark_parser *parser;
	size_t bytes;
	cmark_node *document;
	int width = 0;
	char *unparsed;
	writer_format writer = FORMAT_HTML;
	int options = CMARK_OPT_DEFAULT;

#if defined(_WIN32) && !defined(__CYGWIN__)
	_setmode(_fileno(stdout), _O_BINARY);
#endif

	files = (int *)malloc(argc * sizeof(*files));

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--version") == 0) {
			printf("cmark %s", CMARK_VERSION_STRING);
			printf(" - CommonMark converter\n(C) 2014, 2015 John MacFarlane\n");
			exit(0);
		} else if (strcmp(argv[i], "--sourcepos") == 0) {
			options |= CMARK_OPT_SOURCEPOS;
		} else if (strcmp(argv[i], "--hardbreaks") == 0) {
			options |= CMARK_OPT_HARDBREAKS;
		} else if (strcmp(argv[i], "--smart") == 0) {
			options |= CMARK_OPT_SMART;
		} else if (strcmp(argv[i], "--normalize") == 0) {
			options |= CMARK_OPT_NORMALIZE;
		} else if (strcmp(argv[i], "--validate-utf8") == 0) {
			options |= CMARK_OPT_VALIDATE_UTF8;
		} else if ((strcmp(argv[i], "--help") == 0) ||
		           (strcmp(argv[i], "-h") == 0)) {
			print_usage();
			exit(0);
		} else if (strcmp(argv[i], "--width") == 0) {
			i += 1;
			if (i < argc) {
				width = (int)strtol(argv[i], &unparsed, 10);
				if (unparsed && strlen(unparsed) > 0) {
					fprintf(stderr,
					        "failed parsing width '%s' at '%s'\n",
					        argv[i], unparsed);
					exit(1);
				}
			} else {
				fprintf(stderr,
				        "--width requires an argument\n");
				exit(1);
			}
		} else if ((strcmp(argv[i], "-t") == 0) ||
		           (strcmp(argv[i], "--to") == 0)) {
			i += 1;
			if (i < argc) {
				if (strcmp(argv[i], "man") == 0) {
					writer = FORMAT_MAN;
				} else if (strcmp(argv[i], "html") == 0) {
					writer = FORMAT_HTML;
				} else if (strcmp(argv[i], "xml") == 0) {
					writer = FORMAT_XML;
				} else if (strcmp(argv[i], "commonmark") == 0) {
					writer = FORMAT_COMMONMARK;
				} else if (strcmp(argv[i], "latex") == 0) {
					writer = FORMAT_LATEX;
				} else {
					fprintf(stderr,
					        "Unknown format %s\n", argv[i]);
					exit(1);
				}
			} else {
				fprintf(stderr, "No argument provided for %s\n",
				        argv[i - 1]);
				exit(1);
			}
		} else if (*argv[i] == '-') {
			print_usage();
			exit(1);
		} else { // treat as file argument
			files[numfps++] = i;
		}
	}

	parser = cmark_parser_new(options);
	for (i = 0; i < numfps; i++) {
		FILE *fp = fopen(argv[files[i]], "r");
		if (fp == NULL) {
			fprintf(stderr, "Error opening file %s: %s\n",
			        argv[files[i]], strerror(errno));
			exit(1);
		}

		start_timer();
		while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
			cmark_parser_feed(parser, buffer, bytes);
			if (bytes < sizeof(buffer)) {
				break;
			}
		}
		end_timer("processing lines");

		fclose(fp);
	}

	if (numfps == 0) {

		while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
			cmark_parser_feed(parser, buffer, bytes);
			if (bytes < sizeof(buffer)) {
				break;
			}
		}
	}

	start_timer();
	document = cmark_parser_finish(parser);
	end_timer("finishing document");
	cmark_parser_free(parser);

	start_timer();
	print_document(document, writer, options, width);
	end_timer("print_document");

	start_timer();
	cmark_node_free(document);
	end_timer("free_blocks");

	free(files);

	return 0;
}
