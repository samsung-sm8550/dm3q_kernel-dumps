/*
 * Copyright (c) 1998, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef MACOSX
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statvfs.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>

#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "io_util.h"
#include "io_util_md.h"
#include "java_io_FileSystem.h"
#include "java_io_UnixFileSystem.h"

#include <nativehelper/JNIHelp.h>

#if defined(_AIX)
  #if !defined(NAME_MAX)
    #define NAME_MAX MAXNAMLEN
  #endif
  #define DIR DIR64
  #define opendir opendir64
  #define closedir closedir64
#endif

#if defined(__solaris__) && !defined(NAME_MAX)
  #define NAME_MAX MAXNAMLEN
#endif

// Android-changed: Fuchsia: Alias *64 on Fuchsia builds. http://b/119496969
// #if defined(_ALLBSD_SOURCE)
#if defined(_ALLBSD_SOURCE) || defined(__Fuchsia__)
  #define dirent64 dirent
  #define readdir64 readdir
  #define stat64 stat
  #ifndef MACOSX
    #define statvfs64 statvfs
  #endif
#endif

/* -- Field IDs -- */

static struct {
    jfieldID path;
} ids;


#define NATIVE_METHOD(className, functionName, signature) \
{ #functionName, signature, (void*)(Java_java_io_ ## className ## _ ## functionName) }

JNIEXPORT void JNICALL
Java_java_io_UnixFileSystem_initIDs(JNIEnv *env, jclass cls)
{
    jclass fileClass = (*env)->FindClass(env, "java/io/File");
    if (!fileClass) return;
    ids.path = (*env)->GetFieldID(env, fileClass,
                                  "path", "Ljava/lang/String;");
}

/* -- Path operations -- */

// Android-changed: hidden to avoid conflict with libm (b/135018555)
__attribute__((visibility("hidden")))
extern int canonicalize(char *path, const char *out, int len,
        // Android-added: Remove parent directory /.. at the rootfs. http://b/312399441
        bool isAtLeastTargetSdk35);

JNIEXPORT jstring JNICALL
Java_java_io_UnixFileSystem_canonicalize0(JNIEnv *env, jobject this,
                                          jstring pathname,
        // Android-added: Remove parent directory /.. at the rootfs. http://b/312399441
                                          jboolean isAtLeastTargetSdk35)
{
    jstring rv = NULL;

    WITH_PLATFORM_STRING(env, pathname, path) {
        char canonicalPath[PATH_MAX];
        if (canonicalize((char *)path,
                         canonicalPath, PATH_MAX, isAtLeastTargetSdk35) < 0) {
            JNU_ThrowIOExceptionWithLastError(env, "Bad pathname");
        } else {
#ifdef MACOSX
            rv = newStringPlatform(env, canonicalPath);
#else
            rv = JNU_NewStringPlatform(env, canonicalPath);
#endif
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}


/* -- Attribute accessors -- */


static jboolean
statMode(const char *path, int *mode)
{
    struct stat64 sb;
    if (stat64(path, &sb) == 0) {
        *mode = sb.st_mode;
        return JNI_TRUE;
    }
    return JNI_FALSE;
}


JNIEXPORT jint JNICALL
Java_java_io_UnixFileSystem_getBooleanAttributes0(JNIEnv *env, jobject this,
                                                  jstring abspath)
{
    jint rv = 0;

    /* ----- BEGIN android -----
    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {*/
    WITH_PLATFORM_STRING(env, abspath, path) {
    // ----- END android -----
        int mode;
        if (statMode(path, &mode)) {
            int fmt = mode & S_IFMT;
            rv = (jint) (java_io_FileSystem_BA_EXISTS
                  | ((fmt == S_IFREG) ? java_io_FileSystem_BA_REGULAR : 0)
                  | ((fmt == S_IFDIR) ? java_io_FileSystem_BA_DIRECTORY : 0));
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}

// BEGIN Android-removed: Access files through common interface.
/*
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_checkAccess(JNIEnv *env, jobject this,
                                        jobject file, jint a)
{
    jboolean rv = JNI_FALSE;
    int mode = 0;
    switch (a) {
    case java_io_FileSystem_ACCESS_READ:
        mode = R_OK;
        break;
    case java_io_FileSystem_ACCESS_WRITE:
        mode = W_OK;
        break;
    case java_io_FileSystem_ACCESS_EXECUTE:
        mode = X_OK;
        break;
    default: assert(0);
    }
    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        if (access(path, mode) == 0) {
            rv = JNI_TRUE;
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}
*/
// END Android-removed: Access files through common interface.

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setPermission0(JNIEnv *env, jobject this,
                                           jobject file,
                                           jint access,
                                           jboolean enable,
                                           jboolean owneronly)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        int amode = 0;
        int mode;
        switch (access) {
        case java_io_FileSystem_ACCESS_READ:
            if (owneronly)
                amode = S_IRUSR;
            else
                amode = S_IRUSR | S_IRGRP | S_IROTH;
            break;
        case java_io_FileSystem_ACCESS_WRITE:
            if (owneronly)
                amode = S_IWUSR;
            else
                amode = S_IWUSR | S_IWGRP | S_IWOTH;
            break;
        case java_io_FileSystem_ACCESS_EXECUTE:
            if (owneronly)
                amode = S_IXUSR;
            else
                amode = S_IXUSR | S_IXGRP | S_IXOTH;
            break;
        default:
            assert(0);
        }
        if (statMode(path, &mode)) {
            if (enable)
                mode |= amode;
            else
                mode &= ~amode;
            if (chmod(path, mode) >= 0) {
                rv = JNI_TRUE;
            }
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getLastModifiedTime0(JNIEnv *env, jobject this,
                                                 jobject file)
{
    jlong rv = 0;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        struct stat64 sb;
        if (stat64(path, &sb) == 0) {
#if defined(_AIX)
            rv =  (jlong)sb.st_mtime * 1000;
            rv += (jlong)sb.st_mtime_n / 1000000;
#elif defined(MACOSX)
            rv  = (jlong)sb.st_mtimespec.tv_sec * 1000;
            rv += (jlong)sb.st_mtimespec.tv_nsec / 1000000;
#else
            rv  = (jlong)sb.st_mtim.tv_sec * 1000;
            rv += (jlong)sb.st_mtim.tv_nsec / 1000000;
#endif
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}

// BEGIN Android-removed: Access files through common interface.
/*
JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getLength(JNIEnv *env, jobject this,
                                      jobject file)
{
    jlong rv = 0;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        struct stat64 sb;
        if (stat64(path, &sb) == 0) {
            rv = sb.st_size;
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}
*/
// END Android-removed: Access files through common interface.


/* -- File operations -- */

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_createFileExclusively0(JNIEnv *env, jclass cls,
                                                   jstring pathname)
{
    jboolean rv = JNI_FALSE;

    WITH_PLATFORM_STRING(env, pathname, path) {
        FD fd;
        /* The root directory always exists */
        if (strcmp (path, "/")) {
            fd = handleOpen(path, O_RDWR | O_CREAT | O_EXCL, 0666);
            if (fd < 0) {
                if (errno != EEXIST)
                    JNU_ThrowIOExceptionWithLastError(env, path);
            } else {
                if (close(fd) == -1)
                    JNU_ThrowIOExceptionWithLastError(env, path);
                rv = JNI_TRUE;
            }
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}


// BEGIN Android-removed: Access files through common interface.
/*
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_delete0(JNIEnv *env, jobject this,
                                    jobject file)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        if (remove(path) == 0) {
            rv = JNI_TRUE;
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}
*/
// END Android-removed: Access files through common interface.

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jobjectArray JNICALL
Java_java_io_UnixFileSystem_list0(JNIEnv *env, jobject this,
                                  jobject file)
{
    DIR *dir = NULL;
    struct dirent64 *ptr;
    int len, maxlen;
    jobjectArray rv, old;
    jclass str_class;

    str_class = JNU_ClassString(env);
    CHECK_NULL_RETURN(str_class, NULL);


    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        dir = opendir(path);
    } END_PLATFORM_STRING(env, path);
    if (dir == NULL) return NULL;

    /* Allocate an initial String array */
    len = 0;
    maxlen = 16;
    rv = (*env)->NewObjectArray(env, maxlen, str_class, NULL);
    if (rv == NULL) goto error;

    /* Scan the directory */
    while ((ptr = readdir64(dir)) != NULL) {
        jstring name;
        if (!strcmp(ptr->d_name, ".") || !strcmp(ptr->d_name, ".."))
            continue;
        if (len == maxlen) {
            old = rv;
            rv = (*env)->NewObjectArray(env, maxlen <<= 1, str_class, NULL);
            if (rv == NULL) goto error;
            if (JNU_CopyObjectArray(env, rv, old, len) < 0) goto error;
            (*env)->DeleteLocalRef(env, old);
        }
#ifdef MACOSX
        name = newStringPlatform(env, ptr->d_name);
#else
        name = JNU_NewStringPlatform(env, ptr->d_name);
#endif
        if (name == NULL) goto error;
        (*env)->SetObjectArrayElement(env, rv, len++, name);
        (*env)->DeleteLocalRef(env, name);
    }
    closedir(dir);

    /* Copy the final results into an appropriately-sized array */
    old = rv;
    rv = (*env)->NewObjectArray(env, len, str_class, NULL);
    if (rv == NULL) {
        return NULL;
    }
    if (JNU_CopyObjectArray(env, rv, old, len) < 0) {
        return NULL;
    }
    return rv;

 error:
    closedir(dir);
    return NULL;
}

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_createDirectory0(JNIEnv *env, jobject this,
                                             jobject file)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        if (mkdir(path, 0777) == 0) {
            rv = JNI_TRUE;
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}


// BEGIN Android-removed: Access files through common interface.
/*
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_rename0(JNIEnv *env, jobject this,
                                    jobject from, jobject to)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, from, ids.path, fromPath) {
        WITH_FIELD_PLATFORM_STRING(env, to, ids.path, toPath) {
            if (rename(fromPath, toPath) == 0) {
                rv = JNI_TRUE;
            }
        } END_PLATFORM_STRING(env, toPath);
    } END_PLATFORM_STRING(env, fromPath);
    return rv;
}
*/
// END Android-removed: Access files through common interface.

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setLastModifiedTime0(JNIEnv *env, jobject this,
                                                 jobject file, jlong time)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        struct stat64 sb;

        if (stat64(path, &sb) == 0) {
            struct timeval tv[2];

            /* Preserve access time */
#if defined(_AIX)
            tv[0].tv_sec = sb.st_atime;
            tv[0].tv_usec = sb.st_atime_n / 1000;
#elif defined(MACOSX)
            tv[0].tv_sec = sb.st_atimespec.tv_sec;
            tv[0].tv_usec = sb.st_atimespec.tv_nsec / 1000;
#else
            tv[0].tv_sec = sb.st_atim.tv_sec;
            tv[0].tv_usec = sb.st_atim.tv_nsec / 1000;
#endif
            /* Change last-modified time */
            tv[1].tv_sec = time / 1000;
            tv[1].tv_usec = (time % 1000) * 1000;

            if (utimes(path, tv) == 0)
                rv = JNI_TRUE;
        }
    } END_PLATFORM_STRING(env, path);

    return rv;
}

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jboolean JNICALL
Java_java_io_UnixFileSystem_setReadOnly0(JNIEnv *env, jobject this,
                                         jobject file)
{
    jboolean rv = JNI_FALSE;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
        int mode;
        if (statMode(path, &mode)) {
            if (chmod(path, mode & ~(S_IWUSR | S_IWGRP | S_IWOTH)) >= 0) {
                rv = JNI_TRUE;
            }
        }
    } END_PLATFORM_STRING(env, path);
    return rv;
}

// Android-changed: Name changed because of added thread policy check
JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getSpace0(JNIEnv *env, jobject this,
                                      jobject file, jint t)
{
    jlong rv = 0L;

    WITH_FIELD_PLATFORM_STRING(env, file, ids.path, path) {
#ifdef MACOSX
        struct statfs fsstat;
#else
        struct statvfs64 fsstat;
#endif
        memset(&fsstat, 0, sizeof(fsstat));
#ifdef MACOSX
        if (statfs(path, &fsstat) == 0) {
            switch(t) {
                case java_io_FileSystem_SPACE_TOTAL:
                    rv = jlong_mul(long_to_jlong(fsstat.f_bsize),
                                   long_to_jlong(fsstat.f_blocks));
                    break;
                case java_io_FileSystem_SPACE_FREE:
                    rv = jlong_mul(long_to_jlong(fsstat.f_bsize),
                                   long_to_jlong(fsstat.f_bfree));
                    break;
                case java_io_FileSystem_SPACE_USABLE:
                    rv = jlong_mul(long_to_jlong(fsstat.f_bsize),
                                   long_to_jlong(fsstat.f_bavail));
                    break;
                default:
                    assert(0);
            }
        }
#else
        if (statvfs64(path, &fsstat) == 0) {
            switch(t) {
            case java_io_FileSystem_SPACE_TOTAL:
                rv = jlong_mul(long_to_jlong(fsstat.f_frsize),
                               long_to_jlong(fsstat.f_blocks));
                break;
            case java_io_FileSystem_SPACE_FREE:
                rv = jlong_mul(long_to_jlong(fsstat.f_frsize),
                               long_to_jlong(fsstat.f_bfree));
                break;
            case java_io_FileSystem_SPACE_USABLE:
                rv = jlong_mul(long_to_jlong(fsstat.f_frsize),
                               long_to_jlong(fsstat.f_bavail));
                break;
            default:
                assert(0);
            }
        }
#endif
    } END_PLATFORM_STRING(env, path);
    return rv;
}

JNIEXPORT jlong JNICALL
Java_java_io_UnixFileSystem_getNameMax0(JNIEnv *env, jobject this,
                                        jstring pathname)
{
    jlong length = -1;
    WITH_PLATFORM_STRING(env, pathname, path) {
        length = (jlong)pathconf(path, _PC_NAME_MAX);
    } END_PLATFORM_STRING(env, path);
    return length != -1 ? length : (jlong)NAME_MAX;
}

static JNINativeMethod gMethods[] = {
    NATIVE_METHOD(UnixFileSystem, initIDs, "()V"),
    NATIVE_METHOD(UnixFileSystem, canonicalize0, "(Ljava/lang/String;Z)Ljava/lang/String;"),
    NATIVE_METHOD(UnixFileSystem, getBooleanAttributes0, "(Ljava/lang/String;)I"),
    NATIVE_METHOD(UnixFileSystem, getNameMax0, "(Ljava/lang/String;)J"),
    NATIVE_METHOD(UnixFileSystem, setPermission0, "(Ljava/io/File;IZZ)Z"),
    NATIVE_METHOD(UnixFileSystem, getLastModifiedTime0, "(Ljava/io/File;)J"),
    NATIVE_METHOD(UnixFileSystem, createFileExclusively0, "(Ljava/lang/String;)Z"),
    NATIVE_METHOD(UnixFileSystem, list0, "(Ljava/io/File;)[Ljava/lang/String;"),
    NATIVE_METHOD(UnixFileSystem, createDirectory0, "(Ljava/io/File;)Z"),
    NATIVE_METHOD(UnixFileSystem, setLastModifiedTime0, "(Ljava/io/File;J)Z"),
    NATIVE_METHOD(UnixFileSystem, setReadOnly0, "(Ljava/io/File;)Z"),
    NATIVE_METHOD(UnixFileSystem, getSpace0, "(Ljava/io/File;I)J"),
};

void register_java_io_UnixFileSystem(JNIEnv* env) {
    jniRegisterNativeMethods(env, "java/io/UnixFileSystem", gMethods, NELEM(gMethods));
}
