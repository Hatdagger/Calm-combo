/* Calm Combo - the bridge between GameView.java and the C front end (calm.h). */
#include <jni.h>
#include <android/bitmap.h>
#include <stdlib.h>
#include <string.h>
#include "calm.h"

#define FN(name) Java_com_hatdagger_calmcombo_GameView_##name

JNIEXPORT jint JNICALL FN(nativeInit)(JNIEnv *env, jclass cls, jobject pak, jbyteArray save)
{
    void *p = (*env)->GetDirectBufferAddress(env, pak);
    jlong len = (*env)->GetDirectBufferCapacity(env, pak);
    jbyte *s = 0;
    jsize sl = 0;
    int r;
    (void)cls;
    if (!p) return -10;
    if (save) { sl = (*env)->GetArrayLength(env, save); s = (*env)->GetByteArrayElements(env, save, 0); }
    r = calm_init(p, (long)len, s, (int)sl);
    if (s) (*env)->ReleaseByteArrayElements(env, save, s, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL FN(nativePickScale)(JNIEnv *env, jclass cls, jint w, jint h)
{ (void)env; (void)cls; return calm_pick_scale(w, h); }

JNIEXPORT void JNICALL FN(nativeResize)(JNIEnv *env, jclass cls, jint w, jint h)
{ (void)env; (void)cls; calm_resize(w, h); }

JNIEXPORT void JNICALL FN(nativeTick)(JNIEnv *env, jclass cls, jint n)
{
    (void)env; (void)cls;
    while (n-- > 0) calm_tick();
}

JNIEXPORT void JNICALL FN(nativeRender)(JNIEnv *env, jclass cls, jobject bmp)
{
    AndroidBitmapInfo info;
    void *px;
    (void)cls;
    if (AndroidBitmap_getInfo(env, bmp, &info) != 0 || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return;
    if (AndroidBitmap_lockPixels(env, bmp, &px) != 0) return;
    calm_render((uint32_t *)px, (int)(info.stride / 4));
    AndroidBitmap_unlockPixels(env, bmp);
}

JNIEXPORT void JNICALL FN(nativeTouch)(JNIEnv *env, jclass cls, jint a, jint x, jint y)
{ (void)env; (void)cls; calm_touch(a, x, y); }

JNIEXPORT void JNICALL FN(nativeKey)(JNIEnv *env, jclass cls, jint k, jboolean down)
{ (void)env; (void)cls; calm_key(k, down ? 1 : 0); }

JNIEXPORT jint JNICALL FN(nativePopSound)(JNIEnv *env, jclass cls)
{ (void)env; (void)cls; return calm_pop_sound(); }

JNIEXPORT jint JNICALL FN(nativeMusic)(JNIEnv *env, jclass cls)
{ (void)env; (void)cls; return calm_music(); }

JNIEXPORT jboolean JNICALL FN(nativeQuit)(JNIEnv *env, jclass cls)
{ (void)env; (void)cls; return calm_quit() ? JNI_TRUE : JNI_FALSE; }

JNIEXPORT jbyteArray JNICALL FN(nativeSave)(JNIEnv *env, jclass cls)
{
    unsigned char buf[CALM_SAVE_MAX];
    int n = calm_save(buf, sizeof buf);
    jbyteArray a;
    (void)cls;
    if (n <= 0) return 0;
    a = (*env)->NewByteArray(env, n);
    if (a) (*env)->SetByteArrayRegion(env, a, 0, n, (const jbyte *)buf);
    return a;
}

JNIEXPORT void JNICALL FN(nativeMedia)(JNIEnv *env, jclass cls, jint flags, jstring title)
{
    const char *t = title ? (*env)->GetStringUTFChars(env, title, 0) : 0;
    (void)cls;
    calm_media(flags, t ? t : "");
    if (t) (*env)->ReleaseStringUTFChars(env, title, t);
}

JNIEXPORT jint JNICALL FN(nativePopMedia)(JNIEnv *env, jclass cls)
{ (void)env; (void)cls; return calm_pop_media(); }
