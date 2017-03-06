//HW5
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
int main(int argc, char* argv[])
{
	bool opt_h = false;
	bool opt_d = false;
	bool opt_m = false;
	bool opt_t = false;
	char* d_arg = "";
	int opt = getopt(argc, argv, "hd:mt"); {

	while (opt != -1) {

		if (opt == 'h'){
			opt_h = true;
		}
		else if (opt == 'd') {
			opt_d = true;
			d_arg = optarg;
		}
		else if (opt == 'm') {
			opt_m = true;
		}
		else if (opt == 't') {
			opt_t = true;
		}
		else if (opt == '?') {
			
		}

		opt = getopt(argc, argv, "hd:mt");
	}

	if (opt_h == true) {

	}

	if (opt_d == true) {

	}

	if (opt_m == true) {

	}

	if (opt_t == true) {

	}
	return 1;
}