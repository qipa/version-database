#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "sha256.h"
#include "object.h"
#include "dir.h"
#include "dr.h"
#include "db.h"

#define DB		".db"
#define ROOT		DB"/objects"
#define HEAD		DB"/HEAD"
#define HASH_SIZE	(32)

dr_t
db_hash(dr_t d)
{
	int i;
	char *p;
	dr_t hash;
	uint8_t digest[HASH_SIZE];
	sha256_context ctx;
	hash = dr_new(HASH_SIZE * 2, NULL);
	sha256_starts(&ctx);
	sha256_update(&ctx, d->buf, d->size);
	sha256_finish(&ctx, digest);
	p = (char *)hash->buf;
	for (i = 0; i < HASH_SIZE; i++) {
		sprintf(p, "%02x", digest[i]);
		p += 2;
	}
	return hash;

}

static dr_t
fullname(dr_t name)
{
	dr_t full, *list;
	int i, n, off, size;
	char *p, path[PATH_MAX];
	size = name->size;
	if (size >= 2 * HASH_SIZE)
		return dr_ref(name);
	p = (char *)name->buf;
	off = snprintf(path, PATH_MAX, ROOT"/%c%c/", p[0], p[1]);
	assert(off < PATH_MAX);
	n = dir_scan(path, &list, 1);
	p += 2;
	size -= 2;
	full = NULL;
	for (i = 0; i < n; i++) {
		dr_t d;
		d = list[i];
		if (d->size >= size || memcmp(d->buf, p, size) == 0) {
			int len;
			if (full != NULL) {
				fprintf(stderr, "FATAL: '%s' name too short\n",
					p - 2);
				exit(EINVAL);
			}
			len = strlen((char *)d->buf);
			full = dr_new(len + 2, NULL);
			full->buf[0] = p[-2];
			full->buf[1] = p[-1];
			memcpy(&full->buf[2], d->buf, len);
		}
		dr_unref(d);
	}
	free(list);
	return full;
}

dr_t
db_tryread(dr_t name)
{
	const char *p;
	dr_t d, full;
	char path[PATH_MAX];
	full = fullname(name);
	p = (char *)full->buf;
	snprintf(path, PATH_MAX, ROOT"/%c%c/%s", p[0], p[1], &p[2]);
	d = dir_readfile(path, NULL);
	if (d != NULL) {
		dr_t hash = db_hash(d);
		if (dr_cmp(hash, full) != 0) {
			fprintf(stderr, "FATAL:db_read %s content was corrupt\n",
				name->buf);
			exit(EFAULT);
		}
		dr_unref(hash);
	}
	dr_unref(full);
	return d;
}

dr_t
db_read(dr_t name)
{
	dr_t d;
	d = db_tryread(name);
	if (d == NULL) {
		fprintf(stderr, "FATAL:db_read %s errno:%s\n",
			name->buf, strerror(errno));
		exit(errno);
	}
	return d;
}

dr_t
db_write(dr_t name, dr_t d)
{
	dr_t hash, read;
	int n, ret;
	char path[PATH_MAX] ;
	hash = db_hash(d);
	read = db_tryread(hash);
	if (read) {
		int same;
		same = dr_cmp(d, read);
		dr_unref(read);
		if (same != 0) {
			fprintf(stderr, "FATAL:db_write %s hash conflict:%s\n",
				name->buf, hash->buf);
			exit(EEXIST);
		}
	} else {
		char *p;
		p = (char *)hash->buf;
		n = sprintf(path, ROOT"/%c%c", p[0], p[1]);
		ret = mkdir(path, 0755);
		if (ret == -1 && errno != EEXIST) {
			fprintf(stderr, "FATAL:db_write %s hash %s errno:%s\n",
				path, p, strerror(errno));
			exit(errno);
		}
		sprintf(&path[n], "/%s", &p[2]);
		dir_writefile(path, d, NULL);
	}
	return hash;
}

void
db_readrel(struct release *rel, dr_t hash)
{
	dr_t relobj;
	relobj = db_read(hash);
	release_unmarshal(rel, relobj);
	dr_unref(relobj);
	return ;
}

void
db_readtree(struct tree *tree, dr_t hash)
{
	dr_t treeobj = db_read(hash);
	tree_unmarshal(tree, treeobj);
	dr_unref(treeobj);
}

dr_t
db_readhead(struct release *rel, struct tree *tree)
{
	dr_t h;
	h = dir_readfile(HEAD, NULL);
	if (h == NULL) {
		h = dr_new(0, NULL);
		memset(rel, 0, sizeof(*rel));
		if (tree != NULL)
			memset(tree, 0, sizeof(*tree));
	} else {
		db_readrel(rel, h);
		if (tree != NULL)
			db_readtree(tree, rel->tree);
	}
	return h;
}

dr_t
db_writehead(struct release *rel)
{
	dr_t relobj, relhash;
	relobj = release_marshal(rel);
	relhash = db_write(rel->ver, relobj);
	dir_writefile(HEAD, relhash, NULL);
	dr_unref(relobj);
	return relhash;
}

void
db_init()
{
	int err;
	err = mkdir(DB, 0755);
	if (err == 0)
		err = mkdir(ROOT, 0755);
	if (err == 0) {

		return ;
	} else if (errno == EEXIST) {
		fprintf(stderr, "FATAL: not a empty directory\n");
		exit(errno);
	} else {
		fprintf(stderr, "FATAL: init fail '%s'", strerror(errno));
		exit(errno);
	}
}

