/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nogroup";

static const int blurlevel[NUMLEVELS] = {
       45,     /* after initialization */
       30,   /* during input */
       60,   /* failed/cleared the input */
};
static const Bool failonclear = False;

//Used for multi-threaded blur effect
#define CPU_THREADS 4
