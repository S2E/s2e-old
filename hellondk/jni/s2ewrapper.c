#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include <s2earm.h>

jint Java_ch_epfl_s2e_S2EWrapper_getVersion(JNIEnv * env, jobject this)
{
	return s2e_version();
}

void Java_ch_epfl_s2e_S2EWrapper_printMessage(JNIEnv * env, jobject this, jstring message)
{
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_message(szMsg);
}

void Java_ch_epfl_s2e_S2EWrapper_printWarning(JNIEnv * env, jobject this, jstring message)
{
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_warning(szMsg);
}

void Java_ch_epfl_s2e_S2EWrapper_enableForking(JNIEnv * env, jobject this) {
	s2e_enable_forking();
}

void Java_ch_epfl_s2e_S2EWrapper_disableForking(JNIEnv * env, jobject this) {
	s2e_disable_forking();
}

void Java_ch_epfl_s2e_S2EWrapper_killState(JNIEnv * env, jobject this, jint status, jstring msg) {

	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, msg, &isCopy);
	s2e_kill_state(status,szMsg);
}

jint Java_ch_epfl_s2e_S2EWrapper_getSymbolicInt(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	int x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jdouble Java_ch_epfl_s2e_S2EWrapper_getSymbolicDouble(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	double x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jlong Java_ch_epfl_s2e_S2EWrapper_getSymbolicLong(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	long x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jfloat Java_ch_epfl_s2e_S2EWrapper_getSymbolicFloat(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	float x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jboolean Java_ch_epfl_s2e_S2EWrapper_getSymbolicBoolean(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	int x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    //TODO: jboolean is represented as 8-bit unsigned C type that can store values from 0 to 255.
    //      The value 0 corresponds to the constant JNI_FALSE, and the values from 1 to 255 correspond to JNI_TRUE
    return (jboolean)x;
}

jint Java_ch_epfl_s2e_S2EWrapper_getExampleInt(JNIEnv * env, jobject this,jint var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jdouble Java_ch_epfl_s2e_S2EWrapper_getExampleDouble(JNIEnv * env, jobject this,jdouble var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jlong Java_ch_epfl_s2e_S2EWrapper_getExampleLong(JNIEnv * env, jobject this,jlong var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jfloat Java_ch_epfl_s2e_S2EWrapper_getExampleFloat(JNIEnv * env, jobject this,jfloat var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jboolean Java_ch_epfl_s2e_S2EWrapper_getExampleBoolean(JNIEnv * env, jobject this,jboolean var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jint Java_ch_epfl_s2e_S2EWrapper_concretizeInt(JNIEnv * env, jobject this,jint var) {
	s2e_concretize(&var,sizeof(var));
}

jdouble Java_ch_epfl_s2e_S2EWrapper_concretizeDouble(JNIEnv * env, jobject this,jdouble var) {
	s2e_concretize(&var,sizeof(var));
}

jlong Java_ch_epfl_s2e_S2EWrapper_concretizeLong(JNIEnv * env, jobject this,jlong var) {
	s2e_concretize(&var,sizeof(var));
}

jfloat Java_ch_epfl_s2e_S2EWrapper_concretizeFloat(JNIEnv * env, jobject this,jfloat var) {
	s2e_concretize(&var,sizeof(var));
}

jboolean Java_ch_epfl_s2e_S2EWrapper_concretizeBoolean(JNIEnv * env, jobject this,jboolean var) {
	s2e_concretize(&var,sizeof(var));
}

void Java_ch_epfl_s2e_S2EWrapper_assertThat(JNIEnv * env, jobject this, jboolean condition, jstring failMessage) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, failMessage, &isCopy);
	_s2e_assert(condition,szMsg);
}

void Java_ch_epfl_s2e_S2EWrapper_traceAndroidLocation(JNIEnv * env, jobject this,jstring message) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_android_trace_location(szMsg);
}

void Java_ch_epfl_s2e_S2EWrapper_traceAndroidUID(JNIEnv * env, jobject this,jstring message) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_android_trace_uid(szMsg);
}
