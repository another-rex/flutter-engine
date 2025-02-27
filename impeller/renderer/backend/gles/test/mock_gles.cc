// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "GLES3/gl3.h"
#include "fml/logging.h"
#include "impeller/renderer/backend/gles/proc_table_gles.h"
#include "impeller/renderer/backend/gles/test/mock_gles.h"

namespace impeller {
namespace testing {

// OpenGLES is not thread safe.
//
// This mutex is used to ensure that only one test is using the mock at a time.
static std::mutex g_test_lock;

static std::weak_ptr<MockGLES> g_mock_gles;

static std::vector<const unsigned char*> g_extensions;

// Has friend visibility into MockGLES to record calls.
void RecordGLCall(const char* name) {
  if (auto mock_gles = g_mock_gles.lock()) {
    mock_gles->RecordCall(name);
  }
}

template <typename T, typename U>
struct CheckSameSignature : std::false_type {};

template <typename Ret, typename... Args>
struct CheckSameSignature<Ret(Args...), Ret(Args...)> : std::true_type {};

// This is a stub function that does nothing/records nothing.
void doNothing() {}

auto const kMockVendor = (unsigned char*)"MockGLES";
auto const kMockVersion = (unsigned char*)"3.0";
auto const kExtensions = std::vector<const unsigned char*>{
    (unsigned char*)"GL_KHR_debug"  //
};

const unsigned char* mockGetString(GLenum name) {
  switch (name) {
    case GL_VENDOR:
      return kMockVendor;
    case GL_VERSION:
      return kMockVersion;
    case GL_SHADING_LANGUAGE_VERSION:
      return kMockVersion;
    default:
      return (unsigned char*)"";
  }
}

static_assert(CheckSameSignature<decltype(mockGetString),  //
                                 decltype(glGetString)>::value);

const unsigned char* mockGetStringi(GLenum name, GLuint index) {
  switch (name) {
    case GL_EXTENSIONS:
      return g_extensions[index];
    default:
      return (unsigned char*)"";
  }
}

static_assert(CheckSameSignature<decltype(mockGetStringi),  //
                                 decltype(glGetStringi)>::value);

void mockGetIntegerv(GLenum name, int* value) {
  switch (name) {
    case GL_NUM_EXTENSIONS: {
      *value = g_extensions.size();
    } break;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      *value = 8;
      break;
    default:
      *value = 0;
      break;
  }
}

static_assert(CheckSameSignature<decltype(mockGetIntegerv),  //
                                 decltype(glGetIntegerv)>::value);

GLenum mockGetError() {
  return GL_NO_ERROR;
}

static_assert(CheckSameSignature<decltype(mockGetError),  //
                                 decltype(glGetError)>::value);

void mockPopDebugGroupKHR() {
  RecordGLCall("PopDebugGroupKHR");
}

static_assert(CheckSameSignature<decltype(mockPopDebugGroupKHR),  //
                                 decltype(glPopDebugGroupKHR)>::value);

void mockPushDebugGroupKHR(GLenum source,
                           GLuint id,
                           GLsizei length,
                           const GLchar* message) {
  RecordGLCall("PushDebugGroupKHR");
}

static_assert(CheckSameSignature<decltype(mockPushDebugGroupKHR),  //
                                 decltype(glPushDebugGroupKHR)>::value);

std::shared_ptr<MockGLES> MockGLES::Init(
    const std::optional<std::vector<const unsigned char*>>& extensions) {
  // If we cannot obtain a lock, MockGLES is already being used elsewhere.
  FML_CHECK(g_test_lock.try_lock())
      << "MockGLES is already being used by another test.";
  g_extensions = extensions.value_or(kExtensions);
  auto mock_gles = std::shared_ptr<MockGLES>(new MockGLES());
  g_mock_gles = mock_gles;
  return mock_gles;
}

const ProcTableGLES::Resolver kMockResolver = [](const char* name) {
  if (strcmp(name, "glPopDebugGroupKHR") == 0) {
    return reinterpret_cast<void*>(&mockPopDebugGroupKHR);
  } else if (strcmp(name, "glPushDebugGroupKHR") == 0) {
    return reinterpret_cast<void*>(&mockPushDebugGroupKHR);
  } else if (strcmp(name, "glGetString") == 0) {
    return reinterpret_cast<void*>(&mockGetString);
  } else if (strcmp(name, "glGetStringi") == 0) {
    return reinterpret_cast<void*>(&mockGetStringi);
  } else if (strcmp(name, "glGetIntegerv") == 0) {
    return reinterpret_cast<void*>(&mockGetIntegerv);
  } else if (strcmp(name, "glGetError") == 0) {
    return reinterpret_cast<void*>(&mockGetError);
  } else {
    return reinterpret_cast<void*>(&doNothing);
  }
};

MockGLES::MockGLES() : proc_table_(kMockResolver) {}

MockGLES::~MockGLES() {
  g_test_lock.unlock();
}

}  // namespace testing
}  // namespace impeller
