/* Copyright (c) 2012-2016 LevelDOWN contributors
 * See list at <https://github.com/level/leveldown#contributing>
 * MIT License <https://github.com/level/leveldown/blob/master/LICENSE.md>
 */
#ifndef LD_LEVELDOWN_H
#define LD_LEVELDOWN_H

#include <node.h>
#include <node_buffer.h>
#include <lmdb.h>
#include <nan.h>

typedef struct md_status {
  int code;
  std::string error;
} md_status;

static inline size_t StringOrBufferLength(v8::Local<v8::Value> obj) {
  Nan::HandleScope scope;

  return (!obj->ToObject().IsEmpty()
    && node::Buffer::HasInstance(obj->ToObject()))
    ? node::Buffer::Length(obj->ToObject())
    : obj->ToString()->Utf8Length();
}

// NOTE: this MUST be called on objects created by
// LD_STRING_OR_BUFFER_TO_SLICE
static inline void DisposeStringOrBufferFromSlice(
        Nan::Persistent<v8::Object> &handle
      , MDB_val val) {
  Nan::HandleScope scope;

  v8::Local<v8::Value> obj = Nan::New<v8::Object>(handle)->Get(Nan::New<v8::String>("obj").ToLocalChecked());
  if (!node::Buffer::HasInstance(obj) || node::Buffer::Length(obj) == 0)
    delete[] (char*)val.mv_data;

  handle.Reset();
}

static inline void DisposeStringOrBufferFromSlice(
        v8::Local<v8::Value> handle
      , MDB_val val) {

  if (!node::Buffer::HasInstance(handle) || node::Buffer::Length(handle) == 0)
    delete[] (char*)val.mv_data;
}

// NOTE: must call DisposeStringOrBufferFromSlice() on objects created here
#define LD_STRING_OR_BUFFER_TO_SLICE(to, from, name)                           \
  size_t to ## Sz_;                                                            \
  char* to ## Ch_;                                                             \
  if (from->IsNull() || from->IsUndefined()) {                                 \
    to ## Sz_ = 1;                                                             \
    to ## Ch_ = new char[to ## Sz_];                                           \
    to ## Ch_[0] = 0;                                                          \
  } else if (!from->ToObject().IsEmpty()                                       \
      && node::Buffer::HasInstance(from->ToObject())) {                        \
    to ## Sz_ = node::Buffer::Length(from->ToObject());                        \
    if (to ## Sz_ == 0) {                                                      \
      to ## Sz_ = 1;                                                           \
      to ## Ch_ = new char[to ## Sz_];                                         \
      to ## Ch_[0] = 0;                                                        \
    } else {                                                                   \
      to ## Ch_ = node::Buffer::Data(from->ToObject());                        \
    }                                                                          \
  } else {                                                                     \
    v8::Local<v8::String> to ## Str = from->ToString();                        \
    to ## Sz_ = to ## Str->Utf8Length();                                       \
    if (to ## Sz_ == 0) {                                                      \
      to ## Sz_ = 1;                                                           \
      to ## Ch_ = new char[to ## Sz_];                                         \
      to ## Ch_[0] = 0;                                                        \
    } else {                                                                   \
      to ## Ch_ = new char[to ## Sz_];                                         \
      to ## Str->WriteUtf8(                                                    \
          to ## Ch_                                                            \
        , -1                                                                   \
        , NULL, v8::String::NO_NULL_TERMINATION                                \
      );                                                                       \
    }                                                                          \
  }                                                                            \
  MDB_val to;                                                                  \
  to.mv_data = to ## Ch_;                                                      \
  to.mv_size = to ## Sz_;

