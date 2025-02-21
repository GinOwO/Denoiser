#include <embree3/rtcore.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char const *argv[])
{
	RTCDevice device =
		rtcNewDevice(nullptr); // pass nullptr for default configuration
	if (!device) {
		std::fprintf(stderr, "Error: Unable to create Embree device\n");
		std::exit(1);
	}
	

	return 0;
}
