#pragma once
#include <cmath>

#pragma region 構造体
struct Vector2 {
  float x, y;
};

inline bool operator<(const Vector2 &a, const Vector2 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  return a.y < b.y;
}

inline bool operator!=(const Vector2 &a, const Vector2 &b) {
  return a.x != b.x || a.y != b.y;
}

struct Vector3 {
  float x, y, z;
};

inline bool operator<(const Vector3 &a, const Vector3 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  if (a.y != b.y)
    return a.y < b.y;
  return a.z < b.z;
}

inline bool operator!=(const Vector3 &a, const Vector3 &b) {
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

struct Vector4 {
  float x, y, z, w;
};

inline bool operator<(const Vector4 &a, const Vector4 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  if (a.y != b.y)
    return a.y < b.y;
  if (a.z != b.z)
    return a.z < b.z;
  return a.w < b.w;
}

inline bool operator!=(const Vector4 &a, const Vector4 &b) {
  return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}
typedef struct Matrix4x4 {
  float m[4][4];
} Matrix4x4;

typedef struct Matrix3x3 {
  float m[3][3];
} Matrix3x3;

typedef struct Transform {
  Vector3 scale;
  Vector3 rotate;
  Vector3 translate;
} Ttansform;

#pragma endregion

#pragma region 数学関数

Matrix4x4 MakeIdentity4x4();

Matrix4x4 MakeScaleMatrix(const Vector3 &scale);

Matrix4x4 MakeTranslateMatrix(const Vector3 &translate);

Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);

Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate,
                           const Vector3 &translate);

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio,
                                   float nearClip, float farClip);

Matrix4x4 Inverse(const Matrix4x4 &m);

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
                                 float bottom, float nearClip, float farClip);

#pragma endregion
