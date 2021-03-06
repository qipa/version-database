#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "dr.h"
#include "db.h"
#include "release.h"
#include "history.h"
#include "list.h"
#include "checkout.h"
#include "diff.h"
#include "patch.h"

#define ARRAYSIZE(arr)	(sizeof(arr)/sizeof(arr[0]))

typedef void (cmd_t)(int argc, char *argv[]);

struct subcmd {
	const char *cmd;
	cmd_t *handler;
};

static void help(int argc, char *argv[]);

static void
init_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	db_init();
}

static void
history_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	history();
}

static void
list_h(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	struct list_args args;
	if (argc < 3) {
		fprintf(stderr, "usage: %s list hash\n", argv[0]);
		exit(EINVAL);
	}
	args.hash = dr_newstr(argv[2]);
	list(&args);
	dr_unref(args.hash);
	return ;
}

static void
release_h(int argc, char *argv[])
{
	int c;
	struct release_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "v:m:d:?")) != -1) {
		switch (c) {
		case 'v':
			args.version = dr_newstr(optarg);
			break;
		case 'm':
			args.describe = dr_newstr(optarg);
			break;
		case 'd':
			args.fromdir = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (args.version == NULL || args.describe == NULL) {
		fprintf(stderr, "usage: %s release -v version -m releasenote -d dir\n",
			argv[0]);
		exit(EINVAL);
	}
	if (args.fromdir == NULL)
		args.fromdir = dr_newstr(".");
	release(&args);
	dr_unref(args.version);
	dr_unref(args.describe);
	dr_unref(args.fromdir);
}

static void
checkout_h(int argc, char *argv[])
{
	dr_t hash, outdir;
	if (argc < 4) {
		fprintf(stderr, "usage: %s checkout hash outdir\n", argv[0]);
		exit(EINVAL);
	}
	hash = dr_newstr(argv[2]);
	outdir = dr_newstr(argv[3]);
	checkout(hash, outdir);
	dr_unref(hash);
	dr_unref(outdir);
	return ;
}

static void
diff_h(int argc, char *argv[])
{
	int c;
	struct diff_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "a:b:o:?")) != -1){
		switch (c) {
		case 'a':
			args.a = dr_newstr(optarg);
			break;
		case 'b':
			args.b = dr_newstr(optarg);
			break;
		case 'o':
			args.p = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (!(args.a && args.b && args.p)) {
		fprintf(stderr, "usage: %s diff -a from -b to -o patch\n",
			argv[0]);
		exit(EINVAL);
	}
	diff(&args);
	dr_unref(args.a);
	dr_unref(args.b);
	dr_unref(args.p);
	return ;
}

static void
patch_h(int argc, char *argv[])
{
	int c;
	struct patch_args args;
	memset(&args, 0, sizeof(args));
	while ((c = getopt(argc, argv, "a:p:o:?")) != -1) {
		switch (c) {
		case 'a':
			args.hash = dr_newstr(optarg);
			break;
		case 'p':
			args.patch = dr_newstr(optarg);
			break;
		case 'o':
			args.output = dr_newstr(optarg);
			break;
		case '?':
			exit(EINVAL);
			break;
		}
	}
	if (!(args.hash && args.patch && args.output)) {
		fprintf(stderr, "usage: %s patch -a hash -p patch -o output\n",
			argv[0]);
		exit(EINVAL);
	}
	patch(&args);
	dr_unref(args.hash);
	dr_unref(args.patch);
	dr_unref(args.output);
	return ;
}

static struct subcmd cmds[] = {
	{"help", help},
	{"init", init_h},
	{"history", history_h},
	{"list", list_h},
	{"release", release_h},
	{"checkout", checkout_h},
	{"diff", diff_h},
	{"patch", patch_h},
};

static void
help(int argc, char *argv[])
{
	int i;
	fprintf(stdout, "sub command list:\n");
	for (i = 0; i < ARRAYSIZE(cmds); i++) {
		fprintf(stdout, "%s\n", cmds[i].cmd);
	}
}

int main(int argc, char *argv[])
{
	int i;
	if (argc < 2)
		goto over;
	for (i = 0; i < ARRAYSIZE(cmds); i++) {
		if (strcmp(cmds[i].cmd, argv[1]) == 0) {
			cmds[i].handler(argc, argv);
			break;
		}
	}
	if (i >= ARRAYSIZE(cmds)) {
		over:
		fprintf(stderr, "usage: %s <command> [<args>]\n", argv[0]);
		exit(EINVAL);
	}
	return 0;
}

