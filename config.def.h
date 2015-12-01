static const int blurlevel[NUMLEVELS] = {
	45,     /* after initialization */
	30,   /* during input */
	60,   /* failed/cleared the input */
};
static const Bool failonclear = False;

//Used for multi-threaded blur effect
#define CPU_THREADS 4 
