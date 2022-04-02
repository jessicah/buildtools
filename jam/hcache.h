/*
 * This file is not part of Jam
 */

/*
 * hcache.h - handle #includes in source files
 */

void hcache_init(void);
void hcache_done(void);
StringList hcache(TARGET *t, StringList hdrscan);
