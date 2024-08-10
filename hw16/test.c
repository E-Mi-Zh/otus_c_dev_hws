#include "logger.h"
#include <stdlib.h>

int main(void) {
	int x = 1;
	double y = 2.57;

	loglevel_on(LINFO);
	LOGINFO("Logging uninited, print to stderr. x = %d, y = %.2f", x, y);

	loginit("syslog.txt");
	loglevel_on(LERR);
	x = x * y;
	y = y / x;
	LOGINFO("Logging inited, printing to file. x = %d, y = %.2f", x, y);
	LOGDEBUG( "x = %d", x);
	LOGERROR( "y = %.2f", y);

	exit(EXIT_SUCCESS);
}