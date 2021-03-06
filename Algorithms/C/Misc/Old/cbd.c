/**
 * cbd.c - clipboard handling
 *
 * Copyright (c) 1998
 *      Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "toolkit.h"


jclass     NativeClipboard;
jmethodID  lostOwnership;
jmethodID  createTransferable;
jmethodID  getNativeData;

typedef void ClipBoard;


/*****************************************************************************************
 * internal event handling functions
 *  (note that non of them gets back to Java via events (->return 0);
 */

jobject
selectionClear ( JNIEnv* env, Toolkit* Tlk )
{
UNIMPLEMENTED((
  (*env)->CallStaticVoidMethod( env, NativeClipboard, lostOwnership);
));
  return 0; /* don't pass event to Java */
}

jobject
selectionRequest ( JNIEnv* env, Toolkit* Tlk )
{
UNIMPLEMENTED((
  XEvent     e;
  char       *mime;
  jstring    jMimeType;
  jarray     jData;
  jbyte      *data;
  jboolean   isCopy;
  int        len;
  Atom       target = Tlk->event.xselectionrequest.target;

  if ( target == JAVA_OBJECT )
	mime = "application/x-java-serialized-object";
  else if ( target == XA_STRING )
	mime = "text/plain; charset=unicode";
  else
	mime = XGetAtomName( Tlk->dsp, target);

  jMimeType = (*env)->NewStringUTF( env, (const char*) mime);
  jData = (*env)->CallStaticObjectMethod( env, NativeClipboard, getNativeData, jMimeType);

  if ( jData ) {
	data = (*env)->GetByteArrayElements( env, jData, &isCopy);
	len = (*env)->GetArrayLength( env, jData);

	XChangeProperty( Tlk->dsp,
					 Tlk->event.xselectionrequest.requestor,
					 Tlk->event.xselectionrequest.property,
					 Tlk->event.xselectionrequest.target,
					 8, PropModeReplace, data, len);

	(*env)->ReleaseByteArrayElements( env, jData, data, JNI_ABORT);
	e.xselection.property  = Tlk->event.xselectionrequest.property;
  }
  else {
	e.xselection.property  = None;  /* format not supported */
  }

  e.xselection.type      = SelectionNotify;
  e.xselection.requestor = Tlk->event.xselectionrequest.requestor;
  e.xselection.selection = Tlk->event.xselectionrequest.selection;
  e.xselection.target    = target;
  e.xselection.time      = Tlk->event.xselectionrequest.time;

  XSendEvent( Tlk->dsp, e.xselection.requestor, False, 0, &e);
));

  return 0; /* don't pass event to Java */
}


#if TODO
/*
 * this is the low level helper - get the raw data of the requested 'target' format
 * returns :
 *   -1 if there is no selection owner
 *    0 if the owner cannot prvide the requested format
 *    length of *pData otherwise
 */
int
getRawData ( Toolkit* Tlk, Atom target, unsigned char** pData )
{
UNIMPLEMENTED((
  int              i, format;
  unsigned long    len = 8191, remain = 1;
  Atom             type;
  XEvent           e;

  XConvertSelection( Tlk->dsp, XA_PRIMARY, target, SELECTION_DATA, Tlk->cbdOwner, CurrentTime);

  for ( i=0; i<2; i++ ) {
	XSync( Tlk->dsp, False);
	if ( XCheckTypedWindowEvent( Tlk->dsp, Tlk->cbdOwner, SelectionNotify, &e) ){
	  if ( e.xselection.property == None )  /* target type not supported by owner */
		return 0;

	  while ( remain ) {
		len += remain;
		XGetWindowProperty( Tlk->dsp, Tlk->cbdOwner, SELECTION_DATA, 0, len, False,
							AnyPropertyType, &type, &format, &len, &remain, pData);
	  }
	  return len;
	}
	sleep( 1);
  }
));

  return -1; /* no selection owner at all */
}
#endif /* TODO */

/*****************************************************************************************
 * exported functions
 */

AWT_EXPORT
jobject
Java_java_awt_Toolkit_cbdInitClipboard ( JNIEnv* env, jclass clazz )
{
UNIMPLEMENTED((
  unsigned long mask = 0;
  XSetWindowAttributes attrs;

  attrs.override_redirect = True;
  mask |= CWOverrideRedirect;

  /*
   * We need an appropriate native owner/requestor (Window) since
   * Java requestors/owners can be non-native Windows. We also
   * don't want to introduce artificial events (with all their
   * required infrastructure)
   */
  Tlk->cbdOwner = XCreateWindow( Tlk->dsp, Tlk->root, -10000, -10000, 1, 1, 0, CopyFromParent,
							   InputOutput, CopyFromParent, mask, &attrs);

  NativeClipboard = (*env)->FindClass( env, "java/awt/NativeClipboard");
  lostOwnership   = (*env)->GetStaticMethodID( env, NativeClipboard,
											   "lostOwnership", "()V");
  getNativeData = (*env)->GetStaticMethodID( env, NativeClipboard,
											   "getNativeData", "(Ljava/lang/String;)[B");
  createTransferable = (*env)->GetStaticMethodID( env, NativeClipboard,
												  "createTransferable",
												  "(Ljava/lang/String;[B)Ljava/awt/datatransfer/Transferable;");

  SELECTION_DATA   = XInternAtom( Tlk->dsp, "SELECTION_DATA", False);
  JAVA_OBJECT      = XInternAtom( Tlk->dsp, "application/x-java-serialized-object", False);
));

  return 0;
}

AWT_EXPORT
void
Java_java_awt_Toolkit_cbdFreeClipboard ( JNIEnv* env, jclass clazz, ClipBoard* cbd )
{
}

AWT_EXPORT
jboolean
Java_java_awt_Toolkit_cbdSetOwner ( JNIEnv* env, jclass clazz, ClipBoard* cbd )
{
UNIMPLEMENTED((
  XSetSelectionOwner( Tlk->dsp, XA_PRIMARY, Tlk->cbdOwner, CurrentTime);
  if ( XGetSelectionOwner( Tlk->dsp, XA_PRIMARY) != Tlk->cbdOwner )
	return 0;
  else
	return 1;
));
  return 0;  // !!!
}


/*
 * this is the incoming handler
 * there is no notion of a type request from Java, we therefore have to settle on
 * a stack of suitable formats (with most specific type on top, "fallback" type at bottom)
 */

AWT_EXPORT
jobject
Java_java_awt_Toolkit_cbdGetContents ( JNIEnv* env, jclass clazz, ClipBoard* cbd )
{
UNIMPLEMENTED((
  int             ret;
  unsigned char   *data = 0;
  char            *mime = 0;
  jarray          jdata;
  jstring         jMimeType;

  if ( (ret = getRawData( Tlk, JAVA_OBJECT, &data)) ){
	mime = "application/x-java-serialized-object";
  }
  else if ( (ret == 0) && (ret = getRawData( Tlk, XA_STRING, &data)) ){
	mime = "text/plain; charset=unicode";
  }

  if ( data ) {
	jMimeType = (*env)->NewStringUTF( env, (const char*)mime);
	jdata = (*env)->NewByteArray( env, ret);
	(*env)->SetByteArrayRegion( env, jdata, 0, ret, (jbyte*)data);
	XFree( data);

	return (*env)->CallStaticObjectMethod( env, NativeClipboard, createTransferable,
										   jMimeType, jdata);
  }
  else {
	return 0;
  }
));
  return NULL;  // !!!
}
