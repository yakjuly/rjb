/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005,2006 arton
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id: load.c 14 2006-08-01 13:51:41Z arton $
 */

#include <stdlib.h>
#include <stdio.h>
#include "ruby.h"
#include "intern.h"
#include "st.h"
#include "util.h"
#include "jniwrap.h"
#include "jp_co_infoseek_hp_arton_rjb_RBridge.h"
#include "rjb.h"

#define JVM_TYPE "client"

#if defined(_WIN32) || defined(__CYGWIN__)
 #if defined(__CYGWIN__)
  #define JVMDLL "%s/jre/bin/%s/jvm.dll"
  #define DIRSEPARATOR '/'
 #else
  #define JVMDLL "%s\\jre\\bin\\%s\\jvm.dll"
  #define DIRSEPARATOR '\\'
 #endif
 #define CLASSPATH_SEP  ';'
#elif defined(__APPLE__) && defined(__MACH__)
  #define JVMDLL "%s/Libraries/libjvm_compat.dylib"
  #define DIRSEPARATOR '/'
  #define CLASSPATH_SEP ':'
#else /* defined(_WIN32) || defined(__CYGWIN__) */
 #if defined(__sparc_v9__)
   #define ARCH "sparcv9"
 #elif defined(__sparc__)
   #define ARCH "sparc"
 #elif defined(i586) || defined(__i386__)
  #define ARCH "i386"
 #endif
 #ifndef ARCH
  #include <sys/systeminfo.h>
 #endif
 #define JVMDLL "%s/jre/lib/%s/%s/libjvm.so"
 #define DIRSEPARATOR '/'
 #define CLASSPATH_SEP ':'
#endif

typedef void (*GETDEFAULTJAVAVMINITARGS)(void*);
typedef int (*CREATEJAVAVM)(JavaVM**, void**, void*);

extern JavaVM* jvm;
extern jclass rbridge;
extern jmethodID register_bridge;
static VALUE jvmdll = Qnil;
static VALUE GetDefaultJavaVMInitArgs;
static VALUE CreateJavaVM;

/*
 * not completed, only valid under some circumstances.
 */
static VALUE load_jvm(char* jvmtype)
{
    char* libpath;
    char* java_home;
    char* jh;
    VALUE dl;

#if defined(__APPLE__) && defined(__MACH__)
    jh = "/System/Library/Frameworks/JavaVM.framework";
#else
    jh = getenv("JAVA_HOME");
#endif
    if (!jh)
    {
        return Qnil;
    }
    java_home = ALLOCA_N(char, strlen(jh) + 1);
    strcpy(java_home, jh);
    if (*(java_home + strlen(jh) - 1) == DIRSEPARATOR)
    {
	*(java_home + strlen(jh) - 1) = '\0';
    }
#if defined(_WIN32) || defined(__CYGWIN__)
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home)
		       + strlen(jvmtype) + 1);
    sprintf(libpath, JVMDLL, java_home, jvmtype);
#elif defined(__APPLE__) && defined(__MACH__)
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home) + 1);
    sprintf(libpath, JVMDLL, java_home);
#else /* not Windows / MAC OS-X */
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home)
		       + strlen(ARCH) + strlen(jvmtype) + 1);
    sprintf(libpath, JVMDLL, java_home, ARCH, jvmtype);
#endif
    rb_require("dl");
    dl = rb_eval_string("DL");
    jvmdll = rb_funcall(dl, rb_intern("dlopen"), 1, rb_str_new2(libpath));
    GetDefaultJavaVMInitArgs = rb_funcall(jvmdll, rb_intern("sym"),
			  2, rb_str_new2("JNI_GetDefaultJavaVMInitArgs"),
			     rb_str_new2("0p"));
    CreateJavaVM = rb_funcall(jvmdll, rb_intern("sym"),
			      2, rb_str_new2("JNI_CreateJavaVM"),
			         rb_str_new2("pppp"));
    return Qtrue;
}

