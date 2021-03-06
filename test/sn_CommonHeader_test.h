#ifndef SN_TEST_COMMON_HEADER_H
#define SN_TEST_COMMON_HEADER_H

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <codecvt>
#include <sstream>
#include <iomanip>
#include <vector>
#include <utility>
#include <typeindex>
#include <iterator>

#if !defined(_WIN32) || !defined(__GNUC__)
// #include "../src/sn_Alg.hpp" // due to mingw64 + windows byte type conflict
#endif

#include "../src/sn_Decimal.hpp"
#include "../src/sn_Assist.hpp"
#include "../src/sn_Reflection.hpp"
#include "../src/sn_String.hpp"
#include "../src/sn_Log.hpp"
#include "../src/sn_Builtin.hpp"
#include "../src/sn_Thread.hpp"
#include "../src/sn_Stream.hpp"
#include "../src/sn_Type.hpp"
#include "../src/sn_FileSystem.hpp"
#include "../src/sn_Function.hpp"
#include "../src/sn_AOP.hpp"
#include "../src/sn_LC.hpp"
#include "../src/sn_LCEncoding.hpp"
#include "../src/sn_TC.hpp"
#include "../src/sn_Range.hpp"
#include "../src/sn_TypeLisp.hpp"
#include "../src/sn_LINQ.hpp"
#include "../src/sn_StdStream.hpp"
#include "../src/sn_PC.hpp"
#include "../src/sn_PIC.hpp"
#include "../src/sn_PD.hpp"
#include "../src/sn_PM.hpp"
#include "../src/sn_Binary.hpp"
#include "../src/sn_JSON.hpp"
#include "../src/sn_SM.hpp"
#include "../src/sn_Async.hpp"
#include "../src/sn_Concept.hpp"

#endif
