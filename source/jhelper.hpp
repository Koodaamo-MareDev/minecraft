#ifndef _JHELPER_H_
#define _JHELPER_H_
// https://github.com/phillbush/jvm/tree/master
extern "C" int java_launch(const char* java_appname);
extern "C" int java_exec(const char* java_appname);
#endif