#define LD_STRING_OR_BUFFER_TO_COPY(to, from, name)                            \
  if (from->IsNull() || from->IsUndefined()) {                                 \
    ;                                                                          \
  } else if (!from->ToObject().IsEmpty()                                       \
      && node::Buffer::HasInstance(from->ToObject())) {                        \
    size_t to ## Sz_ = node::Buffer::Length(from->ToObject());                 \
    if (to ## Sz_ != 0) {                                                      \
      to = (MDB_val*)malloc(sizeof(MDB_val));                                  \
      to->mv_size = to ## Sz_;                                                 \
      to->mv_data = (void*)malloc(to->mv_size);                                \
      memcpy(to->mv_data, node::Buffer::Data(from->ToObject()), to->mv_size);  \
    }                                                                          \
  } else {                                                                     \
    v8::Local<v8::String> to ## Str_ = from->ToString();                       \
    size_t to ## Sz_ = to ## Str_->Utf8Length();                               \
    if (to ## Sz_ != 0) {                                                      \
      to = (MDB_val*)malloc(sizeof(MDB_val));                                  \
      to->mv_size = to ## Sz_;                                                 \
      to->mv_data = (void*)malloc(to->mv_size);                                \
      to ## Str_->WriteUtf8(                                                   \
          (char*)to->mv_data                                                   \
        , -1                                                                   \
        , NULL, v8::String::NO_NULL_TERMINATION                                \
      );                                                                       \
    }                                                                          \
  }                                                                            \

#define LD_CREATE_COPY(to, val) do { \
  to = (MDB_val*)malloc(sizeof(MDB_val));                                      \
  to->mv_size = val->mv_size;                                                  \
  to->mv_data = (void*)malloc(val->mv_size);                                   \
  memcpy(to->mv_data, val->mv_data, val->mv_size);                             \
} while (0)

#define LD_FREE_COPY(to) do {                                                  \
  if (to != NULL) {                                                            \
    free(to->mv_data);                                                         \
    free(to);                                                                  \
    to = NULL;                                                                 \
  }                                                                            \
} while (0)

#define LD_RETURN_CALLBACK_OR_ERROR(callback, msg)                             \
  if (!callback.IsEmpty() && callback->IsFunction()) {                         \
    v8::Local<v8::Value> argv[] = {                                            \
      Nan::Error(msg)                                                          \
    };                                                                         \
    LD_RUN_CALLBACK(callback, 1, argv)                                         \
    info.GetReturnValue().SetUndefined();                                      \
    return;                                                                    \
  }                                                                            \
  return Nan::ThrowError(msg);

#define LD_CB_ERR_IF_NULL_OR_UNDEFINED(thing, name)                            \
  if (thing->IsNull() || thing->IsUndefined()) {                               \
    LD_RETURN_CALLBACK_OR_ERROR(callback, #name " cannot be `null` or `undefined`") \
  }

#define LD_RUN_CALLBACK(callback, argc, argv)                                  \
  Nan::MakeCallback(                                                           \
      Nan::GetCurrentContext()->Global(), callback, argc, argv);

/* LD_METHOD_SETUP_COMMON setup the following objects:
 *  - Database* database
 *  - v8::Local<v8::Object> optionsObj (may be empty)
 *  - Nan::Persistent<v8::Function> callback (won't be empty)
 * Will throw/return if there isn't a callback in arg 0 or 1
 */
#define LD_METHOD_SETUP_COMMON(name, optionPos, callbackPos)                   \
  if (info.Length() == 0)                                                      \
    return Nan::ThrowError(#name "() requires a callback argument");           \
  leveldown::Database* database =                                              \
    Nan::ObjectWrap::Unwrap<leveldown::Database>(info.This());                 \
  v8::Local<v8::Object> optionsObj;                                            \
  v8::Local<v8::Function> callback;                                            \
  if (optionPos == -1 && info[callbackPos]->IsFunction()) {                    \
    callback = info[callbackPos].As<v8::Function>();                           \
  } else if (optionPos != -1 && info[callbackPos - 1]->IsFunction()) {         \
    callback = info[callbackPos - 1].As<v8::Function>();                       \
  } else if (optionPos != -1                                                   \
        && info[optionPos]->IsObject()                                         \
        && info[callbackPos]->IsFunction()) {                                  \
    optionsObj = info[optionPos].As<v8::Object>();                             \
    callback = info[callbackPos].As<v8::Function>();                           \
  } else {                                                                     \
    return Nan::ThrowError(#name "() requires a callback argument");           \
  }

#define LD_METHOD_SETUP_COMMON_ONEARG(name) LD_METHOD_SETUP_COMMON(name, -1, 0)

#endif
