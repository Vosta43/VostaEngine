#pragma once
#ifdef _WIN32
	// Winsock2 first: <Windows.h> pulls in the v1 winsock headers otherwise,
	// and a later <winsock2.h> (ENet) then fails to compile.
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <Windows.h>
#endif // _WIN32

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <functional>