int load_bridge(JNIEnv* jenv)
{
    JNINativeMethod nmethod[1];
    jbyte buff[8192];
    char* bridge;
    int len;
    FILE* f;
    jclass loader = (*jenv)->FindClass(jenv, "java/lang/ClassLoader");
    jmethodID getSysLoader = (*jenv)->GetStaticMethodID(jenv, loader,
		   "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jobject iloader = (*jenv)->CallStaticObjectMethod(jenv, loader, getSysLoader);
    VALUE v = rb_const_get_at(rb_const_get(rb_cObject, rb_intern("RjbConf")), 
			      rb_intern("BRIDGE_FILE"));
    bridge = StringValuePtr(v);
#if defined(DOSISH)
    bridge = ALLOCA_N(char, strlen(bridge) + 8);
    strcpy(bridge, StringValuePtr(v));
    for (len = 0; bridge[len]; len++)
    {
	if (bridge[len] == '/')
	{
	    bridge[len] = '\\';
	}
    }
#endif
    f = fopen(bridge, "rb");
    if (f == NULL)
    {
	return -1;
    }
    len = fread(buff, 1, sizeof(buff), f);
    fclose(f);
    rbridge = (*jenv)->DefineClass(jenv,
		   "jp/co/infoseek/hp/arton/rjb/RBridge", iloader, buff, len);
    if (rbridge == NULL)
    {
	check_exception(jenv, 1);
    }
    register_bridge = (*jenv)->GetMethodID(jenv, rbridge, "register",
			   "(Ljava/lang/Class;)Ljava/lang/Object;");
    nmethod[0].name = "call";
    nmethod[0].signature = "(Ljava/lang/String;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;";
    nmethod[0].fnPtr = Java_jp_co_infoseek_hp_arton_rjb_RBridge_call;
    (*jenv)->RegisterNatives(jenv, rbridge, nmethod, 1);
    rbridge = (*jenv)->NewGlobalRef(jenv, rbridge);
    return 0;
}

int create_jvm(JNIEnv** pjenv, JavaVMInitArgs* vm_args, char* userpath, VALUE argv)
{
    static JavaVMOption soptions[] = {
#if defined(__sparc_v9__) || defined(__sparc__)
      { "-Xusealtsigs", NULL },
#elif defined(linux) || defined(__linux__)
      { "-Xrs", NULL },
#elif defined(__APPLE__) && defined(_ARCH_PPC)
      { "-Xrs", NULL },
#endif
#if defined(SHOW_JAVAGC)
      { "-verbose:gc", NULL },
#endif
      { "DUMMY", NULL },   /* for classpath count */
    };
    char* newpath;
    VALUE ptr;
    int len;
    int result;
    GETDEFAULTJAVAVMINITARGS initargs;
    CREATEJAVAVM createjavavm;
    JavaVMOption* options;
    int optlen;
    int i;
    VALUE optval;

    if (jvmdll == Qnil)
    {
	if (load_jvm(JVM_TYPE) == Qnil)
	{
	    return -1;
	}
    }

    ptr = rb_funcall(GetDefaultJavaVMInitArgs, rb_intern("to_i"), 0);
    initargs = *(GETDEFAULTJAVAVMINITARGS*)FIX2INT(ptr);
    initargs(vm_args);
    len = strlen(userpath);
    if (getenv("CLASSPATH"))
    {
        len += strlen(getenv("CLASSPATH"));
    }
    newpath = ALLOCA_N(char, len + 32);
    if (getenv("CLASSPATH"))
    {
        sprintf(newpath, "-Djava.class.path=%s%c%s",
		userpath, CLASSPATH_SEP, getenv("CLASSPATH"));
    }
    else
    {
        sprintf(newpath, "-Djava.class.path=%s", userpath);
    }
    optlen = 0;
    if (!NIL_P(argv))
    {
        optlen += RARRAY(argv)->len;
    }
    optlen += COUNTOF(soptions);
    options = ALLOCA_N(JavaVMOption, optlen);
    options->optionString = newpath;
    options->extraInfo = NULL;
    for (i = 1; i < COUNTOF(soptions); i++)
    {
	*(options + i) = soptions[i - 1];
    }
    for (; i < optlen; i++)
    {
        optval = rb_ary_entry(argv, i - COUNTOF(soptions));
	Check_Type(optval, T_STRING);
	(options + i)->optionString = StringValueCStr(optval);
	(options + i)->extraInfo = NULL;
    }
    vm_args->nOptions = optlen;
    vm_args->options = options;
    vm_args->ignoreUnrecognized = JNI_TRUE;
    ptr = rb_funcall(CreateJavaVM, rb_intern("to_i"), 0);
    createjavavm = *(CREATEJAVAVM*)FIX2INT(ptr);
    result = createjavavm(&jvm, (void**)pjenv, vm_args);
    if (!result)
    {
	result = load_bridge(*pjenv);
    }
    return result;
}

